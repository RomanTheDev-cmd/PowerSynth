#include "PluginProcessor.h"
#include "PluginEditor.h"

PowerSynthProcessor::PowerSynthProcessor()
    // 3.1: a sidechain input (the kick, for SIDECHAIN KICK); auxiliary for VST3 hosts (see vst3Extensions)
    : AudioProcessor (BusesProperties().withInput ("Sidechain", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "PowerSynth", ps::createLayout())
{
    if (const int i = library.indexOf (getPresetName()); i >= 0)
        ps::applySnapshot (state, library.all()[(size_t) i].values);
}

bool PowerSynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto sc = layouts.getMainInputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
        && (sc.isDisabled() || sc == juce::AudioChannelSet::stereo() || sc == juce::AudioChannelSet::mono());
}

void PowerSynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    previewSamplesLeft = 0;
    preparedRate = sampleRate; // not getSampleRate(): it is only valid if the host set it first
    engine.prepare (sampleRate, std::max (32, std::min (samplesPerBlock, 1024)), ps::readParams (state));
    monoScratch.setSize (1, std::max (512, samplesPerBlock));
    ducker.prepare (sampleRate);
    scScratch.setSize (2, std::max (512, samplesPerBlock));
    for (int i = 0; i < 3; ++i)
        latencyByOversampling[(size_t) i] = engine.getLatencySamplesFor (i);
    reportedLatency = engine.getLatencySamples();
    setLatencySamples (reportedLatency);
}

void PowerSynthProcessor::reset()
{
    ducker.reset();
    // AU Reset can replace the final MIDI note-off. Clear voices AND filter/delay tails.
    engine.reset();
    previewSamplesLeft = 0;
    previewRequested.store (false);
    meterL.store (0.0f);
    meterR.store (0.0f);
}

void PowerSynthProcessor::releaseResources()
{
    reset();
}

void PowerSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    // Preview (button, waveform click): a 1 s C3 note.
    if (previewRequested.exchange (false))
    {
        midi.addEvent (juce::MidiMessage::noteOn (1, 48, 0.9f), 0);
        previewSamplesLeft = (int) (preparedRate * 1.0);
    }
    if (previewSamplesLeft > 0)
    {
        if (previewSamplesLeft <= n)
            midi.addEvent (juce::MidiMessage::noteOff (1, 48), previewSamplesLeft - 1);
        previewSamplesLeft = std::max (0, previewSamplesLeft - n);
    }

    auto params = ps::readParams (state);
    if (auto* ph = getPlayHead()) // LFO sync and delay follow the host tempo
        if (const auto pos = ph->getPosition())
            if (const auto bpm = pos->getBpm())
                params.bpm = *bpm;
    // The sidechain arrives in the same buffer the synth is written to: kept aside first, then the output cleared.
    {
        const int scCh = getBusCount (true) > 0 && getBus (true, 0)->isEnabled()
                             ? juce::jlimit (0, 2, std::min (getChannelCountOfBus (true, 0), buffer.getNumChannels())) : 0;
        const int m = std::min (n, scScratch.getNumSamples());
        scConnected = scCh > 0 && scScratch.getNumChannels() == 2 && m > 0;
        if (scScratch.getNumChannels() == 2 && m > 0)
            for (int c = 0; c < 2; ++c)
            {
                if (scConnected) scScratch.copyFrom (c, 0, buffer, std::min (c, scCh - 1), 0, m);
                else scScratch.clear (c, 0, m);
            }
        for (int c = 0; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    }
    float* l = buffer.getWritePointer (0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    if (r != nullptr)
        engine.process (l, r, n, midi, params);
    else
        for (int done = 0; done < n;) // mono bus: render in parts through the preallocated scratch
        {
            const int len = std::min (n - done, monoScratch.getNumSamples());
            auto* sc = monoScratch.getWritePointer (0);
            engine.process (l + done, sc, len, midi, params, done);
            for (int i = 0; i < len; ++i) l[done + i] = 0.5f * (l[done + i] + sc[i]);
            done += len;
        }
    for (int c = 2; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);

    // SIDECHAIN: TEMPO - by the host's song position of the sound leaving now (the latency earlier); KICK - every hit of
    // the kick on the sidechain input (delayed by the latency inside the ducker).
    if (params.scSource == 1)
    {
        const int m = scScratch.getNumChannels() == 2 ? std::min (n, scScratch.getNumSamples()) : 0;
        ducker.processKick (l, r, m, scConnected ? scScratch.getReadPointer (0) : nullptr, scConnected ? scScratch.getReadPointer (1) : nullptr,
                            std::max (0, engine.getLatencySamples()), params.bpm, params.scAmount, params.scRate, params.scShape);
    }
    else if (params.scAmount > 0.0f || ! ducker.isIdle())
    {
        double ppq = 0.0;
        bool playing = false;
        if (auto* ph = getPlayHead())
            if (const auto pos = ph->getPosition())
            {
                playing = pos->getIsPlaying();
                if (const auto q = pos->getPpqPosition()) ppq = *q; else playing = false;
            }
        ppq -= std::max (0, engine.getLatencySamples()) / preparedRate * params.bpm / 60.0;
        ducker.process (l, r, n, ppq, params.bpm, playing, params.scAmount, params.scRate, params.scShape);
    }

    if (const int lat = engine.getLatencySamples(); lat != reportedLatency)
    {
        reportedLatency = lat;
        setLatencySamples (lat);
    }

    int w = scopeWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        scope[(size_t) w] = r != nullptr ? 0.5f * (l[i] + r[i]) : l[i];
        w = (w + 1) & (scopeSize - 1);
    }
    scopeWrite.store (w, std::memory_order_release);

    auto peak = [n] (const float* x)
    {
        const auto range = juce::FloatVectorOperations::findMinAndMax (x, n);
        return std::max (std::abs (range.getStart()), std::abs (range.getEnd()));
    };
    meterL.store (std::max (peak (l), meterL.load() * 0.85f));
    meterR.store (std::max (r != nullptr ? peak (r) : peak (l), meterR.load() * 0.85f));
}

void PowerSynthProcessor::loadPreset (int index)
{
    const auto& all = library.all();
    if (index < 0 || index >= (int) all.size())
        return;
    ps::applySnapshot (state, all[(size_t) index].values);
    setPresetName (all[(size_t) index].name);
}

juce::AudioProcessorEditor* PowerSynthProcessor::createEditor() { return new PowerSynthEditor (*this); }

void PowerSynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto tree = state.copyState();
    tree.setProperty ("presetName", getPresetName(), nullptr);
    tree.setProperty ("fullView", fullView.load(), nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, destData);
}

void PowerSynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            setPresetName (tree.getProperty ("presetName", getPresetName()).toString());
            fullView = (bool) tree.getProperty ("fullView", false);
            // Parameters added after this project was saved come back neutral (old sound).
            juce::StringArray missing;
            for (const auto& [pid, value] : ps::legacyNeutral())
                if (xml->getChildByAttribute ("id", pid) == nullptr)
                    missing.add (pid);
            state.replaceState (tree);
            for (const auto& pid : missing)
                if (auto* param = state.getParameter (pid))
                    param->setValueNotifyingHost (param->convertTo0to1 (ps::legacyNeutral().at (pid)));
            // A host may restore the project after prepareToPlay: report the restored oversampling's
            // latency now, not only after the first block (processBlock keeps it in sync afterwards).
            const auto restored = (size_t) juce::jlimit (0, 2, ps::readParams (state).oversampleIndex);
            if (latencyByOversampling[restored] >= 0)
                setLatencySamples (latencyByOversampling[restored]);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PowerSynthProcessor(); }
