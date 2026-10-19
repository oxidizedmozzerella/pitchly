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

// ------------------------------------------------------------------------------------------------
void PitchlyLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const float r = size * 0.4f, thick = size * 0.0625f;

    Path track; track.addCentredArc (c.x, c.y, r, r, 0.0f, start, end, true);
    g.setColour (line);
    g.strokePath (track, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));

    const float a = start + pos * (end - start);
    if (pos > 0.001f)
    {
        Path val; val.addCentredArc (c.x, c.y, r, r, 0.0f, start, a, true);
        g.setColour (teal);
        g.strokePath (val, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));
    }
    if ((bool) s.getProperties().getWithDefault ("thumb", false))
    {
        g.setColour (text);
        g.fillEllipse (Rectangle<float> (size * 0.075f, size * 0.075f).withCentre ({ c.x + r * std::sin (a), c.y - r * std::cos (a) }));
    }
}

// ------------------------------------------------------------------------------------------------
Knob::Knob (const String& label, bool hero, std::function<String (double)> format)
    : labelText (label), isHero (hero), fmt (std::move (format))
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    slider.getProperties().set ("thumb", hero);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}
void Knob::resized()
{
    if (isHero) slider.setBounds (16, (getHeight() - 104) / 2, 104, 104);
    else        slider.setBounds ((getWidth() - 64) / 2, 18, 64, 64);
}
void Knob::paint (Graphics& g)
{
    const String v = fmt (slider.getValue());
    if (isHero)
    {
        const int tx = 136, mid = getHeight() / 2;
        g.setColour (mute); g.setFont (F (10.0f, false, 0.16f)); g.drawText (labelText.toUpperCase(), tx, mid - 36, 150, 14, Justification::left);
        g.setColour (text); g.setFont (F (26.0f, true));          g.drawText (v, tx, mid - 21, 150, 34, Justification::left);
        g.setColour (mute); g.setFont (F (12.0f));                g.drawText (CharPointer_UTF8 ("Natural \xe2\x86\x92 Hard tune"), tx, mid + 16, 150, 16, Justification::left);
    }
    else
    {
        g.setColour (text); g.setFont (F (13.0f)); g.drawText (v, 0, 86, getWidth(), 16, Justification::centred);
        g.setColour (mute); g.setFont (F (10.0f, false, 0.16f)); g.drawText (labelText.toUpperCase(), 0, 106, getWidth(), 14, Justification::centred);
    }
}

// ------------------------------------------------------------------------------------------------
void Pill::paint (Graphics& g)
{
    const bool hi = highlighted && highlighted();
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (hi ? teal : panel); g.fillRoundedRectangle (b, b.getHeight() / 2);
    g.setColour (hi ? teal : line);  g.drawRoundedRectangle (b, b.getHeight() / 2, 1.0f);

    float x = 16.0f;
    const auto pf = F (12.0f, hi), vf = F (12.0f, true);
    g.setColour (hi ? bg : soft); g.setFont (pf);
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
        const Rectangle<float> bar (x, getHeight() * 0.5f - 2.0f, 90.0f, 4.0f);
        g.setColour (line); g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (teal); g.fillRoundedRectangle (bar.withWidth (jmax (0.0f, 90.0f * jlimit (0.0f, 1.0f, meter()))), 2.0f);
    }
}

void NoteButton::paintButton (Graphics& g, bool over, bool)
{
    const bool on = getToggleState();
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (on ? teal.withAlpha (over ? 0.22f : 0.14f) : (over ? panel : bg)); g.fillRoundedRectangle (b, 8.0f);
    g.setColour (on ? teal : line); g.drawRoundedRectangle (b, 8.0f, 1.0f);
    g.setColour (on ? teal : dim); g.setFont (F (11.0f, on));
    g.drawText (getButtonText(), getLocalBounds(), Justification::centred);
}

// ------------------------------------------------------------------------------------------------
PitchlyEditor::PitchlyEditor (PitchlyProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      retune   ("Retune Speed", true,  [] (double v) { return v < 10.0 ? String (v, 1) + " ms" : String (roundToInt (v)) + " ms"; }),
      humanize ("Humanize",     false, [] (double v) { return String (roundToInt (v)) + "%"; }),
      amount   ("Amount",       false, [] (double v) { return String (roundToInt (v)) + "%"; }),
      formant  ("Formant",      false, [] (double v) { return std::abs (v) < 0.05 ? String ("0 st") : (v > 0 ? "+" : "") + String (v, 1) + " st"; }),
      mix      ("Mix",          false, [] (double v) { return String (roundToInt (v)) + "%"; })
{
    setLookAndFeel (&laf);
    setSize (900, 580);
    setResizable (false, false);

    auto& st = proc.apvts;
    retuneAtt   = std::make_unique<SliderAtt> (st, "retune",   retune.slider);
    humanizeAtt = std::make_unique<SliderAtt> (st, "humanize", humanize.slider);
    amountAtt   = std::make_unique<SliderAtt> (st, "amount",   amount.slider);
    formantAtt  = std::make_unique<SliderAtt> (st, "formant",  formant.slider);
    mixAtt      = std::make_unique<SliderAtt> (st, "mix",      mix.slider);
    retune.slider.setDoubleClickReturnValue (true, 12.0);   humanize.slider.setDoubleClickReturnValue (true, 40.0);
    amount.slider.setDoubleClickReturnValue (true, 100.0);  formant.slider.setDoubleClickReturnValue (true, 0.0);
    mix.slider.setDoubleClickReturnValue (true, 80.0);
    for (auto* k : { &retune, &humanize, &amount, &formant, &mix }) addAndMakeVisible (k);

    for (int i = 0; i < 12; ++i)
    {
        notes[i].setButtonText (PitchlyProcessor::keyNames()[i]);
        notes[i].onUserClick = [this] { setReal ("scale", (float) (PitchlyProcessor::scaleNames().size() - 1)); };   // manual edit -> "Custom"
        addAndMakeVisible (notes[i]);
        noteAtt[i] = std::make_unique<ButtonAtt> (st, PitchlyProcessor::noteParamId (i), notes[i]);
    }

    presetPill.prefix = "PRESET"; presetPill.value = [this] { return PitchlyProcessor::presetNames()[(int) getReal ("preset")]; };
    presetPill.onClick = [this] { showChoiceMenu (presetPill, PitchlyProcessor::presetNames(), "preset", false); };

    inPill.prefix = "IN";   inPill.meter  = [this] { return inLevel; };
    outPill.prefix = "OUT"; outPill.meter = [this] { return outLevel; };

    activePill.prefix = "ACTIVE";
    activePill.highlighted = [this] { return getReal ("active") > 0.5f; };
    activePill.onClick = [this] { setReal ("active", getReal ("active") > 0.5f ? 0.0f : 1.0f); repaint(); };

    keyPill.prefix = "KEY";     keyPill.value = [this] { return PitchlyProcessor::keyNames()[(int) getReal ("key")]; };
    keyPill.onClick = [this] { showChoiceMenu (keyPill, PitchlyProcessor::keyNames(), "key", false); };
    scalePill.prefix = "SCALE"; scalePill.value = [this] { return PitchlyProcessor::scaleNames()[(int) getReal ("scale")]; };
    scalePill.onClick = [this] { showChoiceMenu (scalePill, PitchlyProcessor::scaleNames(), "scale", true); };

    for (auto* pl : { &presetPill, &inPill, &outPill, &activePill, &keyPill, &scalePill }) addAndMakeVisible (pl);
    startTimerHz (30);
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
    //                         retune humanize amount formant mix
    static const float v[4][5] = { { 12, 40, 100, 0, 80 }, { 90, 70, 70, 0, 100 }, { 0, 0, 100, 0, 100 }, { 0, 0, 100, -3, 100 } };
    if (i < 0 || i > 3) return;
    setReal ("retune", v[i][0]); setReal ("humanize", v[i][1]); setReal ("amount", v[i][2]);
    setReal ("formant", v[i][3]); setReal ("mix", v[i][4]);
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
    if (cnt > 10) centreMidi += 0.12f * ((float) (sum / cnt) - centreMidi);
    repaint();
}

void PitchlyEditor::resized()
{
    presetPill.setBounds (294, 30, 164, 34);
    inPill.setBounds (468, 30, 150, 34);
    outPill.setBounds (628, 30, 150, 34);
    activePill.setBounds (788, 30, 80, 34);

    keyPill.setBounds (256, 294, 74, 30);
    scalePill.setBounds (340, 294, 190, 30);
    const float kw = (482.0f - 66.0f) / 12.0f;
    for (int i = 0; i < 12; ++i)
        notes[i].setBounds (Rectangle<float> (48.0f + i * (kw + 6.0f), 336.0f, kw, 36.0f).toNearestInt());

    retune.setBounds (568, 88, 300, 166);
    humanize.setBounds (568, 270, 142, 132);  amount.setBounds (726, 270, 142, 132);
    formant.setBounds  (568, 418, 142, 132);  mix.setBounds    (726, 418, 142, 132);
}

void PitchlyEditor::drawGraph (Graphics& g, Rectangle<float> plot)
{
    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (plot.expanded (0, 4).toNearestInt());
    const float cy = plot.getCentreY(), pxPerSemi = 20.0f;
    auto yOf = [&] (float midi) { return cy - (midi - centreMidi) * pxPerSemi; };

    for (int m = (int) std::floor (centreMidi - 4.5f); m <= (int) std::ceil (centreMidi + 4.5f); ++m)
    {
        const bool allowed = getReal (PitchlyProcessor::noteParamId (((m % 12) + 12) % 12)) > 0.5f;
        const float y = yOf ((float) m);
        g.setColour (line.withAlpha (allowed ? 1.0f : 0.45f));
        g.drawHorizontalLine (roundToInt (y), plot.getX(), plot.getRight());
        if (allowed)
        {
            g.setColour (dim); g.setFont (F (9.0f));
            g.drawText (PitchlyProcessor::keyNames()[((m % 12) + 12) % 12], (int) plot.getX() + 2, (int) y - 11, 24, 10, Justification::left);
        }
    }

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
    g.setColour (dim);  g.strokePath (det, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (teal); g.strokePath (cor, PathStrokeType (2.5f, PathStrokeType::curved, PathStrokeType::rounded));
    if (! cor.isEmpty())
    {
        const auto last = cor.getCurrentPosition();
        if (last.x > plot.getRight() - 6.0f) g.fillEllipse (Rectangle<float> (8.0f, 8.0f).withCentre (last));
    }
}

void PitchlyEditor::paint (Graphics& g)
{
    g.fillAll (bg);
    g.setColour (line); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 20.0f, 1.0f);

    // brand
    g.setColour (teal); g.fillRoundedRectangle (33.0f, 34.0f, 26.0f, 26.0f, 8.0f);
    g.setColour (text); g.setFont (F (17.0f, true, 0.03f)); g.drawText ("PITCHLY", 71, 28, 200, 22, Justification::left);
    g.setColour (mute); g.setFont (F (11.0f, false, 0.14f)); g.drawText ("AUTO PITCH CORRECTION", 71, 50, 220, 14, Justification::left);

    auto panelRect = [&] (Rectangle<float> r)
    {
        g.setColour (panel); g.fillRoundedRectangle (r, 16.0f);
        g.setColour (line);  g.drawRoundedRectangle (r.reduced (0.5f), 16.0f, 1.0f);
    };
    panelRect ({ 32, 88, 514, 176 });   // pitch graph
    panelRect ({ 32, 280, 514, 110 });  // key snapping
    panelRect ({ 568, 88, 300, 166 });
    panelRect ({ 568, 270, 142, 132 }); panelRect ({ 726, 270, 142, 132 });
    panelRect ({ 568, 418, 142, 132 }); panelRect ({ 726, 418, 142, 132 });

    g.setColour (mute); g.setFont (F (10.0f, false, 0.16f));
    g.drawText ("PITCH", 48, 102, 100, 14, Justification::left);
    g.drawText ("KEY SNAPPING", 48, 301, 140, 14, Justification::left);

    drawGraph (g, { 48.0f, 118.0f, 496.0f, 140.0f });
}
