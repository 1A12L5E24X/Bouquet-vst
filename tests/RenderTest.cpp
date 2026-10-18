// Offline smoke test: instantiate the plugin, play notes, and verify the output is
// audible, finite, silent when idle, and that state round-trips.
#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::cout << (ok ? "PASS  " : "FAIL  ") << what << "\n";
    if (! ok) ++failures;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());

    constexpr double sr = 48000.0;
    constexpr int block = 512;
    proc->setPlayConfigDetails (0, 2, sr, block);
    proc->prepareToPlay (sr, block);

    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;

    auto renderBlocks = [&] (int n, float& peak, bool& finite)
    {
        peak = 0.0f; finite = true;
        for (int b = 0; b < n; ++b)
        {
            proc->processBlock (buf, midi);
            midi.clear();
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                {
                    const auto s = buf.getSample (ch, i);
                    finite &= std::isfinite (s);
                    peak = std::max (peak, std::abs (s));
                }
        }
    };

    float peak; bool finite;
    renderBlocks (10, peak, finite);
    check (peak <= 0.0f, "silent with no notes");

    for (int wave = 0; wave < 4; ++wave)
    {
        proc->getParameters()[0]->setValueNotifyingHost ((float) wave / 3.0f);
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 64, 0.9f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 67, 0.9f), 0);
        renderBlocks (40, peak, finite);
        std::cout << "      waveform " << wave << " peak " << peak << "\n";
        check (peak > 0.01f && peak < 2.0f, "chord is audible and not clipping wildly");
        check (finite, "output is finite");

        for (int n : { 60, 64, 67 })
            midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
        renderBlocks (200, peak, finite); // > release time
        renderBlocks (2, peak, finite);
        check (peak <= 0.0f, "voices release to silence");
    }

    juce::MemoryBlock state;
    proc->getParameters()[5]->setValueNotifyingHost (0.123f);
    proc->getStateInformation (state);
    proc->getParameters()[5]->setValueNotifyingHost (0.9f);
    proc->setStateInformation (state.getData(), (int) state.getSize());
    check (std::abs (proc->getParameters()[5]->getValue() - 0.123f) < 1e-4f, "state round-trips");

    proc->releaseResources();
    std::cout << (failures == 0 ? "All tests passed\n" : "Tests FAILED\n");
    return failures == 0 ? 0 : 1;
}
