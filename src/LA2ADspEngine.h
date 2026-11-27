#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <algorithm>

class LA2ADspEngine
{
public:
    void prepare(double newSampleRate, int maxBlockSize, int numChannels)
    {
        baseSampleRate = newSampleRate;
        oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
            numChannels, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true);
        oversampler->initProcessing(static_cast<size_t>(maxBlockSize));
        osSampleRate = baseSampleRate * 4.0;

        gainSmoothed.reset(osSampleRate, 0.015);
        prSmoothed.reset(osSampleRate, 0.015);

        updateCoefficients();
        reset();
    }

    int getLatencySamples() const noexcept
    {
        return oversampler ? static_cast<int>(std::round(oversampler->getLatencyInSamples())) : 0;
    }

    void reset()
    {
        if (oversampler) oversampler->reset();
        fbStateL = fbStateR = 0.0;
        scHpStateL = scHpStateR = 0.0;
        elFast = elSlow = 0.0;
        cdsRelFast = cdsRelSlow = 0.0;
        currentGrStateDb = 0.0;
        dcInL = dcInR = dcOutL = dcOutR = 0.0;
        biasEnvL = biasEnvR = 0.0;
        fluxStateL = fluxStateR = 0.0;
        thermalPhase = 0.0;
        shelfL.reset();
        shelfR.reset();
        currentGrDb = 0.0f;
        gainSmoothed.setCurrentAndTargetValue(computeGainLawDb(gainKnob));
        prSmoothed.setCurrentAndTargetValue(peakReduction);
    }

    void setParameters(float peakReductionNorm, float gainNorm, bool isLimitMode, float r37EmphasisNorm = 0.65f)
    {
        peakReduction = juce::jlimit(0.0f, 1.0f, peakReductionNorm);
        gainKnob      = juce::jlimit(0.0f, 1.0f, gainNorm);
        limitMode     = isLimitMode;
        r37Emphasis   = juce::jlimit(0.0f, 1.0f, r37EmphasisNorm);

        gainSmoothed.setTargetValue(computeGainLawDb(gainKnob));
        prSmoothed.setTargetValue(peakReduction);
    }

    float getGainReductionDb() const noexcept { return currentGrDb; }

    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        if (!oversampler) return;

        juce::dsp::AudioBlock<float> block(buffer);
        auto osBlock = oversampler->processSamplesUp(block);

        const int numCh = static_cast<int>(osBlock.getNumChannels());
        const int numSamples = static_cast<int>(osBlock.getNumSamples());
        double slopeGain = limitMode ? 12.0 : 3.75;

        float maxGrInBlock = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            double curGainDb = gainSmoothed.getNextValue();
            double curPr     = prSmoothed.getNextValue();
            double r1GainLin = std::pow(10.0, curGainDb / 20.0);

            double prDriveDb = (curPr <= 0.001) ? -120.0 : (-10.2 + curPr * 55.0);

            double inL = osBlock.getSample(0, i);
            double inR = (numCh > 1) ? osBlock.getSample(1, i) : inL;

            // Делитель T4A в дБ (физический потолок подавления 42.25 дБ по замерам UAD — исключает залипание!)
            double clampedGrDb = juce::jlimit(0.0, 42.25, currentGrStateDb);
            double attenLin = std::pow(10.0, -clampedGrDb / 20.0);
            double postCellL = inL * attenLin;
            double postCellR = inR * attenLin;

            maxGrInBlock = std::max(maxGrInBlock, static_cast<float>(clampedGrDb));

            double detInL = fbStateL;
            double detInR = fbStateR;
            fbStateL = postCellL;
            fbStateR = postCellR;

            scHpStateL += scCoeff * (detInL - scHpStateL);
            scHpStateR += scCoeff * (detInR - scHpStateR);
            double empWeight = 0.10 + 0.35 * r37Emphasis;
            double scL = detInL + empWeight * (detInL - scHpStateL);
            double scR = detInR + empWeight * (detInR - scHpStateR);

            double rect = std::max(std::abs(scL), std::abs(scR));
            double detDb = (rect > 1e-9) ? (20.0 * std::log10(rect) + 3.0103) : -140.0;
            double overDb = detDb + prDriveDb;
            double targetGr = 0.0;
            if (curPr > 0.001 && overDb > -6.0)
            {
                double x = (overDb + 6.0);
                double knee = (x < 9.0) ? (x * x) / 18.0 : (x - 4.5);
                targetGr = knee * slopeGain;
                if (limitMode && overDb > 8.0 && curPr > 0.65)
                    targetGr += 0.14 * (overDb - 8.0) * (curPr - 0.65);
                targetGr = juce::jlimit(0.0, 42.25, targetGr);
            }

            if (targetGr > currentGrStateDb)
            {
                elFast += atkFastCoeff * (targetGr - elFast);
                elSlow += atkSlowCoeff * (targetGr - elSlow);
                currentGrStateDb = 0.45 * elFast + 0.55 * elSlow;
                cdsRelFast = currentGrStateDb;
                cdsRelSlow = currentGrStateDb;
            }
            else
            {
                cdsRelFast += relFastCoeff * (targetGr - cdsRelFast);
                cdsRelSlow += relSlowCoeff * (targetGr - cdsRelSlow);
                currentGrStateDb = 0.97 * cdsRelFast + 0.03 * cdsRelSlow;
                elFast = currentGrStateDb;
                elSlow = currentGrStateDb;
            }

            thermalPhase += thermalInc;
            if (thermalPhase > juce::MathConstants<double>::twoPi)
                thermalPhase -= juce::MathConstants<double>::twoPi;
            double driftMod = 1.0 + 0.025 * std::sin(thermalPhase);

            double outL = processDynamicTubeAndTransformer(postCellL * r1GainLin, dcInL, dcOutL, biasEnvL, fluxStateL, shelfL, 1.0 * driftMod);
            double outR = processDynamicTubeAndTransformer(postCellR * r1GainLin, dcInR, dcOutR, biasEnvR, fluxStateR, shelfR, 1.035 / driftMod);

            osBlock.setSample(0, i, static_cast<float>(outL));
            if (numCh > 1) osBlock.setSample(1, i, static_cast<float>(outR));
        }

        oversampler->processSamplesDown(block);
        currentGrDb = maxGrInBlock;
    }

private:
    void updateCoefficients()
    {
        auto alpha = [&](double ms) { return 1.0 - std::exp(-1.0 / (osSampleRate * ms * 0.001)); };
        atkFastCoeff = alpha(1.2);
        atkSlowCoeff = alpha(10.0);
        relFastCoeff = alpha(115.0);
        relSlowCoeff = alpha(320.0); // Быстрое восстановление после 100% экстремума
        scCoeff      = alpha(0.16);
        biasCoeff    = alpha(35.0);
        fluxCoeff    = alpha(1.8);
        dcCoeff      = 1.0 - (2.0 * juce::MathConstants<double>::pi * 2.5 / osSampleRate);
        thermalInc   = (2.0 * juce::MathConstants<double>::pi * 0.23) / osSampleRate;

        auto coeffs = juce::dsp::IIR::Coefficients<double>::makeHighShelf(
            osSampleRate, 13500.0, 0.707, juce::Decibels::decibelsToGain(0.85));
        shelfL.coefficients = coeffs;
        shelfR.coefficients = coeffs;
    }

    static double computeGainLawDb(float g)
    {
        if (g <= 0.20f) return -20.0 + (g / 0.20) * 20.0;
        if (g <= 0.30f) return ((g - 0.20) / 0.10) * 5.61;
        if (g <= 0.50f) return 5.61 + ((g - 0.30) / 0.20) * 8.89;
        if (g <= 0.75f) return 14.50 + ((g - 0.50) / 0.25) * 15.75;
        return 30.25 + ((g - 0.75) / 0.25) * 7.36;
    }

    inline double processDynamicTubeAndTransformer(double x, double& dcIn, double& dcOut,
                                                   double& biasEnv, double& fluxState,
                                                   juce::dsp::IIR::Filter<double>& shelf, double channelTolerance)
    {
        double voiced = shelf.processSample(x);

        double absX = std::abs(voiced);
        biasEnv += biasCoeff * (absX - biasEnv);
        double dynamicBias = 0.08 * biasEnv;
        double dynH2Coeff  = (0.020 + 0.018 * std::min(1.5, biasEnv)) * channelTolerance;

        double shifted = voiced + dynamicBias;
        double tube = shifted + dynH2Coeff * (shifted * shifted);

        double ac = tube - dcIn + dcCoeff * dcOut;
        dcIn = tube;
        dcOut = ac;

        fluxState += fluxCoeff * (ac - fluxState);
        double coreDrive = ac + 0.18 * fluxState;

        constexpr double ceiling = 2.304;
        double saturated = ceiling * std::tanh(coreDrive / ceiling);

        return saturated - 0.18 * fluxState * (1.0 / (1.0 + std::abs(coreDrive / ceiling)));
    }

    double baseSampleRate = 48000.0, osSampleRate = 192000.0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::SmoothedValue<double> gainSmoothed, prSmoothed;

    float peakReduction = 0.0f, gainKnob = 0.20f, r37Emphasis = 0.65f, currentGrDb = 0.0f;
    bool limitMode = false;

    double fbStateL = 0.0, fbStateR = 0.0;
    double scHpStateL = 0.0, scHpStateR = 0.0;
    double elFast = 0.0, elSlow = 0.0, cdsRelFast = 0.0, cdsRelSlow = 0.0;
    double currentGrStateDb = 0.0;

    double atkFastCoeff = 0.0, atkSlowCoeff = 0.0, relFastCoeff = 0.0, relSlowCoeff = 0.0;
    double scCoeff = 0.0, biasCoeff = 0.0, fluxCoeff = 0.0, dcCoeff = 0.9999;
    double dcInL = 0.0, dcInR = 0.0, dcOutL = 0.0, dcOutR = 0.0;
    double biasEnvL = 0.0, biasEnvR = 0.0, fluxStateL = 0.0, fluxStateR = 0.0;
    double thermalPhase = 0.0, thermalInc = 0.0;
    juce::dsp::IIR::Filter<double> shelfL, shelfR;
};
