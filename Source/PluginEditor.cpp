#include "PluginEditor.h"
#if RVOXX_HAS_ARTWORK
#include <RvoxxArtwork.h>
#endif

void RvoxxLook::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float pos,
    float min, float max, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, pos, min, max, style, slider);
        return;
    }
    const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    g.setColour(juce::Colour(0xffffad08));
    if (slider.getName() == "Compression")
    {
        g.fillRect(cx - 64, pos - 1.5f, 128.0f, 3.0f);
        g.setColour(juce::Colour(0xff79252a));
        g.fillRoundedRectangle(cx - 40, pos - 17, 80, 34, 17);
        g.setColour(juce::Colour(0xffffbd1c));
        g.drawRoundedRectangle(cx - 40, pos - 17, 80, 34, 17, 3);
        g.drawRoundedRectangle(cx - 34, pos - 12, 68, 24, 12, 1);
        g.setColour(juce::Colours::white);
        g.setFont(18.0f);
        g.drawText(juce::String(slider.getValue(), 1), juce::Rectangle<float>(cx - 33, pos - 11, 66, 22), juce::Justification::centred);
    }
    else
    {
        g.drawEllipse(cx - 16, pos - 16, 32, 32, 2);
        g.setColour(juce::Colours::white);
        g.drawLine(cx - 9, pos, cx + 9, pos, 1);
    }
}
RvoxxEditor::RvoxxEditor(RvoxxProcessor& p) : AudioProcessorEditor(p), processor(p)
{
    setLookAndFeel(&look);
    for (auto* s : { &gate, &comp, &gain })
    {
        addAndMakeVisible(*s);
        s->setSliderStyle(juce::Slider::LinearVertical);
        s->setTextBoxStyle(juce::Slider::TextBoxAbove, false, 90, 23);
        s->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s->setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff303030));
        s->setNumDecimalPlacesToDisplay(1);
        s->setTextValueSuffix(" dB");
        s->setPopupDisplayEnabled(false, false, this);
        s->onDragStart = [this] { processor.undo.beginNewTransaction(); };
        s->valueFromTextFunction = [](const juce::String& text) { return text.containsIgnoreCase("inf") ? -80.0 : -std::abs(text.getDoubleValue()); };
    }
    gate.setName("Gate"); comp.setName("Compression"); gain.setName("Gain");
    gate.setDoubleClickReturnValue(true, -80); comp.setDoubleClickReturnValue(true, 0); gain.setDoubleClickReturnValue(true, 0);
    gateAttachment = std::make_unique<Attachment>(p.state, "gate", gate);
    compAttachment = std::make_unique<Attachment>(p.state, "compression", comp);
    gainAttachment = std::make_unique<Attachment>(p.state, "gain", gain);
    lastComp = comp.getValue();
    // Link applies to user drags only; host automation/state restoration must not
    // recursively change another automated parameter.
    comp.onDragStart = [this] { processor.undo.beginNewTransaction(); lastComp = comp.getValue(); if (link.getToggleState()) processor.state.getParameter("gain")->beginChangeGesture(); };
    comp.onValueChange = [this]
    {
        const auto value = comp.getValue();
        if (link.getToggleState() && comp.isMouseButtonDown() && !adjustingLink)
        { adjustingLink = true; gain.setValue(gain.getValue() + value - lastComp, juce::sendNotificationSync); adjustingLink = false; }
        lastComp = value;
    };
    comp.onDragEnd = [this] { if (link.getToggleState()) processor.state.getParameter("gain")->endChangeGesture(); };
    for (auto* b : { &undo, &redo, &ab, &save, &load, &reductionHold, &leftHold, &rightHold }) addAndMakeVisible(*b);
    addAndMakeVisible(link); addAndMakeVisible(skin); addAndMakeVisible(imageButton);
    imageButton.onClick = [this] { chooseArtwork(); };
    skin.addItemList({ "Light", "Dark", "Legacy" }, 1);
    skin.setSelectedId(static_cast<int>(processor.state.state.getProperty("skin", 3)), juce::dontSendNotification);
    skin.onChange = [this] { processor.state.state.setProperty("skin", skin.getSelectedId(), nullptr); repaint(); };
    undo.onClick = [this] { processor.undo.undo(); };
    redo.onClick = [this] { processor.undo.redo(); };
    processor.getStateInformation(alternate);
    ab.onClick = [this] { switchAB(); };
    save.onClick = [this] { choosePreset(true); }; load.onClick = [this] { choosePreset(false); };
    reductionHold.onClick = [this] { processor.heldReduction.store(0); };
    leftHold.onClick = [this] { processor.heldL.store(-160); };
    rightHold.onClick = [this] { processor.heldR.store(-160); };
    reductionHold.setTooltip("Total gate + compressor reduction. Click to reset peak hold.");
    leftHold.setTooltip("Output left peak hold. Click to reset."); rightHold.setTooltip("Output right peak hold. Click to reset.");
    comp.setTooltip("Compression. Type a value above, or drag. Link moves Gain with it.");
    gate.setTooltip("Downward expansion threshold. -80 dB disables the gate.");
    gain.setTooltip("Limiter output ceiling.");
    setResizable(true, true);
#if RVOXX_HAS_ARTWORK
    if (RvoxxArtwork::namedResourceListSize > 0)
    {
        int size = 0;
        const auto* bytes = RvoxxArtwork::getNamedResource(RvoxxArtwork::namedResourceList[0], size);
        artwork = juce::ImageFileFormat::loadFrom(bytes, static_cast<size_t>(size));
    }
#endif
    updateArtworkLayout();
    startTimerHz(30);
}
RvoxxEditor::~RvoxxEditor() { stopTimer(); setLookAndFeel(nullptr); }
void RvoxxEditor::resized()
{
    const auto rect = [this](int x, int y, int w, int h)
    { return juce::Rectangle<int>(juce::roundToInt(x * getWidth() / 475.0), juce::roundToInt(y * getHeight() / designHeight()), juce::roundToInt(w * getWidth() / 475.0), juce::roundToInt(h * getHeight() / designHeight())); };
    skin.setBounds(rect(7, 7, 76, 22)); undo.setBounds(rect(87, 7, 46, 22)); redo.setBounds(rect(135, 7, 46, 22));
    ab.setBounds(rect(185, 7, 43, 22)); save.setBounds(rect(232, 7, 45, 22)); load.setBounds(rect(281, 7, 45, 22)); link.setBounds(rect(337, 7, 65, 22));
    imageButton.setBounds(rect(405, 7, 63, 22));
    if (artwork.isValid())
    {
        gate.setBounds(rect(10, 480, 145, 76)); comp.setBounds(rect(165, 480, 145, 76)); gain.setBounds(rect(320, 480, 145, 76));
        reductionHold.setBounds(rect(192, 588, 90, 25)); leftHold.setBounds(rect(345, 588, 60, 25)); rightHold.setBounds(rect(407, 588, 60, 25));
        return;
    }
    gate.setBounds(rect(31, 112, 88, 239)); comp.setBounds(rect(171, 112, 137, 252)); gain.setBounds(rect(359, 112, 88, 239));
    reductionHold.setBounds(rect(195, 369, 88, 26)); leftHold.setBounds(rect(352, 391, 57, 24)); rightHold.setBounds(rect(410, 391, 57, 24));
}
void RvoxxEditor::paint(juce::Graphics& g)
{
    g.addTransform(juce::AffineTransform::scale(getWidth() / 475.0f, getHeight() / designHeight()));
    const bool dark = skin.getSelectedId() == 2;
    const auto text = dark ? juce::Colour(0xffededed) : juce::Colour(0xff333333);
    g.fillAll(dark ? juce::Colour(0xff282a2c) : juce::Colour(0xfff4f1e9));
    g.setColour(text); g.drawRect(1.0f, 1.0f, 473.0f, designHeight() - 2.0f, 1.0f);
    const auto label = [&g, text](const juce::String& s, int x, int y, int w, int h, float size)
    { g.setColour(text); g.setFont(size); g.drawText(s, x, y, w, h, juce::Justification::centred); };
    if (artwork.isValid())
    {
        // Preserve the complete approved artwork and its aspect ratio. Live
        // controls/readouts are outside the image; never paint over its pixels.
        g.drawImageWithin(artwork, 0, 40, 475, 412, juce::RectanglePlacement::centred);
        label("Gate", 10, 458, 145, 22, 18); label("Comp", 165, 458, 145, 22, 18); label("Gain", 320, 458, 145, 22, 18);
        label("Energy", 10, 562, 145, 22, 16); label(juce::String(energy, 1), 10, 588, 145, 25, 16);
        label("Reduction", 165, 562, 145, 22, 16); label("Output", 320, 562, 145, 22, 16);
        return;
    }
    label("RVOXX", 180, 48, 120, 33, 28);
    g.setColour(dark ? juce::Colour(0xff45484a) : juce::Colour(0xffd7d5ce));
    g.fillRoundedRectangle(136, 98, 207, 305, 54);
    g.setColour(juce::Colour(0xff001c2e)); g.fillRoundedRectangle(173, 136, 133, 230, 27);
    g.setColour(juce::Colour(0xff203d4c)); g.fillRoundedRectangle(187, 150, 105, 201, 19);
    g.setColour(juce::Colour(0xff79252a));
    const float reductionHeight = juce::jlimit(0.0f, 201.0f, -attenuation / 36.0f * 201.0f);
    g.fillRect(187.0f, 150.0f, 105.0f, reductionHeight);
    for (int n = 6; n <= 36; n += 6)
    {
        const int y = 150 + n * 201 / 36;
        g.setColour(juce::Colour(0xff536470)); g.drawHorizontalLine(y, 187, 292);
        label(juce::String(n) + "-", 140, y - 10, 31, 20, 14);
    }
    const auto meter = [&g](float x, float y, float w, float h, float db, float low)
    {
        g.setColour(juce::Colour(0xff00334e)); g.fillRoundedRectangle(x, y, w, h, w * 0.5f);
        const float amount = juce::jlimit(0.0f, 1.0f, (db - low) / -low) * h;
        g.setColour(juce::Colour(0xffffa509)); g.fillRect(x, y + h - amount, w, amount);
    };
    meter(66, 153, 18, 197, energy, -80);
    meter(394, 153, 6, 197, left, -36);
    if (processor.getTotalNumOutputChannels() == 2) meter(404, 153, 6, 197, right, -36);
    for (int n = 12; n <= 72; n += 12) label(juce::String(n) + "-", 29, 147 + n * 197 / 80, 33, 20, 14);
    label("Gate", 32, 91, 85, 22, 18); label("Comp", 185, 105, 106, 24, 19); label("Gain", 360, 91, 85, 22, 18);
    label("Energy", 30, 361, 90, 25, 18); label(juce::String(energy, 1), 30, 389, 90, 24, 16);
    label("Output", 358, 361, 92, 25, 18);
    if (!selection.isEmpty())
    {
        g.setColour(juce::Colours::orange.withAlpha(0.2f));
        g.fillRect(selection.toFloat().transformedBy(juce::AffineTransform::scale(475.0f / getWidth(), designHeight() / getHeight())));
    }
}
void RvoxxEditor::timerCallback()
{
    const int storedSkin = static_cast<int>(processor.state.state.getProperty("skin", 3));
    if (skin.getSelectedId() != storedSkin) skin.setSelectedId(storedSkin, juce::dontSendNotification);
    energy = std::max(processor.energy.load(), energy - 1.0f);
    attenuation = std::min(processor.reduction.load(), attenuation + 1.0f);
    left = std::max(processor.outL.load(), left - 1.0f); right = std::max(processor.outR.load(), right - 1.0f);
    const auto display = [](float value) { return value <= -159 ? juce::String("-inf") : juce::String(value, 1); };
    reductionHold.setButtonText(juce::String(processor.heldReduction.load(), 1));
    leftHold.setButtonText(display(processor.heldL.load())); rightHold.setButtonText(display(processor.heldR.load()));
    rightHold.setVisible(processor.getTotalNumOutputChannels() == 2);
    undo.setEnabled(processor.undo.canUndo()); redo.setEnabled(processor.undo.canRedo());
    const bool dark = skin.getSelectedId() == 2;
    for (auto* s : { &gate, &comp, &gain }) s->setColour(juce::Slider::textBoxTextColourId, dark ? juce::Colours::white : juce::Colour(0xff303030));
    repaint();
}
void RvoxxEditor::updateArtworkLayout()
{
    for (auto* s : { &gate, &comp, &gain })
        s->setSliderStyle(artwork.isValid() ? juce::Slider::LinearHorizontal : juce::Slider::LinearVertical);
    getConstrainer()->setFixedAspectRatio(475.0 / designHeight());
    setResizeLimits(475, static_cast<int>(designHeight()), 1425, static_cast<int>(designHeight()) * 3);
    setSize(475, static_cast<int>(designHeight()));
    resized(); repaint();
}
void RvoxxEditor::chooseArtwork()
{
    chooser = std::make_unique<juce::FileChooser>("Open original artwork (kept unchanged)", juce::File{}, "*.png;*.jpg;*.jpeg");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<RvoxxEditor>(this)](const juce::FileChooser& fc)
        {
            if (safe == nullptr || fc.getResult() == juce::File{}) return;
            const auto image = juce::ImageFileFormat::loadFrom(fc.getResult());
            if (image.isValid()) { safe->artwork = image; safe->updateArtworkLayout(); }
            else juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "RVOXX", "Could not read artwork.");
        });
}
void RvoxxEditor::switchAB()
{
    juce::MemoryBlock current;
    processor.getStateInformation(current);
    processor.setStateInformation(alternate.getData(), static_cast<int>(alternate.getSize()));
    alternate = std::move(current);
    alternateActive = !alternateActive; ab.setButtonText(alternateActive ? "B / A" : "A / B");
}
void RvoxxEditor::choosePreset(bool saving)
{
    chooser = std::make_unique<juce::FileChooser>(saving ? "Save RVOXX preset" : "Load RVOXX preset", juce::File{}, "*.rvoxx");
    const int dialogFlags = saving ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                             : juce::FileBrowserComponent::openMode;
    chooser->launchAsync(dialogFlags | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<RvoxxEditor>(this), saving](const juce::FileChooser& fc)
        {
            if (safe == nullptr || fc.getResult() == juce::File{}) return;
            if (saving)
            {
                juce::MemoryBlock data; safe->processor.getStateInformation(data);
                if (!fc.getResult().withFileExtension("rvoxx").replaceWithData(data.getData(), data.getSize()))
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "RVOXX", "Could not save preset.");
            }
            else
            {
                juce::MemoryBlock data;
                if (fc.getResult().loadFileAsData(data)) safe->processor.setStateInformation(data.getData(), static_cast<int>(data.getSize()));
            }
        });
}
void RvoxxEditor::mouseDown(const juce::MouseEvent& e)
{ selectionStart = e.getPosition(); selection = {}; }
void RvoxxEditor::mouseDrag(const juce::MouseEvent& e)
{ selection = juce::Rectangle<int>(selectionStart, e.getPosition()); repaint(); }
void RvoxxEditor::mouseUp(const juce::MouseEvent&)
{
    if (!selection.isEmpty()) link.setToggleState(selection.intersects(comp.getBounds()) && selection.intersects(gain.getBounds()), juce::sendNotificationSync);
    selection = {}; repaint();
}
