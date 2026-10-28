"""synth_features of every reference loop / shot with a PowerSynth category, a name stem and a render phrase.
Output: Референсы/synth_features.pkl (numbers and file names only, no audio).
Phrases (Tests --serve): 0 lead line, 1 held chord, 2 stab chord, 3 arpeggio, 4 held note, 5 short note."""
import os, pickle, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
if os.environ.get("PK_NUMPY_PATH"):
    sys.path.insert(0, os.environ["PK_NUMPY_PATH"])
from wavread import read_wav
from synth_features import features

ROOT = os.path.expanduser("~/Library/Mobile Documents/com~apple~CloudDocs/definitionofhardtechno")
LEN = 4.0
# (folder key, category, name stem, phrase)
RULES = [
    ("HDIT3 Synth Leads", "Leads", "Industrial Lead", 0), ("HST1 Synth Hook Loops", "Leads", "Schranz Hook", 0),
    ("SVE2 Synth Leads", "Leads", "Hard Lead", 0), ("Trance Melodies", "Leads", "Trance Melodie", 0),
    ("90s Melody Loops", "Leads", "90s Melody", 0), ("Mixed Synth Melodies", "Leads", "Dark Melody", 0),
    ("OBI Dark Industrial Synthlines", "Leads", "Dark Synthline", 0),
    ("Raving Synth Lines", "Rave & Hoover", "Raving Line", 0), ("Ravy Saw Sequences", "Rave & Hoover", "Ravy Saw", 0),
    ("SSP Rave & Signal Synth Lines", "Rave & Hoover", "Rave Signal", 0),
    ("HDIT3 Bonus Metal Rave Loops", "Rave & Hoover", "Metal Rave", 0),
    ("HDIT3 Dark Acid Lines", "Acid", "Dark Acid", 0),
    ("05 Atmosphere", "Atmospheres", "Atmosphere", 4), ("HDIT3 Abstract Atmo Loops", "Atmospheres", "Abstract Atmo", 4),
    ("HST1 Texture FX", "Atmospheres", "Texture", 4), ("ITHTS Atmo & FX", "Atmospheres", "Industrial Atmo", 4),
    ("OBI Machine Texture Loops", "Atmospheres", "Machine Texture", 4),
    ("HDIT3 Processed Atmo Shots", "Atmospheres", "Processed Atmo", 4),
    ("06 Industrial Noise", "Atmospheres", "Industrial Noise", 4),
    ("SSP Synth & Sound Shots", "Stabs", "Schranz Shot", 2), ("SSP Signature Synth & Sound Shots", "Stabs", "Signature Shot", 2),
    ("ITHTS Synth Shots", "Stabs", "Industrial Shot", 2), ("OBI 20 Years Bonus Synth Shots", "Stabs", "Hardtechno Shot", 2),
    ("Vol.1/Synth Hits", "Stabs", "Schranz Hit", 2), ("HST1 Weird Screech Shots", "Stabs", "Screech", 2),
    ("HDIT3 Horror Bells", "Stabs", "Horror Bell", 5),
    ("HST1 Processed Pluck Shots", "Arps & Plucks", "Processed Pluck", 5),
    ("SSP Driving Synth Sequences", "Arps & Plucks", "Driving Sequence", 3), ("SVE2 Sequences", "Arps & Plucks", "Schranz Sequence", 3),
    ("ITHTS Synth Sequences", "Arps & Plucks", "Industrial Sequence", 3), ("Rhythm Synth Hooks", "Arps & Plucks", "Rhythm Hook", 3),
    ("SSP Rhythm Synth Lines", "Arps & Plucks", "Rhythm Line", 3),
]
THEME = [("Lead", "Leads", "Theme Lead", 0), ("Pad", "Pads", "Theme Pad", 1), ("Arp", "Arps & Plucks", "Theme Arp", 3),
         ("Pluck", "Arps & Plucks", "Theme Pluck", 3)]


def rule_for(rel, name):
    if "Ableton" in rel or "MIDI" in rel or "DEMO" in rel:
        return None
    if "Melodic Schranz Theme" in rel:
        return next(((c, s, p) for key, c, s, p in THEME if key.lower() in name.lower()), None)
    return next(((c, s, p) for key, c, s, p in RULES if key in rel), None)


out = []
for dirpath, _, files in os.walk(ROOT):
    rel = os.path.relpath(dirpath, ROOT)
    for name in sorted(files):
        if not name.lower().endswith(".wav"):
            continue
        r = rule_for(rel, name)
        if r is None:
            continue
        cat, stem, phrase = r
        try:
            x, sr = read_wav(os.path.join(dirpath, name))
            f = features(x[: int(sr * LEN)], sr)
        except Exception as e:  # noqa: BLE001
            print("skip", name, e, file=sys.stderr)
            continue
        if f is None:
            continue
        m = re.search(r"(\d{3})\s*BPM", rel + " " + name, re.I)
        out.append({"file": os.path.join(rel, name), "category": cat, "stem": stem, "phrase": phrase,
                    "bpm": float(m.group(1)) if m else 150.0, "feat": f})
pickle.dump(out, open(os.path.join(HERE, "..", "Референсы", "synth_features.pkl"), "wb"))
from collections import Counter
print(len(out), sorted(Counter(o["category"] for o in out).items()))
