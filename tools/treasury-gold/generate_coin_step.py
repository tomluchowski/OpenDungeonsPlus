#!/usr/bin/env python3
"""Generates sounds/Spatial/Rooms/Treasury/CoinStep/CoinStep1..3.ogg, the clink of a foot in deep treasury gold.

Original work of the project, licence CC0. The sound is synthesised from a few decaying metallic partials and
seeded noise; running the script again gives the same samples. Needs numpy and ffmpeg (libvorbis) on the path.

Usage: python generate_coin_step.py [output directory]
"""

import os
import subprocess
import sys
import tempfile
import wave

import numpy as np

RATE = 44100


def clink(rng, partials, length):
    t = np.arange(int(RATE * length)) / RATE
    signal = np.zeros_like(t)
    for freq, gain, decay in partials:
        freq *= rng.uniform(0.97, 1.03)
        signal += gain * np.sin(2 * np.pi * freq * t + rng.uniform(0, 6.28)) * np.exp(-t * decay)
    return signal


def make(seed):
    rng = np.random.RandomState(seed)
    length = 0.45
    out = np.zeros(int(RATE * length))
    # Several coins shifting against each other: short clinks at slightly different times and pitches
    for i in range(4 + seed % 3):
        start = int(RATE * rng.uniform(0.0, 0.12))
        base = rng.uniform(2200, 4200)
        partials = [(base, 1.0, 38.0), (base * 1.52, 0.6, 52.0), (base * 2.31, 0.35, 70.0)]
        piece = clink(rng, partials, 0.25) * rng.uniform(0.4, 0.9)
        end = min(len(out), start + len(piece))
        out[start:end] += piece[:end - start]
    # A soft rustle under the clinks
    rustle = rng.normal(0, 1, len(out))
    rustle = np.convolve(rustle, np.ones(6) / 6, mode='same')
    envelope = np.exp(-np.arange(len(out)) / RATE * 14.0)
    out += 0.25 * rustle * envelope
    out /= max(1e-6, np.max(np.abs(out)))
    return (out * 0.6 * 32767).astype(np.int16)


def main():
    target = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(__file__), '..', '..', 'sounds', 'Spatial', 'Rooms', 'Treasury', 'CoinStep')
    os.makedirs(target, exist_ok=True)
    for number in range(1, 4):
        samples = make(number)
        with tempfile.TemporaryDirectory() as folder:
            wav_path = os.path.join(folder, 'step.wav')
            with wave.open(wav_path, 'wb') as wav:
                wav.setnchannels(1)
                wav.setsampwidth(2)
                wav.setframerate(RATE)
                wav.writeframes(samples.tobytes())
            subprocess.check_call(['ffmpeg', '-y', '-loglevel', 'error', '-i', wav_path, '-c:a', 'libvorbis',
                                   '-q:a', '4', '-map_metadata', '-1', os.path.join(target, 'CoinStep%d.ogg' % number)])


if __name__ == '__main__':
    main()
