#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
void holdMax(std::atomic<float>& meter, float value)
{ auto old = meter.load(); while (value > old && !meter.compare_exchange_weak(old, value)) {} }
void holdMin(std::atomic<float>& meter, float value)
{ auto old = meter.load(); while (value < old && !meter.compare_exchange_weak(old, value)) {} }
}
juce::AudioProcessorValueTreeState::ParameterLayout RvoxxProcessor::makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto add = [&layout](const char* id, const char* name, float low, float high, float initial)
    {
        const bool release = juce::String(id) == "gateRelease";
        auto attributes = juce::AudioParameterFloatAttributes().withLabel(release ? "ms" : "dB")
            .withStringFromValueFunction([release](float v, int) { return !release && v <= -80 ? juce::String("-inf") : juce::String(v, 1); })
            .withValueFromStringFunction([release, low](const juce::String& text)
            { if (text.containsIgnoreCase("inf")) return low; const auto v = text.getFloatValue(); return release ? v : -std::abs(v); });
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(id, 1), name,
            juce::NormalisableRange<float>(low, high, 0.1f), initial, attributes));
    };
    add("compression", "Compression", -36, 0, 0);
    add("gate", "Gate", -80, 0, -80);
    add("gain", "Gain (Ceiling)", -36, 0, 0);
    add("gateRelease", "Gate Release", 10, 250, 130);
    return layout;
}
RvoxxProcessor::RvoxxProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)), state(*this, &undo, "RVOXXState", makeLayout()) {}
bool RvoxxProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto set = layout.getMainInputChannelSet();
    return (set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo()) && set == layout.getMainOutputChannelSet();
}
rvoxx::Parameters RvoxxProcessor::readParameters() const
{
    return {state.getRawParameterValue("compression")->load(), state.getRawParameterValue("gate")->load(),
        state.getRawParameterValue("gain")->load(), state.getRawParameterValue("gateRelease")->load()};
}
void RvoxxProcessor::prepareToPlay(double rate, int)
{
    dynamics.prepare(rate, readParameters());
    energy = -80; reduction = 0; gateReduction = 0; outL = outR = -160;
    heldReduction = 0; heldL = heldR = -160;
    setLatencySamples(0);
}
void RvoxxProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumSamples() == 0) return;
    const auto meters = dynamics.process(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples(), readParameters());
    energy.store(meters.energy); reduction.store(meters.reduction);
    gateReduction.store(meters.gateReduction);
    outL.store(meters.output[0]); outR.store(meters.output[1]);
    holdMin(heldReduction, meters.reduction); holdMax(heldL, meters.output[0]); holdMax(heldR, meters.output[1]);
}
void RvoxxProcessor::getStateInformation(juce::MemoryBlock& dest)
{ if (const auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest); }
void RvoxxProcessor::setStateInformation(const void* data, int size)
{
    if (const auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessorEditor* RvoxxProcessor::createEditor() { return new RvoxxEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RvoxxProcessor; }
