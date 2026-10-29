// PowerSynth wrapper tests: processor, state, presets and the editor through the plug-in's shared code.
//   powersynth_wrapper_tests                     all checks
//   powersynth_wrapper_tests --snapshot <dir>    main.png, advanced.png, browser.png, browser_favourites.png, main_small.png,
//                                                main_large.png
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
            {
                const bool ok = std::abs (plain (p, k.toRawUTF8()) - v) < 0.01f * std::max (1.0f, std::abs (v));
                if (! ok) std::cout << "  " << all[(size_t) i].name << ": " << k << " = " << v << " loads as " << plain (p, k.toRawUTF8()) << "\n";
                same = same && ok;
            }
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

// Favourites (2026-10-29). A temporary preset folder, never the user's own; left in the system temp folder.
juce::File tempPresetDir()
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("PowerSynthFavouritesTest-" + juce::String (juce::Random::getSystemRandom().nextInt (1 << 30)));
    dir.createDirectory();
    return dir;
}
juce::StringArray starredNames (const ps::PresetLibrary& l)
{
    juce::StringArray out;
    for (int i : ps::filterPresets (l.all(), ps::favouritesCategory, {}, {}))
        out.add (l.all()[(size_t) i].name + (l.all()[(size_t) i].isFactory() ? "" : " (user)"));
    return out;
}
int userIndex (const ps::PresetLibrary& l, const juce::String& name)
{
    for (int i = 0; i < (int) l.all().size(); ++i)
        if (! l.all()[(size_t) i].isFactory() && l.all()[(size_t) i].name == name)
            return i;
    return -1;
}

void testFavourites()
{
    const auto dir = tempPresetDir();
    auto trashed = dir.getChildFile ("trashed");
    trashed.createDirectory();
    auto fakeTrash = [trashed] (const juce::File& f) { return f.moveFileTo (trashed.getChildFile (f.getFileName())); };
    juce::String e;
    ps::PresetLibrary a (dir);
    a.moveToTrash = fakeTrash;
    expect (starredNames (a).isEmpty(), "no favourites in a new preset folder");
    const juce::String shared = a.all()[3].name; // a factory name a user preset will share
    a.saveUser (shared, a.all()[3].values, e);
    a.saveUser ("My Lead", a.all()[0].values, e);
    const bool on = a.toggleFavourite (3);
    expect (on && a.all()[3].favourite && ! a.all()[(size_t) userIndex (a, shared)].favourite,
            "a star on a factory preset leaves the user preset of the same name unstarred");

    ps::PresetLibrary b (dir); // another instance, or another project
    b.moveToTrash = fakeTrash;
    expect (b.all()[3].favourite, "the star shows in another instance");
    b.toggleFavourite (userIndex (b, "My Lead"));
    a.rescan();
    expect (starredNames (a) == juce::StringArray (shared, "My Lead (user)"),
            "Favourites lists the starred presets in library order, both instances' stars kept (" + starredNames (a).joinIntoString (", ") + ")");
    expect (ps::filterPresets (a.all(), ps::favouritesCategory, {}, "my lead").size() == 1, "search works inside Favourites");

    expect (a.rename (userIndex (a, "My Lead"), "Lead X", e) && a.all()[(size_t) userIndex (a, "Lead X")].favourite,
            "a renamed user preset keeps its star");
    b.rescan();
    expect (starredNames (b).contains ("Lead X (user)") && ! starredNames (b).contains ("My Lead (user)"), "the rename reaches other instances");
    expect (a.remove (userIndex (a, "Lead X"), e), "a starred user preset can be deleted: " + e);
    a.saveUser ("Lead X", a.all()[0].values, e); // a new preset of that name starts without a star
    expect (! a.all()[(size_t) userIndex (a, "Lead X")].favourite && starredNames (a) == juce::StringArray (shared),
            "a deleted preset loses its star (" + starredNames (a).joinIntoString (", ") + ")");
    expect (! a.toggleFavourite (3) && ! a.all()[3].favourite && starredNames (a).isEmpty(), "a second click removes the star");

    a.toggleFavourite (5);
    a.favouritesFile().replaceWithText ("{ not json at all");
    ps::PresetLibrary c (dir);
    expect (starredNames (c).isEmpty() && c.all().size() == a.all().size(), "a damaged favourites file loads as no stars, every preset still there");
    c.toggleFavourite (7);
    ps::PresetLibrary d (dir);
    expect (d.all()[7].favourite && starredNames (d).size() == 1, "after a damaged file the next star writes a good one");
}

// The browser: the star at the left of a row stars that preset without loading it; the rest of the row loads it.
// Favourites is a category of the browser; the header star stars the current preset.
void testFavouritesInEditor()
{
    PowerSynthProcessor p;
    p.prepareToPlay (48000.0, 512);
    p.library = ps::PresetLibrary (tempPresetDir());
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (1080, 720);
    auto* star = dynamic_cast<ps::ui::StarButton*> (ed->findChildWithID ("favouriteButton"));
    expect (star != nullptr, "the header has a star for the current preset");
    if (star == nullptr)
        return;
    const auto current = p.getPresetName();
    star->onClick();
    const int ci = p.library.indexOf (current);
    expect (ci >= 0 && p.library.all()[(size_t) ci].favourite && star->isStarred(), "the header star stars the current preset");

    auto* presetButton = dynamic_cast<juce::Button*> (ed->findChildWithID ("presetButton"));
    presetButton->onClick();
    auto* browser = dynamic_cast<ps::ui::PresetBrowser*> (ed->findChildWithID ("presetBrowser"));
    auto* list = browser != nullptr ? dynamic_cast<juce::ListBox*> (browser->findChildWithID ("presetList")) : nullptr;
    expect (list != nullptr && browser->isVisible(), "the browser opens with its list");
    if (list == nullptr)
        return;
    browser->setCategory ({});
    // A row on screen (the list scrolls to the current preset) that is not the current preset.
    const auto order = ps::filterPresets (p.library.all(), {}, {}, {});
    int row = browser->firstVisibleRow();
    while (row < browser->numShown() - 1 && p.library.all()[(size_t) order[(size_t) row]].name == current)
        ++row;
    auto* spot = list->getComponentForRowNumber (row); // the row's own component: the star's click target
    auto* rowComp = spot != nullptr ? spot->getParentComponent() : nullptr;
    expect (rowComp != nullptr, "row " + juce::String (row) + " is on screen with its star (list " + list->getBounds().toString() + ", "
            + juce::String (list->getNumRowsOnScreen()) + " rows on screen)");
    if (rowComp == nullptr)
        return;
    const int h = rowComp->getHeight();
    auto* atStar = rowComp->getComponentAt (juce::roundToInt (12 * ed->getWidth() / 1080.0f), h / 2);
    auto* atName = rowComp->getComponentAt (rowComp->getWidth() / 2, h / 2);
    expect (atStar != nullptr && atStar != rowComp && atName == rowComp, "the star is a click target of its own; the name belongs to the row");
    const int preset = order[(size_t) row];
    if (atStar != nullptr && atStar != rowComp)
    {
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        const juce::Point<float> at (12.0f, (float) h / 2);
        const juce::MouseEvent click (src, at, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, atStar, atStar, juce::Time::getCurrentTime(), at,
                                      juce::Time::getCurrentTime(), 1, false);
        atStar->mouseDown (click);
    }
    expect (p.library.all()[(size_t) preset].favourite, "clicking the star of a row stars that preset");
    expect (p.getPresetName() == current, "starring a row does not load it (still " + p.getPresetName() + ")");
    browser->setCategory (ps::favouritesCategory);
    expect (browser->numShown() == 2, "the Favourites category shows the two starred presets (" + juce::String (browser->numShown()) + ")");
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
    p.library = ps::PresetLibrary (tempPresetDir()); // two stars to show, never in the user's own folder
    p.library.toggleFavourite (0);
    p.library.toggleFavourite (5);
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
        if (auto* b = dynamic_cast<juce::Button*> (pb))
        {
            b->onClick();
            save (*ed, "browser.png");
            if (auto* br = dynamic_cast<ps::ui::PresetBrowser*> (ed->findChildWithID ("presetBrowser")))
            {
                br->setCategory (ps::favouritesCategory);
                save (*ed, "browser_favourites.png");
                br->setCategory ({});
            }
            b->onClick();
        }
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
    testFavourites();
    testProcessing();
#if JUCE_MAC || JUCE_WINDOWS
    testEditor();
    testFavouritesInEditor();
#endif
    std::cout << (failures == 0 ? "PASS " : "FAILED ") << (checks - failures) << "/" << checks << " wrapper checks\n";
    return failures == 0 ? 0 : 1;
}
