#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class FilmstripKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FilmstripKnobLookAndFeel(const void* data, int dataSize, int numFrames)
        : frames(numFrames)
    {
        strip = juce::ImageCache::getFromMemory(data, dataSize);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float, float, juce::Slider&) override
    {
        if (!strip.isValid() || frames <= 0) return;
        const int frameW = strip.getWidth();
        const int frameH = strip.getHeight() / frames;
        int idx = juce::jlimit(0, frames - 1, static_cast<int>(std::round(sliderPosProportional * (frames - 1))));

        int drawSize = std::min(width, height);
        int drawX = x + (width - drawSize) / 2;
        int drawY = y + (height - drawSize) / 2;

        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(strip, drawX, drawY, drawSize, drawSize, 0, idx * frameH, frameW, frameH);
    }

private:
    juce::Image strip;
    int frames;
};

class LA2AToggleButton : public juce::ToggleButton
{
public:
    LA2AToggleButton()
    {
        imgUp   = juce::ImageCache::getFromMemory(BinaryData::LA2A_3D_Toggle_UP_png, BinaryData::LA2A_3D_Toggle_UP_pngSize);
        imgDown = juce::ImageCache::getFromMemory(BinaryData::LA2A_3D_Toggle_DOWN_png, BinaryData::LA2A_3D_Toggle_DOWN_pngSize);
    }

    void paintButton(juce::Graphics& g, bool, bool) override
    {
        auto& img = getToggleState() ? imgUp : imgDown;
        if (img.isValid()) {
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(img, getLocalBounds().toFloat());
        }
    }

private:
    juce::Image imgUp, imgDown;
};

class LA2ACloneAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LA2ACloneAudioProcessorEditor(LA2ACloneAudioProcessor&);
    ~LA2ACloneAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    LA2ACloneAudioProcessor& audioProcessor;
    FilmstripKnobLookAndFeel mainKnobLnf { BinaryData::LA2A_3D_Knob_Strip64_png, BinaryData::LA2A_3D_Knob_Strip64_pngSize, 64 };
    FilmstripKnobLookAndFeel r37KnobLnf  { BinaryData::LA2A_3D_R37_Strip64_png,  BinaryData::LA2A_3D_R37_Strip64_pngSize,  64 };

    juce::Image panelImg, vuBodyOnImg, vuBodyOffImg, vuNeedleImg, pilotOnImg, pilotOffImg;
    juce::Slider gainSlider, prSlider, empSlider;
    LA2AToggleButton modeToggle, powerToggle;

    float smoothedMeterGr = 0.0f;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAtt, prAtt, empAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LA2ACloneAudioProcessorEditor)
};
