#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace ui
{
    const juce::Colour bg     { 0xff0e0f12 }, panel { 0xff15171b }, line { 0xff23262c }, text { 0xffe8e9ec },
                       mute   { 0xff7a7f89 }, soft  { 0xffb6bac2 }, dim  { 0xff5a5f69 }, teal { 0xff5eead4 };
}

// Rotary knob drawn like the Figma design: grey track + teal value arc
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
    std::function<bool()> highlighted;         // filled teal (ACTIVE)
    std::function<void()> onClick;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { if (onClick && contains (getMouseXYRelative())) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { if (onClick) setMouseCursor (juce::MouseCursor::PointingHandCursor); }
};

class NoteButton : public juce::Button
{
public:
    NoteButton() : juce::Button ({}) { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void mouseUp (const juce::MouseEvent& e) override
    {
        const bool userClick = isDown() && contains (e.getPosition());
        juce::Button::mouseUp (e);
        if (userClick && onUserClick) onUserClick();     // only real clicks, not state restored by the host
    }
    std::function<void()> onUserClick;
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
    void drawGraph (juce::Graphics&, juce::Rectangle<float> plot);
    void setReal (const juce::String& id, float realValue);
    float getReal (const juce::String& id) const;
    void applyScaleToNotes();
    void applyPreset (int index);
    void showChoiceMenu (Pill& anchor, const juce::StringArray& items, const juce::String& paramId, bool skipLast);

    PitchlyProcessor& proc;
    PitchlyLookAndFeel laf;

    Pill presetPill, inPill, outPill, activePill, keyPill, scalePill;
    Knob retune, humanize, amount, formant, mix;
    NoteButton notes[12];

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAtt> retuneAtt, humanizeAtt, amountAtt, formantAtt, mixAtt;
    std::unique_ptr<ButtonAtt> noteAtt[12];

    float centreMidi = 60.0f, inLevel = 0.0f, outLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchlyEditor)
};
