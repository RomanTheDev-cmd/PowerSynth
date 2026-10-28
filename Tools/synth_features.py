"""Features of a synth phrase (reference loop or shot, or a rendered preset) and a distance between two of them.

  f0      main pitch: harmonic sum of the long-term spectrum, 60..1000 Hz
  harm    energy in bands relative to f0 (0.7f..1.5f, then octaves up to 96f, and above), dB relative to the total
  flat    spectral flatness 1..12 kHz, mean over active 40 ms frames (dB): tonal (about -30) .. noise (about -5)
  crest   peak / RMS of the active part (dB): density
  sustain fraction of 10 ms frames within 12 dB of the loudest: plucky .. held
  attack  from the first frame above -40 dB to the first within 3 dB of the peak (log10 ms)
  motion  std of the spectral centroid over 20 ms frames (octaves): filter / LFO / FM movement
  side    side/mid in 4 bands above 150 Hz (dB, -40..0)
All levels are relative, never absolute loudness. Only these numbers are kept from the references."""
import numpy as np

HARM_MULT = [0.7, 1.5, 3, 6, 12, 24, 48, 96]
SIDE_BANDS = [(150, 600), (600, 2500), (2500, 8000), (8000, 16000)]


def main_pitch(mid, sr):
    n = len(mid)
    M = np.abs(np.fft.rfft(mid * np.hanning(n)))
    f = np.fft.rfftfreq(n, 1 / sr)
    df = f[1]
    k = max(1, int(3.0 / df))  # smooth over 3 Hz
    M = np.convolve(M, np.ones(k) / k, mode="same")
    cand = np.arange(int(60 / df), int(1000 / df))
    score = np.zeros(len(cand))
    for h, w in ((1, 1.0), (2, 0.6), (3, 0.4), (4, 0.3)):
        idx = np.minimum(cand * h, len(M) - 1)
        score += w * np.log(M[idx] + 1e-9)
    return float(f[cand[np.argmax(score)]])


def features(x, sr, f0=None):
    mid, side = x.mean(axis=1), (x[:, 0] - x[:, 1]) / 2
    peak = np.max(np.abs(mid))
    if peak < 1e-5:
        return None
    hop = int(0.01 * sr)
    fr = np.array([np.sqrt(np.mean(mid[i:i + hop] ** 2)) for i in range(0, len(mid) - hop, hop)])
    frdb = 20 * np.log10(fr + 1e-12)
    act = frdb > np.max(frdb) - 40
    first, last = np.argmax(act), len(act) - np.argmax(act[::-1])
    seg_m, seg_s = mid[first * hop: last * hop], side[first * hop: last * hop]
    if len(seg_m) < hop * 5:
        return None
    n = len(seg_m)
    win = np.hanning(n)
    M, S = np.fft.rfft(seg_m * win), np.fft.rfft(seg_s * win)
    f = np.fft.rfftfreq(n, 1 / sr)
    P = np.abs(M) ** 2
    total = np.sum(P[(f >= 20) & (f < 20000)]) + 1e-30
    f0 = f0 or main_pitch(seg_m, sr)
    harm = []
    for lo, hi in zip(HARM_MULT[:-1], HARM_MULT[1:]):
        mm = (f >= lo * f0) & (f < min(hi * f0, 20000))
        harm.append(10 * np.log10(np.sum(P[mm]) / total + 1e-12))
    mm = (f >= HARM_MULT[-1] * f0) & (f < 20000)
    harm.append(10 * np.log10(np.sum(P[mm]) / total + 1e-12))
    harm = np.maximum(np.array(harm), -80)

    sustain = float(np.mean(frdb[first:last] > np.max(frdb) - 12))
    crest = float(20 * np.log10(peak / (np.sqrt(np.mean(seg_m ** 2)) + 1e-12)))
    pk = int(np.argmax(frdb > np.max(frdb) - 3))
    attack = float(np.log10(max(10.0, (pk - first) * 10.0)))

    hop2 = int(0.02 * sr)
    cents, flats = [], []
    ff = np.fft.rfftfreq(2 * hop2, 1 / sr)
    band = (ff >= 1000) & (ff < 12000)
    for i in range(0, n - 2 * hop2, hop2):
        seg = seg_m[i:i + 2 * hop2]
        if np.sqrt(np.mean(seg ** 2)) < np.max(fr) * 0.1:
            continue
        Q = np.abs(np.fft.rfft(seg * np.hanning(2 * hop2))) ** 2 + 1e-20
        cents.append(np.log2(np.sum(Q * ff) / np.sum(Q) + 1e-9))
        q = Q[band]
        flats.append(10 * np.log10(np.exp(np.mean(np.log(q))) / np.mean(q)))
    motion = float(np.std(cents)) if len(cents) > 3 else 0.0
    flat = float(np.mean(flats)) if flats else -30.0

    st = []
    for lo, hi in SIDE_BANDS:
        mm = (f >= lo) & (f < hi)
        st.append(np.clip(10 * np.log10((np.sum(np.abs(S[mm]) ** 2) + 1e-30) / (np.sum(P[mm]) + 1e-30)), -40, 0))
    return {"f0": f0, "harm": harm, "flat": flat, "crest": crest, "sustain": sustain, "attack": attack,
            "motion": motion, "side": np.array(st), "len": len(mid) / sr}


def distance(a, b, detail=False):
    parts = {
        "harm": float(np.mean(np.abs(a["harm"] - b["harm"]))),
        "flat": abs(a["flat"] - b["flat"]) / 2.0,
        "crest": abs(a["crest"] - b["crest"]),
        "sustain": 10.0 * abs(a["sustain"] - b["sustain"]),
        "attack": 6.0 * abs(a["attack"] - b["attack"]),
        "motion": 6.0 * abs(a["motion"] - b["motion"]),
        "side": float(np.mean(np.abs(a["side"] - b["side"]))),
    }
    w = {"harm": 1.0, "flat": 0.6, "crest": 0.5, "sustain": 0.6, "attack": 0.4, "motion": 0.5, "side": 0.4}
    total = sum(w[k] * v for k, v in parts.items())
    return (total, parts) if detail else total
