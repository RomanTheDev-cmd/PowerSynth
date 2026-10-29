"""PowerSynth factory library 2.1 (Claude, 2026-10-29). Roman on 2.0: "make normal modern examples, delete Classic;
keep the classic engine only if a modern preset is built on it, not just a silly violin; everything sounds too
classical". So: no plain saw/square through a resonant low-pass; the sounds are built on the hidden wavetables, SYNC/PWM/
FOLD/BEND, FM with table modulators, movement inside the note (MORPH zaps, LFO in tempo on SHAPE and FM), and modern
processing: HP/BP/COMB filters, hard drive, SMASH (OTT), SHIFT. Classic shapes appear only as FM carriers.
MASTER is set afterwards by `powersynth_tests --calibrate`.     python3 build_library.py   rewrites Source/FactoryPresets.inc"""
import os

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

A = []
def arch(cat, group, name, base, variants):
    A.append((cat, group, name, base, variants))

# ---------------- LEADS
arch("Leads", "Screech", "Razor", dict(oscAShape=SCREECH, unison=3, detune=.18, oscBLevel=0, warpMode=SYNC, warp=.5, morphEnv=.5, fltDecay=220,
     fltSustain=.3, fltEnv=0, lfoTarget=T_SHAPE, lfoShape=TRI, lfoDiv=D16, lfoAmount=.25, filterMode=HP, cutoff=180, resonance=.1, driveType=CLIP,
     drive=.7, smash=.8, shift=.12, reverbMix=.25, delayMix=.15, **TEMPO),
     [("I", {}), ("II", dict(warp=.75, lfoDiv=D8)), ("Fold", dict(warpMode=FOLD, warp=.4)), ("Mono", dict(voiceMode=2, glide=50, unison=5))])
arch("Leads", "Supersaw", "Rave Saw Wall", dict(oscAShape=9.4, unison=7, detune=.5, oscBLevel=0, warpMode=PWM, warp=.25, lfoTarget=T_SHAPE,
     lfoAmount=.15, lfoRate=.3, filterMode=HP, cutoff=150, resonance=.1, fltEnv=0, driveType=SAT, drive=.65, smash=.85, chorus=.35,
     reverbMix=.35, delayMix=.2, shift=.08),
     [("I", {}), ("Bitter", dict(oscAShape=BITTER)), ("Hard", dict(driveType=CLIP, drive=.8)), ("Rising", dict(pitchEnv=-12, pitchTime=400))])
arch("Leads", "FM", "FM Growl", dict(oscAShape=GROWL, unison=2, detune=.1, fmAmount=.55, oscBShape=INDUST, oscBOct=-1, oscBLevel=0,
     lfoTarget=T_FM, lfoShape=SAW, lfoDiv=D16, lfoAmount=.9, filterMode=BP, cutoff=1500, resonance=.3, fltEnv=0, driveType=SAT, drive=.75,
     smash=.85, reverbMix=.15, **TEMPO),
     [("1/16", {}), ("1/8", dict(lfoDiv=D8)), ("Metal", dict(oscBShape=METAL, oscBSemi=7, oscBOct=0)), ("Wide", dict(unison=5, detune=.3))])
arch("Leads", "Sync", "Industrial Siren", dict(oscAShape=INDUST, unison=2, detune=.12, oscBShape=0, oscBSemi=7, oscBLevel=0, warpMode=SYNC,
     warp=.4, lfoTarget=T_SHAPE, lfoRate=1.5, lfoAmount=.4, pitchEnv=12, pitchTime=120, ring=.25, filterMode=HP, cutoff=200, fltEnv=0,
     driveType=FOLDD, drive=.6, smash=.75, shift=.2, reverbMix=.3),
     [("I", {}), ("Tempo", dict(lfoSync=1, lfoDiv=D4)), ("Dirty", dict(driveType=CRUSH, drive=.5))])
arch("Leads", "Screech", "Vox Scream", dict(oscAShape=VOXA, unison=3, detune=.15, oscBLevel=0, morphEnv=.8, fltDecay=400, fltSustain=.2,
     warpMode=BEND, warp=.3, filterMode=LP12, cutoff=6000, resonance=.2, fltEnv=.2, driveType=CLIP, drive=.7, smash=.8, reverbMix=.25),
     [("I", {}), ("Down", dict(morphEnv=-.8, oscAShape=GROWL)), ("Talk", dict(lfoTarget=T_SHAPE, lfoDiv=D8, lfoAmount=.4, lfoSync=1))])

# ---------------- RAVE & HOOVER
arch("Rave & Hoover", "Hoover", "Hoover 2000", dict(oscAShape=SYNCW, unison=7, detune=.55, warpMode=PWM, warp=.5, lfoTarget=T_SHAPE,
     lfoAmount=.4, lfoRate=.7, pitchEnv=-12, pitchTime=300, voiceMode=1, glide=140, oscBShape=BITTER, oscBOct=-1, oscBLevel=.5,
     filterMode=HP, cutoff=120, fltEnv=0, chorus=.6, driveType=SAT, drive=.55, smash=.8, reverbMix=.3),
     [("I", {}), ("Legato", dict(voiceMode=2)), ("Metal", dict(warpMode=FOLD, warp=.4, ring=.2)), ("Dark", dict(filterMode=LP12, cutoff=2500, grit=.4))])
arch("Rave & Hoover", "Rave", "Rave Signal X", dict(oscAShape=BITTER, unison=3, detune=.12, oscBShape=BITTER, oscBSemi=7, oscBLevel=.5,
     pitchEnv=12, pitchTime=35, morphEnv=.4, fltDecay=150, fltSustain=0, filterMode=HP, cutoff=250, fltEnv=0, driveType=CLIP, drive=.6,
     smash=.8, delayMix=.3, delayTime=D8D),
     [("I", {}), ("Ring", dict(ring=.4)), ("Shift", dict(shift=.25))])
arch("Rave & Hoover", "Rave", "Laser", dict(oscAShape=SYNCW, unison=3, detune=.2, oscBLevel=0, pitchEnv=36, pitchTime=60, warpMode=SYNC, warp=.6,
     morphEnv=.6, fltDecay=200, fltSustain=.2, filterMode=HP, cutoff=200, fltEnv=0, driveType=CLIP, drive=.6, smash=.7, reverbMix=.3,
     delayMix=.25),
     [("I", {}), ("Down", dict(pitchEnv=-24, pitchTime=200))])

# ---------------- ACID
arch("Acid", "Acid", "Acid Razor", dict(oscAShape=BITTER, unison=1, oscBLevel=0, filterMode=LP24, cutoff=450, resonance=.88, fltEnv=.7,
     fltDecay=160, fltSustain=0, grit=.85, morphEnv=.4, warpMode=FOLD, warp=.2, voiceMode=2, glide=65, decay=400, sustain=.6,
     driveType=FOLDD, drive=.55, smash=.55, delayMix=.2, delayTime=D8),
     [("I", {}), ("Sync", dict(oscAShape=SYNCW, warpMode=SYNC, warp=.3)), ("Crushed", dict(driveType=CRUSH, drive=.5)),
      ("Screech", dict(oscAShape=SCREECH, resonance=.8)), ("Growl", dict(oscAShape=GROWL))])

# ---------------- PADS
arch("Pads", "Texture", "OTT Texture", dict(oscAShape=11.5, unison=7, detune=.3, oscBLevel=0, lfoTarget=T_SHAPE, lfoAmount=.35, lfoRate=.12,
     warpMode=BEND, warp=.2, attack=600, decay=2000, sustain=.9, release=1800, filterMode=HP, cutoff=200, fltEnv=0, driveType=SAT,
     drive=.45, smash=.9, shift=.1, chorus=.5, reverbMix=.6, reverbSize=.8),
     [("I", {}), ("Vox", dict(oscAShape=5.5)), ("Growl", dict(oscAShape=7.3, fmAmount=.2, oscBShape=BITTER)),
      ("Gated", dict(lfoTarget=T_AMP, lfoSync=1, lfoDiv=D16, lfoAmount=.6, lfoShape=SQR, attack=200))])
arch("Pads", "Vox", "Dark Choir Grit", dict(oscAShape=VOXO, unison=5, detune=.25, oscBLevel=0, lfoTarget=T_SHAPE, lfoAmount=.3, lfoRate=.2,
     attack=900, decay=2000, sustain=.9, release=2000, filterMode=LP12, cutoff=3500, fltEnv=0, grit=.4, drive=.5, smash=.7, reverbMix=.65,
     reverbSize=.75, chorus=.3),
     [("I", {}), ("II", dict(oscAShape=5.2)), ("Shifted", dict(shift=.25))])

# ---------------- ATMOSPHERES
arch("Atmospheres", "Machine", "Machine Room", dict(noise=.7, oscAShape=METAL, oscALevel=.3, oscBLevel=0, filterMode=COMB, cutoff=4000,
     resonance=.9, fltEnv=0, lfoTarget=T_CUT, lfoSync=1, lfoDiv=D1, lfoAmount=.5, lfoShape=RND, attack=1200, decay=2000, sustain=.9,
     release=3000, driveType=CRUSH, drive=.5, smash=.85, shift=.3, reverbMix=.6, reverbSize=.8),
     [("I", {}), ("II", dict(cutoff=2000, lfoDiv=D2)), ("Clean", dict(driveType=SAT, drive=.4, shift=.1))])
arch("Atmospheres", "Metal", "Distorted Abyss", dict(oscAShape=INDUST, oscAOct=-1, unison=3, detune=.2, fmAmount=.4, oscBShape=METAL,
     oscBSemi=7, oscBLevel=0, ring=.35, filterMode=LP12, cutoff=2500, fltEnv=0, lfoTarget=T_FM, lfoRate=.1, lfoAmount=.8, attack=2000,
     decay=3000, sustain=.9, release=4000, driveType=FOLDD, drive=.65, smash=.85, reverbMix=.75, reverbSize=.85),
     [("I", {}), ("Comb", dict(filterMode=COMB, cutoff=5000, resonance=.8)), ("Shift", dict(shift=.35))])
arch("Atmospheres", "Noise", "Rhythmic Machine", dict(noise=1.0, oscALevel=0, oscBLevel=0, filterMode=BP, cutoff=2200, resonance=.6, fltEnv=0,
     lfoTarget=T_AMP, lfoSync=1, lfoDiv=D16, lfoShape=SQR, lfoAmount=.9, grit=.6, driveType=CRUSH, drive=.6, smash=.7, shift=.3, reverbMix=.25),
     [("1/16", {}), ("1/8T", dict(lfoDiv=D8T, cutoff=1200)), ("Comb", dict(filterMode=COMB, resonance=.85, cutoff=6000))])

# ---------------- STABS
arch("Stabs", "Shot", "Schranz Shot X", dict(oscAShape=BITTER, unison=3, detune=.15, oscBLevel=0, pitchEnv=30, pitchTime=35, morphEnv=.7,
     fltDecay=120, fltSustain=0, warpMode=FOLD, warp=.35, noise=.2, filterMode=HP, cutoff=150, fltEnv=0, decay=260, sustain=0, release=180,
     driveType=CLIP, drive=.8, smash=.9, reverbMix=.35, shift=.15),
     [("I", {}), ("Indust", dict(oscAShape=INDUST)), ("Sync", dict(warpMode=SYNC, warp=.6)), ("Low", dict(oscAOct=-1))])
arch("Stabs", "Chord", "Modern Rave Chord", dict(oscAShape=9.6, unison=7, detune=.35, oscBLevel=0, pitchEnv=12, pitchTime=25, morphEnv=.5,
     fltDecay=180, fltSustain=0, decay=380, sustain=0, release=250, filterMode=HP, cutoff=180, fltEnv=0, driveType=SAT, drive=.6, smash=.85,
     reverbMix=.4, delayMix=.2),
     [("I", {}), ("Bitter", dict(oscAShape=BITTER)), ("Vox", dict(oscAShape=VOXA))])
arch("Stabs", "Metal", "Metal Impact", dict(oscAShape=METAL, unison=2, fmAmount=.7, oscBShape=INDUST, oscBSemi=7, oscBLevel=0, ring=.45,
     filterMode=COMB, resonance=.85, cutoff=8000, fltEnv=0, pitchEnv=24, pitchTime=30, decay=350, sustain=0, release=220, driveType=FOLDD,
     drive=.6, smash=.8, shift=.3, reverbMix=.35),
     [("I", {}), ("II", dict(fmAmount=.9, ring=.25)), ("Low", dict(oscAOct=-1, cutoff=4000))])
arch("Stabs", "Shot", "Zap Shot", dict(oscAShape=SCREECH, unison=3, detune=.2, oscBLevel=0, warpMode=SYNC, warp=.3, morphEnv=1.0, fltDecay=90,
     fltSustain=0, pitchEnv=24, pitchTime=50, filterMode=HP, cutoff=150, fltEnv=0, decay=300, sustain=0, release=200, driveType=CLIP,
     drive=.7, smash=.85, reverbMix=.3),
     [("I", {}), ("Growl", dict(oscAShape=GROWL)), ("Fold", dict(warpMode=FOLD, warp=.5))])

# ---------------- ARPS & PLUCKS
arch("Arps & Plucks", "Zap", "Zap Pluck", dict(oscAShape=SYNCW, unison=2, detune=.1, oscBLevel=0, warpMode=SYNC, warp=.2, morphEnv=.9,
     fltDecay=110, fltSustain=0, decay=220, sustain=0, release=120, filterMode=HP, cutoff=150, fltEnv=0, driveType=CLIP, drive=.55,
     smash=.8, delayMix=.3, delayTime=D8),
     [("I", {}), ("Growl", dict(oscAShape=GROWL)), ("Vox", dict(oscAShape=VOXA)), ("Fold", dict(warpMode=FOLD, warp=.3))])
arch("Arps & Plucks", "FM", "FM Bell Grit", dict(oscAShape=0, unison=1, fmAmount=.65, oscBShape=METAL, oscBSemi=12, oscBLevel=0, lfoTarget=T_FM,
     lfoSync=1, lfoDiv=D16, lfoAmount=.4, filterMode=HP, cutoff=200, fltEnv=0, decay=350, sustain=0, release=220, driveType=CLIP, drive=.5,
     smash=.75, shift=.1, delayMix=.3, reverbMix=.3),
     [("I", {}), ("Fifth", dict(oscBSemi=7)), ("Comb", dict(filterMode=COMB, resonance=.75, cutoff=7000))])
arch("Arps & Plucks", "Zap", "Seq Growl", dict(oscAShape=GROWL, unison=2, detune=.08, fmAmount=.4, oscBShape=BITTER, oscBOct=-1, oscBLevel=0,
     morphEnv=.6, fltDecay=140, fltSustain=0, decay=180, sustain=.1, release=100, filterMode=BP, cutoff=1800, resonance=.4, fltEnv=.2,
     driveType=SAT, drive=.7, smash=.85, delayMix=.2, delayTime=D16),
     [("I", {}), ("Hard", dict(driveType=CLIP, drive=.8)), ("Wide", dict(unison=5, detune=.25))])


def lit(k, v):
    if k in CHOICE: return str(int(round(v)))
    t = "%.4g" % v
    return (t + ".0" if "." not in t and "e" not in t else t) + "f"

lines, names = [], set()
for cat, group, name, base, variants in A:
    for suffix, over in variants:
        n = (name + " " + suffix).strip()
        assert n not in names, n
        names.add(n)
        p = dict(base); p.update(over); p["master"] = -12.0
        body = ",".join('{"%s",%s}' % (k, lit(k, p[k])) for k in sorted(p))
        lines.append('{ "%s", "%s", { %s }, "%s" },' % (n, cat, body, group))
open(INC, "w", encoding="utf-8").write("// PowerSynth library 2.1 (Tools/build_library.py, %d presets): modern hard techno / schranz.\n" % len(lines)
                                      + "\n".join(lines) + "\n")
print("presets:", len(lines))
