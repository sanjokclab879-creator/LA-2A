#include "PluginProcessor.h"
#include "PluginEditor.h"

LA2ACloneAudioProcessor::LA2ACloneAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout LA2ACloneAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>("peakReduction", "Peak Reduction", 0.0f, 100.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gain", "Gain", 0.0f, 100.0f, 23.7f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("mode", "Mode", juce::StringArray{ "Compress", "Limit" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("emphasis", "R37 Emphasis", 0.0f, 100.0f, 65.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("bypass", "Power Bypass", false));
    return { params.begin(), params.end() };
}

bool LA2ACloneAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void LA2ACloneAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    dspEngine.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    int lat = dspEngine.getLatencySamples();
    setLatencySamples(lat);

    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32>(samplesPerBlock),
                                  static_cast<juce::uint32>(getTotalNumOutputChannels()) };
    bypassDelay.prepare(spec);
    bypassDelay.setDelay(static_cast<float>(lat));
    bypassDelay.reset();
    wasBypassed = false;
}

void LA2ACloneAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (!wasBypassed)
    {
        dspEngine.reset();
        wasBypassed = true;
    }

    // Пропускаем чистый сигнал через линию задержки латентности, чтобы в DAW не было сдвига фазы
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    bypassDelay.process(context);
}

void LA2ACloneAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    bool isPowerOff = apvts.getRawParameterValue("bypass")->load() > 0.5f;
    if (isPowerOff)
    {
        processBlockBypassed(buffer, midi);
        return;
    }

    if (wasBypassed)
    {
        dspEngine.reset();
        wasBypassed = false;
    }

    // Держим линию задержки актуальной для мгновенного бесшовного выключения POWER
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* src = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            bypassDelay.pushSample(ch, src[i]);
    }

    float pr   = apvts.getRawParameterValue("peakReduction")->load() / 100.0f;
    float gain = apvts.getRawParameterValue("gain")->load() / 100.0f;
    bool limit = apvts.getRawParameterValue("mode")->load() > 0.5f;
    float emp  = apvts.getRawParameterValue("emphasis")->load() / 100.0f;

    dspEngine.setParameters(pr, gain, limit, emp);
    dspEngine.processBlock(buffer);
}

void LA2ACloneAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, destData);
}

void LA2ACloneAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            dspEngine.reset();
        }
    }
}

juce::AudioProcessorEditor* LA2ACloneAudioProcessor::createEditor()
{
    return new LA2ACloneAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LA2ACloneAudioProcessor();
}
