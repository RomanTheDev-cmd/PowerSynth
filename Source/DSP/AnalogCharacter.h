#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// DIGITAL | ANALOGUE (Roman, 2026-10-28): DIGITAL is the exact sound the series always had; ANALOGUE adds what it
// lacks next to analogue gear - a slow, slight pitch drift and a spread from note to note, a little even-harmonic
// warmth and a softer top. Everything is scaled by a 0..1 blend, so DIGITAL stays bit-exact and switching while
// playing does not click. Deterministic: the random sequence restarts with prepare()/reset() (a bounce is repeatable).
// (The same file is in PowerSynth and PowerKick.)
namespace power_analog
{
struct Random
{
    uint32_t s = 0x2545F491u;
    void seed (uint32_t v) noexcept { s = v != 0 ? v : 1u; }
    double next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (double) (s >> 8) * (2.0 / 16777216.0) - 1.0; } // -1..1
};

// Slow wander in cents: a new random target every 80 ms, followed through two one-pole stages of 0.15 s.
struct Drift
{
    double target = 0.0, a = 0.0, b = 0.0, coef = 0.999;
    int countdown = 0, period = 3840;
    void prepare (double sr) noexcept { coef = std::exp (-1.0 / (0.15 * sr)); period = std::max (1, (int) (0.08 * sr)); reset(); }
    void reset() noexcept { target = a = b = 0.0; countdown = 0; }
    double tick (Random& r, double depthCents) noexcept
    {
        if (--countdown <= 0) { countdown = period; target = r.next() * depthCents; }
        a = target + coef * (a - target);
        b = a + coef * (b - a);
        return b;
    }
};

// Gentle even-harmonic warmth for signals of about +/-2 (at full level 2nd harmonic ~-26 dB, 3rd ~-42 dB).
inline double warm (double x) noexcept
{
    const double c = std::clamp (x, -2.5, 2.5);
    return x + 0.06 * c * c - 0.02 * c * c * c;
}

// Stronger warmth for a driven signal (+/-1): 2nd harmonic ~-18 dB, 3rd ~-36 dB at full level - the rounded,
// thicker sound of an analogue drive stage (Roman, 2026-10-28: the first, gentler version was not audible).
inline double warmDrive (double x) noexcept
{
    const double c = std::clamp (x, -2.0, 2.0);
    return x + 0.12 * c * c - 0.04 * c * c * c;
}

struct OnePole
{
    double a = 0.0, z = 0.0;
    void set (double fc, double sr) noexcept { a = std::exp (-2.0 * 3.14159265358979323846 * std::min (fc, 0.45 * sr) / sr); }
    void reset() noexcept { z = 0.0; }
    double tick (double x) noexcept { z = x + a * (z - x); return z; }
};

// 0 = Digital .. 1 = Analogue in 20 ms; exactly 0 or 1 once settled.
struct Blend
{
    double v = 0.0, k = 0.001;
    void prepare (double sr) noexcept { k = 1.0 - std::exp (-1.0 / (0.02 * sr)); v = 0.0; }
    double tick (bool on) noexcept
    {
        const double t = on ? 1.0 : 0.0;
        v += (t - v) * k;
        if (std::abs (v - t) < 1.0e-6) v = t;
        return v;
    }
};
} // namespace power_analog
