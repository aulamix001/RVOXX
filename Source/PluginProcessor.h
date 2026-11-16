#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Dynamics.h"

class RvoxxProcessor final : public juce::AudioProcessor
{
public:
    RvoxxProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    using AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "RVOXX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    rvoxx::Parameters readParameters() const;
    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState state;
    std::atomic<float> energy { -80 }, reduction { 0 }, gateReduction { 0 }, outL { -160 }, outR { -160 };
    std::atomic<float> heldReduction { 0 }, heldL { -160 }, heldR { -160 };
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    rvoxx::Dynamics dynamics;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RvoxxProcessor)
};
