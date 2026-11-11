// PowerSynth engine and preset tests (headless). Run from the project folder.
//   powersynth_tests               all checks
//   powersynth_tests --dump-presets   name|category|id=value ... per factory preset
//   powersynth_tests --calibrate      the MASTER that puts each factory preset's true peak at -1 dBTP (standard phrase)
//   powersynth_tests --serve          render server for Tools/fit_synth.py (see servePhrase)
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <iostream>
#include <sstream>
#include "DSP/SynthEngine.h"
#include "Presets.h"

using namespace ps;

namespace
{
int checks = 0, failures = 0;
void expect (bool ok, const juce::String& what)
{
    ++checks;
    if (! ok) { ++failures; std::cout << "FAIL: " << what << "\n"; }
}
double db (double g) { return 20.0 * std::log10 (std::max (g, 1.0e-12)); }

struct Note { double on, off; int note; float vel = 0.9f; };

juce::AudioBuffer<float> render (SynthParams p, const std::vector<Note>& notes, double seconds, double sr = 48000.0, int block = 512,
                                 const std::vector<std::pair<double, int>>& bends = {})
{
    dsp::SynthEngine e;
    e.prepare (sr, block, p);
    const int total = (int) (seconds * sr);
    juce::AudioBuffer<float> out (2, total);
    out.clear();
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        juce::MidiBuffer m;
        for (const auto& nt : notes)
        {
            const int on = (int) (nt.on * sr), off = (int) (nt.off * sr);
            if (on >= pos && on < pos + n) m.addEvent (juce::MidiMessage::noteOn (1, nt.note, nt.vel), on - pos);
            if (off >= pos && off < pos + n) m.addEvent (juce::MidiMessage::noteOff (1, nt.note), off - pos);
        }
        for (const auto& [t, v] : bends)
            if ((int) (t * sr) >= pos && (int) (t * sr) < pos + n) m.addEvent (juce::MidiMessage::pitchWheel (1, v), (int) (t * sr) - pos);
        e.process (out.getWritePointer (0, pos), out.getWritePointer (1, pos), n, m, p);
    }
    return out;
}

double peak (const juce::AudioBuffer<float>& a, int from = 0, int to = -1)
{
    if (to < 0) to = a.getNumSamples();
    return std::max (a.getMagnitude (0, from, to - from), a.getMagnitude (1, from, to - from));
}
// True peak: the largest of the samples and of the 3 points between each pair of samples (4x windowed-sinc,
// Blackman, 64 taps), evaluated around the loud samples - the measure of PowerKick. The factory level is set on it.
double truePeak (const float* x, int n)
{
    static const auto taps = []
    {
        std::array<std::array<double, 64>, 3> t {};
        for (int f = 1; f <= 3; ++f)
            for (int k = 0; k < 64; ++k)
            {
                const double d = (k - 31) - f / 4.0, a = juce::MathConstants<double>::pi * d;
                const double w = 0.42 + 0.5 * std::cos (a / 32.0) + 0.08 * std::cos (2.0 * a / 32.0);
                t[(size_t) f - 1][(size_t) k] = (d == 0.0 ? 1.0 : std::sin (a) / a) * w;
            }
        return t;
    }();
    double mx = 0.0;
    for (int i = 0; i < n; ++i) mx = std::max (mx, (double) std::abs (x[i]));
    double best = mx;
    for (int i = 31; i < n - 33; ++i)
        if (std::abs (x[i]) >= 0.5 * mx || std::abs (x[i + 1]) >= 0.5 * mx)
            for (const auto& h : taps)
            {
                double s = 0.0;
                for (int k = 0; k < 64; ++k) s += x[i - 31 + k] * h[(size_t) k];
                best = std::max (best, std::abs (s));
            }
    return best;
}
double truePeak (const juce::AudioBuffer<float>& a)
{
    return std::max (truePeak (a.getReadPointer (0), a.getNumSamples()), truePeak (a.getReadPointer (1), a.getNumSamples()));
}
double rms (const juce::AudioBuffer<float>& a, int from = 0, int to = -1)
{
    if (to < 0) to = a.getNumSamples();
    return std::sqrt (0.5 * (std::pow (a.getRMSLevel (0, from, to - from), 2.0) + std::pow (a.getRMSLevel (1, from, to - from), 2.0)));
}
bool finite (const juce::AudioBuffer<float>& a)
{
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < a.getNumSamples(); ++i)
            if (! std::isfinite (a.getSample (c, i))) return false;
    return true;
}
double maxStep (const juce::AudioBuffer<float>& a)
{
    double m = 0.0;
    for (int c = 0; c < 2; ++c)
        for (int i = 1; i < a.getNumSamples(); ++i)
            m = std::max (m, (double) std::abs (a.getSample (c, i) - a.getSample (c, i - 1)));
    return m;
}

// Largest second difference: tiny for low sines, about the amplitude of a voice for a hard cut (a click).
double maxCurve (const juce::AudioBuffer<float>& a)
{
    double m = 0.0;
    for (int c = 0; c < 2; ++c)
        for (int i = 2; i < a.getNumSamples(); ++i)
            m = std::max (m, (double) std::abs (a.getSample (c, i) - 2.0f * a.getSample (c, i - 1) + a.getSample (c, i - 2)));
    return m;
}

// The phrases presets are judged and levelled on: a chord for pads, atmospheres and stabs, a line for the rest.
bool chordCategory (const juce::String& c) { return c == "Pads" || c == "Atmospheres" || c == "Stabs"; }
std::vector<Note> phraseFor (const juce::String& category);
// The phrase of a preset: by its category; "Classic" (the 1.x library) by the sound itself (a slow attack plays a chord,
// a short one-shot plays stabs, otherwise a line).
std::vector<Note> phraseOf (const Preset& pr)
{
    if (pr.category != "Classic") return phraseFor (pr.category);
    const auto p = paramsFromSnapshot (pr.values);
    if (p.attackMs >= 200.0f) return phraseFor ("Pads");
    if (p.sustain < 0.05f && p.decayMs < 600.0f && p.unison >= 3) return phraseFor ("Stabs");
    return phraseFor ("Leads");
}
std::vector<Note> phraseFor (const juce::String& category)
{
    if (category == "Stabs")
    {
        std::vector<Note> n;
        for (int k = 0; k < 6; ++k)
            for (int note : { 48, 51, 55 })
                n.push_back ({ 0.2 + k * 0.32, 0.2 + k * 0.32 + 0.12, note });
        return n;
    }
    if (chordCategory (category))
        return { { 0.0, 2.5, 48 }, { 0.0, 2.5, 51 }, { 0.0, 2.5, 55 }, { 0.0, 2.5, 60 } };
    std::vector<Note> n;
    const int line[] = { 48, 48, 60, 51, 48, 55, 53, 51, 48, 48, 60, 51, 58, 55, 53, 51 };
    const double step = 60.0 / 140.0 / 4.0;
    for (int k = 0; k < 16; ++k) n.push_back ({ k * step, k * step + step * 0.8, line[k] });
    return n;
}

// Phrases of the render server, from the root note: 0 lead line (16ths, gate 80 %), 1 held chord, 2 one stab chord
// (150 ms), 3 arpeggio (16ths, gate 50 %), 4 one held note, 5 one short note (150 ms). Notes stop 1 s before the end.
std::vector<Note> servePhrase (int pattern, int root, double bpm, double len)
{
    std::vector<Note> n;
    const double step = 60.0 / bpm / 4.0, end = std::max (0.3, len - 1.0);
    auto line = [&] (std::initializer_list<int> iv, double gate)
    {
        const std::vector<int> v (iv);
        for (int k = 0; k * step < end; ++k)
            n.push_back ({ k * step, k * step + step * gate, root + v[(size_t) k % v.size()] });
    };
    switch (pattern)
    {
        case 0: line ({ 0, 0, 12, 3, 0, 7, 5, 3, 0, 0, 12, 3, 10, 7, 5, 3 }, 0.8); break;
        case 1: for (int iv : { 0, 3, 7, 12 }) n.push_back ({ 0.0, end, root + iv }); break;
        case 2: for (int iv : { 0, 7, 12 }) n.push_back ({ 0.0, 0.15, root + iv }); break;
        case 3: line ({ 0, 3, 7, 12, 15, 12, 7, 3 }, 0.5); break;
        case 4: n.push_back ({ 0.0, end, root }); break;
        default: n.push_back ({ 0.0, 0.15, root }); break;
    }
    return n;
}

//==============================================================================
void testFactoryPresets()
{
    const auto& f = factoryPresets();
    expect (f.size() >= 20, "at least 20 factory presets (" + juce::String ((int) f.size()) + ")");
    std::set<juce::String> names;
    for (const auto& pr : f)
    {
        expect (names.insert (pr.name).second, "unique preset name: " + pr.name);
        const auto p = paramsFromSnapshot (pr.values);
        const auto a = render (p, phraseOf (pr), 3.5);
        const double pk = peak (a);
        expect (finite (a), pr.name + ": finite");
        expect (pk <= 1.0, pr.name + ": peak <= 0 dBFS (" + juce::String (db (pk), 2) + ")");
        expect (pk > 0.05, pr.name + ": audible (" + juce::String (db (pk), 1) + " dB)");
        const double tp = db (truePeak (a));
        expect (tp > -1.6 && tp <= -0.95, pr.name + ": library level, true peak -1 dBTP on the standard phrase (" + juce::String (tp, 2) + " dBTP)");
        double mean = 0.0;
        for (int i = 0; i < a.getNumSamples(); ++i) mean += a.getSample (0, i);
        expect (std::abs (mean / a.getNumSamples()) < 5.0e-3, pr.name + ": no DC offset (" + juce::String (mean / a.getNumSamples(), 5) + ")");
        // Silence after the notes and the effects tails (long pad releases + reverb/delay: within 14 s), no stuck notes.
        const auto t = render (p, { { 0.0, 0.5, 48 } }, 14.5);
        expect (peak (t, (int) (14.0 * 48000)) < 1.0e-3, pr.name + ": decays to silence (" + juce::String (db (peak (t, (int) (14.0 * 48000))), 1) + " dB at 14 s)");
    }
}

void testPolyphonyAndStealing()
{
    SynthParams p;
    p.attackMs = 0.5f; p.releaseMs = 2000.0f; p.reverbMix = 0.0f; p.chorus = 0.0f;
    // Sines through an open filter: a hard cut of a stolen voice would be a step far above the sines' own slope.
    p.oscAShape = 0.0f; p.unison = 1; p.oscBLevel = 0.0f; p.drive = 0.0f; p.cutoffHz = 18000.0f; p.fltEnv = 0.0f;
    dsp::SynthEngine e;
    e.prepare (48000.0, 512, p);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer m;
    for (int k = 0; k < 8; ++k) m.addEvent (juce::MidiMessage::noteOn (1, 48 + k, 0.8f), k * 10);
    buf.clear();
    e.process (buf.getWritePointer (0), buf.getWritePointer (1), 512, m, p);
    expect (e.activeVoiceCount() == 8, "8 notes, 8 voices (" + juce::String (e.activeVoiceCount()) + ")");
    std::vector<Note> notes;
    for (int k = 0; k < 12; ++k) notes.push_back ({ 0.05 * k, 3.0, 48 + k });
    const auto a = render (p, notes, 1.5);
    const auto ref = render (p, { notes.begin(), notes.begin() + 8 }, 1.5); // the same voices without stealing
    expect (finite (a) && maxCurve (a) < 1.5 * maxCurve (ref), "stealing voices for notes 9-12 does not click (largest curve "
            + juce::String (maxCurve (a), 4) + ", without stealing " + juce::String (maxCurve (ref), 4) + ")");
}

void testMonoAndLegato()
{
    SynthParams p;
    p.unison = 1; p.oscBLevel = 0.0f; p.oscAShape = 0.0f; p.cutoffHz = 18000.0f; p.fltEnv = 0.0f; p.chorus = 0.0f; p.reverbMix = 0.0f;
    p.attackMs = 0.5f; p.sustain = 1.0f; p.width = 0.0f; p.drive = 0.0f;
    p.voiceMode = VoiceMode::legato; p.glideMs = 80.0f;
    const auto a = render (p, { { 0.0, 0.6, 57 }, { 0.4, 1.2, 69 } }, 1.3);
    // A3 -> A4: the second note slides (no new attack): the level does not dip at 0.4 s.
    const double before = rms (a, (int) (0.30 * 48000), (int) (0.39 * 48000)), at = rms (a, (int) (0.40 * 48000), (int) (0.42 * 48000));
    expect (at > 0.7 * before, "LEGATO: an overlapping note continues the voice (" + juce::String (db (at / before), 1) + " dB)");
    p.voiceMode = VoiceMode::mono;
    const auto b = render (p, { { 0.0, 0.6, 57 }, { 0.4, 1.2, 69 } }, 1.3);
    expect (finite (b) && maxStep (b) < 0.35, "MONO: a new note restarts the envelope without a click");
}

void testBlockSizesAndRates()
{
    const auto& f = factoryPresets();
    for (int i : { 0, 4, 8, 11 })
    {
        const auto p = paramsFromSnapshot (f[(size_t) i].values);
        const auto ph = phraseOf (f[(size_t) i]);
        const auto a = render (p, ph, 2.0, 48000.0, 512), b = render (p, ph, 2.0, 48000.0, 64);
        double d = 0.0, r = 0.0;
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < a.getNumSamples(); ++k) { d += std::pow ((double) a.getSample (c, k) - b.getSample (c, k), 2.0); r += std::pow ((double) a.getSample (c, k), 2.0); }
        expect (10.0 * std::log10 ((d + 1.0e-30) / (r + 1.0e-30)) < -90.0, f[(size_t) i].name + ": the same sound with 64- and 512-sample blocks ("
                + juce::String (10.0 * std::log10 ((d + 1.0e-30) / (r + 1.0e-30)), 1) + " dB)");
        const double l48 = db (rms (a));
        for (double sr : { 44100.0, 96000.0 })
        {
            const auto c = render (p, ph, 2.0, sr, 512);
            expect (finite (c) && std::abs (db (rms (c)) - l48) < 1.5, f[(size_t) i].name + ": level at " + juce::String ((int) sr)
                    + " Hz within 1.5 dB of 48 kHz (" + juce::String (db (rms (c)) - l48, 2) + ")");
        }
    }
}

double pitchHz (const juce::AudioBuffer<float>& a, double t0, double t1)
{
    std::vector<double> z;
    for (int i = (int) (t0 * 48000) + 1; i < (int) (t1 * 48000); ++i)
    {
        const double x0 = a.getSample (0, i - 1) + a.getSample (1, i - 1), x1 = a.getSample (0, i) + a.getSample (1, i);
        if (x0 < 0.0 && x1 >= 0.0) z.push_back (i - 1 + x0 / (x0 - x1));
    }
    return z.size() < 3 ? 0.0 : 48000.0 * (double) (z.size() - 1) / (z.back() - z.front());
}

void testTuningBendAndLfo()
{
    SynthParams p;
    p.unison = 1; p.oscBLevel = 0.0f; p.oscAShape = 0.0f; p.cutoffHz = 18000.0f; p.fltEnv = 0.0f; p.chorus = 0.0f; p.reverbMix = 0.0f;
    p.sustain = 1.0f; p.width = 0.0f; p.drive = 0.0f; p.resonance = 0.0f;
    const auto a = render (p, { { 0.0, 1.0, 69 } }, 1.0);
    const double c = 1200.0 * std::log2 (pitchHz (a, 0.2, 0.9) / 440.0);
    expect (std::abs (c) < 1.0, "A4 plays 440 Hz (" + juce::String (c, 2) + " cents)");
    const auto b = render (p, { { 0.0, 1.0, 69 } }, 1.0, 48000.0, 512, { { 0.0, 16383 } });
    const double cb = 1200.0 * std::log2 (pitchHz (b, 0.2, 0.9) / 440.0);
    expect (std::abs (cb - 200.0) < 3.0, "pitch bend up: +2 semitones (" + juce::String (cb, 1) + " cents)");
    // LFO on the amplitude at 1/4 and 120 BPM: a 0.5 s period.
    p.lfoTarget = LfoTarget::amp; p.lfoAmount = 1.0f; p.lfoSync = true; p.lfoDiv = 2; p.bpm = 120.0; p.lfoShape = LfoShape::sine;
    const auto l = render (p, { { 0.0, 3.0, 57 } }, 3.0);
    std::vector<double> env;
    for (int k = 0; k < 120; ++k) env.push_back (rms (l, k * 1200, (k + 1) * 1200));
    int best = 0; double bestC = -1.0e9;
    for (int lag = 10; lag < 40; ++lag)
    {
        double cc = 0.0;
        for (int k = 0; k + lag < (int) env.size(); ++k) cc += env[(size_t) k] * env[(size_t) (k + lag)];
        if (cc > bestC) { bestC = cc; best = lag; }
    }
    expect (best >= 19 && best <= 21, "LFO 1/4 at 120 BPM: period 0.5 s (" + juce::String (best * 0.025, 3) + " s)");
}

void testAnalogue()
{
    auto p = paramsFromSnapshot (factoryPresets()[0].values);
    auto a = p; a.analog = true;
    const auto ph = phraseFor ("Leads");
    const auto d = render (p, ph, 2.0), x = render (a, ph, 2.0), x2 = render (a, ph, 2.0);
    bool same = true;
    for (int c = 0; c < 2; ++c) for (int i = 0; i < x.getNumSamples(); ++i) same = same && x.getSample (c, i) == x2.getSample (c, i);
    expect (same, "ANALOGUE is repeatable (a bounce is the same every time)");
    const double lvl = db (rms (x)) - db (rms (d));
    expect (std::abs (lvl) < 2.0, "ANALOGUE keeps the level within 2 dB (" + juce::String (lvl, 2) + ")");
    double diff = 0.0, ref = 0.0;
    for (int c = 0; c < 2; ++c) for (int i = 0; i < d.getNumSamples(); ++i) { diff += std::pow ((double) x.getSample (c, i) - d.getSample (c, i), 2.0); ref += std::pow ((double) d.getSample (c, i), 2.0); }
    expect (10.0 * std::log10 (diff / ref) > -25.0, "ANALOGUE changes the sound (" + juce::String (10.0 * std::log10 (diff / ref), 1) + " dB)");
}

//==============================================================================
// 2.0 (2026-10-29): Roman - "the engine is weak, Moog-like sounds instead of modern schranz and techno". The new
// blocks must audibly change the sound, stay band-limited, bounded and finite.
double tonePower (const juce::AudioBuffer<float>& a, double hz, double t0, double t1)
{
    std::complex<double> acc {};
    const int i0 = (int) (t0 * 48000), i1 = (int) (t1 * 48000);
    for (int i = i0; i < i1; ++i) acc += (double) (a.getSample (0, i) + a.getSample (1, i)) * std::polar (1.0, -2.0 * juce::MathConstants<double>::pi * hz * i / 48000.0);
    const double m = std::abs (acc) / (i1 - i0);
    return m * m;
}
// Spectral centroid (Hz) of the mid signal, 0.2..0.6 s, from a 8192-point FFT.
double centroid (const juce::AudioBuffer<float>& a, double t0 = 0.2)
{
    juce::dsp::FFT fft (13);
    std::vector<float> buf (16384, 0.0f);
    const int i0 = (int) (t0 * 48000);
    for (int i = 0; i < 8192 && i0 + i < a.getNumSamples(); ++i)
        buf[(size_t) i] = 0.5f * (a.getSample (0, i0 + i) + a.getSample (1, i0 + i)) * (0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / 8192.0f));
    fft.performFrequencyOnlyForwardTransform (buf.data());
    double num = 0.0, den = 0.0;
    for (int k = 1; k < 4096; ++k) { const double pw = (double) buf[(size_t) k] * buf[(size_t) k]; num += pw * k * 48000.0 / 8192.0; den += pw; }
    return den > 0.0 ? num / den : 0.0;
}
// Energy that is not at the harmonics of f0 (aliasing, inharmonic shift), relative to all, 0.2..1.0 s.
double offHarmonic (const juce::AudioBuffer<float>& a, double f0)
{
    juce::dsp::FFT fft (15);
    std::vector<float> buf (65536, 0.0f);
    for (int i = 0; i < 32768; ++i)
        buf[(size_t) i] = 0.5f * (a.getSample (0, 9600 + i) + a.getSample (1, 9600 + i)) * (0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / 32768.0f));
    fft.performFrequencyOnlyForwardTransform (buf.data());
    double off = 0.0, all = 0.0;
    for (int k = 2; k < 16384; ++k)
    {
        const double hz = k * 48000.0 / 32768.0, pw = (double) buf[(size_t) k] * buf[(size_t) k];
        all += pw;
        const double h = hz / f0;
        if (std::abs (h - std::round (h)) * f0 > 12.0) off += pw;
    }
    return all > 0.0 ? off / all : 0.0;
}
SynthParams plain()
{
    SynthParams p;
    p.unison = 1; p.oscBLevel = 0.0f; p.chorus = 0.0f; p.reverbMix = 0.0f; p.drive = 0.0f; p.cutoffHz = 18000.0f; p.fltEnv = 0.0f;
    p.resonance = 0.0f; p.sustain = 1.0f; p.width = 0.0f;
    return p;
}

void testCharacter()
{
    const std::vector<Note> held { { 0.0, 1.2, 57 } };   // A3, 220 Hz
    // Hidden wavetables: each of the 9 new shapes is its own timbre, finite and band-limited even high up.
    std::vector<double> cents;
    for (int sh = 4; sh <= 12; ++sh)
    {
        auto p = plain(); p.oscAShape = (float) sh;
        const auto a = render (p, held, 1.3);
        const auto hi = render (p, { { 0.0, 1.2, 93 } }, 1.3);   // A6, 1760 Hz
        expect (finite (a) && peak (a) > 0.05 && peak (a) <= 1.0, "wavetable " + shapeNames()[sh] + " plays");
        expect (offHarmonic (hi, 1760.0) < 0.003, "wavetable " + shapeNames()[sh] + " band-limited at 1760 Hz (aliasing "
                + juce::String (100.0 * offHarmonic (hi, 1760.0), 3) + " %)");
        cents.push_back (centroid (a));
    }
    int close = 0;
    for (size_t i = 0; i < cents.size(); ++i)
        for (size_t j = i + 1; j < cents.size(); ++j)
            close += std::abs (std::log2 (cents[i] / cents[j])) < 0.04 ? 1 : 0;
    expect (close <= 2, "the wavetables differ in colour (" + juce::String (close) + " pairs alike)");
    // WARP: every mode moves the spectrum, the PWM hoover too.
    auto base = plain(); base.oscAShape = 2.0f;
    const double c0 = centroid (render (base, held, 1.3));
    for (int m = 0; m < 4; ++m)
    {
        auto p = base; p.warpMode = (WarpMode) m; p.warp = 0.7f;
        const auto a = render (p, held, 1.3);
        const double c = centroid (a);
        expect (finite (a) && peak (a) <= 1.0 && std::abs (std::log2 (c / c0)) > 0.15, "WARP " + juce::String (m) + " changes the timbre (centroid "
                + juce::String (c0, 0) + " -> " + juce::String (c, 0) + " Hz)");
    }
    // RING: sidebands at fA +- fB (B a fifth up: 330 Hz -> 110 and 550 Hz).
    {
        auto p = plain(); p.oscAShape = 0.0f; p.oscBShape = 0.0f; p.oscBSemi = 7; p.oscBFine = 0.0f;
        auto r = p; r.ring = 1.0f;
        const auto a = render (p, held, 1.3), b = render (r, held, 1.3);
        const double f2 = 220.0 * std::exp2 (7.0 / 12.0);
        expect (tonePower (b, f2 + 220.0, 0.2, 1.0) > 100.0 * tonePower (a, f2 + 220.0, 0.2, 1.0) && tonePower (b, f2 - 220.0, 0.2, 1.0) > 1.0e-6,
                "RING makes sum and difference tones");
    }
    // Pitch envelope: +12 st at the start, back at the note after P.TIME.
    {
        auto p = plain(); p.oscAShape = 0.0f; p.pitchEnv = 12.0f; p.pitchTimeMs = 200.0f;
        const auto a = render (p, held, 1.3);
        const double early = pitchHz (a, 0.002, 0.012), late = pitchHz (a, 0.6, 0.9);
        expect (early > 370.0 && std::abs (late - 220.0) < 2.0, "PITCH env dives from " + juce::String (early, 0) + " Hz to " + juce::String (late, 1) + " Hz");
    }
    // GRIT: the filter growls (more harmonics) and stays bounded with full resonance.
    {
        auto p = plain(); p.oscAShape = 0.0f; p.cutoffHz = 900.0f; p.resonance = 0.9f;
        auto g = p; g.grit = 1.0f;
        const auto a = render (p, held, 1.3), b = render (g, held, 1.3);
        const double h3a = tonePower (a, 660.0, 0.2, 1.0) / tonePower (a, 220.0, 0.2, 1.0), h3b = tonePower (b, 660.0, 0.2, 1.0) / tonePower (b, 220.0, 0.2, 1.0);
        expect (finite (b) && peak (b) <= 1.0 && h3b > 10.0 * h3a, "GRIT adds harmonics in the filter (3rd " + juce::String (db (std::sqrt (h3a)), 1) + " -> "
                + juce::String (db (std::sqrt (h3b)), 1) + " dB)");
    }
    // COMB: noise through the comb rings at the note (peaks at 220 Hz multiples).
    {
        auto p = plain(); p.oscALevel = 0.0f; p.noise = 1.0f; p.filterMode = FilterMode::comb; p.resonance = 0.9f; p.cutoffHz = 8000.0f;
        const auto a = render (p, held, 1.3);
        const double on = tonePower (a, 440.0, 0.2, 1.0), offPk = tonePower (a, 550.0, 0.2, 1.0);
        expect (finite (a) && peak (a) <= 1.0 && on > 8.0 * offPk, "COMB rings at the note (" + juce::String (db (std::sqrt (on / offPk)), 1) + " dB on/off)");
    }
    // SMASH 3.0 (2026-11-12; Roman: "limp and weak, no power"). The 2.x SMASH levelled every band to a fixed -26 dBFS
    // after the drive: a held driven note lost 5-6 dB of loudness at the same peak (crest 5.7 -> 11.3 dB, median of the
    // lead presets), the top band and every tail were lifted by up to 30 dB. Now it works before the drive and relative
    // to the note's own level.
    {
        const std::vector<Note> chord { { 0.0, 1.4, 48 }, { 0.0, 1.4, 55 }, { 0.0, 1.4, 60 } };
        auto p = plain(); p.oscAShape = 2.0f; p.unison = 5; p.detune = 0.3f; p.drive = 0.6f; p.cutoffHz = 3000.0f;
        auto sm = p; sm.smash = 0.8f;
        const auto a = render (p, chord, 1.5), b = render (sm, chord, 1.5);
        const int i0 = 9600, i1 = 62400;
        const double ca = db (peak (a, i0, i1) / rms (a, i0, i1)), cb = db (peak (b, i0, i1) / rms (b, i0, i1));
        expect (finite (b) && cb <= ca + 1.5, "SMASH keeps a driven chord dense (crest " + juce::String (ca, 1) + " -> " + juce::String (cb, 1) + " dB)");
    }
    {
        // a stab with a long release: 300..500 ms after the note the tail is not lifted
        auto p = plain(); p.oscAShape = 2.0f; p.releaseMs = 600.0f; p.drive = 0.5f; p.cutoffHz = 2000.0f;
        auto sm = p; sm.smash = 1.0f;
        const std::vector<Note> stab { { 0.0, 0.15, 57 } };
        const auto a = render (p, stab, 1.0), b = render (sm, stab, 1.0);
        const double ta = db (rms (a, 21600, 31200) / peak (a)), tb = db (rms (b, 21600, 31200) / peak (b));
        expect (finite (b) && tb <= ta + 3.0, "SMASH does not lift the tail (" + juce::String (ta, 1) + " -> " + juce::String (tb, 1) + " dB under the peak)");
    }
    {
        // a dull sound gets its top back: the share above 2.5 kHz rises
        auto p = plain(); p.oscAShape = 2.0f; p.cutoffHz = 700.0f; p.filterMode = FilterMode::lp12;
        auto sm = p; sm.smash = 1.0f;
        const auto a = render (p, held, 1.3), b = render (sm, held, 1.3);
        auto top = [] (const juce::AudioBuffer<float>& x)
        {
            double hi = 0.0, all = 0.0;
            for (int k = 1; k <= 60; ++k) { const double pw = tonePower (x, 220.0 * k, 0.2, 1.0); all += pw; if (220.0 * k > 2500.0) hi += pw; }
            return db (std::sqrt (hi / all));
        };
        expect (finite (b) && top (b) >= top (a) + 3.0, "SMASH brings up the weak top of a dull sound (" + juce::String (top (a), 1) + " -> " + juce::String (top (b), 1) + " dB of the whole)");
    }
    // PUNCH 3.0: the start of each note stands out after the drive (the drive flattens the attack of the envelope).
    {
        auto p = plain(); p.oscAShape = 2.0f; p.drive = 0.6f; p.unison = 3; p.detune = 0.2f;
        auto pu = p; pu.punch = 1.0f;
        const auto a = render (p, held, 1.3), b = render (pu, held, 1.3);
        auto ratio = [] (const juce::AudioBuffer<float>& x) { return db (rms (x, 48, 720) / rms (x, 14400, 28800)); };
        expect (finite (b) && ratio (b) >= ratio (a) + 4.0, "PUNCH: the first 15 ms louder against the held part (" + juce::String (ratio (a), 1) + " -> " + juce::String (ratio (b), 1) + " dB)");
        expect (std::abs (db (rms (b, 14400, 28800) / rms (a, 14400, 28800))) < 0.5, "PUNCH leaves the held part alone ("
                + juce::String (db (rms (b, 14400, 28800) / rms (a, 14400, 28800)), 2) + " dB)");
        // a second note over a held one: the gain bump does not click (sines, largest curve)
        auto s0 = plain(); s0.oscAShape = 0.0f; auto s1 = s0; s1.punch = 1.0f;
        const std::vector<Note> two { { 0.0, 1.2, 45 }, { 0.5, 1.2, 52 } };
        expect (maxCurve (render (s1, two, 1.3)) < 2.5 * maxCurve (render (s0, two, 1.3)), "PUNCH on a second note does not click (largest curve "
                + juce::String (maxCurve (render (s1, two, 1.3)), 5) + ", without " + juce::String (maxCurve (render (s0, two, 1.3)), 5) + ")");
    }
    // SHIFT: a small frequency shift makes the partials inharmonic.
    {
        auto p = plain(); p.oscAShape = 2.0f;
        auto s2 = p; s2.shift = 0.6f; s2.drive = 0.3f;
        expect (offHarmonic (render (s2, held, 1.3), 220.0) > 0.2, "SHIFT moves the partials off the harmonics ("
                + juce::String (100.0 * offHarmonic (render (s2, held, 1.3), 220.0), 1) + " %)");
    }
}

// MORPH (the filter envelope moves the wavetable position and WARP) and LFO -> FM: modern movement inside a note.
double centroidAt (const juce::AudioBuffer<float>& a, double t0)
{
    juce::dsp::FFT fft (10);
    std::vector<float> buf (2048, 0.0f);
    const int i0 = (int) (t0 * 48000);
    for (int i = 0; i < 1024; ++i)
        buf[(size_t) i] = 0.5f * (a.getSample (0, i0 + i) + a.getSample (1, i0 + i)) * (0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / 1024.0f));
    fft.performFrequencyOnlyForwardTransform (buf.data());
    double num = 0.0, den = 0.0;
    for (int k = 1; k < 512; ++k) { const double pw = (double) buf[(size_t) k] * buf[(size_t) k]; num += pw * k * 48000.0 / 1024.0; den += pw; }
    return den > 0.0 ? num / den : 0.0;
}
void testMovement()
{
    const std::vector<Note> held { { 0.0, 1.2, 57 } };
    auto p = plain(); p.oscAShape = 5.0f; p.fltAttackMs = 1.0f; p.fltDecayMs = 200.0f; p.fltSustain = 0.0f;
    auto m = p; m.morphEnv = 1.0f;
    const auto a = render (p, held, 1.3), b = render (m, held, 1.3);
    const double moveA = std::abs (std::log2 (centroidAt (a, 0.02) / centroidAt (a, 0.8))), moveB = std::abs (std::log2 (centroidAt (b, 0.02) / centroidAt (b, 0.8)));
    expect (finite (b) && moveB > moveA + 0.3, "MORPH moves the timbre within the note (" + juce::String (moveA, 2) + " -> " + juce::String (moveB, 2) + " oct)");
    auto f = plain(); f.oscAShape = 0.0f; f.fmAmount = 0.5f; f.oscBShape = 0.0f; f.lfoRateHz = 4.0f;
    auto g = f; g.lfoTarget = LfoTarget::fm; g.lfoAmount = 1.0f;
    auto spread = [] (const juce::AudioBuffer<float>& x) { double lo = 1e9, hi = 0; for (double t = 0.2; t < 1.0; t += 0.025) { const double c = centroidAt (x, t); lo = std::min (lo, c); hi = std::max (hi, c); } return std::log2 (hi / lo); };
    const double sf = spread (render (f, held, 1.3)), sg = spread (render (g, held, 1.3));
    expect (sg > sf + 0.3, "LFO -> FM moves the FM timbre (" + juce::String (sf, 2) + " -> " + juce::String (sg, 2) + " oct)");
}

void testCpu()
{
    SynthParams p;
    p.unison = 7; p.drive = 0.6f; p.chorus = 0.5f; p.delayMix = 0.3f; p.reverbMix = 0.4f; p.oversampleIndex = 2; p.analog = true;
    p.oscAShape = 8.5f; p.oscBShape = 11.0f; p.warp = 0.7f; p.ring = 0.5f; p.grit = 0.7f; p.smash = 0.8f; p.shift = 0.4f; p.fmAmount = 0.4f; p.pitchEnv = 12.0f;
    std::vector<Note> chord;
    for (int k = 0; k < 8; ++k) chord.push_back ({ 0.0, 4.0, 48 + 2 * k });
    const auto t0 = std::chrono::steady_clock::now();
    const auto a = render (p, chord, 4.0, 48000.0, 64);
    const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::cout << "  CPU: 8 voices x 7 unison, wavetables, SYNC, RING, GRIT, FM, 8x drive, SHIFT, SMASH, all effects, ANALOGUE: " << juce::String (100.0 * sec / 4.0, 1) << " % of real time\n";
    expect (finite (a) && sec / 4.0 < 0.5, "the heaviest setting runs at least twice faster than real time");
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String mode = argc > 1 ? juce::String (argv[1]) : juce::String();
    if (mode == "--dump-presets")
    {
        for (const auto& pr : factoryPresets())
        {
            std::cout << pr.name << "|" << pr.category << "|";
            for (const auto& [k, v] : pr.values) std::cout << k << "=" << v << " ";
            std::cout << "\n";
        }
        return 0;
    }
    if (mode == "--serve")
    {
        // Each stdin line: "paramId=value ... _root=48 _bpm=150 _len=4 _pattern=0"; writes float32 stereo interleaved.
        std::string line;
        std::vector<float> out;
        while (std::getline (std::cin, line))
        {
            Snapshot s;
            double len = 4.0, bpm = 150.0;
            int root = 48, pattern = 0;
            std::istringstream ss (line);
            std::string tok;
            while (ss >> tok)
            {
                const auto eq = tok.find ('=');
                if (eq == std::string::npos) continue;
                const auto key = juce::String (tok.substr (0, eq));
                const float value = std::stof (tok.substr (eq + 1));
                if (key == "_len") len = value;
                else if (key == "_bpm") bpm = value;
                else if (key == "_root") root = (int) value;
                else if (key == "_pattern") pattern = (int) value;
                else s[key] = value;
            }
            const auto audio = render (paramsFromSnapshot (s), servePhrase (pattern, root, bpm, len), len);
            out.resize ((size_t) audio.getNumSamples() * 2);
            for (int i = 0; i < audio.getNumSamples(); ++i)
            {
                out[(size_t) i * 2] = audio.getSample (0, i);
                out[(size_t) i * 2 + 1] = audio.getSample (1, i);
            }
            std::fwrite (out.data(), sizeof (float), out.size(), stdout);
            std::fflush (stdout);
        }
        return 0;
    }
    if (mode == "--calibrate")
    {
        for (const auto& pr : factoryPresets())
        {
            auto p = paramsFromSnapshot (pr.values);
            // Two passes: the second at the found MASTER corrects presets that reach the soft ceiling.
            double m = -20.0;
            for (int pass = 0; pass < 2; ++pass)
            {
                p.masterDb = (float) m;
                m += -1.0 - db (truePeak (render (p, phraseOf (pr), 3.5)));
            }
            std::cout << pr.name << "|" << juce::String (std::floor (m * 10.0) / 10.0, 1) << "\n";
        }
        return 0;
    }
    testFactoryPresets();
    testPolyphonyAndStealing();
    testMonoAndLegato();
    testBlockSizesAndRates();
    testTuningBendAndLfo();
    testAnalogue();
    testCharacter();
    testMovement();
    testCpu();
    std::cout << (failures == 0 ? "PASS " : "FAILED ") << (checks - failures) << "/" << checks << " checks\n";
    return failures == 0 ? 0 : 1;
}
