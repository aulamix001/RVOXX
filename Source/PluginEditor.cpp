#include "PluginEditor.h"
#if RVOXX_HAS_ARTWORK
#include <RvoxxArtwork.h>
#endif

RvoxxLook::RvoxxLook()
{
    setColour(juce::TextButton::textColourOffId, juce::Colour(0xffecd8ae));
    setColour(juce::TextButton::textColourOnId, juce::Colour(0xffffe4a1));
    setColour(juce::ToggleButton::textColourId, juce::Colour(0xffecd8ae));
    setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffe4b75f));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff30271d));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff987547));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xffecd8ae));
    setColour(juce::Slider::thumbColourId, juce::Colour(0xffc99c51));
    setColour(juce::Slider::trackColourId, juce::Colour(0xffba843b));
}
void RvoxxLook::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down)
{
    const auto r = button.getLocalBounds().toFloat().reduced(0.7f);
    const auto top = juce::Colour(down ? 0xff30251a : over ? 0xff775737 : 0xff59452f);
    g.setGradientFill(juce::ColourGradient(top, 0, r.getY(), juce::Colour(0xff241d16), 0, r.getBottom(), false));
    g.fillRoundedRectangle(r, 3);
    g.setColour(juce::Colour(0xffaa8350).withAlpha(button.isEnabled() ? 0.9f : 0.3f));
    g.drawRoundedRectangle(r, 3, 1);
    g.setColour(juce::Colour(0xffead4a4).withAlpha(0.2f));
    g.drawLine(r.getX() + 3, r.getY() + 1.5f, r.getRight() - 3, r.getY() + 1.5f);
}
void RvoxxLook::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float pos,
    float min, float max, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, pos, min, max, style, slider);
        return;
    }
    const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    g.setColour(juce::Colour(0xffc79b50));
    if (slider.getName() == "Compression")
    {
        g.fillRect(cx - 64, pos - 1.5f, 128.0f, 3.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff894333), cx, pos - 17, juce::Colour(0xff3c1a16), cx, pos + 17, false));
        g.fillRoundedRectangle(cx - 40, pos - 17, 80, 34, 17);
        g.setColour(juce::Colour(0xffd7b367));
        g.drawRoundedRectangle(cx - 40, pos - 17, 80, 34, 17, 3);
        g.drawRoundedRectangle(cx - 34, pos - 12, 68, 24, 12, 1);
        g.setColour(juce::Colour(0xffffe5b0));
        g.setFont(18.0f);
        g.drawText(juce::String(slider.getValue(), 1), juce::Rectangle<float>(cx - 33, pos - 11, 66, 22), juce::Justification::centred);
    }
    else
    {
        g.setColour(juce::Colour(0xff130e0b).withAlpha(0.7f));
        g.fillEllipse(cx - 17, pos - 15, 34, 34);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe0be79), cx - 15, pos - 15, juce::Colour(0xff70512b), cx + 15, pos + 15, false));
        g.fillEllipse(cx - 16, pos - 16, 32, 32);
        g.setColour(juce::Colour(0xff352618)); g.fillEllipse(cx - 11, pos - 11, 22, 22);
        g.setColour(juce::Colour(0xffedd4a0));
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
    timerCallback();
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
    gate.setBounds(rect(31, 128, 88, 222)); comp.setBounds(rect(171, 128, 137, 236)); gain.setBounds(rect(359, 128, 88, 222));
    reductionHold.setBounds(rect(195, 369, 88, 26)); leftHold.setBounds(rect(352, 391, 57, 24)); rightHold.setBounds(rect(410, 391, 57, 24));
}
void RvoxxEditor::paint(juce::Graphics& g)
{
    g.addTransform(juce::AffineTransform::scale(getWidth() / 475.0f, getHeight() / designHeight()));
    const bool dark = skin.getSelectedId() == 2;
    const auto text = dark ? juce::Colour(0xffe7d5ad) : juce::Colour(0xff493522);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2c170e), 0, 0, juce::Colour(0xff71472a), 475, designHeight(), false));
    g.fillRect(0.0f, 0.0f, 475.0f, designHeight());
    // Deterministic wood grain and brushed metal; no regenerated artwork.
    for (int i = 0; i < 180; ++i)
    {
        const float y = i * 3.9f;
        g.setColour(juce::Colour(0xff130a06).withAlpha(0.15f + 0.08f * std::sin(i * 1.7f)));
        juce::Path grain; grain.startNewSubPath(0, y);
        grain.cubicTo(120, y + std::sin(i * 0.9f) * 6, 310, y - 4, 475, y + 2);
        g.strokePath(grain, juce::PathStrokeType(0.8f));
    }
    const juce::Rectangle<float> face(17, 38, 441, designHeight() - 55);
    g.setColour(juce::Colour(0xff140e09)); g.fillRoundedRectangle(face.expanded(3), 6);
    g.setGradientFill(juce::ColourGradient(dark ? juce::Colour(0xff4a4538) : juce::Colour(0xffe1d3b1), 20, 38,
        dark ? juce::Colour(0xff28271f) : juce::Colour(0xffb7a27b), 450, designHeight(), false));
    g.fillRoundedRectangle(face, 5);
    g.setColour(juce::Colour(0xffb28f53)); g.drawRoundedRectangle(face, 5, 1);
    for (int y = 41; y < static_cast<int>(designHeight()) - 20; y += 3)
    {
        g.setColour(juce::Colour(0xff51391d).withAlpha(y % 9 == 0 ? 0.05f : 0.025f));
        g.drawHorizontalLine(y, 19, 456);
    }
    for (const auto pt : {juce::Point<float>(28, 50), juce::Point<float>(447, 50), juce::Point<float>(28, designHeight() - 30), juce::Point<float>(447, designHeight() - 30)})
    {
        g.setColour(juce::Colour(0xff25190e)); g.fillEllipse(pt.x - 5, pt.y - 4, 11, 11);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffcfad6c), pt.x - 4, pt.y - 4, juce::Colour(0xff5a4125), pt.x + 4, pt.y + 4, false));
        g.fillEllipse(pt.x - 4, pt.y - 4, 8, 8);
        g.setColour(juce::Colour(0xff322316)); g.drawLine(pt.x - 2, pt.y + 2, pt.x + 2, pt.y - 2, 1.4f);
    }
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
    g.setColour(text); g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultSerifFontName(), 28, juce::Font::bold)));
    g.drawText("RVOXX", 173, 45, 133, 34, juce::Justification::centred);
    label("V O C A L   D Y N A M I C S", 145, 80, 189, 13, 9);
    g.setColour(juce::Colour(0xff2e271e)); g.fillRoundedRectangle(136, 99, 207, 304, 13);
    g.setColour(juce::Colour(0xffa5844c)); g.drawRoundedRectangle(137, 100, 205, 302, 12, 1.2f);
    g.setColour(juce::Colour(0xff151b17)); g.fillRoundedRectangle(173, 151, 133, 215, 13);
    g.setColour(juce::Colour(0xff30392b)); g.fillRoundedRectangle(187, 155, 105, 196, 7);
    g.setColour(juce::Colour(0xff7b3527));
    const float reductionHeight = juce::jlimit(0.0f, 201.0f, -attenuation / 36.0f * 201.0f);
    g.fillRect(187.0f, 150.0f, 105.0f, reductionHeight);
    for (int n = 6; n <= 36; n += 6)
    {
        const int y = 150 + n * 201 / 36;
        g.setColour(juce::Colour(0xff8b8059).withAlpha(0.4f)); g.drawHorizontalLine(y, 187, 292);
        g.setColour(juce::Colour(0xffdfcba0)); g.setFont(13.0f);
        g.drawText(juce::String(n) + "-", 140, y - 10, 31, 20, juce::Justification::centred);
    }
    const auto meter = [&g](float x, float y, float w, float h, float db, float low)
    {
        g.setColour(juce::Colour(0xff201f16)); g.fillRoundedRectangle(x, y, w, h, w * 0.3f);
        const float amount = juce::jlimit(0.0f, 1.0f, (db - low) / -low) * h;
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff1d387), x, y, juce::Colour(0xffa06623), x, y + h, false));
        g.fillRect(x, y + h - amount, w, amount);
    };
    meter(66, 153, 18, 197, energy, -80);
    meter(394, 153, 6, 197, left, -36);
    if (processor.getTotalNumOutputChannels() == 2) meter(404, 153, 6, 197, right, -36);
    for (int n = 12; n <= 72; n += 12) label(juce::String(n) + "-", 29, 147 + n * 197 / 80, 33, 20, 14);
    label("GATE", 32, 104, 85, 22, 16);
    g.setColour(juce::Colour(0xffe8d4aa)); g.setFont(16.0f); g.drawText("COMP", 185, 104, 106, 22, juce::Justification::centred);
    label("GAIN", 360, 104, 85, 22, 16);
    const bool gateOff = processor.state.getRawParameterValue("gate")->load() <= -79.99f;
    const bool expanding = processor.gateReduction.load() < -1.0f;
    g.setColour(juce::Colour(gateOff ? 0xff74664d : expanding ? 0xffb86b2c : 0xff788c3e));
    g.fillEllipse(44, 419, 5, 5);
    label(gateOff ? "GATE OFF" : expanding ? "EXPANDING" : "GATE OPEN", 53, 414, 100, 14, 9);
    label("VINTAGE SERIES", 306, 414, 125, 14, 9);
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
    for (auto* s : { &gate, &comp, &gain })
        s->setColour(juce::Slider::textBoxTextColourId, !artwork.isValid() && s == &comp ? juce::Colour(0xffead4a4) : dark ? juce::Colour(0xffead4a4) : juce::Colour(0xff493522));
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
