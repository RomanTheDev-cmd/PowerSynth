#pragma once

#include <algorithm>
#include <array>
#include <cmath>

// SIDECHAIN (2026-11-12, Roman: "a sidechain like Kickstart 2, so that the volume changes"): the level ducks on every
// beat of the host and comes back along a curve - room for the kick without routing its signal. It follows the host's
// song position, so it stays on the grid wherever playback starts; with the transport stopped nothing is ducked.
// The curve is set per period (1/4, 1/8 or 1/2), so it scales with the tempo and the rate, as in Kickstart.
namespace ps::dsp
{
struct Ducker
{
    static constexpr int numRates = 3, numShapes = 4;
    static double beatsOf (int rate) noexcept { static constexpr double b[numRates] { 1.0, 0.5, 2.0 }; return b[std::clamp (rate, 0, numRates - 1)]; }

    // How far the level is down at this point of the period (phase 0..1): 1 = fully ducked, 0 = back.
    //   CLASSIC: silent for 2 % of the period, back by 65 % (fast at first, then gently)
    //   SHORT:   silent for 1.5 %, back by 35 % - only the kick's attack gets room
    //   LONG:    silent for 3 %, an S-curve back by 95 % - the whole beat pumps
    //   HARD:    silent for 20 %, then back by 50 % - a gate-like cut
    static double depthAt (double phase, int shape) noexcept
    {
        struct Curve { double hold, end; int kind; };   // kind 0 ease-out, 1 S-curve
        static constexpr Curve c[numShapes] { { 0.02, 0.65, 0 }, { 0.015, 0.35, 0 }, { 0.03, 0.95, 1 }, { 0.20, 0.50, 0 } };
        const auto& k = c[std::clamp (shape, 0, numShapes - 1)];
        if (phase < k.hold) return 1.0;
        if (phase >= k.end) return 0.0;
        const double x = (phase - k.hold) / (k.end - k.hold);
        const double level = k.kind == 0 ? 1.0 - (1.0 - x) * (1.0 - x) : x * x * (3.0 - 2.0 * x);
        return 1.0 - level;
    }

    // 1.5 ms smoothing: the drop takes ~5 ms to reach silence and never clicks.
    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate; coef = 1.0 - std::exp (-1.0 / (0.0015 * sr));
        fastUp = std::exp (-1.0 / (0.0003 * sr)); fastDown = std::exp (-1.0 / (0.03 * sr));
        slowUp = std::exp (-1.0 / (0.02 * sr)); slowDown = std::exp (-1.0 / (0.05 * sr));
        reset();
    }
    void reset() noexcept { gain = 1.0; resetKick(); }
    bool isIdle() const noexcept { return gain > 0.999999; }

    // ppq: the song position (quarter notes) of the first sample that leaves the plug-in in this block
    // (the processor subtracts the plug-in latency, so the dip lands on the beat the listener hears).
    void process (float* l, float* r, int n, double ppq, double bpm, bool playing, double amount, int rate, int shape) noexcept
    {
        amount = std::clamp (amount, 0.0, 1.0);
        if ((amount <= 0.0 || ! playing) && isIdle()) return;
        const double period = beatsOf (rate), step = std::max (1.0, bpm) / 60.0 / sr;
        for (int i = 0; i < n; ++i)
        {
            double target = 1.0;
            if (playing && amount > 0.0)
            {
                double ph = std::fmod (ppq + i * step + 1.0e-9, period);   // a beat computed as 1.9999999 still starts on its sample
                if (ph < 0.0) ph += period;
                target = 1.0 - amount * depthAt (ph / period, shape);
            }
            gain += coef * (target - gain);
            if (gain > 0.999999 && target >= 1.0) gain = 1.0;
            l[i] *= (float) gain;
            if (r != nullptr) r[i] *= (float) gain;
        }
    }

    // KICK mode (2026-11-12): the curve starts on every hit of the kick on the sidechain input, so the bass ducks under
    // rolls, breaks and off-beat kicks too. A hit: the fast level (0.3 ms up, 30 ms down) above -40 dBFS and at least
    // 1.5 times the slow level (20 ms up, 50 ms down: a roll's hit over the tail of the last one counts) just before it; 40 ms before the next hit may count. The input is
    // delayed by the plug-in latency first: the host plays the plug-in's output that much later than the kick.
    // The curve lasts the chosen period at the song tempo (1/4 at 120 BPM = 0.5 s); a later hit restarts it.
    void processKick (float* l, float* r, int n, const float* scL, const float* scR, int latency, double bpm, double amount,
                      int rate, int shape) noexcept
    {
        amount = std::clamp (amount, 0.0, 1.0);
        latency = std::clamp (latency, 0, (int) scDelay.size() - 1);
        const double periodSamples = beatsOf (rate) * 60.0 / std::max (1.0, bpm) * sr;
        for (int i = 0; i < n; ++i)
        {
            const float in = std::max (std::abs (scL != nullptr ? scL[i] : 0.0f), std::abs (scR != nullptr ? scR[i] : 0.0f));
            scDelay[(size_t) scPos] = in;
            const float x = scDelay[(size_t) ((scPos - latency + (int) scDelay.size()) % (int) scDelay.size())];
            scPos = (scPos + 1) % (int) scDelay.size();
            const double before = slow;
            fast = x > fast ? x + fastUp * (fast - x) : x + fastDown * (fast - x);
            slow = fast > slow ? fast + slowUp * (slow - fast) : fast + slowDown * (slow - fast);
            if (holdoff > 0) --holdoff;
            else if (fast > 0.01 && fast > 1.5 * before + 1.0e-4)
            {
                since = 0.0;
                holdoff = (int) (0.04 * sr);
            }
            const double ph = since / periodSamples;
            since += 1.0;
            const double target = 1.0 - amount * (ph < 1.0 ? depthAt (ph, shape) : 0.0);
            gain += coef * (target - gain);
            if (gain > 0.999999 && target >= 1.0) gain = 1.0;
            l[i] *= (float) gain;
            if (r != nullptr) r[i] *= (float) gain;
        }
    }
    void resetKick() noexcept { scDelay.fill (0.0f); scPos = 0; fast = slow = 0.0; holdoff = 0; since = 1.0e12; }

    std::array<float, 4096> scDelay {};
    int scPos = 0, holdoff = 0;
    double fast = 0.0, slow = 0.0, since = 1.0e12, fastUp = 0.0, fastDown = 0.0, slowUp = 0.0, slowDown = 0.0;

    double sr = 48000.0, coef = 0.01, gain = 1.0;
};
}
