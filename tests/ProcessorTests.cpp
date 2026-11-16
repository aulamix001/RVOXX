#include "../Source/PluginProcessor.h"
#include <iostream>
#include <cstdlib>
static void check(bool ok, const char* what)
{ if (!ok) { std::cerr << "FAIL: " << what << '\n'; std::exit(1); } }
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    RvoxxProcessor p;
    check(p.getParameters().size() == 4, "four automated parameters");
    auto* comp = p.state.getParameter("compression");
    comp->setValueNotifyingHost(comp->convertTo0to1(-18.5f));
    auto* gain = p.state.getParameter("gain");
    gain->setValueNotifyingHost(gain->convertTo0to1(-6));
    check(std::abs(comp->getValueForText("12") - comp->convertTo0to1(-12)) < 1e-6f, "positive text entry becomes negative");
    juce::MemoryBlock saved; p.getStateInformation(saved);
    RvoxxProcessor restored; restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    check(std::abs(restored.readParameters().compression + 18.5f) < 1e-4f, "compression restores from host state");
    check(std::abs(restored.readParameters().gain + 6) < 1e-4f, "gain restores from host state");
    const char invalid[] = "not a preset"; restored.setStateInformation(invalid, sizeof invalid);
    check(std::abs(restored.readParameters().gain + 6) < 1e-4f, "invalid state leaves parameters intact");
    for (int channels : {1, 2})
    {
        auto layout = restored.getBusesLayout();
        layout.inputBuses.set(0, juce::AudioChannelSet::canonicalChannelSet(channels));
        layout.outputBuses.set(0, juce::AudioChannelSet::canonicalChannelSet(channels));
        check(restored.setBusesLayout(layout), "mono/stereo bus accepted");
        restored.prepareToPlay(48000, 512);
        juce::AudioBuffer<float> audio(channels, 512); juce::MidiBuffer midi;
        for (int c = 0; c < channels; ++c) juce::FloatVectorOperations::fill(audio.getWritePointer(c), 3.0f, 512);
        restored.processBlock(audio, midi);
        check(audio.getMagnitude(0, 512) <= rvoxx::toGain(-6) + 1e-6f, "processor enforces output ceiling");
        check(restored.heldL.load() > -160, "output peak hold receives audio");
        restored.releaseResources();
    }
    auto unsupported = restored.getBusesLayout();
    unsupported.inputBuses.set(0, juce::AudioChannelSet::create5point1());
    unsupported.outputBuses.set(0, juce::AudioChannelSet::create5point1());
    check(!restored.isBusesLayoutSupported(unsupported), "surround bus rejected");
    std::cout << "PASS: processor automation, state, buses, limiter and meters\n";
}
