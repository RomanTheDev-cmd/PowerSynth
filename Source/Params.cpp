#include "Params.h"

namespace ps
{
namespace
{
using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;
using Attr = juce::AudioParameterFloatAttributes;
using Range = juce::NormalisableRange<float>;
using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

juce::String pct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
juce::String ms (float v, int) { return v < 1000.0f ? juce::String (v, v < 10.0f ? 1 : 0) + " ms" : juce::String (v / 1000.0f, 2) + " s"; }
juce::String hz (float v, int) { return v < 1000.0f ? juce::String (juce::roundToInt (v)) + " Hz" : juce::String (v / 1000.0f, 1) + " kHz"; }
juce::String integer (float v, int) { return juce::String (juce::roundToInt (v)); }
juce::String signedInt (float v, int) { const int i = juce::roundToInt (v); return (i > 0 ? "+" : "") + juce::String (i); }

Range skewed (float lo, float hi, float centre)
{
    Range r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

void unit (Layout& l, const char* pid, const char* name, float def)
{
    l.add (std::make_unique<APF> (juce::ParameterID { pid, 1 }, name, Range (0.0f, 1.0f), def, Attr().withStringFromValueFunction (pct)));
}

void flt (Layout& l, const char* pid, const char* name, Range r, float def, std::function<juce::String (float, int)> text)
{
    l.add (std::make_unique<APF> (juce::ParameterID { pid, 1 }, name, r, def, Attr().withStringFromValueFunction (std::move (text))));
}

void choice (Layout& l, const char* pid, const char* name, const juce::StringArray& items, int def)
{
    l.add (std::make_unique<APC> (juce::ParameterID { pid, 1 }, name, items, def));
}

void envelope (Layout& l, const char* a, const char* d, const char* s, const char* r, const char* prefix,
               float da, float dd, float ds, float dr)
{
    const juce::String p (prefix);
    flt (l, a, (p + "Attack").toRawUTF8(), skewed (0.5f, 8000.0f, 200.0f), da, ms);
    flt (l, d, (p + "Decay").toRawUTF8(), skewed (5.0f, 10000.0f, 600.0f), dd, ms);
    unit (l, s, (p + "Sustain").toRawUTF8(), ds);
    flt (l, r, (p + "Release").toRawUTF8(), skewed (5.0f, 10000.0f, 600.0f), dr, ms);
}
}

const juce::StringArray& shapeNames()
{
    static const juce::StringArray n { "SINE", "TRI", "SAW", "SQUARE", "HOLLOW", "VOX A", "VOX O", "GROWL", "SCREECH", "SYNC", "BITTER", "METAL", "INDUST" };
    return n;
}

const juce::StringArray& allParamIds()
{
    static const juce::StringArray ids { id::oscAShape, id::oscAOct, id::unison, id::detune, id::oscALevel, id::oscBShape, id::oscBOct, id::oscBSemi,
                                         id::oscBFine, id::oscBLevel, id::noise, id::fmAmount, id::filterMode, id::cutoff, id::resonance,
                                         id::fltEnv, id::fltAttack, id::fltDecay, id::fltSustain, id::fltRelease, id::attack, id::decay,
                                         id::sustain, id::release, id::lfoShape, id::lfoTarget, id::lfoRate, id::lfoSync, id::lfoDiv,
                                         id::lfoAmount, id::driveType, id::drive, id::chorus, id::delayMix, id::delayTime, id::delayFb,
                                         id::reverbMix, id::reverbSize, id::voiceMode, id::glide, id::width, id::master, id::velSens,
                                         id::bendRange, id::oversample, id::analog, id::warpMode, id::warp, id::ring,
                                         id::pitchEnv, id::pitchTime, id::grit, id::smash, id::shift, id::morphEnv, id::punch };
    return ids;
}

// Defaults: the SynthParams member initialisers (one place).
juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    const SynthParams d;
    Layout l;
    auto shapeText = [] (float v, int)
    {
        const auto& names = shapeNames();
        const int i = juce::jlimit (0, (int) maxShape, juce::roundToInt (v));
        return std::abs (v - (float) i) < 0.05f ? names[i]
                                                 : names[(int) v] + juce::String::fromUTF8 ("\xe2\x86\x92") + names[std::min ((int) maxShape, (int) v + 1)];
    };
    flt (l, id::oscAShape, "Osc A Shape", Range (0.0f, maxShape), d.oscAShape, shapeText);
    flt (l, id::oscAOct, "Osc A Octave", Range (-2.0f, 2.0f, 1.0f), (float) d.oscAOct, signedInt);
    flt (l, id::unison, "Unison", Range (1.0f, 7.0f, 1.0f), (float) d.unison, integer);
    unit (l, id::detune, "Detune", d.detune);
    unit (l, id::oscALevel, "Osc A Level", d.oscALevel);
    flt (l, id::oscBShape, "Osc B Shape", Range (0.0f, maxShape), d.oscBShape, shapeText);
    flt (l, id::oscBOct, "Osc B Octave", Range (-2.0f, 2.0f, 1.0f), (float) d.oscBOct, signedInt);
    flt (l, id::oscBSemi, "Osc B Semitones", Range (-12.0f, 12.0f, 1.0f), (float) d.oscBSemi, signedInt);
    flt (l, id::oscBFine, "Osc B Fine", Range (-50.0f, 50.0f), d.oscBFine, [] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " ct"; });
    unit (l, id::oscBLevel, "Osc B Level", d.oscBLevel);
    unit (l, id::noise, "Noise", d.noise);
    unit (l, id::fmAmount, "FM B > A", d.fmAmount);

    choice (l, id::filterMode, "Filter Mode", { "LP 24", "LP 12", "BP", "HP", "Comb" }, (int) d.filterMode);
    flt (l, id::cutoff, "Cutoff", skewed (30.0f, 18000.0f, 1200.0f), d.cutoffHz, hz);
    unit (l, id::resonance, "Resonance", d.resonance);
    flt (l, id::fltEnv, "Filter Env", Range (-1.0f, 1.0f), d.fltEnv, [] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (juce::roundToInt (v * 100.0f)) + " %"; });
    envelope (l, id::fltAttack, id::fltDecay, id::fltSustain, id::fltRelease, "Filter ", d.fltAttackMs, d.fltDecayMs, d.fltSustain, d.fltReleaseMs);
    envelope (l, id::attack, id::decay, id::sustain, id::release, "", d.attackMs, d.decayMs, d.sustain, d.releaseMs);

    choice (l, id::lfoShape, "LFO Shape", { "Sine", "Triangle", "Saw", "Square", "Random" }, (int) d.lfoShape);
    choice (l, id::lfoTarget, "LFO Target", { "Pitch", "Cutoff", "Shape", "Amp", "FM" }, (int) d.lfoTarget);
    flt (l, id::lfoRate, "LFO Rate", skewed (0.02f, 30.0f, 2.0f), d.lfoRateHz, [] (float v, int) { return juce::String (v, v < 10.0f ? 2 : 1) + " Hz"; });
    l.add (std::make_unique<APB> (juce::ParameterID { id::lfoSync, 1 }, "LFO Sync", d.lfoSync));
    choice (l, id::lfoDiv, "LFO Division", divisionNames(), d.lfoDiv);
    unit (l, id::lfoAmount, "LFO Amount", d.lfoAmount);

    choice (l, id::driveType, "Drive Type", { "Saturate", "Clip", "Fold", "Crush" }, (int) d.driveType);
    unit (l, id::drive, "Drive", d.drive);
    unit (l, id::chorus, "Chorus", d.chorus);
    unit (l, id::delayMix, "Delay", d.delayMix);
    choice (l, id::delayTime, "Delay Time", divisionNames(), d.delayTime);
    flt (l, id::delayFb, "Delay Feedback", Range (0.0f, 0.9f), d.delayFb, pct);
    unit (l, id::reverbMix, "Reverb", d.reverbMix);
    unit (l, id::reverbSize, "Reverb Size", d.reverbSize);

    choice (l, id::voiceMode, "Voice Mode", { "Poly", "Mono", "Legato" }, (int) d.voiceMode);
    flt (l, id::glide, "Glide", skewed (0.0f, 2000.0f, 150.0f), d.glideMs, ms);
    unit (l, id::width, "Width", d.width);
    flt (l, id::master, "Master", Range (-30.0f, 12.0f, 0.1f), d.masterDb, [] (float v, int) { return juce::String (v, 1) + " dB"; });
    unit (l, id::velSens, "Velocity Sens", d.velSens);
    flt (l, id::bendRange, "Bend Range", Range (0.0f, 12.0f, 1.0f), d.bendRange, [] (float v, int) { return juce::String (juce::roundToInt (v)) + " st"; });
    choice (l, id::oversample, "Oversampling", { "2x", "4x", "8x" }, d.oversampleIndex);
    // DIGITAL | ANALOGUE, as in PowerKick and PowerBass: Analogue adds drift, spread, warmth and a softer top.
    choice (l, id::analog, "Digital/Analogue", { "Digital", "Analogue" }, d.analog ? 1 : 0);
    // 2.0 (2026-10-29)
    choice (l, id::warpMode, "Warp Mode", { "Sync", "Bend", "PWM", "Fold" }, (int) d.warpMode);
    unit (l, id::warp, "Warp", d.warp);
    unit (l, id::ring, "Ring", d.ring);
    flt (l, id::pitchEnv, "Pitch Env", Range (-24.0f, 48.0f, 0.1f), d.pitchEnv, [] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " st"; });
    flt (l, id::pitchTime, "Pitch Time", skewed (2.0f, 2000.0f, 120.0f), d.pitchTimeMs, ms);
    unit (l, id::grit, "Grit", d.grit);
    unit (l, id::smash, "Smash", d.smash);
    unit (l, id::shift, "Shift", d.shift);
    unit (l, id::punch, "Punch", d.punch);   // 3.0 (2026-11-12)
    flt (l, id::morphEnv, "Morph Env", Range (-1.0f, 1.0f), d.morphEnv, [] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (juce::roundToInt (v * 100.0f)) + " %"; });
    return l;
}

SynthParams paramsFrom (const std::function<float (const char*, float)>& get)
{
    SynthParams p;
    auto i = [&get] (const char* pid, int def) { return juce::roundToInt (get (pid, (float) def)); };
    p.oscAShape = get (id::oscAShape, p.oscAShape);
    p.oscAOct = juce::jlimit (-2, 2, i (id::oscAOct, p.oscAOct));
    p.unison = juce::jlimit (1, 7, i (id::unison, p.unison));
    p.detune = get (id::detune, p.detune);
    p.oscALevel = get (id::oscALevel, p.oscALevel);
    p.oscBShape = get (id::oscBShape, p.oscBShape);
    p.oscBOct = juce::jlimit (-2, 2, i (id::oscBOct, p.oscBOct));
    p.oscBSemi = juce::jlimit (-12, 12, i (id::oscBSemi, p.oscBSemi));
    p.oscBFine = get (id::oscBFine, p.oscBFine);
    p.oscBLevel = get (id::oscBLevel, p.oscBLevel);
    p.noise = get (id::noise, p.noise);
    p.fmAmount = get (id::fmAmount, p.fmAmount);
    p.filterMode = (FilterMode) juce::jlimit (0, 4, i (id::filterMode, (int) p.filterMode));
    p.cutoffHz = get (id::cutoff, p.cutoffHz);
    p.resonance = get (id::resonance, p.resonance);
    p.fltEnv = get (id::fltEnv, p.fltEnv);
    p.fltAttackMs = get (id::fltAttack, p.fltAttackMs);
    p.fltDecayMs = get (id::fltDecay, p.fltDecayMs);
    p.fltSustain = get (id::fltSustain, p.fltSustain);
    p.fltReleaseMs = get (id::fltRelease, p.fltReleaseMs);
    p.attackMs = get (id::attack, p.attackMs);
    p.decayMs = get (id::decay, p.decayMs);
    p.sustain = get (id::sustain, p.sustain);
    p.releaseMs = get (id::release, p.releaseMs);
    p.lfoShape = (LfoShape) juce::jlimit (0, 4, i (id::lfoShape, (int) p.lfoShape));
    p.lfoTarget = (LfoTarget) juce::jlimit (0, 4, i (id::lfoTarget, (int) p.lfoTarget));
    p.lfoRateHz = get (id::lfoRate, p.lfoRateHz);
    p.lfoSync = get (id::lfoSync, p.lfoSync ? 1.0f : 0.0f) > 0.5f;
    p.lfoDiv = juce::jlimit (0, numDivisions - 1, i (id::lfoDiv, p.lfoDiv));
    p.lfoAmount = get (id::lfoAmount, p.lfoAmount);
    p.driveType = (DriveType) juce::jlimit (0, 3, i (id::driveType, (int) p.driveType));
    p.drive = get (id::drive, p.drive);
    p.chorus = get (id::chorus, p.chorus);
    p.delayMix = get (id::delayMix, p.delayMix);
    p.delayTime = juce::jlimit (0, numDivisions - 1, i (id::delayTime, p.delayTime));
    p.delayFb = get (id::delayFb, p.delayFb);
    p.reverbMix = get (id::reverbMix, p.reverbMix);
    p.reverbSize = get (id::reverbSize, p.reverbSize);
    p.voiceMode = (VoiceMode) juce::jlimit (0, 2, i (id::voiceMode, (int) p.voiceMode));
    p.glideMs = get (id::glide, p.glideMs);
    p.width = get (id::width, p.width);
    p.masterDb = get (id::master, p.masterDb);
    p.velSens = get (id::velSens, p.velSens);
    p.bendRange = get (id::bendRange, p.bendRange);
    p.oversampleIndex = juce::jlimit (0, 2, i (id::oversample, p.oversampleIndex));
    p.analog = get (id::analog, p.analog ? 1.0f : 0.0f) > 0.5f;
    p.warpMode = (WarpMode) juce::jlimit (0, 3, i (id::warpMode, (int) p.warpMode));
    p.warp = get (id::warp, p.warp);
    p.ring = get (id::ring, p.ring);
    p.pitchEnv = get (id::pitchEnv, p.pitchEnv);
    p.pitchTimeMs = get (id::pitchTime, p.pitchTimeMs);
    p.grit = get (id::grit, p.grit);
    p.smash = get (id::smash, p.smash);
    p.shift = get (id::shift, p.shift);
    p.punch = get (id::punch, p.punch);
    p.morphEnv = get (id::morphEnv, p.morphEnv);
    return p;
}

SynthParams readParams (const juce::AudioProcessorValueTreeState& state)
{
    return paramsFrom ([&state] (const char* pid, float def)
    {
        if (auto* v = state.getRawParameterValue (pid)) return v->load();
        return def;
    });
}
}
