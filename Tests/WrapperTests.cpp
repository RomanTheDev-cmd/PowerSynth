// PowerSynth wrapper tests: processor, state, presets and the editor through the plug-in's shared code.
//   powersynth_wrapper_tests                     all checks
//   powersynth_wrapper_tests --snapshot <dir>    main.png, advanced.png, browser.png, main_small.png, main_large.png
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
int checks = 0, failures = 0;
void expect (bool ok, const juce::String& what)
{
    ++checks;
    if (! ok) { ++failures; std::cout << "FAIL: " << what << "\n"; }
}

void setPlain (PowerSynthProcessor& p, const char* id, float v)
{
    auto* prm = dynamic_cast<juce::RangedAudioParameter*> (p.state.getParameter (id));
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}
float plain (PowerSynthProcessor& p, const char* id) { return p.state.getRawParameterValue (id)->load(); }

void play (PowerSynthProcessor& p, int blocks, bool note = true)
{
    juce::AudioBuffer<float> b (2, 512);
    for (int i = 0; i < blocks; ++i)
    {
        juce::MidiBuffer m;
        if (note && i == 0) m.addEvent (juce::MidiMessage::noteOn (1, 48, 0.9f), 0);
        b.clear();
        p.processBlock (b, m);
    }
}

void testStateRoundTrip()
{
    PowerSynthProcessor a;
    a.prepareToPlay (48000.0, 512);
    setPlain (a, ps::id::cutoff, 777.0f);
    setPlain (a, ps::id::unison, 5.0f);
    setPlain (a, ps::id::analog, 1.0f);
    juce::MemoryBlock m;
    a.getStateInformation (m);
    PowerSynthProcessor b;
    b.prepareToPlay (48000.0, 512);
    b.setStateInformation (m.getData(), (int) m.getSize());
    expect (std::abs (plain (b, ps::id::cutoff) - 777.0f) < 1.0f && plain (b, ps::id::unison) == 5.0f && plain (b, ps::id::analog) == 1.0f,
            "state round trip keeps the sound");
}

void testPresets()
{
    PowerSynthProcessor p;
    p.prepareToPlay (48000.0, 512);
    const auto& all = p.library.all();
    expect (p.getPresetName() == all[0].name, "a fresh instance shows the first preset (" + p.getPresetName() + ")");
    int ok = 0;
    for (int i = 0; i < (int) all.size(); ++i)
    {
        p.loadPreset (i);
        bool same = p.getPresetName() == all[(size_t) i].name;
        for (const auto& [k, v] : all[(size_t) i].values)
            if (k != ps::id::oversample && k != ps::id::analog)
                same = same && std::abs (plain (p, k.toRawUTF8()) - v) < 0.01f * std::max (1.0f, std::abs (v));
        ok += same ? 1 : 0;
    }
    expect (ok == (int) all.size(), "every factory preset loads its values (" + juce::String (ok) + " of " + juce::String ((int) all.size()) + ")");
    setPlain (p, ps::id::oversample, 2.0f);
    setPlain (p, ps::id::analog, 1.0f);
    p.loadPreset (1);
    expect (plain (p, ps::id::oversample) == 2.0f && plain (p, ps::id::analog) == 1.0f, "a preset change keeps OVERSAMPLE and DIGITAL | ANALOGUE");
    // A project without the switch opens in Digital.
    juce::MemoryBlock m;
    p.getStateInformation (m);
    auto xml = juce::AudioProcessor::getXmlFromBinary (m.getData(), (int) m.getSize());
    if (xml != nullptr)
    {
        for (auto* c : xml->getChildIterator())
            if (c->getStringAttribute ("id") == ps::id::analog) { xml->removeChildElement (c, true); break; }
        juce::MemoryBlock m2;
        juce::AudioProcessor::copyXmlToBinary (*xml, m2);
        PowerSynthProcessor q;
        q.setStateInformation (m2.getData(), (int) m2.getSize());
        expect (plain (q, ps::id::analog) == 0.0f, "a project without DIGITAL | ANALOGUE opens in DIGITAL");
    }
}

void testProcessing()
{
    PowerSynthProcessor p;
    p.prepareToPlay (44100.0, 256);
    play (p, 50);
    juce::AudioBuffer<float> b (2, 256);
    juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 10);
    b.clear();
    p.processBlock (b, m);
    expect (b.getMagnitude (0, 0, 256) > 0.0f && b.getMagnitude (0, 0, 256) < 1.0f, "a note sounds below 0 dBFS through the processor");
    // Mono output bus.
    PowerSynthProcessor mono;
    juce::AudioProcessor::BusesLayout layout;
    layout.outputBuses.add (juce::AudioChannelSet::mono());
    expect (mono.setBusesLayout (layout), "a mono output bus is accepted");
    mono.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> mb (1, 512);
    juce::MidiBuffer mm;
    mm.addEvent (juce::MidiMessage::noteOn (1, 48, 1.0f), 0);
    mb.clear();
    mono.processBlock (mb, mm);
    expect (mb.getMagnitude (0, 0, 512) > 0.0f, "the mono bus plays");
    expect (p.getLatencySamples() >= 0, "latency reported");
}

void testEditor()
{
    PowerSynthProcessor p;
    p.prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    for (auto [w, h] : { std::pair { 1080, 720 }, std::pair { 864, 576 }, std::pair { 1620, 1080 } })
    {
        ed->setSize (w, h);
        int outside = 0, overlaps = 0;
        juce::Array<juce::Component*> shown;
        for (auto* c : ed->getChildren())
            if (c->isVisible() && c->getComponentID() != "presetBrowser")
            {
                outside += ed->getLocalBounds().contains (c->getBounds()) ? 0 : 1;
                for (auto* o : shown)
                    if (o->getBounds().intersects (c->getBounds()))
                    {
                        ++overlaps;
                        std::cout << "  overlap " << w << "x" << h << ": " << o->getComponentID() << " / " << c->getComponentID() << " ("
                                  << o->getBounds().toString() << " / " << c->getBounds().toString() << ")\n";
                    }
                shown.add (c);
            }
        expect (outside == 0 && overlaps == 0, "editor " + juce::String (w) + "x" + juce::String (h) + ": all controls inside, none overlapping ("
                + juce::String (outside) + " outside, " + juce::String (overlaps) + " overlaps)");
    }
    auto* seg = ed->findChildWithID ("analog");
    expect (seg != nullptr && seg->getNumChildComponents() == 2, "the header has DIGITAL | ANALOGUE");
    auto* wave = ed->findChildWithID ("waveform");
    expect (wave != nullptr && wave->getHeight() > ed->getHeight() / 6, "a large waveform display");
}

void snapshot (const juce::File& dir)
{
    dir.createDirectory();
    PowerSynthProcessor p;
    p.prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto save = [&dir] (juce::Component& c, const juce::String& name)
    {
        const auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
        juce::FileOutputStream out (dir.getChildFile (name));
        out.setPosition (0); out.truncate();
        juce::PNGImageFormat().writeImageToStream (img, out);
    };
    ed->setSize (1080, 720);
    save (*ed, "main.png");
    if (auto* adv = ed->findChildWithID ("advancedButton"))
        if (auto* b = dynamic_cast<juce::Button*> (adv)) { b->setToggleState (true, juce::sendNotification); save (*ed, "advanced.png"); b->setToggleState (false, juce::sendNotification); }
    if (auto* pb = ed->findChildWithID ("presetButton"))
        if (auto* b = dynamic_cast<juce::Button*> (pb)) { b->onClick(); save (*ed, "browser.png"); b->onClick(); }
    ed->setSize (864, 576); save (*ed, "main_small.png");
    ed->setSize (1620, 1080); save (*ed, "main_large.png");
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        snapshot (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String::fromUTF8 (argv[2])));
        return 0;
    }
    testStateRoundTrip();
    testPresets();
    testProcessing();
#if JUCE_MAC || JUCE_WINDOWS
    testEditor();
#endif
    std::cout << (failures == 0 ? "PASS " : "FAILED ") << (checks - failures) << "/" << checks << " wrapper checks\n";
    return failures == 0 ? 0 : 1;
}
