// Headless UI check: runs a synthetic wobbly voice through the real processor, then renders the editor to a PNG.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    PitchlyProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    double ph = 0.0; juce::MidiBuffer midi; juce::AudioBuffer<float> buf (2, 512);
    for (int blockIdx = 0; blockIdx < 400; ++blockIdx)   // ~4.2 s
    {
        for (int i = 0; i < 512; ++i)
        {
            const double t = (blockIdx * 512 + i) / 48000.0;
            const double hz = (t < 2.0 ? 232.0 : 252.0) * std::pow (2.0, 25.0 * std::sin (6.28318 * 5.5 * t) / 1200.0);
            ph += 6.28318530718 * hz / 48000.0;
            float v = 0.0f; for (int h = 1; h <= 10; ++h) v += (float) std::sin (h * ph) / (float) h;
            buf.setSample (0, i, 0.25f * v); buf.setSample (1, i, 0.25f * v);
        }
        proc.processBlock (buf, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setBounds (0, 0, 900, 580);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (argc > 1 ? argv[1] : "pitchly_ui.png");
    out.deleteFile();
    juce::FileOutputStream fos (out);
    juce::PNGImageFormat().writeImageToStream (img, fos);
    std::printf ("wrote %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
