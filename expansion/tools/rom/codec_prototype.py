"""SUPERSEDED / EXPERIMENTAL / NOT PRODUCTION.

Isolated XROM-D1 audition experiment; Python standard library only.

No plugin imports/integration. See README.md for experimental search choices.
"""
from __future__ import annotations

import argparse
import csv
from decimal import Decimal, localcontext, ROUND_HALF_EVEN
import hashlib
import json
import math
from pathlib import Path
import struct
import wave

N, BLOCK, RATE, SECONDS = 128, 16, 48000, 3
GAIN = 0.5
PI = Decimal("3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986")


def sine(turns: Decimal) -> Decimal:
    x = (turns % 1) * 2 * PI
    if x > PI:
        x -= 2 * PI
    term = total = x
    for k in range(1, 200):
        term *= -x * x / ((2 * k) * (2 * k + 1))
        total += term
        if abs(term) < Decimal("1e-85"):
            return total
    raise AssertionError("Taylor limit")


def sources() -> dict[str, list[Decimal]]:
    with localcontext() as ctx:
        ctx.prec = 80
        result = {}
        for name in ("smooth", "edge", "complex"):
            values = []
            for n in range(N):
                p = Decimal(n) / N
                if name == "smooth":
                    v = sine(p) + Decimal(".22") * sine(3*p) + Decimal(".08") * sine(7*p)
                elif name == "edge":
                    # Non-50% pulse plus a small folded sine, no sampled input.
                    pulse = Decimal(1 if n < 36 else -1)
                    f = 3 * sine(p)
                    folded = 1 - abs(((f + 1) % 4 + 4) % 4 - 2)
                    v = pulse + Decimal(".18") * folded
                else:
                    v = Decimal(0)
                    for k in range(1, 48):
                        amplitude = Decimal((17*k) % 23 + 3) / ((k+2)*20)
                        if k % 7 in (0, 3):
                            amplitude /= 5
                        phase = Decimal((k*k*7) % 97) / 97
                        v += amplitude * sine(k*p + phase)
                values.append(v)
            dc = sum(values) / N
            values = [v-dc for v in values]
            scale = Decimal(30000) / max(abs(v) for v in values)
            # Preserve high precision; methods get exactly the same source/gain.
            result[name] = [v*scale/32768 for v in values]
        return result


def nearest_div(n: int, d: int) -> int:
    q, r = divmod(n, d)
    return q + int(2*r > d or (2*r == d and q % 2 != 0))


# Search state: cost, max error, exponents, codes, accumulator, first error, last error.
def encode(target: list[int], slope_weight: int, width: int = 64,
           minimum_exponent: int = 0, terminal_exponent: int | None = None) -> bytes:
    N = len(target)
    if N < 32 or N % 32:
        raise ValueError("Cycle length must be a positive multiple of 32")
    anchor = target[-1]
    initial = (0, 0, (), (), anchor, 0, 0)
    survivors = [initial]
    for block in range(N // BLOCK):
        finished = []
        choices_of_exponent = [terminal_exponent] if terminal_exponent is not None and block == N//BLOCK-1 else range(minimum_exponent,10)
        for exponent in choices_of_exponent:
            step = 1 << exponent
            beam = []
            for cost, maximum, exps, codes, acc, first, previous in survivors:
                penalty = 8*(exponent-exps[-1])**2 if exps else 0
                beam.append((cost+penalty, maximum, exps+(exponent,), codes, acc, first, previous))
            for n in range(block*BLOCK, (block+1)*BLOCK):
                candidates = {}
                for cost, maximum, exps, codes, acc, first, previous in beam:
                    if n == N-1:
                        difference = anchor-acc
                        choices = [difference//step] if difference % step == 0 else []
                    else:
                        q = nearest_div(target[n]-acc, step)
                        choices = sorted({0, max(-128, min(127, q-1)),
                                          max(-128, min(127, q)), max(-128, min(127, q+1))})
                    for code in choices:
                        if not -128 <= code <= 127:
                            continue
                        decoded = acc + code*step
                        if not -32768 <= decoded <= 32767:
                            continue  # Valid encodes never saturate.
                        error = decoded-target[n]
                        score = cost + 16*error*error
                        if n:
                            score += slope_weight*(error-previous)**2
                        first_error = first if n else error
                        if n == N-1:
                            score += slope_weight*(error-first_error)**2
                            score += 8*(exps[-1]-exps[0])**2
                        item = (score, max(maximum, abs(error)), exps, codes+(code,),
                                decoded, first_error, error)
                        # Preserve future cost-relevant state; deterministic ties.
                        key = (decoded, first_error)
                        if key not in candidates or item < candidates[key]:
                            candidates[key] = item
                beam = sorted(candidates.values())[:width]
                if not beam:
                    break
            finished.extend(beam)
        # Keep the explicitly feasible zero-code ring outside the quality beam.
        exps = (minimum_exponent,)*(block+1)
        codes = (0,)*((block+1)*BLOCK)
        errors = [anchor-v for v in target[:len(codes)]]
        cost = 16*sum(v*v for v in errors)
        cost += slope_weight*sum((a-b)**2 for a, b in zip(errors[1:], errors))
        if block == N//BLOCK-1:
            cost += slope_weight*(errors[-1]-errors[0])**2
        zero = (cost, max(map(abs, errors)), exps, codes, anchor, errors[0], errors[-1])
        survivors = sorted(finished)[:width]
        if zero not in survivors:
            survivors.append(zero)
    best = min(survivors)
    if all(c == 0 for c in best[3]):
        raise RuntimeError("Search found only the silent feasibility sentinel; widen search")
    exps, codes = best[2], best[3]
    integers = []
    acc = anchor
    for i, code in enumerate(codes):
        acc += code*(1 << exps[i//BLOCK])
        integers.append(acc)
    assert acc == anchor
    correction = nearest_div(sum(integers), N)
    assert min(integers)-correction >= -32768 and max(integers)-correction <= 32767
    anchor -= correction
    scales = bytes(exps[i] | (exps[i+1] << 4) for i in range(0, len(exps), 2))
    return struct.pack("<h", anchor) + scales + struct.pack(f"<{N}b", *codes)


def decode(payload: bytes, cycle_length: int = 128) -> tuple[list[int], dict]:
    N = cycle_length
    if len(payload) != 2 + N//BLOCK//2 + N:
        raise ValueError("Invalid frame payload length")
    anchor, = struct.unpack_from("<h", payload)
    packed = payload[2:2+N//BLOCK//2]
    exps = [value for byte in packed for value in (byte & 15, byte >> 4)]
    if any(e > 9 for e in exps):
        raise ValueError("Invalid exponent")
    codes = struct.unpack_from(f"<{N}b", payload, 2+len(packed))
    acc, saturation, values = anchor, 0, []
    for n, code in enumerate(codes):
        candidate = acc + code*(1 << exps[n//BLOCK])
        saturation += not -32768 <= candidate <= 32767
        acc = max(-32768, min(32767, candidate))
        values.append(acc)
    return values, dict(anchor=anchor, exponents=exps, saturation=int(saturation),
                        closure_error_lsb=acc-anchor,
                        exponent_range=[min(exps), max(exps)],
                        exponent_changes=sum(a != b for a,b in zip(exps, exps[1:]+exps[:1])))


def wav(path: Path, cycle: list[float]) -> None:
    # Exact periodic repetition, no playback interpolation, no effects/fades.
    pcm = bytearray()
    for n in range(RATE*SECONDS):
        value = round(cycle[n % N]*GAIN*(1 << 23))
        if not -(1 << 23) <= value < (1 << 23):
            raise ValueError("Audition clipping")
        pcm.extend((value & 0xffffff).to_bytes(3, "little"))
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(3)
        out.setframerate(RATE)
        out.writeframes(pcm)


def metrics(reference: list[float], values: list[float]) -> dict:
    errors = [v-r for r,v in zip(reference, values)]
    rms = math.sqrt(sum(e*e for e in errors)/N)
    source_rms = math.sqrt(sum(r*r for r in reference)/N)
    return dict(rms_error=rms, max_abs_error=max(map(abs, errors)),
                dc=sum(values)/N, peak=max(map(abs, values)),
                error_db_relative=20*math.log10(rms/source_rms) if rms else None,
                # Boundary error relative to the source's intended last->first delta.
                seam_delta_error=(values[0]-values[-1])-(reference[0]-reference[-1]))


def generate(out: Path) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    measurements = {}
    for name, precise in sources().items():
        reference = [float(v) for v in precise]
        target = [int((v*32768).to_integral_value(rounding=ROUND_HALF_EVEN)) for v in precise]
        pcm = [max(-128, min(127, int((v*128).to_integral_value(rounding=ROUND_HALF_EVEN))))/128
               for v in precise]
        payload = encode(target, 4 if name == "smooth" else 1)
        path = out/f"{name}.xromd1-frame"
        path.write_bytes(payload)
        # Read the artifact back; XROM audition samples only come from its decoder.
        integers, info = decode(path.read_bytes())
        assert info["closure_error_lsb"] == 0 and info["saturation"] == 0
        assert abs(sum(integers)/N) <= 0.5
        decoded = [v/32768 for v in integers]
        measurements[name] = {"pcm8": metrics(reference, pcm),
                              "xromd1": {**metrics(reference, decoded), **info}}
        (out/f"{name}.source.json").write_text(json.dumps([str(v) for v in precise])+"\n", encoding="utf-8")
        (out/f"{name}.decoded-s16le").write_bytes(struct.pack(f"<{N}h", *integers))
        with (out/f"{name}.csv").open("w", newline="", encoding="utf-8") as stream:
            writer = csv.writer(stream)
            writer.writerow(("sample", "reference", "pcm8", "xromd1"))
            writer.writerows(zip(range(N), reference, pcm, decoded))
        for method, cycle in (("reference", reference), ("pcm8", pcm), ("xromd1", decoded)):
            wav(out/f"{name}_{method}.wav", cycle)
        print(f"Prepared {name}: exponents {info['exponents']}", flush=True)
    (out/"measurements.json").write_text(json.dumps(measurements, indent=2, sort_keys=True)+"\n", encoding="utf-8")
    return measurements


def hashes(folder: Path) -> dict[str, str]:
    return {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(folder.iterdir()) if p.is_file()}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[2]/"artifacts/xromd1-prototype")
    args = parser.parse_args()
    measurements = generate(args.output)
    first = hashes(args.output)
    # A regular workspace subdirectory avoids restrictive Windows temp ACLs.
    scratch = args.output.resolve()/"_second-run"
    scratch.mkdir(exist_ok=False)
    generate(scratch)
    second = hashes(scratch)
    # Ignore previous report/manifests if rerunning into the same output directory.
    generated = {k:v for k,v in first.items() if k in second}
    assert generated == second, "Local determinism failure"
    assert scratch.parent == args.output.resolve()
    for filename in second:
        (scratch/filename).unlink()
    scratch.rmdir()
    assert len(list(args.output.glob("*.wav"))) == 9
    manifest = {"verified_equal_local_runs": 2, "sha256": second}
    (args.output/"determinism.json").write_text(json.dumps(manifest, indent=2, sort_keys=True)+"\n", encoding="utf-8")
    lines = ["# XROM-D1 isolated prototype results", "",
             "3 seconds, 48000 Hz, mono PCM24; 375 Hz exact cycle repetition; common gain 0.5.",
             "Metrics below are normalized source-domain values BEFORE the common audition gain.", "",
             "| Source | Method | RMS error | Max error | DC | Peak | Error/source dB | Closure LSB | Saturations |",
             "|---|---|---:|---:|---:|---:|---:|---:|---:|"]
    for name, methods in measurements.items():
        for method, m in methods.items():
            closure = str(m["closure_error_lsb"]) if method == "xromd1" else "N/A (PCM)"
            lines.append(f"| {name} | {method} | {m['rms_error']:.8g} | {m['max_abs_error']:.8g} | {m['dc']:.8g} | {m['peak']:.8g} | {m['error_db_relative']:.2f} | {closure} | 0 |")
    lines += ["", "PCM8 has no predictor accumulator; its loop repeats exactly. Do not confuse",
              "predictor closure with first/last sample equality. See measurements.json for",
              "seam-delta error, exponent sequences and cyclic exponent-change counts.", "",
              "Two independent local generations matched every generated artifact byte-for-byte.",
              "See determinism.json for source, encoded, decoded, CSV, metrics and WAV hashes.", "",
              "No plugin integration or production changes. Listening preference remains for human review."]
    (args.output/"RESULTS.md").write_text("\n".join(lines)+"\n", encoding="utf-8")
    print("Two-run exact determinism PASS. Nine audition WAVs ready.", flush=True)


if __name__ == "__main__":
    main()
