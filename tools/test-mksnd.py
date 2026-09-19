#!/usr/bin/env python3
"""Regression: Quake's 22 kHz 16-bit lightning sounds must enter the 11 kHz mixer."""

from __future__ import annotations

import io
import struct
import wave

import mksnd


def main() -> int:
    out = io.BytesIO()
    with wave.open(out, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(22050)
        wav.writeframes(struct.pack("<4h", -32768, -16384, 0, 16384))
    samples, loop = mksnd.wav_samples("weapons/test", out.getvalue())
    if samples != bytes((0, 128)) or loop != -1:
        raise SystemExit(f"16-bit resample produced {list(samples)}, loop {loop}")
    print("ok: 22 kHz 16-bit PCM resamples to unsigned 11 kHz")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
