#!/usr/bin/env python3
"""Synthesises the short sounds of the worker reactions (see WorkerReactions in source/render).

    python tools/gen_worker_sounds.py

Writes two variants each of digging in dirt, rock, gold and gems, a coin jingle, stomping, knocking and a
cheer to sounds/Spatial/Creatures/Worker/<Family>/<Family>01.ogg and 02.ogg. Everything is computed from sine
waves and noise by this script, nothing is taken from a recording or another work.

Needs numpy and ffmpeg (with the Vorbis encoder) on the path.
"""

import math
import os
import subprocess
import tempfile
import wave

import numpy as np

RATE = 44100


def write_ogg(samples, path, level=0.6):
    peak = float(np.max(np.abs(samples)))
    if peak > 0.0:
        samples = samples * (level / peak)
    data = (samples * 32767.0).astype("<i2").tobytes()
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with tempfile.TemporaryDirectory() as folder:
        wav_path = os.path.join(folder, "sound.wav")
        with wave.open(wav_path, "wb") as handle:
            handle.setnchannels(1)
            handle.setsampwidth(2)
            handle.setframerate(RATE)
            handle.writeframes(data)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav_path, "-c:a", "libvorbis", "-q:a", "4", path],
                       check=True)


def times(length):
    return np.arange(int(RATE * length)) / RATE


def lowpass(signal, width):
    kernel = np.ones(width) / float(width)
    return np.convolve(signal, kernel, mode="same")


def highpass(signal, width):
    return signal - lowpass(signal, width)


def attack(t, seconds=0.002):
    return np.minimum(1.0, t / seconds)


def ring(t, frequency, decay, phase=0.0):
    return np.exp(-t / decay) * np.sin(2 * math.pi * frequency * t + phase)


def place(out, sound, start):
    offset = int(start * RATE)
    count = min(len(sound), len(out) - offset)
    if count > 0:
        out[offset:offset + count] += sound[:count]


def dig_dirt(rng):
    t = times(0.3)
    crumble = lowpass(rng.uniform(-1, 1, len(t)), 40) * np.exp(-t / 0.07)
    thud = ring(t, rng.uniform(95, 125), 0.06)
    grit = highpass(rng.uniform(-1, 1, len(t)), 8) * np.exp(-t / 0.03) * 0.25
    return (1.4 * crumble + 0.9 * thud + grit) * attack(t)


def dig_rock(rng):
    t = times(0.35)
    crack = highpass(rng.uniform(-1, 1, len(t)), 6) * np.exp(-t / 0.012)
    body = ring(t, rng.uniform(380, 460), 0.05) + 0.5 * ring(t, rng.uniform(1100, 1300), 0.03)
    clink = 0.35 * ring(t, rng.uniform(2600, 3300), 0.04)
    chips = np.zeros_like(t)
    for start in (0.05, 0.09, 0.15):
        place(chips, 0.2 * highpass(rng.uniform(-1, 1, 400), 4) * np.exp(-np.arange(400) / 90.0), start + rng.uniform(0, 0.02))
    return (crack + 0.8 * body + clink + chips) * attack(t, 0.001)


def dig_gold(rng):
    t = times(0.6)
    out = np.zeros_like(t)
    place(out, dig_rock(rng)[:int(0.2 * RATE)] * 0.7, 0.0)
    base = rng.uniform(1700, 2000)
    for ratio, level, decay in ((1.0, 1.0, 0.25), (1.51, 0.6, 0.18), (2.32, 0.45, 0.12), (3.07, 0.3, 0.08)):
        out += 0.5 * level * ring(t, base * ratio, decay, rng.uniform(0, 6)) * attack(t, 0.001)
    return out


def dig_gem(rng):
    t = times(0.9)
    out = np.zeros_like(t)
    place(out, dig_rock(rng)[:int(0.15 * RATE)] * 0.5, 0.0)
    base = rng.uniform(2300, 2700)
    for ratio, level, decay in ((1.0, 1.0, 0.5), (1.41, 0.5, 0.4), (2.0, 0.55, 0.3), (2.76, 0.3, 0.2), (3.9, 0.2, 0.12)):
        beat = 1.0 + 0.004 * ratio
        out += 0.4 * level * np.exp(-t / decay) * (np.sin(2 * math.pi * base * ratio * t) +
                                                  np.sin(2 * math.pi * base * ratio * beat * t)) * attack(t, 0.001)
    return out


def coin_jingle(rng):
    length = 0.9
    t = times(length)
    out = np.zeros_like(t)
    start = 0.0
    for _ in range(rng.randint(5, 8)):
        local = times(0.25)
        base = rng.uniform(2200, 3600)
        clink = (ring(local, base, 0.05) + 0.6 * ring(local, base * 1.52, 0.035)) * attack(local, 0.0008)
        place(out, rng.uniform(0.5, 1.0) * clink, start)
        start += rng.uniform(0.04, 0.13)
    return out


def stomp(rng):
    t = times(0.3)
    drop = 110.0 * np.exp(-t / 0.05) + 45.0
    phase = 2 * math.pi * np.cumsum(drop) / RATE
    thump = np.sin(phase) * np.exp(-t / 0.07)
    dust = lowpass(rng.uniform(-1, 1, len(t)), 30) * np.exp(-t / 0.05)
    slap = highpass(rng.uniform(-1, 1, len(t)), 10) * np.exp(-t / 0.01) * 0.4
    return (1.2 * thump + 0.8 * dust + slap) * attack(t, 0.001)


def knock(rng):
    t = times(0.45)
    out = np.zeros_like(t)
    start = 0.0
    for _ in range(rng.randint(2, 3)):
        local = times(0.2)
        base = rng.uniform(330, 480)
        hit = (ring(local, base, 0.035) + 0.5 * ring(local, base * 2.3, 0.02)) * attack(local, 0.0008)
        hit += 0.3 * highpass(rng.uniform(-1, 1, len(local)), 6) * np.exp(-local / 0.006)
        place(out, rng.uniform(0.7, 1.0) * hit, start)
        start += rng.uniform(0.11, 0.17)
    return out


def cheer(rng):
    length = 1.0
    t = times(length)
    # a short rising shout that holds and falls away, a bit of breath in it
    pitch = 190.0 * (1.0 + 0.35 * np.minimum(1.0, t / 0.25)) * (1.0 - 0.12 * np.clip((t - 0.5) / 0.5, 0.0, 1.0))
    pitch = pitch * (1.0 + 0.012 * np.sin(2 * math.pi * 6.0 * t))
    phase = 2 * math.pi * np.cumsum(pitch) / RATE
    # the vowel opens from "oo" to "aa"
    open_amount = np.minimum(1.0, t / 0.3)
    voice = np.zeros_like(t)
    for harmonic in range(1, 30):
        frequency = pitch * harmonic
        formant1 = 320.0 + 450.0 * open_amount
        formant2 = 850.0 + 350.0 * open_amount
        shape = np.exp(-((frequency - formant1) / 250.0) ** 2) + 0.7 * np.exp(-((frequency - formant2) / 350.0) ** 2) + \
            0.15 * np.exp(-((frequency - 2600.0) / 600.0) ** 2)
        voice += shape * np.sin(harmonic * phase) / math.sqrt(harmonic)
    breath = lowpass(rng.uniform(-1, 1, len(t)), 5) * 0.25
    envelope = np.minimum(1.0, t / 0.05) * np.exp(-np.clip(t - 0.45, 0.0, None) / 0.3)
    return (voice + breath) * envelope


FAMILIES = (
    ("DigDirt", dig_dirt, 3),
    ("DigRock", dig_rock, 4),
    ("DigGold", dig_gold, 5),
    ("DigGem", dig_gem, 6),
    ("CoinJingle", coin_jingle, 7),
    ("Stomp", stomp, 8),
    ("Knock", knock, 9),
    ("Cheer", cheer, 10),
)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    for name, function, seed in FAMILIES:
        for variant in (1, 2):
            rng = np.random.RandomState(seed * 10 + variant)
            path = os.path.join(root, "sounds", "Spatial", "Creatures", "Worker", name, "%s%02d.ogg" % (name, variant))
            write_ogg(function(rng), path)
            print("wrote " + os.path.relpath(path, root))


if __name__ == "__main__":
    main()
