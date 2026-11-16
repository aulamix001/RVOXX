#include "PluginEditor.h"
#if RVOXX_HAS_ARTWORK
#include <RvoxxArtwork.h>
#endif

RvoxxLook::RvoxxLook()
{
    setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdddddd));
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    setColour(juce::ToggleButton::textColourId, juce::Colour(0xffdddddd));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff303030));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff777777));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xffdddddd));
    setColour(juce::Slider::thumbColourId, juce::Colour(0xffffa500));
}
juce::Slider::SliderLayout RvoxxLook::getSliderLayout(juce::Slider& slider)
{
    if (slider.getSliderStyle() != juce::Slider::LinearVertical)
        return juce::LookAndFeel_V4::getSliderLayout(slider);
    juce::Slider::SliderLayout result;
    const int w = slider.getWidth(), h = slider.getHeight();
    if (slider.getName() == "Compression")
    {
        result.sliderBounds = {0, 18, w, h - 36};
        const double proportion = slider.valueToProportionOfLength(slider.getValue());
        const int y = juce::roundToInt(18 + (1 - proportion) * (h - 36));
        result.textBoxBounds = juce::Rectangle<int>(68, 24).withCentre({w / 2, y});
    }
    else
    {
        result.textBoxBounds = {0, 0, w, 23};
        result.sliderBounds = {0, 38, w, h - 54};
    }
    return result;
}
void RvoxxLook::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down)
{
    if (button.getComponentID() == "meter") return;
    const auto r = button.getLocalBounds().toFloat().reduced(0.7f);
    g.setColour(juce::Colour(down ? 0xff181818 : over ? 0xff555555 : 0xff383838));
    g.fillRect(r);
    g.setColour(juce::Colour(0xff888888)); g.drawRect(r, 1);
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
    g.setColour(juce::Colour(0xffffa51b));
    if (slider.getName() == "Compression")
    {
        g.fillRect(cx - 64, pos - 1.5f, 128.0f, 3.0f);
        g.setColour(juce::Colour(0xff203d4c));
        g.fillRoundedRectangle(cx - 40, pos - 17, 80, 34, 17);
        g.setColour(juce::Colour(0xffffa51b));
        g.drawRoundedRectangle(cx - 40, pos - 17, 80, 34, 17, 3);
        g.setColour(juce::Colour(0xffffcf66));
        g.drawRoundedRectangle(cx - 36, pos - 13, 72, 26, 13, 1);
        // The editable slider text box is placed inside this thumb by the
        // layout override. It follows automation and never duplicates a value.
    }
    else
    {
        g.drawEllipse(cx - 16, pos - 16, 32, 32, 2);
        g.setColour(juce::Colours::white);
        g.drawLine(cx - 10, pos, cx + 10, pos, 1.5f);
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
        s->setTextValueSuffix("");
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
    // Attachments install their own text callbacks; configure formatting after
    // attachment construction so the OFF endpoint always reads -Inf.
    gate.textFromValueFunction = [](double v) { return v <= -79.99 ? juce::String("-Inf") : juce::String(v, 1); };
    gate.valueFromTextFunction = [](const juce::String& t) { return t.containsIgnoreCase("inf") ? -80.0 : -std::abs(t.getDoubleValue()); };
    gate.updateText();
    // Link applies to user drags only; host automation/state restoration must not
    // recursively change another automated parameter.
    comp.onDragStart = [this] { processor.undo.beginNewTransaction(); lastComp = comp.getValue(); if (link.getToggleState()) processor.state.getParameter("gain")->beginChangeGesture(); };
    comp.onValueChange = [this]
    {
        const auto value = comp.getValue();
        if (link.getToggleState() && comp.isMouseButtonDown() && !adjustingLink)
        { adjustingLink = true; gain.setValue(gain.getValue() + value - lastComp, juce::sendNotificationSync); adjustingLink = false; }
        lastComp = value;
        comp.resized();
    };
    comp.onDragEnd = [this] { if (link.getToggleState()) processor.state.getParameter("gain")->endChangeGesture(); };
    for (auto* b : { &undo, &redo, &ab, &save, &load, &reductionHold, &leftHold, &rightHold }) addAndMakeVisible(*b);
    addAndMakeVisible(link); addAndMakeVisible(skin); addAndMakeVisible(imageButton);
    imageButton.onClick = [this] { chooseArtwork(); };
    skin.addItemList({ "Light", "Dark", "Original" }, 1);
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
    reductionHold.setName("ReductionPeak"); leftHold.setName("OutputLeftPeak"); rightHold.setName("OutputRightPeak");
    for (auto* meter : {&reductionHold, &leftHold, &rightHold})
    {
        meter->setComponentID("meter");
        meter->setColour(juce::TextButton::textColourOffId, juce::Colour(0xff333333));
    }
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
    gate.setBounds(rect(31, 115, 88, 251)); comp.setBounds(rect(171, 132, 137, 237)); gain.setBounds(rect(359, 115, 88, 251));
    reductionHold.setBounds(rect(195, 374, 88, 26)); leftHold.setBounds(rect(350, 398, 58, 24)); rightHold.setBounds(rect(410, 398, 58, 24));
}
void RvoxxEditor::paint(juce::Graphics& g)
{
    g.addTransform(juce::AffineTransform::scale(getWidth() / 475.0f, getHeight() / designHeight()));
    const bool dark = skin.getSelectedId() == 2;
    const auto text = dark ? juce::Colour(0xffeeeeee) : juce::Colour(0xff333333);
    g.setGradientFill(juce::ColourGradient(dark ? juce::Colour(0xff414141) : juce::Colour(0xfffaf8f2), 0, 40,
        dark ? juce::Colour(0xff252525) : juce::Colour(0xffddddd5), 475, designHeight(), false));
    g.fillRect(0.0f, 0.0f, 475.0f, designHeight());
    g.setColour(juce::Colour(0xff222222)); g.fillRect(0, 0, 475, 36);
    g.setColour(juce::Colour(0xff333333)); g.drawRect(1.0f, 37.0f, 473.0f, designHeight() - 38, 1.0f);
    const auto label = [&g, text](const juce::String& s, int x, int y, int w, int h, float size)
    { g.setColour(text); g.setFont(size); g.drawText(s, x, y, w, h, juce::Justification::centred); };
    if (artwork.isValid())
    {
        g.drawImageWithin(artwork, 0, 40, 475, 412, juce::RectanglePlacement::centred);
        label("Gate", 10, 458, 145, 22, 18); label("Comp", 165, 458, 145, 22, 18); label("Gain", 320, 458, 145, 22, 18);
        label("Energy", 10, 562, 145, 22, 16);
        label(energy <= -80 ? juce::String("-Inf") : juce::String(energy, 1), 10, 588, 145, 25, 16);
        label("Reduction", 165, 562, 145, 22, 16); label("Output", 320, 562, 145, 22, 16);
        return;
    }
    g.setColour(text);
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultSerifFontName(), 30, juce::Font::italic)));
    g.drawText("R", 195, 48, 34, 35, juce::Justification::centred);
    g.setColour(juce::Colour(0xff9e191d)); g.setFont(25.0f);
    g.drawText("VOX", 226, 52, 61, 30, juce::Justification::centredLeft);
    g.setColour(juce::Colours::black.withAlpha(0.13f)); g.fillRoundedRectangle(137, 105, 207, 304, 54);
    g.setGradientFill(juce::ColourGradient(dark ? juce::Colour(0xff575757) : juce::Colour(0xffd8d7d1), 136, 98,
        dark ? juce::Colour(0xff343434) : juce::Colour(0xffc7c7c1), 343, 403, false));
    g.fillRoundedRectangle(136, 98, 207, 305, 54);
    g.setColour(juce::Colours::white.withAlpha(0.5f)); g.drawRoundedRectangle(137, 99, 205, 303, 53, 1);
    g.setColour(juce::Colour(0xff001c2e)); g.fillRoundedRectangle(173, 136, 133, 230, 27);
    g.setColour(juce::Colour(0xff203d4c)); g.fillRoundedRectangle(187, 150, 105, 201, 19);
    const float amount = juce::jlimit(0.0f, 201.0f, -attenuation / 36.0f * 201.0f);
    if (amount > 0.1f)
    {
        juce::Path inside; inside.addRoundedRectangle(187, 150, 105, 201, 19);
        juce::Graphics::ScopedSaveState saveState(g); g.reduceClipRegion(inside);
        g.setColour(juce::Colour(0xff79252a)); g.fillRect(187.0f, 150.0f, 105.0f, amount);
    }
    for (int n = 6; n <= 36; n += 6)
    {
        const int y = 150 + n * 201 / 36;
        g.setColour(juce::Colour(0xff536470)); g.drawHorizontalLine(y, 187, 292);
        label(juce::String(n) + "-", 140, y - 10, 31, 20, 14);
    }
    const auto meter = [&g](float x, float w, float db, float low)
    {
        g.setColour(juce::Colour(0xff00334e)); g.fillRoundedRectangle(x, 153, w, 197, w * 0.5f);
        const float height = juce::jlimit(0.0f, 1.0f, (db - low) / -low) * 197;
        g.setColour(juce::Colour(0xffffa509)); g.fillRect(x, 350 - height, w, height);
    };
    meter(66, 18, energy, -80);
    const bool stereo = processor.getTotalNumOutputChannels() == 2;
    meter(stereo ? 394.0f : 400.0f, 6, left, -36);
    if (stereo) meter(404, 6, right, -36);
    for (int n = 12; n <= 72; n += 12) label(juce::String(n) + "-", 29, 147 + n * 197 / 80, 33, 20, 14);
    label("Gate", 32, 91, 85, 22, 18); label("Comp", 185, 105, 106, 24, 19); label("Gain", 360, 91, 85, 22, 18);
    label("Energy", 30, 371, 90, 25, 18);
    label(energy <= -80 ? juce::String("-Inf") : juce::String(energy, 1), 30, 398, 90, 24, 16);
    label("Output", 358, 371, 92, 25, 18);
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
    const auto display = [](float value) { return value <= -159 ? juce::String("-Inf") : juce::String(value, 1); };
    reductionHold.setButtonText(juce::String(processor.heldReduction.load(), 1));
    leftHold.setButtonText(display(processor.heldL.load())); rightHold.setButtonText(display(processor.heldR.load()));
    rightHold.setVisible(processor.getTotalNumOutputChannels() == 2);
    undo.setEnabled(processor.undo.canUndo()); redo.setEnabled(processor.undo.canRedo());
    const bool dark = skin.getSelectedId() == 2;
    for (auto* s : { &gate, &comp, &gain })
        s->setColour(juce::Slider::textBoxTextColourId, !artwork.isValid() && s == &comp ? juce::Colour(0xffe1e1e1) : dark ? juce::Colour(0xffeeeeee) : juce::Colour(0xff333333));
    for (auto* meter : {&reductionHold, &leftHold, &rightHold})
        meter->setColour(juce::TextButton::textColourOffId, dark ? juce::Colour(0xffeeeeee) : juce::Colour(0xff333333));
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
