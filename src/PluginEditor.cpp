#include "PluginEditor.h"

LA2ACloneAudioProcessorEditor::LA2ACloneAudioProcessorEditor(LA2ACloneAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    panelImg     = juce::ImageCache::getFromMemory(BinaryData::LA2A_3D_Front_Panel_png, BinaryData::LA2A_3D_Front_Panel_pngSize);
    vuBodyOnImg  = juce::ImageCache::getFromMemory(BinaryData::LA2A_3D_VU_Body_ON_png, BinaryData::LA2A_3D_VU_Body_ON_pngSize);
    vuBodyOffImg = juce::ImageCache::getFromMemory(BinaryData::LA2A_3D_VU_Body_OFF_png, BinaryData::LA2A_3D_VU_Body_OFF_pngSize);
    vuNeedleImg  = juce::ImageCache::getFromMemory(BinaryData::LA2A_3D_VU_Needle_png, BinaryData::LA2A_3D_VU_Needle_pngSize);
    pilotOnImg   = juce::ImageCache::getFromMemory(BinaryData::LA2A_3D_Pilot_ON_png, BinaryData::LA2A_3D_Pilot_ON_pngSize);
    pilotOffImg  = juce::ImageCache::getFromMemory(BinaryData::LA2A_3D_Pilot_OFF_png, BinaryData::LA2A_3D_Pilot_OFF_pngSize);

    for (auto* s : { &gainSlider, &prSlider }) {
        s->setLookAndFeel(&mainKnobLnf);
        s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(s);
    }

    empSlider.setLookAndFeel(&r37KnobLnf);
    empSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    empSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(empSlider);

    if (auto* modeParam = p.apvts.getParameter("mode")) {
        modeToggle.setToggleState(modeParam->getValue() > 0.5f, juce::dontSendNotification);
        modeToggle.onClick = [this, modeParam]() {
            modeParam->beginChangeGesture();
            modeParam->setValueNotifyingHost(modeToggle.getToggleState() ? 1.0f : 0.0f);
            modeParam->endChangeGesture();
        };
    }
    addAndMakeVisible(modeToggle);

    if (auto* bypParam = p.apvts.getParameter("bypass")) {
        powerToggle.setToggleState(bypParam->getValue() < 0.5f, juce::dontSendNotification);
        powerToggle.onClick = [this, bypParam]() {
            bool isPowerOn = powerToggle.getToggleState();
            bypParam->beginChangeGesture();
            bypParam->setValueNotifyingHost(isPowerOn ? 0.0f : 1.0f);
            bypParam->endChangeGesture();
            repaint();
        };
    }
    addAndMakeVisible(powerToggle);

    gainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "gain", gainSlider);
    prAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "peakReduction", prSlider);
    empAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "emphasis", empSlider);

    startTimerHz(40);
    setSize(1100, 480);
}

LA2ACloneAudioProcessorEditor::~LA2ACloneAudioProcessorEditor()
{
    gainSlider.setLookAndFeel(nullptr);
    prSlider.setLookAndFeel(nullptr);
    empSlider.setLookAndFeel(nullptr);
}

void LA2ACloneAudioProcessorEditor::timerCallback()
{
    if (auto* modeParam = audioProcessor.apvts.getParameter("mode"))
        modeToggle.setToggleState(modeParam->getValue() > 0.5f, juce::dontSendNotification);
    if (auto* bypParam = audioProcessor.apvts.getParameter("bypass"))
        powerToggle.setToggleState(bypParam->getValue() < 0.5f, juce::dontSendNotification);

    bool isPowerOn = powerToggle.getToggleState();
    float targetGr = isPowerOn ? audioProcessor.getCurrentGainReductionDb() : 20.0f;
    float coeff = (targetGr > smoothedMeterGr) ? 0.35f : 0.14f;
    smoothedMeterGr += coeff * (targetGr - smoothedMeterGr);
    repaint();
}

void LA2ACloneAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

    if (panelImg.isValid())
        g.drawImage(panelImg, getLocalBounds().toFloat());

    bool isPowerOn = powerToggle.getToggleState();

    // Подсветка VU-метра переключается между LA2A_3D_VU_Body_ON и LA2A_3D_VU_Body_OFF
    juce::Rectangle<float> vuRect(106.0f, 52.0f, 316.0f, 234.0f);
    auto& vuBody = isPowerOn ? vuBodyOnImg : vuBodyOffImg;
    if (vuBody.isValid())
        g.drawImage(vuBody, vuRect);

    if (vuNeedleImg.isValid()) {
        float clampedGr = juce::jlimit(0.0f, 20.0f, smoothedMeterGr);
        // -16.5° ставит стрелку точно на деление "0" в покое, при росте GR отклоняет влево до "-20" (-84°)
        float angleDeg = -18.8f - (clampedGr / 20.0f) * 65.2f;

        juce::Point<float> pivot(vuRect.getX() + vuRect.getWidth() * 0.50f,
                                 vuRect.getY() + vuRect.getHeight() * 0.835f);

        g.saveState();
        g.reduceClipRegion(vuRect.reduced(18.0f, 18.0f).toNearestInt());

        auto transform = juce::AffineTransform::scale(vuRect.getWidth() / vuNeedleImg.getWidth(),
                                                      vuRect.getHeight() / vuNeedleImg.getHeight())
                             .translated(vuRect.getX(), vuRect.getY())
                             .rotated(juce::degreesToRadians(angleDeg), pivot.x, pivot.y);
        g.drawImageTransformed(vuNeedleImg, transform, false);
        g.restoreState();
    }

    auto& pilotImg = isPowerOn ? pilotOnImg : pilotOffImg;
    if (pilotImg.isValid())
        g.drawImage(pilotImg, juce::Rectangle<float>(680.0f, 314.0f, 68.0f, 68.0f));
}

void LA2ACloneAudioProcessorEditor::resized()
{
    gainSlider.setBounds(564 - 104, 250 - 104, 208, 208);
    prSlider.setBounds(865 - 104, 250 - 104, 208, 208);
    empSlider.setBounds(434 - 18, 152 - 18, 36, 36);
    modeToggle.setBounds(155 - 32, 345 - 38, 64, 77);
    powerToggle.setBounds(264 - 32, 345 - 38, 64, 77);
}






