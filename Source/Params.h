#pragma once

#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>

namespace ps
{
namespace id
{
    // OSC A (unison) / OSC B / NOISE / FM
    inline constexpr const char* oscAShape  = "oscAShape";
    inline constexpr const char* oscAOct    = "oscAOct";
    inline constexpr const char* unison     = "unison";
    inline constexpr const char* detune     = "detune";
    inline constexpr const char* oscALevel  = "oscALevel";
    inline constexpr const char* oscBShape  = "oscBShape";
    inline constexpr const char* oscBOct    = "oscBOct";
    inline constexpr const char* oscBSemi   = "oscBSemi";
    inline constexpr const char* oscBFine   = "oscBFine";
    inline constexpr const char* oscBLevel  = "oscBLevel";
    inline constexpr const char* noise      = "noise";
    inline constexpr const char* fmAmount   = "fmAmount";
    // FILTER
    inline constexpr const char* filterMode = "filterMode";
    inline constexpr const char* cutoff     = "cutoff";
    inline constexpr const char* resonance  = "resonance";
    inline constexpr const char* fltEnv     = "fltEnv";
    inline constexpr const char* fltAttack  = "fltAttack";
    inline constexpr const char* fltDecay   = "fltDecay";
    inline constexpr const char* fltSustain = "fltSustain";
    inline constexpr const char* fltRelease = "fltRelease";
    // AMP
    inline constexpr const char* attack     = "attack";
    inline constexpr const char* decay      = "decay";
    inline constexpr const char* sustain    = "sustain";
    inline constexpr const char* release    = "release";
    // LFO
    inline constexpr const char* lfoShape   = "lfoShape";
    inline constexpr const char* lfoTarget  = "lfoTarget";
    inline constexpr const char* lfoRate    = "lfoRate";
    inline constexpr const char* lfoSync    = "lfoSync";
    inline constexpr const char* lfoDiv     = "lfoDiv";
    inline constexpr const char* lfoAmount  = "lfoAmount";
    // DRIVE / FX
    inline constexpr const char* driveType  = "driveType";
    inline constexpr const char* drive      = "drive";
    inline constexpr const char* chorus     = "chorus";
    inline constexpr const char* delayMix   = "delayMix";
    inline constexpr const char* delayTime  = "delayTime";
    inline constexpr const char* delayFb    = "delayFb";
    inline constexpr const char* reverbMix  = "reverbMix";
    inline constexpr const char* reverbSize = "reverbSize";
    // GLOBAL
    inline constexpr const char* voiceMode  = "voiceMode";
    inline constexpr const char* glide      = "glide";
    inline constexpr const char* width      = "width";
    inline constexpr const char* master     = "master";
    inline constexpr const char* velSens    = "velSens";
    inline constexpr const char* bendRange  = "bendRange";
    inline constexpr const char* oversample = "oversample";
    inline constexpr const char* analog     = "analog";
    // 2.0 (2026-10-29): modern character
    inline constexpr const char* warpMode   = "warpMode";
    inline constexpr const char* warp       = "warp";
    inline constexpr const char* ring       = "ring";
    inline constexpr const char* pitchEnv   = "pitchEnv";
    inline constexpr const char* pitchTime  = "pitchTime";
    inline constexpr const char* grit       = "grit";
    inline constexpr const char* smash      = "smash";
    inline constexpr const char* shift      = "shift";
    inline constexpr const char* punch      = "punch";
    // SIDECHAIN (3.1, 2026-11-12), as in PowerBass 1.3: an instance setting, like OVERSAMPLE
    inline constexpr const char* scAmount   = "scAmount";
    inline constexpr const char* scRate     = "scRate";
    inline constexpr const char* scShape    = "scShape";
    inline constexpr const char* scSource   = "scSource";
    inline constexpr const char* morphEnv   = "morphEnv";
}

enum class FilterMode { lp24, lp12, bp, hp, comb };
enum class WarpMode { sync, bend, pwm, fold };
// Oscillator shapes: 0 sine, 1 triangle, 2 saw, 3 square (classic, polyBLEP), 4..12 the hidden wavetables (WaveBank).
inline constexpr float maxShape = 12.0f;
const juce::StringArray& shapeNames();
enum class DriveType { saturate, clip, fold, crush };
enum class LfoShape { sine, triangle, saw, square, random };
enum class LfoTarget { pitch, cutoff, shape, amp, fm };
enum class VoiceMode { poly, mono, legato };

// LFO / delay note values at the host tempo, in beats.
inline constexpr int numDivisions = 9;
inline constexpr float divisionBeats[numDivisions] = { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f, 2.0f / 3.0f, 1.0f / 3.0f, 0.75f };
inline const juce::StringArray& divisionNames()
{
    static const juce::StringArray n { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/8D" };
    return n;
}

// The sound of one instance, read once per block. The defaults here ARE the parameter defaults (createLayout reads
// them), so a preset that leaves a parameter out and a fresh instance agree.
struct SynthParams
{
    float oscAShape = 2.0f;       // 0 sine .. 1 triangle .. 2 saw .. 3 square (morph)
    int oscAOct = 0;              // -2..+2 octaves
    int unison = 3;               // 1..7 voices of OSC A
    float detune = 0.25f;         // unison spread 0..1 (up to +/-50 cents)
    float oscALevel = 1.0f;
    float oscBShape = 3.0f;
    int oscBOct = 0;
    int oscBSemi = 0;             // -12..+12
    float oscBFine = 7.0f;        // cents -50..+50
    float oscBLevel = 0.4f;
    float noise = 0.0f;
    float fmAmount = 0.0f;        // OSC B modulates the phase of OSC A
    FilterMode filterMode = FilterMode::lp24;
    float cutoffHz = 3500.0f;
    float resonance = 0.25f;
    float fltEnv = 0.3f;          // -1..1 (+/- 6 octaves at full)
    float fltAttackMs = 2.0f, fltDecayMs = 400.0f, fltSustain = 0.3f, fltReleaseMs = 300.0f;
    float attackMs = 5.0f, decayMs = 600.0f, sustain = 0.8f, releaseMs = 250.0f;
    LfoShape lfoShape = LfoShape::sine;
    LfoTarget lfoTarget = LfoTarget::cutoff;
    float lfoRateHz = 2.0f;
    bool lfoSync = false;
    int lfoDiv = 3;               // 1/8
    float lfoAmount = 0.0f;
    DriveType driveType = DriveType::saturate;
    float drive = 0.2f;
    float chorus = 0.2f;
    float delayMix = 0.0f;
    int delayTime = 3;            // 1/8
    float delayFb = 0.35f;
    float reverbMix = 0.15f;
    float reverbSize = 0.6f;
    VoiceMode voiceMode = VoiceMode::poly;
    float glideMs = 0.0f;
    float width = 0.6f;
    float masterDb = -9.0f;
    float velSens = 0.5f;
    float bendRange = 2.0f;
    int oversampleIndex = 1;      // drive oversampling: 0 = 2x, 1 = 4x, 2 = 8x
    bool analog = false;          // DIGITAL | ANALOGUE
    // 2.0: all neutral by default (an older project sounds as before)
    WarpMode warpMode = WarpMode::sync;
    float warp = 0.0f;            // OSC A: SYNC (hard sync), BEND (phase bend), PWM (hoover), FOLD (wave folding)
    float ring = 0.0f;            // OSC A x OSC B
    float pitchEnv = 0.0f;        // semitones at the start of a note
    float pitchTimeMs = 80.0f;    // time to fall to about 5 %
    float grit = 0.0f;            // drive into (and inside) the filter
    float smash = 0.0f;           // 3-band upward + downward compression (OTT-like density)
    float shift = 0.0f;           // a small frequency shift before the drive
    float punch = 0.0f;           // 3.0: the start of every note up to 6 dB louder for about 25 ms, after the drive
    float scAmount = 0.0f;        // SIDECHAIN depth (0 = off)
    int scRate = 0, scShape = 0, scSource = 0;   // 1/4 1/8 1/2; CLASSIC SHORT LONG HARD; TEMPO KICK
    float morphEnv = 0.0f;        // -1..1: the filter envelope moves SHAPE (3 positions at full) and WARP
    double bpm = 120.0;           // host tempo (not a parameter; set by the processor)
};

const juce::StringArray& allParamIds();
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
SynthParams readParams (const juce::AudioProcessorValueTreeState& state);
// The one mapping from plain values to SynthParams; get (id, default) returns the plain value of a parameter.
SynthParams paramsFrom (const std::function<float (const char*, float)>& get);
}
