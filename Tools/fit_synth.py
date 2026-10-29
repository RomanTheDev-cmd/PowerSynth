"""Builds the reference-fitted part of the PowerSynth factory library.

  python3 fit_synth.py fit OUT.json        k-medoids over the references of each category (ALLOC presets), one
                                          preset fitted per medoid, starting from the hand-made presets
  python3 fit_synth.py apply FIT.json      writes them between the markers in Source/FactoryPresets.inc
  python3 fit_synth.py audit               distance of every factory preset to the nearest reference of its category

Only feature numbers of the references are used; no reference audio ends up in PowerSynth. MASTER is fixed while
fitting (density must come from the synthesis) and set afterwards by `powersynth_tests --calibrate`.
Env: PS_TESTS (render server), FIT_ITERS (60), FIT_WORKERS (9), FIT_ONLY (category)."""
import json, math, os, pickle, re, subprocess, sys, time
from concurrent.futures import ProcessPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
if os.environ.get("PK_NUMPY_PATH"):
    sys.path.insert(0, os.environ["PK_NUMPY_PATH"])
import numpy as np
from synth_features import features, distance

TESTS = os.environ.get("PS_TESTS", os.path.expanduser("~/Library/Caches/PowerSynthBuildUniversal/powersynth_tests_artefacts/Release/powersynth_tests"))
INC = os.path.join(HERE, "..", "Source", "FactoryPresets.inc")
SR, LEN, FIT_MASTER = 48000, 4.0, -14.0
ALLOC = {"Leads": 24, "Rave & Hoover": 12, "Acid": 8, "Pads": 10, "Atmospheres": 14, "Stabs": 16, "Arps & Plucks": 14}
# name: (lo, hi, log-scale). 2.2 (2026-10-29): the 2.x engine (wavetables, WARP, FM, RING, pitch, GRIT, SMASH, SHIFT,
# MORPH) with modern floors: SMASH >= 0.3 and DRIVE >= 0.25 (Roman: no plain classic sounds).
SPACE = {
    "oscAShape": (0, 12, False), "detune": (0, 1, False), "oscALevel": (0.3, 1, False), "oscBShape": (0, 12, False),
    "oscBFine": (-50, 50, False), "oscBLevel": (0, 1, False), "noise": (0, 1, False), "fmAmount": (0, 1, False),
    "cutoff": (60, 18000, True), "resonance": (0, 0.95, False), "fltEnv": (-1, 1, False), "fltDecay": (10, 4000, True),
    "fltSustain": (0, 1, False), "attack": (0.5, 4000, True), "decay": (10, 6000, True), "sustain": (0, 1, False),
    "release": (5, 4000, True), "drive": (0.25, 1, False), "chorus": (0, 1, False), "delayMix": (0, 0.6, False),
    "delayFb": (0, 0.6, False), "reverbMix": (0, 0.9, False), "reverbSize": (0, 1, False), "width": (0, 1, False),
    "lfoAmount": (0, 1, False), "lfoRate": (0.05, 12, True),
    "warp": (0, 1, False), "ring": (0, 0.8, False), "pitchEnv": (-12, 36, False), "pitchTime": (5, 800, True),
    "grit": (0, 1, False), "smash": (0.3, 1, False), "shift": (0, 0.6, False), "morphEnv": (-1, 1, False),
}
INTS = {"unison": (1, 7), "oscAOct": (-1, 1), "oscBOct": (-2, 2)}
SEMIS = [0, 0, 0, 7, 12, -12, 5, 3, -5]
CHOICES = {"filterMode": 5, "driveType": 4, "lfoShape": 5, "lfoTarget": 5, "delayTime": 9, "warpMode": 4, "lfoDiv": 9}
# Usability limits per category (a lead must speak at once, a pad must swell).
LIMITS = {"Leads": {"attack": (0.5, 40), "release": (5, 800)}, "Rave & Hoover": {"attack": (0.5, 60), "release": (5, 1500)},
          "Acid": {"attack": (0.5, 20), "release": (5, 300)}, "Pads": {"attack": (30, 4000)}, "Atmospheres": {},
          "Stabs": {"attack": (0.5, 15)}, "Arps & Plucks": {"attack": (0.5, 20), "release": (5, 800)}}
FIXED = {}
DEFAULTS = {"oscAShape": 2.0, "detune": 0.25, "oscALevel": 1.0, "oscBShape": 3.0, "oscBFine": 7.0, "oscBLevel": 0.4,
            "noise": 0.0, "fmAmount": 0.0, "cutoff": 3500, "resonance": 0.25, "fltEnv": 0.3, "fltDecay": 400,
            "fltSustain": 0.3, "attack": 5, "decay": 600, "sustain": 0.8, "release": 250, "drive": 0.25, "chorus": 0.2,
            "delayMix": 0.0, "delayFb": 0.35, "reverbMix": 0.15, "reverbSize": 0.6, "width": 0.6, "lfoAmount": 0.0,
            "lfoRate": 2.0, "unison": 3, "oscAOct": 0, "oscBOct": 0, "oscBSemi": 0, "filterMode": 0, "driveType": 0,
            "lfoShape": 0, "lfoTarget": 1, "delayTime": 3, "warpMode": 0, "lfoDiv": 3, "warp": 0.0, "ring": 0.0,
            "pitchEnv": 0.0, "pitchTime": 80.0, "grit": 0.0, "smash": 0.3, "shift": 0.0, "morphEnv": 0.0}


def note_for(f0):
    n = int(round(69 + 12 * math.log2(max(f0, 20) / 440.0)))
    while n < 36: n += 12
    while n > 72: n -= 12
    return n


class Renderer:
    def __init__(self):
        self.p = subprocess.Popen([TESTS, "--serve"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)

    def render(self, params, root, phrase, bpm, length):
        line = " ".join("%s=%g" % kv for kv in params.items()) + " _root=%d _bpm=%g _len=%g _pattern=%d\n" % (root, bpm, length, phrase)
        self.p.stdin.write(line.encode()); self.p.stdin.flush()
        n = int(length * SR) * 2 * 4
        buf = b""
        while len(buf) < n:
            buf += self.p.stdout.read(n - len(buf))
        return np.frombuffer(buf, dtype="<f4").reshape(-1, 2).astype(np.float64)


def setup(ref):
    """Root, phrase, bpm, render length and analysed length for a reference."""
    shot = ref["phrase"] in (2, 5)
    ln = min(max(ref["feat"]["len"], 1.0), LEN) if shot else LEN
    return note_for(ref["feat"]["f0"]), ref["phrase"], ref["bpm"], (ln if shot else LEN + 1.0), ln


def bounds(cat, k):
    lo, hi, lg = SPACE[k]
    if k in LIMITS.get(cat, {}):
        lo, hi = LIMITS[cat][k]
    return lo, hi, lg


def to_unit(cat, k, v):
    lo, hi, lg = bounds(cat, k)
    v = min(max(v, lo), hi)
    return (math.log(v) - math.log(lo)) / (math.log(hi) - math.log(lo)) if lg else (v - lo) / (hi - lo)


def from_unit(cat, k, u):
    lo, hi, lg = bounds(cat, k)
    u = min(max(u, 0.0), 1.0)
    return math.exp(math.log(lo) + u * (math.log(hi) - math.log(lo))) if lg else lo + u * (hi - lo)


def clamp(cat, p):
    q = {**DEFAULTS, **{k: v for k, v in p.items() if k != "master"}}
    for k in SPACE:
        q[k] = from_unit(cat, k, to_unit(cat, k, q[k]))
    for k, (lo, hi) in INTS.items():
        q[k] = int(min(max(round(q[k]), lo), hi))
    if q["oscAShape"] < 3.5 and q["fmAmount"] < 0.3:   # a classic wave only with something modern on it
        q["warp"] = max(q["warp"], 0.2)
    return q


def mutate(cat, p, sigma, rng):
    q = dict(p)
    for k in SPACE:
        if rng.random() < 0.35:
            q[k] = from_unit(cat, k, to_unit(cat, k, q[k]) + rng.normal(0, sigma))
    for k, (lo, hi) in INTS.items():
        if rng.random() < 0.08:
            q[k] = int(min(max(q[k] + rng.choice([-1, 1]), lo), hi))
    if rng.random() < 0.05:
        q["oscBSemi"] = int(rng.choice(SEMIS))
    for k, n in CHOICES.items():
        if rng.random() < 0.05:
            q[k] = int(rng.integers(n))
    return q


def fit_one(job):
    ref, cat, starts, seed, iters = job
    rng = np.random.default_rng(seed)
    r = Renderer()
    root, phrase, bpm, rlen, alen = setup(ref)
    fixed = {**FIXED.get(cat, {}), "master": FIT_MASTER}
    def cost(p):
        x = r.render({**p, **fixed}, root, phrase, bpm, rlen)[: int(alen * SR)]
        f = features(x, SR)
        return (1e9, {}) if f is None else distance(f, ref["feat"], True)
    best, bc, bparts = None, 1e9, {}
    for s in starts:
        s = clamp(cat, s)
        c, parts = cost(s)
        if c < bc:
            best, bc, bparts = s, c, parts
    sigma = 0.15
    for _ in range(iters):
        improved = False
        for _ in range(8):
            q = mutate(cat, best, sigma, rng)
            c, parts = cost(q)
            if c < bc:
                best, bc, bparts, improved = q, c, parts, True
        sigma = min(0.3, sigma * 1.25) if improved else max(0.01, sigma * 0.85)
    for k in SPACE:  # polish: a coarse grid over every parameter
        for u in np.linspace(0, 1, 7):
            q = dict(best); q[k] = from_unit(cat, k, float(u))
            c, parts = cost(q)
            if c < bc:
                best, bc, bparts = q, c, parts
    for k, n in list(CHOICES.items()) + [("unison", 0)]:
        if k == "lfoDiv" and not best.get("lfoSync"): continue
        for v in (range(n) if n else range(1, 8)):
            q = dict(best); q[k] = v
            c, parts = cost(q)
            if c < bc:
                best, bc, bparts = q, c, parts
    r.p.kill()
    return clamp(cat, {**best, **FIXED.get(cat, {})}), bc, bparts


def kmedoids(fs, k, iters=30, seed=1):
    n = len(fs)
    D = np.array([[distance(a, b) for b in fs] for a in fs]); D = (D + D.T) / 2
    rng = np.random.default_rng(seed)
    med = list(rng.choice(n, min(k, n), replace=False))
    for _ in range(iters):
        lab = np.argmin(D[:, med], axis=1)
        new = []
        for j in range(len(med)):
            m = np.where(lab == j)[0]
            new.append(med[j] if len(m) == 0 else int(m[np.argmin(D[np.ix_(m, m)].sum(axis=1))]))
        if new == med:
            break
        med = new
    return med


ENTRY = re.compile(r'^\{ "([^"]+)", "([^"]+)", \{ (.*) \}, "([^"]+)" \},')


def hand_presets():
    out = []
    for line in open(INC, encoding="utf-8"):
        if line.startswith("// --- fitted"):
            break
        m = ENTRY.match(line)
        if m:
            vals = {k: float(v.rstrip("f")) for k, v in re.findall(r'\{"([^"]+)",(-?[\d.e+]+f?)\}', m.group(3))}
            out.append({"name": m.group(1), "cat": m.group(2), "params": vals})
    return out


def fit(out_path):
    refs = pickle.load(open(os.path.join(HERE, "..", "Референсы", "synth_features.pkl"), "rb"))
    hand = hand_presets()
    rng = np.random.default_rng(7)
    jobs, meta = [], []
    for cat, k in ALLOC.items():
        if os.environ.get("FIT_ONLY") and cat != os.environ["FIT_ONLY"]:
            continue
        pool = [r for r in refs if r["category"] == cat]
        for j, i in enumerate(kmedoids([r["feat"] for r in pool], k)):
            own = [h["params"] for h in hand if h["cat"] == cat]
            others = [hand[i2]["params"] for i2 in rng.choice(len(hand), 4, replace=False)]
            jobs.append((pool[i], cat, own + others + [{}], 1000 + len(jobs), int(os.environ.get("FIT_ITERS", 60))))
            meta.append((cat, pool[i]))
    t0 = time.time()
    with ProcessPoolExecutor(max_workers=int(os.environ.get("FIT_WORKERS", 9))) as ex:
        results = list(ex.map(fit_one, jobs, chunksize=1))
    out = []
    for (cat, ref), (p, c, parts) in zip(meta, results):
        out.append({"category": cat, "stem": ref["stem"], "target": ref["file"].split("/")[-1], "params": p, "distance": c,
                    "parts": parts})
        print("%-14s %-20s %5.2f  <- %s" % (cat, ref["stem"], c, ref["file"].split("/")[-1]))
    json.dump(out, open(out_path, "w"), indent=1, ensure_ascii=False)
    print("%d presets in %.0f s" % (len(out), time.time() - t0))


def group_of(p):
    if p.get("noise", 0) > 0.35:
        return "Noise"
    if p.get("fmAmount", 0) > 0.3:
        return "FM"
    return "Square" if p.get("oscAShape", 2) >= 2.5 else "Saw"


def lit(k, v):
    if k in CHOICES or k in INTS or k in ("oscBSemi", "voiceMode", "lfoSync"):
        return str(int(round(v)))
    t = "%.4g" % v
    return (t + ".0" if "." not in t and "e" not in t else t) + "f"


def apply(fit_path):
    fits = json.load(open(fit_path))
    taken = {h["name"] for h in hand_presets()}
    count, lines = {}, []
    order = list(ALLOC)
    for f in sorted(fits, key=lambda f: (order.index(f["category"]), f["stem"])):
        stem = f["stem"]
        count[stem] = count.get(stem, 0) + 1
        name = "%s %02d" % (stem, count[stem])
        assert name not in taken, name
        taken.add(name)
        p = {k: v for k, v in f["params"].items() if k not in ("master", "lfoSync")}
        p = {k: v for k, v in p.items() if not (k in DEFAULTS and abs(v - DEFAULTS[k]) < 1e-6)}
        p["master"] = -12.0  # set next by --calibrate
        body = ",".join('{"%s",%s}' % (k, lit(k, p[k])) for k in sorted(p))
        lines.append('{ "%s", "%s", { %s }, "%s" }, // fit %.1f to %s\n' % (name, f["category"], body, group_of(f["params"]),
                                                                           f["distance"], f["target"]))
    src = open(INC, encoding="utf-8").read()
    head = src.split("// --- fitted")[0].rstrip("\n") + "\n"
    block = "// --- fitted to the reference features (Tools/fit_synth.py, %d presets) ---\n" % len(lines) + "".join(lines) + "// --- end of fitted ---\n"
    tail = src.split("// --- end of fitted ---\n")[1] if "// --- end of fitted ---\n" in src else ""
    open(INC, "w", encoding="utf-8").write(head + block + tail)
    print("written:", len(lines))


def audit():
    refs = pickle.load(open(os.path.join(HERE, "..", "Референсы", "synth_features.pkl"), "rb"))
    out = subprocess.run([TESTS, "--dump-presets"], capture_output=True, text=True).stdout.strip().split("\n")
    r = Renderer()
    for line in out:
        name, cat, vals = line.split("|")
        p = {k: float(v) for k, v in (t.split("=") for t in vals.split())}
        best = (1e9, None)
        for ref in [x for x in refs if x["category"] == cat]:
            root, phrase, bpm, rlen, alen = setup(ref)
            f = features(r.render({**p, "master": FIT_MASTER}, root, phrase, bpm, rlen)[: int(alen * SR)], SR)
            if f is not None:
                best = min(best, (distance(f, ref["feat"]), ref["file"].split("/")[-1]), key=lambda t: t[0])
        print("%-24s %-14s %6.2f  %s" % (name, cat, best[0], best[1]))


def refit(out_path):
    """Each preset of the current library is fitted to a reference of its own category: k-medoids (k = presets in the
    category), presets assigned greedily to the nearest free medoid; distance before and after is reported."""
    refs = pickle.load(open(os.path.join(HERE, "..", "Референсы", "synth_features.pkl"), "rb"))
    lib = hand_presets()
    r = Renderer()
    jobs, meta = [], []
    for cat in ALLOC:
        mine = [e for e in lib if e["cat"] == cat]
        if not mine: continue
        pool = [x for x in refs if x["category"] == cat]
        meds = [pool[i] for i in kmedoids([x["feat"] for x in pool], len(mine))]
        D = []
        for e in mine:
            row = []
            for ref in meds:
                root, phrase, bpm, rlen, alen = setup(ref)
                f = features(r.render({**clamp(cat, e["params"]), "master": FIT_MASTER}, root, phrase, bpm, rlen)[: int(alen * SR)], SR)
                row.append(1e9 if f is None else distance(f, ref["feat"]))
            D.append(row)
        free = set(range(len(meds)))
        order = sorted(range(len(mine)), key=lambda a: min(D[a]))
        for a in order:
            b = min(free, key=lambda j: D[a][j]); free.discard(b)
            others = [x["params"] for x in mine if x is not mine[a]][:6]
            jobs.append((meds[b], cat, [mine[a]["params"]] + others, 2000 + len(jobs), int(os.environ.get("FIT_ITERS", 60))))
            meta.append((mine[a], meds[b], D[a][b]))
    r.p.kill()
    t0 = time.time()
    with ProcessPoolExecutor(max_workers=int(os.environ.get("FIT_WORKERS", 9))) as ex:
        results = list(ex.map(fit_one, jobs, chunksize=1))
    out = []
    for (e, ref, before), (p, c, parts) in zip(meta, results):
        out.append({"name": e["name"], "category": e["cat"], "target": ref["file"].split("/")[-1], "params": p, "before": before,
                    "distance": c, "parts": parts})
        print("%-24s %-14s %5.2f -> %5.2f  <- %s" % (e["name"], e["cat"], before, c, ref["file"].split("/")[-1]))
    json.dump(out, open(out_path, "w"), indent=1, ensure_ascii=False)
    b = sorted(o["before"] for o in out); a = sorted(o["distance"] for o in out)
    print("median distance %.2f -> %.2f, %d presets in %.0f s" % (b[len(b) // 2], a[len(a) // 2], len(out), time.time() - t0))


BEATS = [4, 2, 1, 0.5, 0.25, 0.125, 2 / 3, 1 / 3, 0.75]


def finish(name, p):
    """After fitting: PITCH on its 0.1 st grid; delay repeats gone within 12 s at 120 BPM (a 1/1 delay with feedback 0.6
    rang for 27 s); headroom for two presets whose level needed more than MASTER +6 dB."""
    p = dict(p)
    p["pitchEnv"] = round(p.get("pitchEnv", 0.0), 1)
    t = 0.5 * BEATS[int(round(p.get("delayTime", 3)))]
    p["delayFb"] = min(p.get("delayFb", 0.35), 10 ** (-3.0 * t / 12.0))
    if name == "Rave Signal X Shift":
        p["oscALevel"] = 1.0; p["oscBLevel"] = 1.0; p["smash"] = min(1.0, p.get("smash", 0.3) + 0.1)
    if name == "Rhythmic Machine Comb":
        p.update(noise=1.0, oscALevel=1.0, resonance=0.55, smash=1.0, driveType=0, drive=0.7)
    return p


def apply_refit(fit_path):
    fits = {f["name"]: f for f in json.load(open(fit_path))}
    lines, n = [], 0
    for line in open(INC, encoding="utf-8"):
        m = ENTRY.match(line)
        if m and m.group(1) in fits:
            f = fits[m.group(1)]
            p = finish(m.group(1), {k: v for k, v in f["params"].items() if k != "master"})
            p["master"] = -12.0
            body = ",".join('{"%s",%s}' % (k, lit(k, p[k])) for k in sorted(p))
            line = '{ "%s", "%s", { %s }, "%s" }, // fit %.1f (was %.1f) to %s\n' % (m.group(1), m.group(2), body, m.group(4),
                                                                                       f["distance"], f["before"], f["target"])
            n += 1
        lines.append(line)
    open(INC, "w", encoding="utf-8").write("".join(lines))
    print("refitted:", n)


if __name__ == "__main__":
    {"fit": lambda: fit(sys.argv[2]), "apply": lambda: apply(sys.argv[2]), "audit": audit, "refit": lambda: refit(sys.argv[2]),
     "apply-refit": lambda: apply_refit(sys.argv[2])}[sys.argv[1]]()
