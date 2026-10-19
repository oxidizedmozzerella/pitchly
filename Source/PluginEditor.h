#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace ui
{
    inline const juce::Colour bg { 0xff0b0c0f }, panelTop { 0xff161920 }, panelBot { 0xff111317 }, line { 0xff23262d },
                              text { 0xffeceef2 }, mute { 0xff7d828d }, soft { 0xffb8bcc5 }, dim { 0xff595e69 };
    inline juce::Colour accent { 0xff5eead4 };      // changed at runtime by the theme dots

    inline const juce::Colour themeColours[5] = { juce::Colour (0xff5eead4), juce::Colour (0xff60a5fa), juce::Colour (0xffa78bfa),
                                                  juce::Colour (0xfffbbf24), juce::Colour (0xfffb7185) };
}

// Layered knob: glowing value ring + metal cap with an indicator line
class PitchlyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
};

class Knob : public juce::Component
{
public:
    Knob (const juce::String& label, bool hero, std::function<juce::String (double)> format);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
private:
    juce::String labelText; bool isHero; std::function<juce::String (double)> fmt;
};

class Pill : public juce::Component
{
public:
    juce::String prefix;
    std::function<juce::String()> value;       // optional bold value text
    std::function<float()> meter;              // optional 0..1 level bar
    std::function<bool()> highlighted;         // filled accent (ACTIVE)
    std::function<void()> onClick;
    int meterWidth = 90;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { if (onClick && contains (getMouseXYRelative())) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { if (onClick) setMouseCursor (juce::MouseCursor::PointingHandCursor); }
};

// One key of the piano-style note picker
class NoteButton : public juce::Button
{
public:
    explicit NoteButton (bool isBlackKey = false) : juce::Button ({}), black (isBlackKey) { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void mouseUp (const juce::MouseEvent& e) override
    {
        const bool userClick = isDown() && contains (e.getPosition());
        juce::Button::mouseUp (e);
        if (userClick && onUserClick) onUserClick();     // only real clicks, not state restored by the host
    }
    std::function<void()> onUserClick;
    bool black;
};

class ThemeDot : public juce::Component
{
public:
    juce::Colour colour; bool selected = false; std::function<void()> onClick;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { if (onClick && contains (getMouseXYRelative())) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::PointingHandCursor); repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
};

class PitchlyEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PitchlyEditor (PitchlyProcessor&);
    ~PitchlyEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawGraph (juce::Graphics&, juce::Rectangle<float> panelRect);
    void setReal (const juce::String& id, float realValue);
    float getReal (const juce::String& id) const;
    void applyScaleToNotes();
    void applyPreset (int index);
    void applyTheme (int index);
    void showChoiceMenu (Pill& anchor, const juce::StringArray& items, const juce::String& paramId, bool skipLast);

    PitchlyProcessor& proc;
    PitchlyLookAndFeel laf;

    Pill presetPill, inPill, outPill, activePill, keyPill, scalePill;
    Knob retune, humanize, amount, formant, mix;
    NoteButton notes[12];
    ThemeDot themeDots[5];

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAtt> retuneAtt, humanizeAtt, amountAtt, formantAtt, mixAtt;
    std::unique_ptr<ButtonAtt> noteAtt[12];

    const juce::Rectangle<int> graphBounds { 28, 76, 576, 312 };
    float centreMidi = 60.0f, inLevel = 0.0f, outLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchlyEditor)
};
