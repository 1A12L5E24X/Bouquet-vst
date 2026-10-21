"""
Builds Bouquet's sampled instruments from recordings of real instruments.

Sources (all free to use and redistribute):
  - Versilian Studios Chamber Orchestra 2, Community Edition (CC0): upright piano, solo
    violin, cello section, flute, harp, marimba, clarinet, French horn, glockenspiel and
    tubular bells. https://github.com/sgossner/VSCO-2-CE
    Point --vsco at a clone; only the needed files are fetched (partial clone is fine).
  - FluidR3 GM by Frank Wen (MIT), via the npm package soundfont-for-samplers: the
    electric piano. Point --fluid at the unpacked FluidR3_GM folder.

For every note: trims the silence before the onset, levels it, gives sustained notes
a seamless baked-in loop (a crossfade at the loop point), fades decaying notes out,
and encodes Ogg Vorbis. Writes <out>/<instrument>/*.ogg and <out>/samples.json.

Needs ffmpeg and numpy.
    python build_samples.py --vsco ~/vsco-2-ce --fluid .../FluidR3_GM --out ../../Bouquet/assets/samples
"""

import argparse
import json
import math
import os
import re
import subprocess

import numpy as np

SR = 44100

# name: (source, sfz or folder, velocity layers to keep, max seconds, sustained, mono)
INSTRUMENTS = {
    "piano":   ("vsco", "UprightPiano.sfz", [(0, 60), (61, 110), (111, 127)], 6.0, False, False),
    "violin":  ("vsco", "SViolinVib.sfz", None, 4.0, True, True),
    "cello":   ("vsco", "CelloEnsSusVib.sfz", None, 4.0, True, True),
    "flute":   ("vsco", "FluteSusVib.sfz", None, 4.0, True, True),
    "harp":    ("vsco", "Harp.sfz", None, 6.0, False, True),
    "marimba": ("vsco", "Marimba.sfz", None, 3.0, False, True),
    "rhodes":  ("fluid", "electric_piano_1-mp3", None, 5.0, False, True),
    "clarinet": ("vsco", "ClarinetSus.sfz", None, 4.0, True, True),
    "horn":    ("vsco", "FHornSus.sfz", None, 4.0, True, True),
    "glockenspiel": ("vsco", "Glockenspiel.sfz", None, 5.0, False, True),
    "bells":   ("vsco", "TubularBells.sfz", None, 8.0, False, True),
}

NOTE_NAMES = {"C": 0, "Db": 1, "D": 2, "Eb": 3, "E": 4, "F": 5, "Gb": 6, "G": 7, "Ab": 8, "A": 9, "Bb": 10, "B": 11}


def decode(data, mono):
    cmd = ["ffmpeg", "-v", "error", "-i", "pipe:0", "-f", "f32le", "-ar", str(SR), "-ac", "1" if mono else "2", "pipe:1"]
    raw = subprocess.run(cmd, input=data, capture_output=True, check=True).stdout
    x = np.frombuffer(raw, np.float32).copy()
    return x.reshape(-1, 1 if mono else 2)


def encode(x, path):
    cmd = ["ffmpeg", "-v", "error", "-y", "-f", "f32le", "-ar", str(SR), "-ac", str(x.shape[1]), "-i", "pipe:0",
           "-c:a", "libvorbis", "-q:a", "5", path]
    subprocess.run(cmd, input=np.ascontiguousarray(x, np.float32).tobytes(), check=True)


def git_blob(repo, path):
    return subprocess.run(["git", "-C", repo, "show", f"HEAD:{path}"], capture_output=True, check=True).stdout


def sfz_regions(repo, sfz):
    text = subprocess.run(["git", "-C", repo, "show", f"FETCH_HEAD:{sfz}"], capture_output=True, check=True,
                          text=True).stdout.replace("\r", "")
    default_path = re.search(r"default_path=(.+)", text).group(1).strip().replace("\\", "/")
    regions = []
    for chunk in text.split("<region>")[1:]:
        d = dict(re.findall(r"(\w+)=([^\n]+)", chunk))
        regions.append({
            "path": default_path + d["sample"].strip(),
            "key": int(d.get("pitch_keycenter", d.get("key", 60))),
            "lo": int(d.get("lokey", d.get("key", 0))),
            "hi": int(d.get("hikey", d.get("key", 127))),
            "lovel": int(d.get("lovel", 0)),
            "hivel": int(d.get("hivel", 127)),
        })
    return regions


def onset(x, threshold_db=-46.0):
    env = np.abs(x).max(axis=1)
    peak = env.max()
    above = np.nonzero(env > peak * 10 ** (threshold_db / 20))[0]
    return max(0, int(above[0]) - int(0.004 * SR)) if len(above) else 0


def find_loop(x, length):
    """A loop late in the note whose ends match well; returns (start, end) in samples."""
    mono = x.mean(axis=1)
    end_lo, end_hi = int(length - 0.45 * SR), int(length - 0.15 * SR)
    win = int(0.012 * SR)
    best = (-2.0, 0, 0)
    for end in range(end_lo, end_hi, int(0.01 * SR)):
        b = mono[end - win:end]
        for loop_len in np.arange(0.7, 1.6, 0.05):
            start = int(end - loop_len * SR)
            if start < int(0.6 * SR):
                continue
            a = mono[start - win:start]
            score = float(np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-9))
            # Prefer similar loudness at both ends too.
            score -= abs(math.log((np.abs(a).mean() + 1e-6) / (np.abs(b).mean() + 1e-6))) * 0.5
            if score > best[0]:
                best = (score, start, end)
    _, start, end = best
    # Nudge both ends onto rising zero crossings.
    def zero_cross(i):
        for k in range(i, min(i + 2000, len(mono) - 1)):
            if mono[k] <= 0.0 < mono[k + 1]:
                return k + 1
        return i
    return zero_cross(start), zero_cross(end)


def bake_loop(x, start, end):
    """Crossfades the end of the loop into the audio leading up to its start, so playback
    can jump from end back to start without a click or a bump."""
    fade = min(int(0.18 * SR), (end - start) // 3, start)
    t = np.linspace(0.0, 1.0, fade, dtype=np.float32)[:, None]
    out = x.copy()
    out[end - fade:end] = x[end - fade:end] * np.cos(t * math.pi / 2) + x[start - fade:start] * np.sin(t * math.pi / 2)
    return out


def process(x, max_len, sustained):
    x = x[onset(x):]
    if sustained:
        length = min(len(x), int(max_len * SR))
        start, end = find_loop(x, length)
        x = bake_loop(x[:end + 16], start, end)[:end]
        return x, start, end
    length = min(len(x), int(max_len * SR))
    x = x[:length].copy()
    fade = min(int(0.6 * SR), length // 3)
    x[-fade:] *= np.linspace(1.0, 0.0, fade, dtype=np.float32)[:, None] ** 2
    return x, None, None


def loudness(x):
    head = x[: int(1.2 * SR)]
    return float(np.sqrt(np.mean(head ** 2)) + 1e-9)


def build(args):
    manifest = {"version": 1, "sampleRate": SR, "instruments": {}}
    for name, (source, where, layers, max_len, sustained, mono) in INSTRUMENTS.items():
        if args.only and name not in args.only.split(","):
            continue
        out_dir = os.path.join(args.out, name)
        os.makedirs(out_dir, exist_ok=True)
        if source == "vsco":
            regions = sfz_regions(args.vsco, where)
            if layers:
                regions = [r for r in regions if (r["lovel"], r["hivel"]) in layers]
                for r in regions:            # the top kept layer plays up to full velocity
                    if r["hivel"] == max(l[1] for l in layers):
                        r["hivel"] = 127
            blobs = [lambda r=r: git_blob(args.vsco, r["path"]) for r in regions]
        else:
            folder = os.path.join(args.fluid, where)
            regions = []
            notes = list(range(28, 104, 3))
            for i, key in enumerate(notes):
                octave, pc = key // 12 - 1, key % 12
                fname = next(n for n, v in NOTE_NAMES.items() if v == pc) + str(octave) + ".mp3"
                lo = key - 1 if i else 0
                hi = key + 1 if i < len(notes) - 1 else 127
                regions.append({"path": os.path.join(folder, fname), "key": key, "lo": lo, "hi": hi, "lovel": 0, "hivel": 127})
            blobs = [lambda r=r: open(r["path"], "rb").read() for r in regions]

        entries, levels = [], []
        for i, (r, blob) in enumerate(zip(regions, blobs)):
            x = decode(blob(), mono)
            x, loop_start, loop_end = process(x, max_len, sustained)
            levels.append(loudness(x))
            entries.append((r, x, loop_start, loop_end))
            print(f"{name} {r['key']} v{r['lovel']}-{r['hivel']} {len(x) / SR:.2f}s"
                  + (f" loop {loop_start / SR:.2f}-{loop_end / SR:.2f}" if loop_start is not None else ""), flush=True)

        # Level every note, then let the dynamic layers keep their natural steps:
        # softer layers sit lower, so a velocity sweep still sounds like playing harder.
        target = 10 ** (-20 / 20)
        top = max(0.5 * (r["lovel"] + r["hivel"]) for r, *_ in entries)
        records = []
        for i, (r, x, ls, le) in enumerate(entries):
            layer = (0.5 * (r["lovel"] + r["hivel"]) / top) ** 0.6
            gain = target / levels[i] * layer
            peak = np.abs(x).max() * gain
            if peak > 0.95:
                gain *= 0.95 / peak
            fname = f"{r['key']:03d}_{r['lovel']:03d}.ogg"
            encode(x * gain, os.path.join(out_dir, fname))
            rec = {"file": f"{name}/{fname}", "key": r["key"], "lo": r["lo"], "hi": r["hi"],
                   "lovel": r["lovel"], "hivel": r["hivel"]}
            if ls is not None:
                rec["loopStart"], rec["loopEnd"] = int(ls), int(le)
            records.append(rec)
        manifest["instruments"][name] = {"sustained": sustained, "channels": 1 if mono else 2, "regions": records}

    path = os.path.join(args.out, "samples.json")
    if args.only and os.path.exists(path):
        old = json.load(open(path))
        old["instruments"].update(manifest["instruments"])
        manifest = old
    with open(path, "w") as f:
        json.dump(manifest, f, indent=1)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--vsco", required=True)
    ap.add_argument("--fluid", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--only", default="")
    build(ap.parse_args())
