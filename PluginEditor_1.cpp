#include "PluginEditor.h"

using namespace juce;
using namespace ui;

static Font F (float size, bool bold = false, float kerning = 0.0f)
{
    Font f (FontOptions (size, bold ? Font::bold : Font::plain));
    return kerning > 0.0f ? f.withExtraKerningFactor (kerning) : f;
}
static float textWidth (const Font& f, const String& s)
{
    GlyphArrangement ga; ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}
static void drawPanel (Graphics& g, Rectangle<float> r, float radius = 14.0f)
{
    g.setGradientFill (ColourGradient (panelTop, 0, r.getY(), panelBot, 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    g.setColour (line); g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    g.setColour (Colours::white.withAlpha (0.035f));                       // faint top highlight
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + radius, r.getRight() - radius);
}

// ------------------------------------------------------------------------------------------------
void PitchlyLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const bool hot = s.isMouseOverOrDragging();
    const float r = size * 0.44f - 2.0f, thick = jmax (2.5f, size * 0.055f);
    const PathStrokeType round (thick, PathStrokeType::curved, PathStrokeType::rounded);

    Path track; track.addCentredArc (c.x, c.y, r, r, 0.0f, start, end, true);
    g.setColour (Colour (0xff252931)); g.strokePath (track, round);

    const float a = start + pos * (end - start);
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (start + end) * 0.5f : start;          // centred controls fill outward from 12 o'clock
    if (std::abs (a - from) > 0.01f)
    {
        Path val; val.addCentredArc (c.x, c.y, r, r, 0.0f, jmin (from, a), jmax (from, a), true);
        const float glow[2][2] = { { 3.6f, 0.07f }, { 2.2f, 0.15f } };
        for (auto& gl : glow)
        {
            g.setColour (accent.withAlpha (gl[1] * (hot ? 1.7f : 1.0f)));
            g.strokePath (val, PathStrokeType (thick * gl[0], PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setColour (accent); g.strokePath (val, round);
    }

    // metal cap
    const float rc = size * 0.30f;
    const auto cap = Rectangle<float> (rc * 2, rc * 2).withCentre (c);
    g.setColour (Colours::black.withAlpha (0.45f)); g.fillEllipse (cap.translated (0.0f, 2.0f).expanded (1.0f));
    g.setGradientFill (ColourGradient (Colour (hot ? 0xff373b45 : 0xff30343d), c.x, c.y - rc, Colour (0xff16181d), c.x, c.y + rc, false));
    g.fillEllipse (cap);
    g.setColour (Colour (0xff3b404a)); g.drawEllipse (cap.reduced (0.5f), 1.0f);

    // indicator
    const Point<float> dir (std::sin (a), -std::cos (a));
    Path ind; ind.startNewSubPath (c + dir * (rc * 0.38f)); ind.lineTo (c + dir * (rc * 0.88f));
    g.setColour (accent); g.strokePath (ind, PathStrokeType (2.6f, PathStrokeType::curved, PathStrokeType::rounded));
}

// ------------------------------------------------------------------------------------------------
Knob::Knob (const String& label, bool wide, const String& subtitle, std::function<String (double)> format)
    : labelText (label), subtitleText (subtitle), isWide (wide), fmt (std::move (format))
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}
void Knob::resized()
{
    if (isWide) { slider.setBounds (10, (getHeight() - 116) / 2, 116, 116); return; }
    const int k = jlimit (56, 104, jmin (getWidth() - 20, getHeight() - 52));
    slider.setBounds ((getWidth() - k) / 2, (getHeight() - (k + 40)) / 2, k, k);
}
void Knob::paint (Graphics& g)
{
    const String v = fmt (slider.getValue());
    if (isWide)
    {
        const int tx = 132, mid = getHeight() / 2;
        g.setColour (mute); g.setFont (F (10.0f, false, 0.16f)); g.drawText (labelText.toUpperCase(), tx, mid - 34, 112, 14, Justification::left);
        g.setColour (ui::text); g.setFont (F (24.0f, true));      g.drawText (v, tx, mid - 19, 112, 34, Justification::left);
        g.setColour (mute); g.setFont (F (11.0f));                g.drawText (subtitleText, tx, mid + 18, 112, 16, Justification::left);
    }
    else
    {
        const int vy = slider.getBottom() + 3;
        g.setColour (ui::text); g.setFont (F (13.0f, true)); g.drawText (v, 0, vy, getWidth(), 16, Justification::centred);
        g.setColour (mute); g.setFont (F (10.0f, false, 0.16f)); g.drawText (labelText.toUpperCase(), 0, vy + 19, getWidth(), 14, Justification::centred);
    }
}

// ------------------------------------------------------------------------------------------------
void Pill::paint (Graphics& g)
{
    const bool hi = highlighted && highlighted();
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (hi ? accent : panelTop); g.fillRoundedRectangle (b, b.getHeight() / 2);
    g.setColour (hi ? accent : line);     g.drawRoundedRectangle (b, b.getHeight() / 2, 1.0f);

    float x = 16.0f;
    const auto pf = F (12.0f, hi), vf = F (12.0f, true);
    g.setColour (hi ? bg : soft); g.setFont (pf);
    if (centred) { g.drawText (prefix, getLocalBounds(), Justification::centred); return; }
    const float pw = textWidth (pf, prefix);
    g.drawText (prefix, Rectangle<float> (x, 0, pw + 4, (float) getHeight()), Justification::centredLeft);
    x += pw + 8.0f;

    if (value)
    {
        const String v = value();
        g.setColour (text); g.setFont (vf);
        g.drawText (v, Rectangle<float> (x, 0, textWidth (vf, v) + 4, (float) getHeight()), Justification::centredLeft);
    }
    if (meter)
    {
        const float mw = (float) meterWidth;
        const Rectangle<float> bar (x, getHeight() * 0.5f - 2.0f, mw, 4.0f);
        g.setColour (line); g.fillRoundedRectangle (bar, 2.0f);
        const float lv = jlimit (0.0f, 1.0f, meter());
        if (lv > 0.01f)
        {
            g.setColour (accent.withAlpha (0.25f)); g.fillRoundedRectangle (bar.withWidth (mw * lv).expanded (0.0f, 2.0f), 3.0f);
            g.setColour (accent);                   g.fillRoundedRectangle (bar.withWidth (mw * lv), 2.0f);
        }
    }
}

void NoteButton::paintButton (Graphics& g, bool over, bool)
{
    const bool on = getToggleState();
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    const float rad = black ? 6.0f : 9.0f;

    if (black)
    {
        g.setColour (on ? accent.withAlpha (over ? 0.55f : 0.42f) : Colour (over ? 0xff14161a : 0xff08090b));
        g.fillRoundedRectangle (b, rad);
        g.setColour (on ? accent : Colour (0xff2c3038)); g.drawRoundedRectangle (b, rad, 1.0f);
        g.setColour (on ? ui::text : dim); g.setFont (F (9.5f, on));
        g.drawText (getButtonText(), getLocalBounds().withTrimmedBottom (6), Justification::centredBottom);
    }
    else
    {
        if (on) g.setGradientFill (ColourGradient (accent.withAlpha (over ? 0.28f : 0.20f), 0, b.getY(), accent.withAlpha (0.07f), 0, b.getBottom(), false));
        else    g.setGradientFill (ColourGradient (Colour (over ? 0xff20242b : 0xff1b1e25), 0, b.getY(), Colour (0xff14161b), 0, b.getBottom(), false));
        g.fillRoundedRectangle (b, rad);
        g.setColour (on ? accent.withAlpha (0.85f) : line); g.drawRoundedRectangle (b, rad, 1.0f);
        if (on) { g.setColour (accent); g.fillRoundedRectangle (b.getX() + 14.0f, b.getBottom() - 6.0f, b.getWidth() - 28.0f, 3.0f, 1.5f); }
        g.setColour (on ? accent : dim); g.setFont (F (11.5f, on));
        g.drawText (getButtonText(), getLocalBounds().withTrimmedBottom (14), Justification::centredBottom);
    }
}

void TabButton::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (3.0f, 3.0f);
    if (selected) { g.setColour (accent.withAlpha (0.16f)); g.fillRoundedRectangle (b, 7.0f); }
    else if (isMouseOver()) { g.setColour (Colours::white.withAlpha (0.04f)); g.fillRoundedRectangle (b, 7.0f); }
    g.setColour (selected ? accent : mute); g.setFont (F (10.0f, selected, 0.1f));
    g.drawText (label, getLocalBounds(), Justification::centred);
}

// ------------------------------------------------------------------------------------------------
PitchlyEditor::PitchlyEditor (PitchlyProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    setSize (900, 580);
    setResizable (false, false);

    auto pct  = [] (double v) { return String (roundToInt (v)) + "%"; };
    auto sgn  = [] (double v, int dec) { return String (std::abs (v) < 0.05 ? 0.0 : v, dec).replaceCharacter ('-', (juce_wchar) 0x2212); };
    auto signedStr = [sgn] (double v, int dec, const String& unit) { return String (v > 0.049 ? "+" : "") + sgn (v, dec) + unit; };

    // ---- TUNE ----
    addKnob (Tune, { 624, 114, 248, 138 }, "Retune Speed", true, CharPointer_UTF8 ("Natural \xe2\x86\x92 Hard"), "retune", 12.0,
             [] (double v) { return v < 10.0 ? String (v, 1) + " ms" : String (roundToInt (v)) + " ms"; });
    addKnob (Tune, { 624, 264, 118, 138 }, "Humanize",  false, {}, "humanize",  40.0, pct);
    addKnob (Tune, { 754, 264, 118, 138 }, "Tolerance", false, {}, "tolerance", 8.0,  [] (double v) { return String (roundToInt (v)) + " ct"; });
    addKnob (Tune, { 624, 414, 118, 138 }, "Amount",    false, {}, "amount",    100.0, pct);
    addKnob (Tune, { 754, 414, 118, 138 }, "Formant",   false, {}, "formant",   0.0,  [signedStr] (double v) { return signedStr (v, 1, " st"); });
    // ---- SHIFT ----
    addKnob (Shift, { 624, 114, 118, 213 }, "Transpose", false, {}, "transpose", 0.0,   [signedStr] (double v) { return signedStr (v, 0, " st"); });
    addKnob (Shift, { 754, 114, 118, 213 }, "Detune",    false, {}, "detune",    0.0,   [signedStr] (double v) { return signedStr (v, 0, " ct"); });
    addKnob (Shift, { 624, 339, 118, 213 }, "Reference", false, {}, "reference", 440.0, [] (double v) { return String (v, 1) + " Hz"; });
    // ---- VIBRATO ----
    addKnob (Vibrato, { 624, 114, 118, 213 }, "Keep Vibrato", false, {}, "vibkeep",  50.0, pct);
    addKnob (Vibrato, { 754, 114, 118, 213 }, "Add Depth",    false, {}, "vibdepth", 0.0,  [] (double v) { return String (roundToInt (v)) + " ct"; });
    addKnob (Vibrato, { 624, 339, 118, 213 }, "Add Rate",     false, {}, "vibrate",  5.5,  [] (double v) { return String (v, 1) + " Hz"; });
    addKnob (Vibrato, { 754, 339, 118, 213 }, "Add Delay",    false, {}, "vibdelay", 0.0,  [] (double v) { return String (roundToInt (v)) + " ms"; });
    // ---- I/O ----
    addKnob (IO, { 624, 114, 248, 138 }, "Mix",         true, CharPointer_UTF8 ("Dry \xe2\x86\x94 Wet"), "mix",     80.0, pct);
    addKnob (IO, { 624, 264, 248, 138 }, "Gate",        true, "Input threshold", "gate",    -48.0, [] (double v) { return String (v, 1) + " dB"; });
    addKnob (IO, { 624, 414, 248, 138 }, "Output Gain", true, "Output level",    "outgain", 0.0,   [signedStr] (double v) { return signedStr (v, 1, " dB"); });

    // piano keys: white keys first, black keys last so they sit on top
    static const int whiteKeys[7] = { 0, 2, 4, 5, 7, 9, 11 }, blackKeys[5] = { 1, 3, 6, 8, 10 };
    for (int pc : blackKeys) notes[pc].black = true;
    for (int pc : whiteKeys) addAndMakeVisible (notes[pc]);
    for (int pc : blackKeys) addAndMakeVisible (notes[pc]);
    for (int i = 0; i < 12; ++i)
    {
        notes[i].setButtonText (PitchlyProcessor::keyNames()[i]);
        notes[i].onUserClick = [this] { setReal ("scale", (float) (PitchlyProcessor::scaleNames().size() - 1)); };   // manual edit -> "Custom"
        noteAtt[i] = std::make_unique<ButtonAtt> (proc.apvts, PitchlyProcessor::noteParamId (i), notes[i]);
    }

    presetPill.prefix = "PRESET"; presetPill.value = [this] { return PitchlyProcessor::presetNames()[(int) getReal ("preset")]; };
    presetPill.onClick = [this] { showChoiceMenu (presetPill, PitchlyProcessor::presetNames(), "preset", false); };

    inPill.prefix = "IN";   inPill.meter  = [this] { return inLevel; };  inPill.meterWidth = 56;
    outPill.prefix = "OUT"; outPill.meter = [this] { return outLevel; }; outPill.meterWidth = 56;

    activePill.prefix = "ACTIVE";
    activePill.highlighted = [this] { return getReal ("active") > 0.5f; };
    activePill.onClick = [this] { setReal ("active", getReal ("active") > 0.5f ? 0.0f : 1.0f); repaint(); };

    keyPill.prefix = "KEY";     keyPill.value = [this] { return PitchlyProcessor::keyNames()[(int) getReal ("key")]; };
    keyPill.onClick = [this] { showChoiceMenu (keyPill, PitchlyProcessor::keyNames(), "key", false); };
    scalePill.prefix = "SCALE"; scalePill.value = [this] { return PitchlyProcessor::scaleNames()[(int) getReal ("scale")]; };
    scalePill.onClick = [this] { showChoiceMenu (scalePill, PitchlyProcessor::scaleNames(), "scale", true); };

    for (auto* pl : { &presetPill, &inPill, &outPill, &activePill, &keyPill, &scalePill }) addAndMakeVisible (pl);

    // voice range selector (SHIFT page)
    for (int i = 0; i < 4; ++i)
    {
        rangePills[i].prefix = PitchlyProcessor::rangeNames()[i];
        rangePills[i].centred = true;
        rangePills[i].highlighted = [this, i] { return (int) (getReal ("range") + 0.5f) == i; };
        rangePills[i].onClick = [this, i] { setReal ("range", (float) i); repaint(); };
        addChildComponent (rangePills[i]);
    }

    static const char* tabNames[NumPages] = { "TUNE", "SHIFT", "VIBRATO", "I/O" };
    for (int i = 0; i < NumPages; ++i)
    {
        tabs[i].label = tabNames[i];
        tabs[i].onClick = [this, i] { setPage (i); };
        addAndMakeVisible (tabs[i]);
    }
    resized();
    setPage (Tune);
    startTimerHz (40);
}

void PitchlyEditor::addKnob (int pg, Rectangle<int> bounds, const String& label, bool wide, const String& subtitle,
                             const String& id, double def, std::function<String (double)> fmt)
{
    auto e = std::make_unique<KnobEntry>();
    e->page = pg; e->bounds = bounds;
    e->knob = std::make_unique<Knob> (label, wide, subtitle, std::move (fmt));
    e->att  = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, e->knob->slider);
    e->knob->slider.setDoubleClickReturnValue (true, def);
    if (id == "transpose" || id == "detune" || id == "formant" || id == "outgain") e->knob->slider.getProperties().set ("bipolar", true);
    addChildComponent (*e->knob);
    e->knob->setBounds (bounds);
    knobs.push_back (std::move (e));
}

void PitchlyEditor::setPage (int pg)
{
    page = pg;
    for (int i = 0; i < NumPages; ++i) tabs[i].selected = (i == pg);
    for (auto& k : knobs) k->knob->setVisible (k->page == pg);
    for (auto& rp : rangePills) rp.setVisible (pg == Shift);
    repaint();
}

PitchlyEditor::~PitchlyEditor() { setLookAndFeel (nullptr); }

float PitchlyEditor::getReal (const String& id) const
{
    auto* p = dynamic_cast<RangedAudioParameter*> (proc.apvts.getParameter (id));
    return p != nullptr ? p->convertFrom0to1 (p->getValue()) : 0.0f;
}
void PitchlyEditor::setReal (const String& id, float v)
{
    if (auto* p = dynamic_cast<RangedAudioParameter*> (proc.apvts.getParameter (id)))
    {
        p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture();
    }
}

void PitchlyEditor::showChoiceMenu (Pill& anchor, const StringArray& items, const String& id, bool skipLast)
{
    PopupMenu m;
    const int cur = (int) getReal (id);
    for (int i = 0; i < items.size() - (skipLast ? 1 : 0); ++i) m.addItem (i + 1, items[i], true, i == cur);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&anchor), [this, id] (int r)
    {
        if (r <= 0) return;
        setReal (id, (float) (r - 1));
        if (id == "key" || id == "scale") applyScaleToNotes();
        if (id == "preset") applyPreset (r - 1);
        repaint();
    });
}

void PitchlyEditor::applyScaleToNotes()
{
    static const std::vector<std::vector<int>> scales {
        { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, { 0, 2, 4, 7, 9 }, { 0, 3, 5, 7, 10 } };
    const int scale = (int) getReal ("scale"), root = (int) getReal ("key");
    if (scale < 0 || scale >= (int) scales.size()) return;   // Custom: leave the notes alone
    for (int pc = 0; pc < 12; ++pc)
    {
        const int degree = (pc - root + 12) % 12;
        const bool in = std::find (scales[(size_t) scale].begin(), scales[(size_t) scale].end(), degree) != scales[(size_t) scale].end();
        setReal (PitchlyProcessor::noteParamId (pc), in ? 1.0f : 0.0f);
    }
}

void PitchlyEditor::applyPreset (int i)
{
    static const char* ids[7] = { "retune", "humanize", "amount", "formant", "mix", "tolerance", "vibkeep" };
    static const float v[4][7] = { { 12, 40, 100,  0,  80,  8, 50 },     // Natural Vocal
                                   { 90, 70,  70,  0, 100, 15, 80 },     // Soft Correction
                                   {  0,  0, 100,  0, 100,  0,  0 },     // Hard Tune
                                   {  0,  0, 100, -3, 100,  0,  0 } };   // Robot
    if (i < 0 || i > 3) return;
    for (int k = 0; k < 7; ++k) setReal (ids[k], v[i][k]);
}

void PitchlyEditor::timerCallback()
{
    auto toLevel = [] (float pk) { return jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (pk, -60.0f) + 60.0f) / 60.0f); };
    inLevel = toLevel (proc.inPeak.load()); outLevel = toLevel (proc.outPeak.load());

    // keep the pitch graph centred on what the singer is doing
    const auto& e = proc.engine;
    const int64_t w = (int64_t) e.histWrite.load (std::memory_order_acquire);
    double sum = 0.0; int cnt = 0;
    for (int i = 1; i <= 150 && w - i >= 0; ++i)
    {
        const float m = e.histDetected[(size_t) ((w - i) % pitchly::PitchEngine::kHistory)].load (std::memory_order_relaxed);
        if (std::isfinite (m)) { sum += m; ++cnt; }
    }
    if (cnt > 10) centreMidi += 0.10f * ((float) (sum / cnt) - centreMidi);

    repaint (graphBounds);          // animated graph
    inPill.repaint(); outPill.repaint();
}

void PitchlyEditor::resized()
{
    activePill.setBounds (792, 18, 80, 34);
    outPill.setBounds (662, 18, 120, 34);
    inPill.setBounds (532, 18, 120, 34);
    presetPill.setBounds (358, 18, 164, 34);

    for (int i = 0; i < NumPages; ++i) tabs[i].setBounds (624 + i * 62, 76, 62, 28);
    for (auto& k : knobs) k->knob->setBounds (k->bounds);
    for (int i = 0; i < 4; ++i) rangePills[i].setBounds (766, 373 + i * 38, 94, 30);

    // key snapping panel: (28,404,576,148)
    scalePill.setBounds (398, 414, 190, 30);
    keyPill.setBounds (314, 414, 74, 30);
    const float x0 = 44.0f, gap = 4.0f, ww = (544.0f - 6 * gap) / 7.0f, ky = 456.0f;
    static const int whiteKeys[7] = { 0, 2, 4, 5, 7, 9, 11 };
    for (int i = 0; i < 7; ++i)
        notes[whiteKeys[i]].setBounds (Rectangle<float> (x0 + i * (ww + gap), ky, ww, 80.0f).toNearestInt());
    static const int blackKeys[5] = { 1, 3, 6, 8, 10 }, leftWhite[5] = { 0, 1, 3, 4, 5 };
    for (int i = 0; i < 5; ++i)
    {
        const float seam = x0 + leftWhite[i] * (ww + gap) + ww + gap * 0.5f;
        notes[blackKeys[i]].setBounds (Rectangle<float> (seam - 21.0f, ky, 42.0f, 48.0f).toNearestInt());
    }
}

static String noteLabel (int midi)
{
    const int pc = ((midi % 12) + 12) % 12;
    return PitchlyProcessor::keyNames()[pc] + String ((midi - pc) / 12 - 1);
}

void PitchlyEditor::drawGraph (Graphics& g, Rectangle<float> panelR)
{
    drawPanel (g, panelR);
    g.setColour (mute); g.setFont (F (10.0f, false, 0.16f));
    g.drawText ("PITCH", (int) panelR.getX() + 18, (int) panelR.getY() + 12, 100, 14, Justification::left);

    // legend
    g.setFont (F (9.5f, false, 0.12f));
    const float ly = panelR.getY() + 19.0f;
    g.setColour (accent); g.fillEllipse (panelR.getRight() - 100.0f, ly - 3.0f, 6.0f, 6.0f);
    g.setColour (mute);   g.drawText ("CORRECTED", (int) panelR.getRight() - 90, (int) ly - 7, 76, 14, Justification::left);
    g.setColour (Colour (0xff6b7280)); g.fillEllipse (panelR.getRight() - 190.0f, ly - 3.0f, 6.0f, 6.0f);
    g.setColour (mute);   g.drawText ("INPUT", (int) panelR.getRight() - 180, (int) ly - 7, 60, 14, Justification::left);

    const auto plot = panelR.withTrimmedTop (40.0f).withTrimmedBottom (16.0f).withTrimmedLeft (50.0f).withTrimmedRight (16.0f);
    const float cy = plot.getCentreY(), pxPerSemi = plot.getHeight() / 12.0f;
    auto yOf = [&] (float midi) { return cy - (midi - centreMidi) * pxPerSemi; };

    {   // note bands + labels
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (Rectangle<float> (panelR.getX() + 1.0f, plot.getY(), panelR.getWidth() - 2.0f, plot.getHeight()).toNearestInt());
        for (int m = (int) std::floor (centreMidi - 7.5f); m <= (int) std::ceil (centreMidi + 7.5f); ++m)
        {
            const bool allowed = getReal (PitchlyProcessor::noteParamId (((m % 12) + 12) % 12)) > 0.5f;
            const float y = yOf ((float) m);
            if (allowed)
            {
                g.setColour (accent.withAlpha (0.055f)); g.fillRect (plot.getX(), y - pxPerSemi * 0.5f + 1.0f, plot.getWidth(), pxPerSemi - 2.0f);
                g.setColour (accent.withAlpha (0.16f));  g.drawHorizontalLine (roundToInt (y), plot.getX(), plot.getRight());
            }
            else { g.setColour (Colours::white.withAlpha (0.035f)); g.drawHorizontalLine (roundToInt (y), plot.getX(), plot.getRight()); }
            g.setColour (allowed ? soft : dim.withAlpha (0.6f)); g.setFont (F (9.5f, allowed));
            g.drawText (noteLabel (m), (int) panelR.getX() + 12, (int) y - 7, 34, 14, Justification::left);
        }
    }

    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (plot.expanded (2.0f, 6.0f).toNearestInt());

    const auto& e = proc.engine;
    const int64_t w = (int64_t) e.histWrite.load (std::memory_order_acquire);
    const int N = (int) plot.getWidth();
    auto build = [&] (const std::atomic<float>* hist, Path& path)
    {
        bool pen = false; float prev = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            const int64_t k = w - N + i;
            const float m = k >= 0 ? hist[(size_t) (k % pitchly::PitchEngine::kHistory)].load (std::memory_order_relaxed) : (float) NAN;
            if (! std::isfinite (m)) { pen = false; continue; }
            const float x = plot.getX() + (float) i, y = yOf (m);
            if (! pen || std::abs (m - prev) > 3.0f) path.startNewSubPath (x, y); else path.lineTo (x, y);
            pen = true; prev = m;
        }
    };
    Path det, cor; build (e.histDetected, det); build (e.histCorrected, cor);

    // curves fade in from the left so the graph feels like it is flowing
    auto fade = [&] (Colour c) { g.setGradientFill (ColourGradient (c.withAlpha (0.0f), plot.getX(), 0, c, plot.getX() + plot.getWidth() * 0.5f, 0, false)); };
    fade (Colour (0xff6b7280)); g.strokePath (det, PathStrokeType (1.7f, PathStrokeType::curved, PathStrokeType::rounded));
    const float glows[3][2] = { { 12.0f, 0.05f }, { 7.0f, 0.09f }, { 4.0f, 0.16f } };
    for (auto& gl : glows) { fade (accent.withAlpha (gl[1])); g.strokePath (cor, PathStrokeType (gl[0], PathStrokeType::curved, PathStrokeType::rounded)); }
    fade (accent); g.strokePath (cor, PathStrokeType (2.4f, PathStrokeType::curved, PathStrokeType::rounded));

    if (! cor.isEmpty())
    {
        const auto last = cor.getCurrentPosition();
        if (last.x > plot.getRight() - 8.0f)
        {
            const float pulse = 0.5f + 0.5f * std::sin ((float) Time::getMillisecondCounter() * 0.006f);
            const float rad = 16.0f + 6.0f * pulse;
            g.setGradientFill (ColourGradient (accent.withAlpha (0.45f), last, accent.withAlpha (0.0f), last.translated (rad, 0), true));
            g.fillEllipse (Rectangle<float> (rad * 2, rad * 2).withCentre (last));
            g.setColour (accent);        g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (last));
            g.setColour (Colours::white); g.fillEllipse (Rectangle<float> (3.5f, 3.5f).withCentre (last));
        }
    }
}

void PitchlyEditor::paint (Graphics& g)
{
    g.setGradientFill (ColourGradient (Colour (0xff0e1014), 0, 0, bg, 0, (float) getHeight(), false));
    g.fillAll();
    g.setColour (line); g.drawRect (getLocalBounds(), 1);

    // brand mark: accent tile with a little stepped pitch line
    const Rectangle<float> logo (28.0f, 19.0f, 32.0f, 32.0f);
    g.setColour (accent.withAlpha (0.18f)); g.fillRoundedRectangle (logo.expanded (3.0f), 11.0f);
    g.setColour (accent); g.fillRoundedRectangle (logo, 9.0f);
    Path steps; steps.startNewSubPath (logo.getX() + 6, logo.getBottom() - 9); steps.lineTo (logo.getX() + 13, logo.getBottom() - 9);
    steps.lineTo (logo.getX() + 13, logo.getCentreY()); steps.lineTo (logo.getX() + 20, logo.getCentreY());
    steps.lineTo (logo.getX() + 20, logo.getY() + 8); steps.lineTo (logo.getRight() - 5, logo.getY() + 8);
    g.setColour (bg); g.strokePath (steps, PathStrokeType (2.4f, PathStrokeType::mitered, PathStrokeType::rounded));
    g.setColour (text); g.setFont (F (18.0f, true, 0.04f)); g.drawText ("PITCHLY", 72, 16, 200, 24, Justification::left);
    g.setColour (mute); g.setFont (F (10.0f, false, 0.16f)); g.drawText ("AUTO PITCH CORRECTION", 72, 39, 170, 14, Justification::left);

    drawGraph (g, graphBounds.toFloat());
    drawPanel (g, { 28, 404, 576, 148 });                       // key snapping

    drawPanel (g, { 624, 76, 248, 28 }, 10.0f);                 // tab strip
    for (auto& k : knobs) if (k->page == page) drawPanel (g, k->bounds.toFloat());
    if (page == Shift)
    {
        drawPanel (g, { 754, 339, 118, 213 });
        g.setColour (mute); g.setFont (F (10.0f, false, 0.16f)); g.drawText ("VOICE RANGE", 754, 352, 118, 14, Justification::centred);
        g.setFont (F (9.0f)); g.setColour (dim);
        g.drawText ("match your singer", 754, 524, 118, 12, Justification::centred);
    }

    g.setColour (mute); g.setFont (F (10.0f, false, 0.16f));
    g.drawText ("KEY SNAPPING", 46, 421, 140, 14, Justification::left);
}
