"""PowerSynth 2.3 (Claude, 2026-10-29): more leads, pads and a new Textures category. Roman on 2.2: "much better now...
add even more leads, textures and pads". These are the starting archetypes; each is then fitted to a reference of its
category that no existing preset uses (`fit_synth.py extend`), the same way as the 2.2 library, and MASTER is set by
`powersynth_tests --calibrate`.

    python3 extend_library.py     inserts the archetypes into Source/FactoryPresets.inc (skips names already there)

Unlike build_library.py it never rewrites the existing (fitted) presets. Textures go between Pads and Atmospheres,
which is where the browser lists the category."""
import os, re

HERE = os.path.dirname(os.path.abspath(__file__))
INC = os.path.join(HERE, "..", "Source", "FactoryPresets.inc")
CHOICE = {"warpMode", "filterMode", "driveType", "lfoShape", "lfoTarget", "lfoDiv", "delayTime", "voiceMode", "unison",
          "oscAOct", "oscBOct", "oscBSemi", "lfoSync"}
SYNC, BEND, PWM, FOLD = 0, 1, 2, 3                       # WARP
LP24, LP12, BP, HP, COMB = 0, 1, 2, 3, 4                # FILTER
SAT, CLIP, FOLDD, CRUSH = 0, 1, 2, 3                     # DRIVE
SINE, TRI, SAW, SQR, RND = 0, 1, 2, 3, 4                 # LFO shape
T_PITCH, T_CUT, T_SHAPE, T_AMP, T_FM = 0, 1, 2, 3, 4     # LFO target
D1, D2, D4, D8, D16, D32, D4T, D8T, D8D = range(9)       # divisions
HOLLOW, VOXA, VOXO, GROWL, SCREECH, SYNCW, BITTER, METAL, INDUST = 4, 5, 6, 7, 8, 9, 10, 11, 12
TEMPO = dict(lfoSync=1)
MARK = "// 2.3 archetype"

A = []
def arch(cat, group, name, base, variants):
    A.append((cat, group, name, base, variants))

# ---------------- LEADS: hard trance gate, schranz hook, metal FM, legato fold, robot vowels
arch("Leads", "Supersaw", "Trance Gate", dict(oscAShape=9.3, unison=7, detune=.45, oscBLevel=0, warpMode=PWM, warp=.3,
     lfoTarget=T_AMP, lfoShape=SQR, lfoDiv=D16, lfoAmount=.7, filterMode=HP, cutoff=160, resonance=.1, fltEnv=0, driveType=SAT,
     drive=.6, smash=.85, chorus=.3, delayMix=.25, delayTime=D8D, reverbMix=.3, **TEMPO),
     [("I", {}), ("1/8T", dict(lfoDiv=D8T)), ("Bitter", dict(oscAShape=BITTER))])
arch("Leads", "Hook", "Schranz Hook", dict(oscAShape=BITTER, unison=1, oscBLevel=0, voiceMode=1, warpMode=FOLD, warp=.45, pitchEnv=12,
     pitchTime=20, morphEnv=.6, fltDecay=90, fltSustain=.1, filterMode=BP, cutoff=1400, resonance=.35, fltEnv=0, decay=250, sustain=.3,
     release=80, driveType=CLIP, drive=.85, smash=.9, shift=.1, delayMix=.15, delayTime=D16),
     [("I", {}), ("Grit", dict(filterMode=LP24, cutoff=900, resonance=.8, grit=.8, fltEnv=.5)), ("Indust", dict(oscAShape=INDUST))])
arch("Leads", "FM", "Metal Lead", dict(oscAShape=METAL, unison=2, detune=.1, fmAmount=.5, oscBShape=SINE, oscBSemi=7, oscBLevel=0, ring=.3,
     lfoTarget=T_FM, lfoShape=TRI, lfoDiv=D8, lfoAmount=.5, filterMode=HP, cutoff=220, fltEnv=0, driveType=FOLDD, drive=.55, smash=.85,
     reverbMix=.3, **TEMPO),
     [("I", {}), ("Ring", dict(ring=.6)), ("Octave", dict(oscBSemi=12, fmAmount=.65))])
arch("Leads", "Fold", "Glide Fold", dict(oscAShape=SAW, unison=3, detune=.15, oscBLevel=0, voiceMode=2, glide=90, warpMode=FOLD, warp=.6,
     morphEnv=.5, fltDecay=300, fltSustain=.3, filterMode=LP12, cutoff=5000, resonance=.3, fltEnv=0, grit=.5, driveType=SAT, drive=.7,
     smash=.8, delayMix=.2, delayTime=D8),
     [("I", {}), ("Hard", dict(driveType=CLIP, drive=.9, warp=.8))])
arch("Leads", "Vox", "Robot Talk", dict(oscAShape=VOXA, unison=2, detune=.1, oscBLevel=0, lfoTarget=T_SHAPE, lfoShape=RND, lfoDiv=D16,
     lfoAmount=.5, filterMode=BP, cutoff=1800, resonance=.45, fltEnv=0, driveType=CLIP, drive=.7, smash=.85, reverbMix=.2, **TEMPO),
     [("I", {}), ("Fast", dict(lfoDiv=D32))])

# ---------------- PADS: sidechain pump, FM glass, industrial drone, hollow morph
arch("Pads", "Pump", "Pump Pad", dict(oscAShape=HOLLOW, unison=7, detune=.3, oscBLevel=0, lfoTarget=T_AMP, lfoShape=SAW, lfoDiv=D4,
     lfoAmount=.8, attack=150, decay=2000, sustain=.9, release=1200, filterMode=LP12, cutoff=4000, resonance=.15, fltEnv=0, driveType=SAT,
     drive=.4, smash=.9, chorus=.4, reverbMix=.5, reverbSize=.7, **TEMPO),
     [("I", {}), ("Vox", dict(oscAShape=VOXO)), ("1/8", dict(lfoDiv=D8))])
arch("Pads", "FM", "Frozen Glass", dict(oscAShape=SINE, unison=5, detune=.2, fmAmount=.35, oscBShape=METAL, oscBSemi=12, oscBLevel=0,
     lfoTarget=T_FM, lfoRate=.15, lfoAmount=.5, attack=800, decay=3000, sustain=.9, release=2500, filterMode=HP, cutoff=300, fltEnv=0,
     driveType=SAT, drive=.3, smash=.7, shift=.05, chorus=.5, reverbMix=.7, reverbSize=.9),
     [("I", {}), ("Dark", dict(oscBSemi=7, filterMode=LP12, cutoff=2500))])
arch("Pads", "Metal", "Industrial Drone", dict(oscAShape=INDUST, oscAOct=-1, unison=5, detune=.35, oscBLevel=0, filterMode=COMB,
     cutoff=3000, resonance=.6, fltEnv=0, shift=.2, lfoTarget=T_SHAPE, lfoRate=.08, lfoAmount=.4, attack=1500, decay=3000, sustain=.9,
     release=3000, driveType=FOLDD, drive=.45, smash=.85, reverbMix=.6, reverbSize=.85),
     [("I", {}), ("Wide", dict(width=1.0, chorus=.6))])
arch("Pads", "Texture", "Hollow Morph", dict(oscAShape=HOLLOW, unison=5, detune=.25, oscBLevel=0, morphEnv=.7, fltAttack=1500, fltDecay=3000,
     fltSustain=.5, filterMode=BP, cutoff=1200, resonance=.3, fltEnv=.3, attack=700, decay=2500, sustain=.9, release=2000, driveType=SAT,
     drive=.4, smash=.8, chorus=.3, reverbMix=.6, reverbSize=.8),
     [("I", {})])

# ---------------- TEXTURES (new category): machine loops, industrial noise, metal scrape, radio static
arch("Textures", "Machine", "Machine Texture", dict(noise=.6, oscAShape=METAL, oscALevel=.4, oscBLevel=0, filterMode=COMB, cutoff=3000,
     resonance=.85, fltEnv=0, lfoTarget=T_CUT, lfoShape=RND, lfoDiv=D16, lfoAmount=.6, attack=5, decay=2000, sustain=1.0, release=400,
     driveType=CRUSH, drive=.55, smash=.9, shift=.25, reverbMix=.3, delayMix=.2, delayTime=D16, **TEMPO),
     [("I", {}), ("1/8T", dict(lfoDiv=D8T)), ("Metal", dict(noise=.2, fmAmount=.5, oscBShape=INDUST))])
arch("Textures", "Noise", "Industrial Noise", dict(noise=1.0, oscALevel=0, oscBLevel=0, filterMode=BP, cutoff=3000, resonance=.5, fltEnv=0,
     grit=.7, lfoTarget=T_CUT, lfoShape=RND, lfoRate=6, lfoAmount=.7, attack=5, sustain=1.0, release=300, driveType=CRUSH, drive=.7,
     smash=1.0, shift=.4, reverbMix=.2),
     [("I", {}), ("Low", dict(cutoff=600)), ("Gated", dict(lfoTarget=T_AMP, lfoSync=1, lfoDiv=D16, lfoShape=SQR, lfoAmount=.9))])
arch("Textures", "Metal", "Metal Scrape", dict(oscAShape=SCREECH, unison=2, detune=.15, fmAmount=.6, oscBShape=METAL, oscBSemi=5, oscBLevel=0,
     ring=.4, lfoTarget=T_PITCH, lfoShape=RND, lfoRate=9, lfoAmount=.15, filterMode=HP, cutoff=500, fltEnv=0, attack=10, sustain=1.0,
     release=500, driveType=FOLDD, drive=.6, smash=.9, shift=.3, reverbMix=.35),
     [("I", {}), ("Slow", dict(lfoRate=2, lfoAmount=.3))])
arch("Textures", "Noise", "Radio Static", dict(noise=.5, oscAShape=BITTER, oscALevel=.5, oscBLevel=0, oscBSemi=12, ring=.6, filterMode=BP,
     cutoff=2000, resonance=.6, fltEnv=0, lfoTarget=T_SHAPE, lfoShape=RND, lfoDiv=D32, lfoAmount=.5, attack=5, sustain=1.0, release=400,
     driveType=CRUSH, drive=.8, smash=.9, shift=.5, reverbMix=.25, **TEMPO),
     [("I", {}), ("Wide", dict(unison=5, detune=.4, width=1.0))])


def lit(k, v):
    if k in CHOICE: return str(int(round(v)))
    t = "%.4g" % v
    return (t + ".0" if "." not in t and "e" not in t else t) + "f"


ENTRY = re.compile(r'^\{ "([^"]+)", "([^"]+)",')
src = open(INC, encoding="utf-8").read().split("\n")
have = {m.group(1) for m in map(ENTRY.match, src) if m}
added = 0
for cat, group, name, base, variants in A:
    new = []
    for suffix, over in variants:
        n = (name + " " + suffix).strip()
        if n in have:
            continue
        have.add(n)
        p = dict(base); p.update(over); p["master"] = -12.0
        body = ",".join('{"%s",%s}' % (k, lit(k, p[k])) for k in sorted(p))
        new.append('{ "%s", "%s", { %s }, "%s" }, %s' % (n, cat, body, group, MARK))
    if not new:
        continue
    # After the last preset of the category; a new category after the one it follows in the browser.
    after = {"Textures": ("Textures", "Pads")}.get(cat, (cat,))
    at = None
    for c in after:
        rows = [i for i, line in enumerate(src) if (m := ENTRY.match(line)) and m.group(2) == c]
        if rows:
            at = rows[-1] + 1
            break
    assert at is not None, cat
    src[at:at] = new
    added += len(new)
open(INC, "w", encoding="utf-8").write("\n".join(src))
print("added:", added, "presets:", len(have))
