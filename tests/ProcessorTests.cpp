#include "../Source/PluginProcessor.h"
#include <iostream>
#include <cstdlib>
static void check(bool ok, const char* what)
{ if (!ok) { std::cerr << "FAIL: " << what << '\n'; std::exit(1); } }
int main(int argc, char** argv)
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
    {
        RvoxxProcessor ui;
        ui.prepareToPlay(48000, 512);
        check(ui.readParameters().gate == -80, "fresh instance starts with gate OFF");
        juce::AudioBuffer<float> silent(2, 512); silent.clear(); juce::MidiBuffer midi;
        auto* gateParam = ui.state.getParameter("gate");
        gateParam->setValueNotifyingHost(gateParam->convertTo0to1(-78.2f));
        for (int i = 0; i < 100; ++i) ui.processBlock(silent, midi);
        check(ui.reduction.load() == 0 && ui.heldReduction.load() == 0, "enabled gate does not create a false red bar or held reduction in silence");
        check(ui.energy.load() <= -159 && ui.heldL.load() <= -159, "silent levels retain their infinity sentinel");
        gateParam->setValueNotifyingHost(gateParam->convertTo0to1(-80));
        std::unique_ptr<juce::AudioProcessorEditor> editor(ui.createEditor());
        check(editor != nullptr && editor->getWidth() == 475 && editor->getHeight() >= 452, "editor opens at supported size");
        int sliders = 0; bool imageButton = false, zeroReduction = false, infiniteOutput = false;
        for (auto* child : editor->getChildren())
        {
            if (auto* slider = dynamic_cast<juce::Slider*>(child))
            {
                ++sliders;
                if (slider->getName() == "Gate")
                {
                    check(slider->getValue() == -80, "opening editor leaves gate OFF");
                    check(slider->getTextFromValue(-80) == "-Inf", "gate endpoint displays -Inf without units or a numeric floor");
                }
            }
            if (auto* button = dynamic_cast<juce::TextButton*>(child))
            {
                imageButton |= button->getButtonText() == "Imagem";
                zeroReduction |= button->getName() == "ReductionPeak" && button->getButtonText() == "0.0";
                infiniteOutput |= button->getName() == "OutputLeftPeak" && button->getButtonText() == "-Inf";
            }
        }
        check(sliders == 3 && imageButton, "three live controls and original image loader present");
        check(zeroReduction && infiniteOutput, "idle readouts show 0.0 reduction and -Inf output");
        const auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds());
        check(snapshot.isValid(), "editor renders successfully");
        const auto idleColour = snapshot.getPixelAt(230, 220);
        check(idleColour.getBlue() > idleColour.getRed(), "idle compressor remains blue rather than red");
        check(ui.readParameters().gate == -80, "rendering never moves the gate parameter");
        if (argc > 1)
        {
            juce::FileOutputStream output{juce::File(argv[1])};
            check(output.openedOk() && juce::PNGImageFormat().writeImageToStream(snapshot, output), "idle editor snapshot written");
        }
        for (auto* child : editor->getChildren())
            if (auto* slider = dynamic_cast<juce::Slider*>(child))
            {
                if (slider->getName() == "Gate") slider->setValue(-40, juce::sendNotificationSync);
                if (slider->getName() == "Compression") slider->setValue(-12, juce::sendNotificationSync);
            }
        check(std::abs(ui.readParameters().gate + 40) < 1e-4f && std::abs(ui.readParameters().compression + 12) < 1e-4f,
            "editor controls update DSP parameters");
        ui.prepareToPlay(48000, 512);
        for (int i = 0; i < 100; ++i)
        {
            for (int c = 0; c < 2; ++c) juce::FloatVectorOperations::fill(silent.getWritePointer(c), 0.001f, 512);
            ui.processBlock(silent, midi);
        }
        check(ui.reduction.load() < -10, "real quiet audio still shows gate attenuation");
        std::unique_ptr<juce::AudioProcessorEditor> active(ui.createEditor());
        const auto activeSnapshot = active->createComponentSnapshot(active->getLocalBounds());
        const auto activeColour = activeSnapshot.getPixelAt(230, 180);
        check(activeColour.getRed() > activeColour.getBlue(), "red reduction appears when real audio is attenuated");
        if (argc > 2)
        {
            juce::FileOutputStream output{juce::File(argv[2])};
            check(output.openedOk() && juce::PNGImageFormat().writeImageToStream(activeSnapshot, output), "active editor snapshot written");
        }
    }
    std::cout << "PASS: processor automation, state, buses, limiter, meters and editor\n";
}
