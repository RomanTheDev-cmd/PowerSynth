"""PowerSynth 2.0 factory library (Claude, 2026-10-29). Roman: "the synth is nothing special... no aggression, no
character... Moog-like sounds instead of modern schranz and techno". Hand-designed archetypes of modern hard techno /
schranz sounds on the 2.0 engine (hidden wavetables, WARP, FM, RING, pitch envelope, GRIT, COMB, SMASH, SHIFT), each with
variations. The 1.x library stays as the category "Classic". MASTER is set afterwards by `powersynth_tests --calibrate`.
    python3 build_library_v2.py        rewrites Source/FactoryPresets.inc"""
import os, re

HERE = os.path.dirname(os.path.abspath(__file__))
INC = os.path.join(HERE, "..", "Source", "FactoryPresets.inc")
CHOICE = {"warpMode", "filterMode", "driveType", "lfoShape", "lfoTarget", "lfoDiv", "delayTime", "voiceMode", "unison",
          "oscAOct", "oscBOct", "oscBSemi", "lfoSync"}
SYNC, BEND, PWM, FOLD = 0, 1, 2, 3
LP24, LP12, BP, HP, COMB = 0, 1, 2, 3, 4
SAT, CLIP, FOLDD, CRUSH = 0, 1, 2, 3
SAW, SQUARE, HOLLOW, VOXA, VOXO, GROWL, SCREECH, SYNCW, BITTER, METAL, INDUST = 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12

# (category, group, base name, base parameters, [(variant name suffix, overrides), ...])
A = []
def arch(cat, group, name, base, variants):
    A.append((cat, group, name, base, variants))

# ---------------- LEADS
arch("Leads", "Sync", "Screamer", dict(oscAShape=SAW, unison=3, detune=.2, warpMode=SYNC, warp=.55, oscBLevel=0, cutoff=2600, resonance=.45,
     fltEnv=.35, fltDecay=250, grit=.55, pitchEnv=7, pitchTime=60, driveType=CLIP, drive=.55, smash=.5, reverbMix=.2, delayMix=.2, delayTime=3),
     [("I", {}), ("II", dict(warp=.75, grit=.7)), ("III", dict(warp=.35, driveType=FOLDD, drive=.45)), ("Mono", dict(voiceMode=2, glide=60, unison=5))])
arch("Leads", "Supersaw", "Hard Supersaw", dict(oscAShape=SAW, unison=7, detune=.45, oscBShape=SAW, oscBOct=1, oscBLevel=.3, cutoff=6000,
     resonance=.2, fltEnv=.2, driveType=SAT, drive=.4, smash=.6, chorus=.3, reverbMix=.35, delayMix=.25),
     [("I", {}), ("Bitter", dict(oscAShape=BITTER, drive=.5)), ("Shifted", dict(shift=.3, driveType=CLIP)), ("Rave", dict(pitchEnv=12, pitchTime=30, grit=.3))])
arch("Leads", "Wavetable", "Industrial Lead", dict(oscAShape=INDUST, unison=2, detune=.15, warpMode=BEND, warp=.4, fmAmount=.25,
     oscBShape=METAL, oscBSemi=7, oscBLevel=0, grit=.4, cutoff=3000, resonance=.5, fltEnv=.3, driveType=FOLDD, drive=.5, smash=.55, shift=.25,
     reverbMix=.2),
     [("X1", {}), ("X2", dict(warpMode=FOLD, warp=.5)), ("X3", dict(fmAmount=.5, ring=.25)), ("X4", dict(oscAShape=GROWL, driveType=CLIP))])
arch("Leads", "Wavetable", "Screech Lead", dict(oscAShape=SCREECH, unison=3, detune=.18, warpMode=SYNC, warp=.4, oscBLevel=0, lfoTarget=0,
     lfoAmount=.08, lfoRate=5.5, cutoff=5000, resonance=.6, fltEnv=.25, grit=.5, driveType=CLIP, drive=.6, smash=.5, reverbMix=.25),
     [("I", {}), ("II", dict(warp=.7, pitchEnv=12, pitchTime=90)), ("Vox", dict(oscAShape=VOXA, lfoTarget=2, lfoAmount=.3, lfoRate=.6))])

# ---------------- RAVE & HOOVER
arch("Rave & Hoover", "Hoover", "Hoover Dive", dict(oscAShape=SAW, unison=7, detune=.55, warpMode=PWM, warp=.45, lfoTarget=2, lfoAmount=.35,
     lfoRate=.8, pitchEnv=-12, pitchTime=250, voiceMode=1, glide=120, oscBShape=SQUARE, oscBOct=-1, oscBLevel=.5, cutoff=7000, fltEnv=0,
     chorus=.7, driveType=SAT, drive=.35, smash=.4, reverbMix=.3),
     [("I", {}), ("Legato", dict(voiceMode=2, glide=180)), ("Dark", dict(cutoff=2200, resonance=.35, grit=.35)), ("Metal", dict(warpMode=FOLD, warp=.35, ring=.2))])
arch("Rave & Hoover", "Square", "Rave Alarm", dict(oscAShape=SQUARE, unison=3, detune=.1, oscBShape=SQUARE, oscBSemi=7, oscBLevel=.5, ring=.2,
     pitchEnv=12, pitchTime=40, cutoff=2500, resonance=.55, fltEnv=.5, fltDecay=200, driveType=CLIP, drive=.45, delayMix=.3, smash=.4),
     [("I", {}), ("II", dict(ring=.45, shift=.2)), ("Sync", dict(warpMode=SYNC, warp=.45, grit=.4))])
arch("Rave & Hoover", "Metal", "Metal Rave Line", dict(oscAShape=METAL, unison=2, detune=.1, fmAmount=.45, oscBShape=0, oscBSemi=12, oscBLevel=0,
     ring=.35, pitchEnv=12, pitchTime=50, filterMode=COMB, cutoff=6000, resonance=.7, driveType=CLIP, drive=.5, smash=.5, delayMix=.2),
     [("I", {}), ("II", dict(fmAmount=.65, filterMode=LP24, cutoff=3500, resonance=.5, grit=.5)), ("III", dict(shift=.35, driveType=FOLDD))])

# ---------------- ACID
arch("Acid", "Saw", "Acid Scream", dict(oscAShape=SAW, unison=1, oscBLevel=0, cutoff=500, resonance=.85, fltEnv=.75, fltDecay=180, fltSustain=0,
     grit=.75, voiceMode=2, glide=70, decay=400, sustain=.6, driveType=SAT, drive=.45, delayMix=.2, delayTime=3, smash=.3, reverbMix=.1),
     [("303", {}), ("Square", dict(oscAShape=SQUARE)), ("Bitter", dict(oscAShape=BITTER, drive=.55)), ("Fold", dict(warpMode=FOLD, warp=.3, driveType=FOLDD)),
      ("Crushed", dict(driveType=CRUSH, drive=.4, grit=.6))])

# ---------------- PADS
arch("Pads", "Supersaw", "Grit Pad", dict(oscAShape=2.3, unison=7, detune=.35, attack=800, decay=1500, sustain=.8, release=1500, cutoff=1800,
     resonance=.35, fltEnv=0, lfoTarget=1, lfoAmount=.3, lfoRate=.2, grit=.35, chorus=.5, reverbMix=.55, reverbSize=.7, smash=.45, drive=.25),
     [("I", {}), ("II", dict(oscAShape=BITTER, cutoff=1200)), ("Shifted", dict(shift=.2, grit=.5))])
arch("Pads", "Wavetable", "Vox Pad", dict(oscAShape=5.5, unison=5, detune=.3, lfoTarget=2, lfoAmount=.4, lfoRate=.15, attack=1200, decay=1500,
     sustain=.8, release=1800, cutoff=4000, fltEnv=0, reverbMix=.6, reverbSize=.75, chorus=.4, smash=.3, drive=.2),
     [("Choir", {}), ("Dark", dict(oscAShape=6.5, cutoff=1800, grit=.3)), ("Growl", dict(oscAShape=7.3, warpMode=BEND, warp=.3))])
arch("Pads", "Wavetable", "Industrial Pad", dict(oscAShape=INDUST, unison=4, detune=.25, fmAmount=.2, oscBShape=METAL, oscBSemi=-5, oscBLevel=0,
     ring=.15, filterMode=LP12, cutoff=1200, resonance=.3, fltEnv=0, attack=1500, decay=2000, sustain=.8, release=2000, grit=.3, reverbMix=.7,
     reverbSize=.8, shift=.2, smash=.4, drive=.25),
     [("I", {}), ("II", dict(ring=.35, cutoff=900)), ("Wide", dict(unison=7, detune=.4, chorus=.5))])

# ---------------- ATMOSPHERES
arch("Atmospheres", "Noise", "Machine Air", dict(oscALevel=.2, oscBLevel=0, noise=.8, filterMode=COMB, resonance=.85, cutoff=3000, fltEnv=0,
     lfoTarget=1, lfoAmount=.5, lfoRate=.1, attack=1500, decay=2000, sustain=.9, release=3000, reverbMix=.7, reverbSize=.85, smash=.5, shift=.3,
     drive=.3),
     [("I", {}), ("II", dict(cutoff=1500, resonance=.92)), ("Rust", dict(driveType=CRUSH, drive=.45))])
arch("Atmospheres", "Metal", "Abyss Drone", dict(oscAShape=METAL, oscAOct=-1, unison=3, detune=.2, fmAmount=.3, oscBShape=INDUST, oscBSemi=7,
     oscBLevel=0, ring=.3, cutoff=700, resonance=.5, fltEnv=0, lfoTarget=2, lfoAmount=.3, lfoRate=.07, attack=2000, decay=3000, sustain=.9,
     release=4000, reverbMix=.8, reverbSize=.9, grit=.3, smash=.5, drive=.3),
     [("I", {}), ("II", dict(filterMode=COMB, cutoff=2500, resonance=.8)), ("III", dict(shift=.35, ring=.5))])
arch("Atmospheres", "Noise", "Rhythmic Noise", dict(oscALevel=0, oscBLevel=0, noise=1.0, filterMode=BP, cutoff=1800, resonance=.5, fltEnv=0,
     lfoTarget=3, lfoAmount=.85, lfoSync=1, lfoDiv=4, lfoShape=3, grit=.5, driveType=CRUSH, drive=.6, reverbMix=.3, smash=.4),
     [("1/16", {}), ("1/8", dict(lfoDiv=3, cutoff=900)), ("Comb", dict(filterMode=COMB, resonance=.8, cutoff=5000))])

# ---------------- STABS
arch("Stabs", "Metal", "Schranz Shot", dict(oscAShape=BITTER, unison=3, detune=.15, pitchEnv=24, pitchTime=30, noise=.25, warpMode=FOLD, warp=.5,
     cutoff=3000, resonance=.4, fltEnv=.6, fltDecay=120, fltSustain=0, decay=250, sustain=0, release=150, driveType=CLIP, drive=.7, smash=.7,
     reverbMix=.3),
     [("A", {}), ("B", dict(oscAShape=INDUST, pitchEnv=36)), ("C", dict(shift=.35, driveType=FOLDD)), ("D", dict(filterMode=BP, cutoff=1800, resonance=.6))])
arch("Stabs", "Supersaw", "Rave Chord", dict(oscAShape=SAW, unison=7, detune=.3, pitchEnv=7, pitchTime=20, cutoff=2500, fltEnv=.5, fltDecay=200,
     fltSustain=0, decay=350, sustain=0, release=200, driveType=SAT, drive=.45, smash=.55, reverbMix=.35, delayMix=.2),
     [("I", {}), ("II", dict(oscAShape=SQUARE, ring=.15)), ("Hard", dict(grit=.5, driveType=CLIP, drive=.6))])
arch("Stabs", "Metal", "Metal Shot", dict(oscAShape=METAL, unison=2, fmAmount=.6, oscBShape=0, oscBSemi=7, oscBLevel=0, ring=.5, filterMode=COMB,
     resonance=.8, cutoff=7000, fltEnv=0, pitchEnv=12, pitchTime=25, decay=300, sustain=0, release=200, driveType=FOLDD, drive=.5, smash=.6,
     shift=.3, reverbMix=.3),
     [("I", {}), ("II", dict(fmAmount=.85, ring=.3)), ("Low", dict(oscAOct=-1, cutoff=3500))])
arch("Stabs", "Sync", "Screech Hit", dict(oscAShape=SCREECH, unison=3, detune=.2, warpMode=SYNC, warp=.7, pitchEnv=24, pitchTime=80, grit=.6,
     cutoff=4500, resonance=.5, fltEnv=.3, fltDecay=250, decay=400, sustain=0, release=250, driveType=CLIP, drive=.55, smash=.6, reverbMix=.3),
     [("I", {}), ("II", dict(warp=.9, pitchTime=140)), ("Fold", dict(warpMode=FOLD, warp=.6))])

# ---------------- ARPS & PLUCKS
arch("Arps & Plucks", "Saw", "Hard Pluck", dict(oscAShape=SAW, unison=2, detune=.12, pitchEnv=12, pitchTime=15, grit=.4, cutoff=1500, resonance=.4,
     fltEnv=.6, fltDecay=150, fltSustain=0, decay=250, sustain=0, release=150, driveType=SAT, drive=.4, delayMix=.3, delayTime=3, smash=.5),
     [("I", {}), ("Square", dict(oscAShape=SQUARE, driveType=CLIP)), ("Growl", dict(oscAShape=GROWL, warpMode=BEND, warp=.4))])
arch("Arps & Plucks", "FM", "FM Metal Pluck", dict(oscAShape=0, unison=1, fmAmount=.6, oscBShape=METAL, oscBSemi=7, oscBLevel=0, cutoff=9000,
     fltEnv=0, decay=300, sustain=0, release=200, smash=.4, delayMix=.25, delayTime=4, drive=.3, driveType=CLIP),
     [("I", {}), ("II", dict(oscBSemi=12, fmAmount=.8)), ("Comb", dict(filterMode=COMB, resonance=.75, cutoff=6000))])
arch("Arps & Plucks", "Wavetable", "Table Arp", dict(oscAShape=GROWL, unison=2, detune=.1, warpMode=BEND, warp=.4, cutoff=2500, resonance=.5,
     fltEnv=.5, fltDecay=120, fltSustain=0, decay=200, sustain=0, release=120, driveType=CLIP, drive=.4, delayMix=.35, delayTime=4, smash=.45),
     [("I", {}), ("Vox", dict(oscAShape=VOXA, warp=.2)), ("Sync", dict(oscAShape=SYNCW, warpMode=SYNC, warp=.5, grit=.4))])


def lit(k, v):
    if k in CHOICE: return str(int(round(v)))
    t = "%.4g" % v
    return (t + ".0" if "." not in t and "e" not in t else t) + "f"

ENTRY = re.compile(r'^\{ "([^"]+)", "([^"]+)", \{ (.*) \}, "([^"]+)" \},')
text0 = open(INC, encoding="utf-8").read()
END = "// --- end of 2.0 library; below: the 1.x library as \"Classic\" ---\n"
if END in text0:                       # a rerun: the 1.x library is below the 2.0 block
    text0 = text0.split(END, 1)[1]
old_names, out_old = set(), []
for line in text0.split("\n"):
    m = ENTRY.match(line)
    if m:
        old_names.add(m.group(1))
        line = line.replace('", "%s", {' % m.group(2), '", "Classic", {', 1)
    out_old.append(line)
new_lines, names = [], set()
for cat, group, name, base, variants in A:
    for suffix, over in variants:
        n = (name + " " + suffix).strip()
        assert n not in old_names and n not in names, n
        names.add(n)
        p = dict(base); p.update(over); p["master"] = -12.0
        # Harder after the first measurement (2026-10-29): the first pass was hardly more aggressive than 1.x on average
        # (pads, atmospheres and hoovers too dark and clean). Pads / atmospheres: open, gritty textures; the rest: more
        # drive and SMASH.
        if cat in ("Pads", "Atmospheres"):
            p["cutoff"] = min(9000.0, p.get("cutoff", 3500.0) * 1.8)
            p["grit"] = min(0.8, p.get("grit", 0.0) + 0.15)
            p["drive"] = min(0.8, p.get("drive", 0.2) + 0.1)
        else:
            p["drive"] = min(0.85, p.get("drive", 0.2) + 0.15)
            p["smash"] = min(0.85, p.get("smash", 0.0) + 0.15)
        if group == "Hoover":
            p["cutoff"] = 12000.0; p["grit"] = max(p.get("grit", 0.0), 0.25)
        if name == "Abyss Drone":
            p["cutoff"] = max(p["cutoff"], 1800.0); p["drive"] = 0.45; p["driveType"] = FOLDD
        body = ",".join('{"%s",%s}' % (k, lit(k, p[k])) for k in sorted(p))
        new_lines.append('{ "%s", "%s", { %s }, "%s" },' % (n, cat, body, group))
head = "\n".join(out_old).rstrip("\n")
text = ("// --- PowerSynth 2.0 library (Tools/build_library_v2.py, %d presets): modern hard techno / schranz, first in the browser ---\n" % len(new_lines)
        + "\n".join(new_lines) + "\n" + END + head + "\n")
open(INC, "w", encoding="utf-8").write(text)
print("new:", len(new_lines), "classic:", len(old_names))
