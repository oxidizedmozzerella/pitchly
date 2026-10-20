#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

const StringArray& PitchlyProcessor::keyNames()
{
    static const StringArray k { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return k;
}
const StringArray& PitchlyProcessor::scaleNames()
{
    static const StringArray s { "Major", "Minor", "Chromatic", "Pentatonic Major", "Pentatonic Minor", "Custom" };
    return s;
}
const StringArray& PitchlyProcessor::presetNames()
{
    static const StringArray s { "Natural Vocal", "Soft Correction", "Hard Tune", "Robot" };
    return s;
}

const StringArray& PitchlyProcessor::rangeNames()
{
    static const StringArray s { "Wide", "Low", "Mid", "High" };
    return s;
}

AudioProcessorValueTreeState::ParameterLayout PitchlyProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "retune", 1 }, "Retune Speed",
                 NormalisableRange<float> (0.0f, 200.0f, 0.1f, 0.4f), 12.0f,
                 AudioParameterFloatAttributes().withLabel ("ms")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "humanize", 1 }, "Humanize",
                 NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f, AudioParameterFloatAttributes().withLabel ("%")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, "Amount",
                 NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, AudioParameterFloatAttributes().withLabel ("%")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "formant", 1 }, "Formant",
                 NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("st")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                 NormalisableRange<float> (0.0f, 100.0f, 0.1f), 80.0f, AudioParameterFloatAttributes().withLabel ("%")));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "active", 1 }, "Active", true));

    // ---- pro controls ----
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tolerance", 1 }, "Tolerance",
                 NormalisableRange<float> (0.0f, 100.0f, 0.1f), 8.0f, AudioParameterFloatAttributes().withLabel ("cents")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "transpose", 1 }, "Transpose",
                 NormalisableRange<float> (-12.0f, 12.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withLabel ("st")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "detune", 1 }, "Detune",
                 NormalisableRange<float> (-50.0f, 50.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("cents")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "reference", 1 }, "Reference Pitch",
                 NormalisableRange<float> (430.0f, 450.0f, 0.1f), 440.0f, AudioParameterFloatAttributes().withLabel ("Hz")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "vibkeep", 1 }, "Keep Vibrato",
                 NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, AudioParameterFloatAttributes().withLabel ("%")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "vibdepth", 1 }, "Vibrato Depth",
                 NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("cents")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "vibrate", 1 }, "Vibrato Rate",
                 NormalisableRange<float> (0.5f, 9.0f, 0.1f), 5.5f, AudioParameterFloatAttributes().withLabel ("Hz")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "vibdelay", 1 }, "Vibrato Delay",
                 NormalisableRange<float> (0.0f, 1500.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withLabel ("ms")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gate", 1 }, "Gate Threshold",
                 NormalisableRange<float> (-70.0f, -20.0f, 0.5f), -48.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "outgain", 1 }, "Output Gain",
                 NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "range", 1 }, "Voice Range", rangeNames(), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", keyNames(), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scaleNames(), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "preset", 1 }, "Preset", presetNames(), 0));

    const bool cMajor[12] = { true, false, true, false, true, true, false, true, false, true, false, true };
    for (int i = 0; i < 12; ++i)
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { noteParamId (i), 1 }, "Note " + keyNames()[i], cMajor[i]));

    return { p.begin(), p.end() };
}

PitchlyProcessor::PitchlyProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PITCHLY", createLayout())
{
    pRetune = apvts.getRawParameterValue ("retune");     pHumanize = apvts.getRawParameterValue ("humanize");
    pAmount = apvts.getRawParameterValue ("amount");     pFormant  = apvts.getRawParameterValue ("formant");
    pMix    = apvts.getRawParameterValue ("mix");        pActive   = apvts.getRawParameterValue ("active");
    pTolerance = apvts.getRawParameterValue ("tolerance"); pTranspose = apvts.getRawParameterValue ("transpose");
    pDetune    = apvts.getRawParameterValue ("detune");    pReference = apvts.getRawParameterValue ("reference");
    pVibKeep   = apvts.getRawParameterValue ("vibkeep");   pVibDepth  = apvts.getRawParameterValue ("vibdepth");
    pVibRate   = apvts.getRawParameterValue ("vibrate");   pVibDelay  = apvts.getRawParameterValue ("vibdelay");
    pGate      = apvts.getRawParameterValue ("gate");      pOutGain   = apvts.getRawParameterValue ("outgain");
    pRange     = apvts.getRawParameterValue ("range");
    for (int i = 0; i < 12; ++i) pNote[i] = apvts.getRawParameterValue (noteParamId (i));
}

bool PitchlyProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& in = l.getMainInputChannelSet();
    return in == l.getMainOutputChannelSet() && (in == AudioChannelSet::mono() || in == AudioChannelSet::stereo());
}

void PitchlyProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate, jmax (1, getTotalNumInputChannels()));
    setLatencySamples (engine.getLatencySamples());   // lets FL Studio compensate the delay automatically
}

void PitchlyProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int numCh = jmin (buffer.getNumChannels(), pitchly::PitchEngine::kMaxChannels);
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);

    float pkIn = 0.0f;
    for (int c = 0; c < numCh; ++c) pkIn = jmax (pkIn, buffer.getMagnitude (c, 0, n));

    pitchly::EngineParams p;
    p.retuneMs     = pRetune->load();
    p.humanize     = pHumanize->load() * 0.01f;
    p.amount       = pAmount->load() * 0.01f;
    p.formantSemis = pFormant->load();
    p.mix          = pMix->load() * 0.01f;
    p.active       = pActive->load() > 0.5f;
    p.tolerance    = pTolerance->load();
    p.transpose    = pTranspose->load();
    p.detuneCents  = pDetune->load();
    p.refHz        = pReference->load();
    p.keepVibrato  = pVibKeep->load() * 0.01f;
    p.vibDepthCents = pVibDepth->load();
    p.vibRateHz    = pVibRate->load();
    p.vibDelayMs   = pVibDelay->load();
    p.gateDb       = pGate->load();
    p.outGainDb    = pOutGain->load();
    p.range        = (int) (pRange->load() + 0.5f);
    for (int i = 0; i < 12; ++i) p.notes[i] = pNote[i]->load() > 0.5f;

    engine.process (buffer.getArrayOfWritePointers(), numCh, n, p);

    float pkOut = 0.0f;
    for (int c = 0; c < numCh; ++c) pkOut = jmax (pkOut, buffer.getMagnitude (c, 0, n));
    const float decay = std::pow (0.5f, (float) n / (float) jmax (1.0, getSampleRate() * 0.25));
    inPeak.store  (jmax (pkIn,  inPeak.load()  * decay));
    outPeak.store (jmax (pkOut, outPeak.load() * decay));
}

AudioProcessorEditor* PitchlyProcessor::createEditor() { return new PitchlyEditor (*this); }

void PitchlyProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}
void PitchlyProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PitchlyProcessor(); }
