#include "PluginEditor.h"

SimpleSynthEditor::SimpleSynthEditor (SimpleSynthProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    waveBox.addItemList (juce::StringArray { "Sine", "Saw", "Square", "Triangle" }, 1);
    waveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "wave", waveBox);
    waveLabel.setText ("Waveform", juce::dontSendNotification);
    waveLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (waveBox);
    addAndMakeVisible (waveLabel);

    addKnob (attack, "attack", "Attack");
    addKnob (decay, "decay", "Decay");
    addKnob (sustain, "sustain", "Sustain");
    addKnob (release, "release", "Release");
    addKnob (cutoff, "cutoff", "Cutoff");
    addKnob (resonance, "resonance", "Resonance");
    addKnob (envAmount, "envAmount", "Filter Env");
    addKnob (gain, "gain", "Gain");

    keyboard.setKeyWidth (24.0f);
    addAndMakeVisible (keyboard);

    setSize (720, 380);
}

void SimpleSynthEditor::addKnob (Knob& k, const juce::String& paramID, const juce::String& name)
{
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, paramID, k.slider);
    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
}

void SimpleSynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2129));
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("Simple Synth", getLocalBounds().removeFromTop (40).reduced (12, 0), juce::Justification::centredLeft);
}

void SimpleSynthEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (34);

    keyboard.setBounds (area.removeFromBottom (80));
    area.removeFromBottom (10);

    auto top = area.removeFromTop (area.getHeight() / 2);
    auto bottom = area;

    auto place = [] (juce::Rectangle<int>& row, int width, juce::Component& c, juce::Label& l)
    {
        auto cell = row.removeFromLeft (width).reduced (4);
        l.setBounds (cell.removeFromTop (18));
        c.setBounds (cell);
    };

    const int w = top.getWidth() / 5;
    {
        auto cell = top.removeFromLeft (w).reduced (4);
        waveLabel.setBounds (cell.removeFromTop (18));
        waveBox.setBounds (cell.withSizeKeepingCentre (cell.getWidth(), 26));
    }
    for (auto* k : { &attack, &decay, &sustain, &release })
        place (top, w, k->slider, k->label);

    const int w2 = bottom.getWidth() / 4;
    for (auto* k : { &cutoff, &resonance, &envAmount, &gain })
        place (bottom, w2, k->slider, k->label);
}
