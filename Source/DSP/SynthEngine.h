#pragma once

#include <array>
#include <cmath>
#include <complex>
#include <memory>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "../Params.h"
#include "AnalogCharacter.h"
#include "Filters.h"

// PowerSynth (Claude, 2026-10-28): leads, pads, atmospheres and stabs for techno and schranz.
//   8 voices (+2 spare slots, so a stolen voice fades out in 3 ms while the new note starts): OSC A with up to 7
//   unison voices spread in stereo, OSC B (octave, semitones, fine, level; phase-modulates A with FM), noise ->
//   per-voice filter (LP24/LP12/BP/HP, its own ADSR, key tracking) -> amp ADSR. One LFO (free or at the host tempo)
//   on pitch, cutoff, shape or amplitude. MONO / LEGATO with last-note priority and GLIDE.
//   Sum -> DC blocker -> DRIVE (oversampled) -> chorus -> tempo delay -> reverb -> width (lows mono) ->
//   DIGITAL | ANALOGUE top -> MASTER -> linked soft ceiling.
// Oscillators run at the base rate with polyBLEP (the morph of PowerBass). No allocation in process().
namespace ps::dsp
{
inline double dbToGain (double db) noexcept { return std::pow (10.0, db / 20.0); }

inline double polyBlep (double t, double dt) noexcept
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
    return 0.0;
}

// Sine -> triangle -> saw -> square, band-limited; shape 0..3.
inline double oscWave (double ph, double dt, double shape) noexcept
{
    auto wave = [ph, dt] (int w)
    {
        switch (w)
        {
            case 0: return std::sin (2.0 * pi * ph);
            case 1: return ph < 0.25 ? 4.0 * ph : (ph < 0.75 ? 2.0 - 4.0 * ph : 4.0 * ph - 4.0);
            case 2: return (1.0 - 2.0 * ph + polyBlep (ph, dt)) * 0.8;
            default:
            {
                double ph2 = ph + 0.5; ph2 -= std::floor (ph2);
                return ((ph < 0.5 ? 1.0 : -1.0) + polyBlep (ph, dt) - polyBlep (ph2, dt)) * 0.7;
            }
        }
    };
    const int si = juce::jlimit (0, 2, (int) shape);
    const double sf = juce::jlimit (0.0, 1.0, shape - si);
    return sf <= 0.0 ? wave (si) : wave (si) * (1.0 - sf) + wave (si + 1) * sf;
}

// Hidden wavetables (2.0, 2026-10-29): SHAPE 4..12 morphs through nine modern frames - HOLLOW, VOX A, VOX O, GROWL,
// SCREECH, SYNC, BITTER, METAL, INDUST. Built once (off the audio thread), band-limited in half-octave levels: the
// level used never has a harmonic above 0.45 of the sample rate. The user sees only the shape's name.
class WaveBank
{
public:
    static constexpr int size = 2048, levels = 21, frames = 9;
    static const WaveBank& get() { static const WaveBank b; return b; }

    float read (int frame, double ph, double inc) const noexcept
    {
        const double h = 0.45 / std::max (inc, 1.0e-9);                          // highest harmonic allowed
        const int lv = juce::jlimit (0, levels - 1, (int) std::ceil (2.0 * std::log2 (1024.0 / std::max (h, 1.0))));
        const float* t = data.data() + ((size_t) frame * levels + (size_t) lv) * size;
        const double x = ph * size;
        const int i = (int) x & (size - 1);
        const float f = (float) (x - std::floor (x));
        return t[i] + (t[(i + 1) & (size - 1)] - t[i]) * f;
    }

private:
    std::vector<float> data;

    static void fft (std::vector<std::complex<double>>& a, bool inverse)
    {
        const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i)
        {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap (a[i], a[j]);
        }
        for (size_t len = 2; len <= n; len <<= 1)
        {
            const double ang = 2.0 * juce::MathConstants<double>::pi / (double) len * (inverse ? 1.0 : -1.0);
            const std::complex<double> wl (std::cos (ang), std::sin (ang));
            for (size_t i = 0; i < n; i += len)
            {
                std::complex<double> w (1.0);
                for (size_t k = 0; k < len / 2; ++k, w *= wl)
                {
                    const auto u = a[i + k], v = a[i + k + len / 2] * w;
                    a[i + k] = u + v; a[i + k + len / 2] = u - v;
                }
            }
        }
    }

    static double prototype (int frame, double ph)
    {
        const double tp = 2.0 * juce::MathConstants<double>::pi;
        auto g = [] (double k, double c, double w) { return std::exp (-((k - c) / w) * ((k - c) / w)); };
        auto series = [&] (auto amp) { double v = 0.0; for (int k = 1; k <= 300; ++k) v += amp ((double) k) * std::sin (tp * k * ph); return v; };
        const double saw = 1.0 - 2.0 * ph, sq = ph < 0.5 ? 1.0 : -1.0;
        switch (frame)
        {
            case 0: return series ([] (double k) { return ((int) k % 2) ? std::pow (k, -0.7) : 0.0; });                          // HOLLOW
            case 1: return series ([&] (double k) { return (1.0 + 4.0 * g (k, 6, 1.5) + 3.0 * g (k, 10, 2)) / k; });             // VOX A
            case 2: return series ([&] (double k) { return (1.0 + 5.0 * g (k, 2.5, 1) + 2.5 * g (k, 5, 1.5)) / k; });            // VOX O
            case 3: return series ([&] (double k) { return (((int) k % 3) ? 1.0 : 0.15) * (1.0 + 1.5 * g (k, 7, 2.5)) / k; });  // GROWL
            case 4: return series ([&] (double k) { return std::pow (k, -0.6) * (1.0 + 6.0 * g (k, 14, 3)); });                  // SCREECH
            case 5: { double v = 2.7 * ph; return 1.0 - 2.0 * (v - std::floor (v)); }                                           // SYNC
            case 6: return std::tanh (5.0 * (saw + 0.6 * sq));                                                                    // BITTER
            case 7: return series ([] (double k)                                                                                  // METAL
                    {
                        static const int set[] { 1, 2, 3, 5, 7, 9, 11, 13, 17, 19, 23, 29, 31 };
                        for (int m : set) if ((int) k == m) return std::pow (k, -0.35) * (0.15 + 0.5 + 0.5 * std::cos (k * 1.7));
                        return 0.0;
                    });
            default: return std::tanh (2.5 * std::sin (tp * ph + 2.2 * std::sin (tp * 3.0 * ph) + 0.8 * std::sin (tp * 7.0 * ph)));  // INDUST
        }
    }

    WaveBank()
    {
        data.assign ((size_t) frames * levels * size, 0.0f);
        constexpr int an = 4096;
        for (int f = 0; f < frames; ++f)
        {
            std::vector<std::complex<double>> spec (an);
            for (int i = 0; i < an; ++i) spec[(size_t) i] = prototype (f, (double) i / an);
            fft (spec, false);
            double scale = 1.0;
            for (int lv = 0; lv < levels; ++lv)
            {
                const int maxH = std::max (1, (int) std::floor (1024.0 / std::pow (2.0, lv / 2.0)));
                std::vector<std::complex<double>> b (size);
                for (int k = 1; k <= std::min (maxH, size / 2 - 1); ++k) { b[(size_t) k] = spec[(size_t) k]; b[(size_t) (size - k)] = std::conj (spec[(size_t) k]); }
                fft (b, true);
                float* t = data.data() + ((size_t) f * levels + (size_t) lv) * size;
                if (lv == 0)
                {
                    // the loudness of a saw (RMS), the peak never above 1
                    double e = 0.0, pk = 0.0;
                    for (int i = 0; i < size; ++i) { e += b[(size_t) i].real() * b[(size_t) i].real(); pk = std::max (pk, std::abs (b[(size_t) i].real())); }
                    scale = 0.46 / std::sqrt (e / size + 1.0e-30);
                    if (pk * scale > 1.0) scale = 1.0 / pk;
                }
                for (int i = 0; i < size; ++i) t[i] = (float) (b[(size_t) i].real() * scale);
            }
        }
    }
};

// SHAPE 0..3 the classic morph (polyBLEP), 3..4 square into the first table, 4..12 between the tables.
inline double shapeWave (double ph, double inc, double shape) noexcept
{
    if (shape <= 3.0) return oscWave (ph, inc, shape);
    const auto& bank = WaveBank::get();
    if (shape < 4.0)
    {
        const double f = shape - 3.0;
        return oscWave (ph, inc, 3.0) * (1.0 - f) + bank.read (0, ph, inc) * f;
    }
    const int i = juce::jlimit (0, WaveBank::frames - 1, (int) (shape - 4.0));
    const double f = juce::jlimit (0.0, 1.0, shape - 4.0 - i);
    const double a = bank.read (i, ph, inc);
    return f <= 0.0 || i == WaveBank::frames - 1 ? a : a * (1.0 - f) + bank.read (i + 1, ph, inc) * f;
}

// Hilbert pair (Olli Niemitalo) for single-sideband frequency shifting (SHIFT).
struct Shifter
{
    std::array<double, 4> ax1 {}, ax2 {}, ay1 {}, ay2 {}, bx1 {}, bx2 {}, by1 {}, by2 {};
    double delayed = 0.0, phase = 0.0;
    void reset() { ax1.fill (0); ax2.fill (0); ay1.fill (0); ay2.fill (0); bx1.fill (0); bx2.fill (0); by1.fill (0); by2.fill (0); delayed = phase = 0.0; }
    double tick (double x, double hz, double sr)
    {
        static constexpr double ca[4] { .6923878, .9360654322959, .9882295226860, .9987488452737 };
        static constexpr double cb[4] { .4021921162426, .8561710882420, .9722909545651, .9952884791278 };
        double a = x, b = x;
        for (size_t i = 0; i < 4; ++i) { const double y = ca[i] * ca[i] * (a + ay2[i]) - ax2[i]; ax2[i] = ax1[i]; ax1[i] = a; ay2[i] = ay1[i]; ay1[i] = y; a = y; }
        for (size_t i = 0; i < 4; ++i) { const double y = cb[i] * cb[i] * (b + by2[i]) - bx2[i]; bx2[i] = bx1[i]; bx1[i] = b; by2[i] = by1[i]; by1[i] = y; b = y; }
        const double re = a, im = delayed;
        delayed = b;
        phase += hz / sr; phase -= std::floor (phase);
        const double p = 2.0 * juce::MathConstants<double>::pi * phase;
        return re * std::cos (p) - im * std::sin (p);
    }
};

// SMASH: three bands (Linkwitz-Riley, flat sum), each levelled towards -26 dBFS from above and below (OTT-like).
struct Smash
{
    juce::dsp::LinkwitzRileyFilter<float> s1, s2, ap;
    std::array<double, 3> env {};
    double sr = 48000.0, att = 0.0, rel = 0.0;
    void prepare (double rate, int block)
    {
        sr = rate;
        const juce::dsp::ProcessSpec spec { sr, (juce::uint32) std::max (1, block), 2 };
        for (auto* f : { &s1, &s2, &ap }) f->prepare (spec);
        ap.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
        s1.setCutoffFrequency (120.0f); s2.setCutoffFrequency ((float) std::min (2500.0, sr * 0.45)); ap.setCutoffFrequency ((float) std::min (2500.0, sr * 0.45));
        att = std::exp (-1.0 / (0.0005 * sr)); rel = std::exp (-1.0 / (0.06 * sr));
        reset();
    }
    void reset() { s1.reset(); s2.reset(); ap.reset(); env.fill (0.0); }
    void process (float* L, float* R, int n, double amount)
    {
        if (amount <= 0.001) return;
        for (int i = 0; i < n; ++i)
        {
            float b[3][2];
            float* ch[2] = { L, R };
            for (int c = 0; c < 2; ++c)
            {
                float lo, rest, mid, top;
                s1.processSample (c, ch[c][i], lo, rest);
                s2.processSample (c, rest, mid, top);
                b[0][c] = ap.processSample (c, lo); b[1][c] = mid; b[2][c] = top;
            }
            float out[2] { 0.0f, 0.0f };
            for (int k = 0; k < 3; ++k)
            {
                const double lv = std::max (std::abs (b[k][0]), std::abs (b[k][1]));
                env[(size_t) k] = lv > env[(size_t) k] ? lv : lv + rel * (env[(size_t) k] - lv);   // instant attack: a lifted tail never lets the next hit through boosted
                const double d = 20.0 * std::log10 (env[(size_t) k] + 1.0e-9);
                // levelled towards -26 dBFS: loud parts down (up to 8:1), quiet parts up (up to 6:1, +30 dB), silence untouched
                const double over = d + 26.0;
                const double gainDb = over > 0.0 ? -over * (1.0 - 1.0 / (1.0 + 7.0 * amount))
                                                 : std::min (30.0 * amount, -over * (1.0 - 1.0 / (1.0 + 5.0 * amount))) * juce::jlimit (0.0, 1.0, (d + 72.0) / 12.0);
                const double g = std::pow (10.0, (gainDb + 8.0 * amount) / 20.0);
                const double mix = 1.0 + amount * (g - 1.0);
                out[0] += (float) (b[k][0] * mix); out[1] += (float) (b[k][1] * mix);
            }
            L[i] = out[0]; R[i] = out[1];
        }
    }
};

// ADSR with exponential segments (analogue-like); level 0..1.
struct Adsr
{
    enum class Stage { idle, attack, decay, sustain, release };
    Stage stage = Stage::idle;
    double level = 0.0;
    void noteOn() noexcept { stage = Stage::attack; }
    void noteOff() noexcept { if (stage != Stage::idle) stage = Stage::release; }
    void reset() noexcept { stage = Stage::idle; level = 0.0; }
    double tick (double aS, double dS, double s, double rS, double sr) noexcept
    {
        switch (stage)
        {
            case Stage::attack:
                level += (1.3 - level) * (1.0 - std::exp (-1.0 / (std::max (0.0005, aS) * sr * 0.8)));
                if (level >= 1.0) { level = 1.0; stage = Stage::decay; }
                break;
            case Stage::decay:
                level = s + (level - s) * std::exp (-1.0 / (std::max (0.003, dS) * sr * 0.25));
                if (std::abs (level - s) < 1.0e-5) { level = s; stage = Stage::sustain; }
                break;
            case Stage::sustain: level = s; break;
            case Stage::release:
                level *= std::exp (-1.0 / (std::max (0.003, rS) * sr * 0.25));
                if (level < 1.0e-5) { level = 0.0; stage = Stage::idle; }
                break;
            case Stage::idle: break;
        }
        return level;
    }
};

struct Control // per-sample values shared by all voices
{
    const float* lfo = nullptr;   // -1..1
    const float* ana = nullptr;   // DIGITAL 0 .. ANALOGUE 1
    double bendSemis = 0.0;
};

class Voice
{
public:
    static constexpr int maxUnison = 7;

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        noiseNorm = std::sqrt (sr / 48000.0); // the same noise level per Hz (what is heard) at every rate
        WaveBank::get();                      // built once, here and not on the audio thread
        for (auto& c : comb) c.prepare ((int) (sr / 20.0) + 8);
        drift.prepare (sr);
        for (auto& d : unisonDrift) d.prepare (sr);
        glideCoef = 0.0;
        reset();
    }

    void reset() noexcept
    {
        amp.reset(); flt.reset(); active = false; fading = false; fade = 1.0;
        for (auto& c : comb) c.reset();
        combLp = { 0.0, 0.0 }; dcX = { 0.0, 0.0 }; dcY = { 0.0, 0.0 };
        for (auto& f : fL) f.reset();
        for (auto& f : fR) f.reset();
    }

    // legatoFrom < 0: start at the note; otherwise glide from that log-frequency.
    void start (int midiNote, float velocity, const SynthParams& p, power_analog::Random& rnd, double glideFrom, bool retrigger)
    {
        note = midiNote;
        targetLogFreq = std::log (440.0 * std::exp2 ((midiNote - 69) / 12.0));
        logFreq = glideFrom < 0.0 ? targetLogFreq : glideFrom;
        velGain = (1.0 - p.velSens) + p.velSens * velocity;
        if (retrigger || ! active)
        {
            if (! active)
            {
                for (int k = 0; k < maxUnison; ++k) phase[(size_t) k] = std::fmod (0.37 * k + 0.13 * k * k, 1.0); // spread: no phasey start
                phaseB = 0.0;
                for (auto& f : fL) f.reset();
                for (auto& f : fR) f.reset();
                for (auto& c : comb) c.reset();
                combLp = { 0.0, 0.0 };
                amp.level = 0.0; flt.level = 0.0;
            }
            amp.noteOn(); flt.noteOn();
            pitchAge = 0.0;                     // the pitch envelope starts with each new attack (not on legato)
        }
        // ANALOGUE spread for this note (drawn in DIGITAL too, so the sequence does not depend on the switch).
        noteCents = 6.0 * rnd.next();
        noteCentsB = 6.0 * rnd.next();
        noteCut = 1.0 + 0.03 * rnd.next();
        for (auto& c : unisonCents) c = 5.0 * rnd.next();
        noise.seed ((unsigned int) (0x51ED0000u + (unsigned int) midiNote * 7919u));
        active = true; fading = false; fade = 1.0; released = false; age = 0.0;
    }

    void release() noexcept { amp.noteOff(); flt.noteOff(); released = true; }
    void steal() noexcept { fading = true; fadeStep = 1.0 / (0.003 * sr); }
    bool isActive() const noexcept { return active; }
    bool isFading() const noexcept { return fading; }
    bool isReleased() const noexcept { return released; }
    int getNote() const noexcept { return note; }
    double getLevel() const noexcept { return amp.level * fade; }
    double getAge() const noexcept { return age; }
    double getLogFreq() const noexcept { return logFreq; }

    void render (float* outL, float* outR, int n, const SynthParams& p, const Control& c, power_analog::Random& rnd) noexcept
    {
        if (! active) return;
        const double dt = 1.0 / sr;
        glideCoef = p.glideMs > 0.5f ? std::exp (-1.0 / (p.glideMs * 0.001 * sr * 0.3)) : 0.0;
        const int uni = juce::jlimit (1, maxUnison, p.unison);
        const double uniNorm = 1.0 / std::sqrt ((double) uni);
        const double bRatio = std::exp2 (p.oscBOct + p.oscBSemi / 12.0 + p.oscBFine / 1200.0);
        const double aOct = std::exp2 ((double) p.oscAOct);
        const double fmIdx = std::pow (p.fmAmount, 1.5) * 3.0;
        const double q = 0.55 + std::pow (p.resonance, 2.0) * 14.0;
        const double keyOct = 0.5 * (note - 60) / 12.0;
        for (int k = 0; k < uni; ++k)
        {
            const double pos = uni == 1 ? 0.0 : -1.0 + 2.0 * k / (uni - 1);   // -1..1 across the stereo field
            spreadCents[(size_t) k] = pos * p.detune * 50.0 * (0.6 + 0.4 * std::abs (pos));
            const double pan = pos * p.width;
            panL[(size_t) k] = std::sqrt (0.5 * (1.0 - pan));
            panR[(size_t) k] = std::sqrt (0.5 * (1.0 + pan));
        }
        for (int i = 0; i < n; ++i)
        {
            age += dt; pitchAge += dt;
            const double ana = c.ana[i];
            const double lfo = c.lfo[i];
            logFreq = targetLogFreq + glideCoef * (logFreq - targetLogFreq);
            const double dM = drift.tick (rnd, 15.0);
            double semis = c.bendSemis + (p.lfoTarget == LfoTarget::pitch ? lfo * p.lfoAmount * 2.0 : 0.0);
            if (p.pitchEnv != 0.0f) semis += p.pitchEnv * std::exp (-pitchAge * 3.0 / std::max (0.002, p.pitchTimeMs * 0.001));
            const double fBase = std::exp (logFreq) * std::exp2 (semis / 12.0 + ana * (noteCents + dM) / 1200.0);
            const double fA = fBase * aOct;
            const double fe = flt.tick (p.fltAttackMs * 0.001, p.fltDecayMs * 0.001, p.fltSustain, p.fltReleaseMs * 0.001, sr);
            double shapeA = p.oscAShape, shapeB = p.oscBShape, warp = p.warp;
            if (p.morphEnv != 0.0f)   // MORPH: the envelope moves the wavetable position and WARP (zaps, growls)
            {
                shapeA = juce::jlimit (0.0, (double) maxShape, shapeA + p.morphEnv * fe * 3.0);
                warp = juce::jlimit (0.0, 1.0, warp + p.morphEnv * fe);
            }
            if (p.lfoTarget == LfoTarget::shape)
            {
                // a classic shape stays among the classic ones (sounds of 1.x unchanged), a table among the tables
                shapeA = juce::jlimit (0.0, p.oscAShape <= 3.0f ? 3.0 : (double) maxShape, shapeA + lfo * p.lfoAmount * 1.5);
                shapeB = juce::jlimit (0.0, p.oscBShape <= 3.0f ? 3.0 : (double) maxShape, shapeB + lfo * p.lfoAmount * 1.5);
                if (warp > 0.0) warp = juce::jlimit (0.0, 1.0, warp + lfo * p.lfoAmount * 0.5);   // PWM hoover etc.
            }

            const double incB = std::min (fBase * bRatio * std::exp2 (ana * noteCentsB / 1200.0) * dt, 0.45);
            phaseB += incB; phaseB -= std::floor (phaseB);
            const double b = shapeWave (phaseB, incB, shapeB);
            const double fmNow = p.lfoTarget == LfoTarget::fm ? fmIdx * std::max (0.0, 1.0 + lfo * p.lfoAmount) : fmIdx;
            const double pm = fmNow * b / (2.0 * pi);

            double l = 0.0, r = 0.0;
            for (int k = 0; k < uni; ++k)
            {
                const double cents = spreadCents[(size_t) k] + ana * (unisonCents[(size_t) k] + unisonDrift[(size_t) k].tick (rnd, 10.0));
                const double inc = std::min (fA * std::exp2 (cents / 1200.0) * dt, 0.45);
                double& ph = phase[(size_t) k];
                ph += inc; ph -= std::floor (ph);
                double pp = ph + pm; pp -= std::floor (pp);
                const double a = warp > 0.0005 ? warped (pp, inc, shapeA, warp, p.warpMode, fA) : shapeWave (pp, inc, shapeA);
                l += a * panL[(size_t) k];
                r += a * panR[(size_t) k];
            }
            l *= uniNorm * 1.41421356 * p.oscALevel; r *= uniNorm * 1.41421356 * p.oscALevel;
            if (p.ring > 0.0f) { const double rm = 1.0 + p.ring * (1.6 * b - 1.0); l *= rm; r *= rm; }   // A x B
            if (warp > 0.0005)   // BEND and FOLD shift the mean: a 10 Hz DC blocker per voice (only with WARP: 1.x sounds unchanged)
            {
                const double k = 1.0 - 2.0 * pi * 10.0 / sr;
                const double nl = l - dcX[0] + k * dcY[0], nr = r - dcX[1] + k * dcY[1];
                dcX = { l, r }; dcY = { nl, nr }; l = nl; r = nr;
            }
            const double bLvl = p.oscBLevel * b * 0.8, nz = p.noise * 0.5 * noiseNorm * noise.white();
            l += bLvl + nz; r += bLvl + nz;
            // ANALOGUE warmth before the filter.
            l += ana * (power_analog::warmDrive (l * 0.6) / 0.6 - l);
            r += ana * (power_analog::warmDrive (r * 0.6) / 0.6 - r);

            if (--ctlCountdown <= 0)
            {
                ctlCountdown = 16;
                const double lfoOct = p.lfoTarget == LfoTarget::cutoff ? lfo * p.lfoAmount * 3.0 : 0.0;
                const double cut = juce::jlimit (25.0, sr * 0.45, p.cutoffHz * std::exp2 (p.fltEnv * fe * 6.0 + keyOct + lfoOct)
                                                                   * (1.0 + ana * (noteCut - 1.0)));
                for (auto* f : { &fL[0], &fL[1], &fR[0], &fR[1] }) f->set (cut, p.filterMode == FilterMode::lp24 ? std::sqrt (q) * 0.9 : q, sr);
                combCut = cut;
            }
            if (p.grit > 0.0f)                                  // drive into the filter
            {
                const double d = 1.0 + 12.0 * p.grit, nd = std::pow (d, 0.6);
                l = std::tanh (l * d) / nd; r = std::tanh (r * d) / nd;
            }
            if (p.filterMode == FilterMode::comb)
            {
                // a comb tuned to the note: metallic, industrial; resonance = feedback, cutoff = brightness
                const double period = sr / std::max (20.0, fA);
                const double fb = 0.6 + 0.38 * p.resonance, lp = 1.0 - std::exp (-2.0 * pi * std::min (combCut, sr * 0.45) / sr);
                double* io[2] = { &l, &r };
                for (size_t ch = 0; ch < 2; ++ch)
                {
                    combLp[ch] += lp * (comb[ch].read (period - 1.0) - combLp[ch]);
                    const double y = *io[ch] + fb * std::tanh (combLp[ch]);
                    comb[ch].push (y);
                    *io[ch] = y * (1.0 - fb) * 2.0;
                }
            }
            else
            {
                l = filter (fL, l, p.filterMode, p.grit);
                r = filter (fR, r, p.filterMode, p.grit);
            }

            const double env = amp.tick (p.attackMs * 0.001, p.decayMs * 0.001, p.sustain, p.releaseMs * 0.001, sr);
            double g = env * velGain;
            if (p.lfoTarget == LfoTarget::amp) g *= 1.0 - p.lfoAmount * (0.5 - 0.5 * lfo);
            if (fading)
            {
                fade -= fadeStep;
                if (fade <= 0.0) { fade = 0.0; active = false; }
            }
            g *= fade;
            outL[i] += (float) (l * g);
            outR[i] += (float) (r * g);
            if (! active || amp.stage == Adsr::Stage::idle) { active = false; return; }
        }
    }

private:
    // WARP of OSC A: SYNC (a slave at 1..8x, reset by the note's phase), BEND (phase bent), PWM (a flat part inserted:
    // the hoover), FOLD (the wave folded; less on high notes, against aliasing).
    static double warped (double pp, double inc, double shape, double w, WarpMode m, double freq) noexcept
    {
        switch (m)
        {
            case WarpMode::sync:
            {
                const double ratio = 1.0 + 7.0 * std::pow (w, 1.3);
                const double sp = pp * ratio - std::floor (pp * ratio);
                double v = shapeWave (sp, inc * ratio, shape);
                const double end = ratio - std::floor (ratio);
                const double jump = shapeWave (0.0, inc * ratio, shape) - shapeWave (end > 0.0 ? end : 0.999999, inc * ratio, shape);
                return v + 0.5 * jump * polyBlep (pp, inc);   // the reset of the slave, band-limited
            }
            case WarpMode::bend:
            {
                const double e = 1.0 + 5.0 * w;
                return shapeWave (std::pow (pp, e), inc * e, shape);
            }
            case WarpMode::pwm:
            {
                const double width = 1.0 - 0.9 * w;
                return shapeWave (std::min (0.999999, pp / width), inc / width, shape);
            }
            case WarpMode::fold:
            {
                const double g = 1.0 + 10.0 * w * std::min (1.0, 1500.0 / std::max (freq, 1.0));
                return std::sin (0.5 * pi * (shapeWave (pp, inc, shape) * g + 0.35 * w));   // asymmetric: odd and even
            }
        }
        return shapeWave (pp, inc, shape);
    }

    static double filter (std::array<Svf, 2>& f, double x, FilterMode m, double grit) noexcept
    {
        switch (m)
        {
            case FilterMode::lp24:
            {
                double s1 = f[0].tick (x).low;
                if (grit > 0.0) { const double d = 1.0 + 3.0 * grit; s1 = std::tanh (s1 * d) / std::sqrt (d); }   // the resonance growls
                return f[1].tick (s1).low;
            }
            case FilterMode::comb:
            case FilterMode::lp12: return f[0].tick (x).low;
            case FilterMode::bp:   return f[0].tick (x).band;
            case FilterMode::hp:   return f[0].tick (x).high;
        }
        return x;
    }

    double sr = 48000.0;
    int note = 60;
    bool active = false, released = false, fading = false;
    double fade = 1.0, fadeStep = 0.0, age = 0.0;
    double logFreq = 0.0, targetLogFreq = 0.0, glideCoef = 0.0, velGain = 1.0;
    std::array<double, maxUnison> phase {}, spreadCents {}, unisonCents {}, panL {}, panR {};
    std::array<power_analog::Drift, maxUnison> unisonDrift;
    power_analog::Drift drift;
    double phaseB = 0.0, noteCents = 0.0, noteCentsB = 0.0, noteCut = 1.0;
    Rng noise;
    double noiseNorm = 1.0;
    Adsr amp, flt;
    std::array<Svf, 2> fL, fR;
    int ctlCountdown = 0;
    double pitchAge = 0.0, combCut = 8000.0;
    std::array<DelayLine, 2> comb;
    std::array<double, 2> combLp {}, dcX {}, dcY {};
};

class SynthEngine
{
public:
    static constexpr int numVoices = 8, numSlots = 10;

    void prepare (double sampleRate, int maxBlock, const SynthParams& p)
    {
        sr = sampleRate;
        blockMax = std::max (32, maxBlock);
        for (auto& v : voices) v.prepare (sr);
        lfoBuf.assign ((size_t) blockMax, 0.0f);
        anaBuf.assign ((size_t) blockMax, 0.0f);
        mixL.assign ((size_t) blockMax, 0.0f);
        mixR.assign ((size_t) blockMax, 0.0f);
        for (int i = 0; i < 3; ++i)
        {
            os[(size_t) i] = std::make_unique<juce::dsp::Oversampling<float>> (2, (size_t) (i + 1),
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
            os[(size_t) i]->initProcessing ((size_t) blockMax);
        }
        const juce::dsp::ProcessSpec spec { sr, (juce::uint32) blockMax, 2 };
        chorusFx.prepare (spec);
        smashFx.prepare (sr, blockMax);
        reverbFx.prepare (spec);
        const int maxDelay = (int) (sr * 4.0) + 4;
        delayL.assign ((size_t) maxDelay, 0.0f);
        delayR.assign ((size_t) maxDelay, 0.0f);
        dcL.set (16.0, sr); dcR.set (16.0, sr);   // 16 Hz: the biased SAT on short notes leaves an envelope-shaped offset (2.1)
        sideHp.set (150.0, 0.707, sr);
        topL.set (8000.0, sr); topR.set (8000.0, sr);
        anaBlend.prepare (sr); anaBlendPost.prepare (sr);
        lfoSlew = 1.0 - std::exp (-1.0 / (0.002 * sr));
        smooth = 1.0 - std::exp (-1.0 / (0.02 * sr));
        held.reserve (128); // at most 128 distinct notes: push_back never allocates on the audio thread
        activeOs = p.oversampleIndex;
        reset();
    }

    void reset()
    {
        for (auto& v : voices) v.reset();
        for (auto& o : os) if (o) o->reset();
        chorusFx.reset(); reverbFx.reset(); smashFx.reset(); for (auto& sh : shifter) sh.reset();
        std::fill (delayL.begin(), delayL.end(), 0.0f);
        std::fill (delayR.begin(), delayR.end(), 0.0f);
        delayPos = 0; dampL = dampR = 0.0;
        dcL.reset(); dcR.reset(); sideHp.reset(); topL.reset(); topR.reset();
        held.clear(); bendSemis = 0.0; lfoPhase = 0.0; lfoValue = 0.0; lfoHeld = 0.0;
        anaRand.seed (0x5EED2810u);
        gMaster = gDrive = gChorus = gDelay = gReverb = gWidth = -1.0;
        anaEIn = anaEOut = 0.0; anaG = 1.0;
        lastVoice = -1;
    }

    int getLatencySamplesFor (int index) const noexcept
    {
        const auto& o = os[(size_t) juce::jlimit (0, 2, index)];
        return o ? (int) std::round (o->getLatencyInSamples()) : 0;
    }
    int getLatencySamples() const noexcept { return getLatencySamplesFor (activeOs); }

    void process (float* outL, float* outR, int n, const juce::MidiBuffer& midi, const SynthParams& p, int midiOffset = 0)
    {
        if (p.oversampleIndex != activeOs) { activeOs = p.oversampleIndex; os[(size_t) activeOs]->reset(); }
        auto it = midi.findNextSamplePosition (midiOffset);
        int pos = 0;
        while (pos < n)
        {
            int len = std::min (n - pos, blockMax);
            // Note events land on their sample: render up to the next one.
            if (it != midi.cend())
            {
                const int evPos = (*it).samplePosition - midiOffset;
                if (evPos < pos + len && evPos >= pos)
                {
                    if (evPos == pos)
                    {
                        while (it != midi.cend() && (*it).samplePosition - midiOffset == pos) { handle ((*it).getMessage(), p); ++it; }
                        continue;
                    }
                    len = evPos - pos;
                }
                else if (evPos < pos) { handle ((*it).getMessage(), p); ++it; continue; }
            }
            renderChunk (outL + pos, outR + pos, len, p);
            pos += len;
        }
        while (it != midi.cend() && (*it).samplePosition - midiOffset < n) { handle ((*it).getMessage(), p); ++it; }
    }

    int activeVoiceCount() const noexcept
    {
        int c = 0;
        for (const auto& v : voices) c += v.isActive() && ! v.isFading() ? 1 : 0;
        return c;
    }

private:
    void handle (const juce::MidiMessage& m, const SynthParams& p)
    {
        if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity(), p);
        else if (m.isNoteOff()) noteOff (m.getNoteNumber(), p);
        else if (m.isPitchWheel()) bendSemis = (m.getPitchWheelValue() - 8192) / 8192.0 * p.bendRange;
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            held.clear();
            for (auto& v : voices) if (v.isActive()) { if (m.isAllSoundOff()) v.steal(); else v.release(); }
        }
    }

    void noteOn (int note, float vel, const SynthParams& p)
    {
        held.erase (std::remove (held.begin(), held.end(), note), held.end());
        if (held.size() < 128) held.push_back (note);
        if (p.voiceMode != VoiceMode::poly)
        {
            auto& v = voices[0];
            for (int i = 1; i < numSlots; ++i) if (voices[(size_t) i].isActive()) voices[(size_t) i].steal();
            const bool sounding = v.isActive() && ! v.isReleased();
            const bool legato = p.voiceMode == VoiceMode::legato && sounding && held.size() > 1;
            const double from = v.isActive() && p.glideMs > 0.5f ? v.getLogFreq() : -1.0;
            v.start (note, vel, p, anaRand, from, ! legato);
            return;
        }
        // Poly: a free slot; when 8 already sound, the quietest (released first, else oldest) fades out in 3 ms.
        int sounding = 0;
        for (auto& v : voices) sounding += v.isActive() && ! v.isFading() ? 1 : 0;
        if (sounding >= numVoices)
        {
            int victim = -1; double best = 1.0e9;
            for (int i = 0; i < numSlots; ++i)
            {
                const auto& v = voices[(size_t) i];
                if (! v.isActive() || v.isFading()) continue;
                const double score = (v.isReleased() ? 0.0 : 10.0) + v.getLevel() - v.getAge() * 1.0e-3;
                if (score < best) { best = score; victim = i; }
            }
            if (victim >= 0) voices[(size_t) victim].steal();
        }
        int slot = -1;
        for (int i = 0; i < numSlots && slot < 0; ++i) if (! voices[(size_t) i].isActive()) slot = i;
        if (slot < 0) // every slot busy (fading): take the quietest
        {
            double best = 1.0e9;
            for (int i = 0; i < numSlots; ++i) if (voices[(size_t) i].getLevel() < best) { best = voices[(size_t) i].getLevel(); slot = i; }
            voices[(size_t) slot].reset();
        }
        const double from = p.glideMs > 0.5f && lastVoice >= 0 ? lastLogFreq : -1.0;
        voices[(size_t) slot].start (note, vel, p, anaRand, from, true);
        lastVoice = slot;
        lastLogFreq = std::log (440.0 * std::exp2 ((note - 69) / 12.0));
    }

    void noteOff (int note, const SynthParams& p)
    {
        held.erase (std::remove (held.begin(), held.end(), note), held.end());
        if (p.voiceMode != VoiceMode::poly)
        {
            auto& v = voices[0];
            if (v.isActive() && v.getNote() == note)
            {
                if (held.empty()) v.release();
                else v.start (held.back(), 0.8f, p, anaRand, v.getLogFreq(), p.voiceMode == VoiceMode::mono); // back to the held note
            }
            return;
        }
        for (auto& v : voices) if (v.isActive() && ! v.isReleased() && v.getNote() == note) v.release();
    }

    double lfoTick (const SynthParams& p) noexcept
    {
        const double rate = p.lfoSync ? p.bpm / 60.0 / divisionBeats[juce::jlimit (0, numDivisions - 1, p.lfoDiv)] : p.lfoRateHz;
        lfoPhase += rate / sr;
        bool wrapped = false;
        if (lfoPhase >= 1.0) { lfoPhase -= std::floor (lfoPhase); wrapped = true; }
        double v = 0.0;
        switch (p.lfoShape)
        {
            case LfoShape::sine:     v = std::sin (2.0 * pi * lfoPhase); break;
            case LfoShape::triangle: v = lfoPhase < 0.5 ? 4.0 * lfoPhase - 1.0 : 3.0 - 4.0 * lfoPhase; break;
            case LfoShape::saw:      v = 1.0 - 2.0 * lfoPhase; break;
            case LfoShape::square:   v = lfoPhase < 0.5 ? 1.0 : -1.0; break;
            case LfoShape::random:   if (wrapped) lfoHeld = lfoRand.white(); v = lfoHeld; break;
        }
        lfoValue += (v - lfoValue) * lfoSlew; // 2 ms slew: square and random steps do not click
        return lfoValue;
    }

    static double driveShape (double x, double g, DriveType t) noexcept
    {
        const double v = x * g;
        switch (t)
        {
            case DriveType::saturate: return std::tanh (v + 0.1) - std::tanh (0.1);
            case DriveType::clip:     return juce::jlimit (-1.0, 1.0, v);
            case DriveType::fold:     return std::sin (1.3 * v);
            case DriveType::crush:    return std::tanh (v);
        }
        return v;
    }

    void renderChunk (float* outL, float* outR, int n, const SynthParams& p)
    {
        auto* L = mixL.data(); auto* R = mixR.data();
        std::fill (L, L + n, 0.0f); std::fill (R, R + n, 0.0f);
        for (int i = 0; i < n; ++i) { lfoBuf[(size_t) i] = (float) lfoTick (p); anaBuf[(size_t) i] = (float) anaBlend.tick (p.analog); }
        Control c { lfoBuf.data(), anaBuf.data(), bendSemis };
        for (auto& v : voices) v.render (L, R, n, p, c, anaRand);

        // SHIFT: a small frequency shift before the drive (stabs sound less like a plug-in, more like a machine)
        if (p.shift > 0.001f)
        {
            const double hz = 60.0 * p.shift * p.shift;
            for (int i = 0; i < n; ++i) { L[i] = (float) shifter[0].tick (L[i], hz, sr); R[i] = (float) shifter[1].tick (R[i], hz * 1.07, sr); }
        }

        // DRIVE, oversampled; drive 0 is clean (blend in over the first quarter of the knob).
        if (p.drive > 0.001f || gDrive > 0.001)
        {
            float* ch[2] = { L, R };
            juce::dsp::AudioBlock<float> block (ch, 2, (size_t) n);
            auto up = os[(size_t) activeOs]->processSamplesUp (block);
            const int upN = (int) up.getNumSamples();
            const double target = p.drive;
            const double g = dbToGain (target * 30.0), norm = p.driveType == DriveType::fold ? 1.0 // sin is bounded already; sin (1.3 g) can be near 0
                                                     : 1.0 / std::max (0.2, std::abs (driveShape (1.0, g, p.driveType)));
            for (int c2 = 0; c2 < 2; ++c2)
            {
                auto* u = up.getChannelPointer ((size_t) c2);
                double gd = gDrive < 0.0 ? target : gDrive;
                for (int i = 0; i < upN; ++i)
                {
                    gd += (target - gd) * smooth * n / upN;
                    const double wet = std::min (1.0, gd * 4.0);
                    const double x = u[i] * 0.7;
                    u[i] = (float) ((x + wet * (driveShape (x, g, p.driveType) * norm * 0.8 - x)) / 0.7);
                }
                if (c2 == 1) gDrive = gd;
            }
            os[(size_t) activeOs]->processSamplesDown (block);
            if (p.driveType == DriveType::crush && p.drive > 0.01f) // CRUSH: fewer bits and a lower sample rate
            {
                const double bits = 16.0 - 12.0 * p.drive, q = std::exp2 (bits - 1.0);
                const int hold = 1 + (int) (p.drive * 8.0 * sr / 48000.0);
                for (int i = 0; i < n; ++i)
                {
                    if (++crushCount >= hold) { crushCount = 0; crushL = std::round (L[i] * q) / q; crushR = std::round (R[i] * q) / q; }
                    L[i] = (float) crushL; R[i] = (float) crushR;
                }
            }
        }

        // DC blocker after the drive: SAT is biased on purpose (even harmonics), so it adds an offset.
        for (int i = 0; i < n; ++i) { L[i] = (float) dcL.tick (L[i]); R[i] = (float) dcR.tick (R[i]); }

        // SMASH: three-band upward + downward compression, the modern density
        smashFx.process (L, R, n, p.smash);

        // CHORUS
        if (p.chorus > 0.001f)
        {
            chorusFx.setRate (0.35f + 0.5f * p.chorus);
            chorusFx.setDepth (0.15f + 0.35f * p.chorus);
            chorusFx.setCentreDelay (7.0f);
            chorusFx.setFeedback (0.0f);
            chorusFx.setMix (0.5f * p.chorus);
            float* ch[2] = { L, R };
            juce::dsp::AudioBlock<float> block (ch, 2, (size_t) n);
            chorusFx.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

        // DELAY at the host tempo, ping-pong, darker repeats.
        {
            const int len = (int) delayL.size();
            const double t = 60.0 / std::max (40.0, p.bpm) * divisionBeats[juce::jlimit (0, numDivisions - 1, p.delayTime)];
            const int d = juce::jlimit (1, len - 2, (int) (t * sr));
            const double fb = p.delayFb, damp = std::exp (-2.0 * pi * 4500.0 / sr);
            for (int i = 0; i < n; ++i)
            {
                gDelay = gDelay < 0.0 ? p.delayMix : gDelay + (p.delayMix - gDelay) * smooth;
                const int rp = (delayPos - d + len) % len;
                const double yl = delayL[(size_t) rp], yr = delayR[(size_t) rp];
                dampL = yl + damp * (dampL - yl); dampR = yr + damp * (dampR - yr);
                delayL[(size_t) delayPos] = (float) (0.5 * (L[i] + R[i]) + fb * dampR); // ping-pong
                delayR[(size_t) delayPos] = (float) (fb * dampL);
                delayPos = (delayPos + 1) % len;
                L[i] += (float) (gDelay * yl);
                R[i] += (float) (gDelay * yr);
            }
        }

        // REVERB
        if (p.reverbMix > 0.001f)
        {
            juce::dsp::Reverb::Parameters rp;
            rp.roomSize = 0.55f + 0.40f * p.reverbSize; // longest tail about 6 s
            rp.damping = 0.45f - 0.25f * p.reverbSize;
            rp.width = 1.0f;
            rp.wetLevel = 0.5f * p.reverbMix;
            rp.dryLevel = 1.0f - 0.3f * p.reverbMix;
            reverbFx.setParameters (rp);
            float* ch[2] = { L, R };
            juce::dsp::AudioBlock<float> block (ch, 2, (size_t) n);
            reverbFx.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

        // WIDTH (side above 150 Hz: lows stay mono), ANALOGUE top, MASTER, linked soft ceiling.
        for (int i = 0; i < n; ++i)
        {
            gWidth = gWidth < 0.0 ? p.width : gWidth + (p.width - gWidth) * smooth;
            const double m = 0.5 * (L[i] + R[i]);
            const double s = sideHp.tick (0.5 * (L[i] - R[i])).high * (0.4 + 1.2 * gWidth);
            double l = m + s, r = m - s;
            const double a = anaBlendPost.tick (p.analog);
            const double tl = topL.tick (l), tr = topR.tick (r);
            const double e = 0.5 * (l * l + r * r), et = 0.5 * (tl * tl + tr * tr);
            anaEIn = e + anaKE * (anaEIn - e);
            anaEOut = et + anaKE * (anaEOut - et);
            const double gt = juce::jlimit (1.0, 1.585, std::sqrt ((anaEIn + 1.0e-12) / (anaEOut + 1.0e-12)));
            anaG = gt + anaKG * (anaG - gt);
            l += a * (tl * anaG - l);
            r += a * (tr * anaG - r);
            const double target = dbToGain (p.masterDb);
            gMaster = gMaster < 0.0 ? target : gMaster + (target - gMaster) * smooth;
            l *= gMaster; r *= gMaster;
            const double pk = std::max (std::abs (l), std::abs (r));
            if (pk > 0.9) { const double k = (0.9 + 0.1 * std::tanh ((pk - 0.9) / 0.1)) / pk; l *= k; r *= k; }
            outL[i] = (float) l;
            outR[i] = (float) r;
        }
    }

    double sr = 48000.0;
    int blockMax = 512, activeOs = 1, lastVoice = -1, delayPos = 0, crushCount = 0;
    double lastLogFreq = 0.0, bendSemis = 0.0;
    std::array<Voice, numSlots> voices;
    std::array<Shifter, 2> shifter;
    Smash smashFx;
    std::vector<int> held;
    std::vector<float> lfoBuf, anaBuf, mixL, mixR, delayL, delayR;
    double lfoPhase = 0.0, lfoValue = 0.0, lfoHeld = 0.0, lfoSlew = 0.1, smooth = 0.001;
    Rng lfoRand;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> os;
    juce::dsp::Chorus<float> chorusFx;
    juce::dsp::Reverb reverbFx;
    DcBlock dcL, dcR;
    Svf sideHp;
    power_analog::OnePole topL, topR;
    power_analog::Blend anaBlend, anaBlendPost;
    power_analog::Random anaRand;
    double anaEIn = 0.0, anaEOut = 0.0, anaG = 1.0;
    const double anaKE = 0.9990, anaKG = 0.9996;
    double gMaster = -1.0, gDrive = -1.0, gChorus = -1.0, gDelay = -1.0, gReverb = -1.0, gWidth = -1.0;
    double dampL = 0.0, dampR = 0.0, crushL = 0.0, crushR = 0.0;
};
}
