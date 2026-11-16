#pragma once
#include "PluginProcessor.h"

class RvoxxLook final : public juce::LookAndFeel_V4
{
public:
    RvoxxLook();
    juce::Label* createSliderTextBox(juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout(juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float,
        juce::Slider::SliderStyle, juce::Slider&) override;
};
class RvoxxCompSlider final : public juce::Slider
{
public:
    void mouseDoubleClick(const juce::MouseEvent&) override { showTextBox(); }
};
class RvoxxEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RvoxxEditor(RvoxxProcessor&);
    ~RvoxxEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    void choosePreset(bool save);
    void switchAB();
    void chooseArtwork();
    void updateArtworkLayout();
    float designHeight() const { return artwork.isValid() ? 630.0f : 452.0f; }
    RvoxxProcessor& processor;
    RvoxxLook look;
    juce::Slider gate, gain;
    RvoxxCompSlider comp;
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> gateAttachment, compAttachment, gainAttachment;
    juce::TextButton undo { "Undo" }, redo { "Redo" }, ab { "A/B" }, save { "Save" }, load { "Load" };
    juce::ToggleButton link { "Link" };
    juce::ComboBox skin;
    juce::TooltipWindow tooltips { this, 700 };
    juce::TextButton imageButton { "Imagem" };
    juce::Image artwork;
    juce::TextButton reductionHold, leftHold, rightHold;
    juce::MemoryBlock alternate;
    bool alternateActive = false, adjustingLink = false;
    double lastComp = 0;
    float energy = -160, attenuation = 0, left = -160, right = -160;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Point<int> selectionStart;
    juce::Rectangle<int> selection;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RvoxxEditor)
};
