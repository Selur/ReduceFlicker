#!/usr/bin/env python3
"""Functional tests for the ReduceFlicker VapourSynth plugin (rdfl).

Usage: python3 test/test_reduceflicker.py [path/to/libReduceFlicker.so]

Without arguments the plugin is expected to be autoloaded (e.g. from the
installed wheel). Every strength / aggressive / opt combination is compared
against a straightforward numpy transcription of the algorithm, bit for bit
for integer formats and within rounding for float.

Requires the vapoursynth Python module and numpy.
"""
import sys

import numpy as np
import vapoursynth as vs

core = vs.core

WIDTH, HEIGHT, FRAMES = 72, 40, 14
SEED = 4321


def flicker_clip(fmt, frames=FRAMES):
    """A static picture with noise whose brightness flickers from frame to frame."""
    f = core.get_video_format(fmt)
    depth = f.bits_per_sample
    is_float = f.sample_type == vs.FLOAT
    rng = np.random.default_rng(SEED)
    base = core.std.BlankClip(width=WIDTH, height=HEIGHT, length=frames, format=fmt)
    planes = []
    for p in range(f.num_planes):
        w = WIDTH >> (f.subsampling_w if p else 0)
        h = HEIGHT >> (f.subsampling_h if p else 0)
        yy, xx = np.mgrid[0:h, 0:w]
        picture = (xx * 255 / max(w - 1, 1) + yy * 255 / max(h - 1, 1)) / 2
        out = []
        for n in range(frames):
            img = picture + rng.normal(0, 3, size=(h, w)) + 12 * (1 if n % 2 else -1)
            img = np.clip(img, 0, 255)
            if is_float:
                out.append((img / 255.0).astype(np.float32))
            else:
                out.append(np.rint(img).astype(np.int64) << (depth - 8))
        planes.append(out)

    dtype = np.float32 if is_float else (np.uint8 if depth == 8 else np.uint16)

    def fill(n, f):
        f = f.copy()
        for p in range(f.format.num_planes):
            np.asarray(f[p])[:] = planes[p][n].astype(dtype)
        return f

    return core.std.ModifyFrame(base, base, fill), planes


def to_arrays(clip):
    out = []
    is_float = clip.format.sample_type == vs.FLOAT
    for n in range(clip.num_frames):
        f = clip.get_frame(n)
        out.append([np.array(f[p], dtype=np.float64 if is_float else np.int64)
                    for p in range(f.format.num_planes)])
    return out


def check(cond, msg):
    if not cond:
        raise SystemExit("FAIL: " + msg)
    print("ok  ", msg)


def reference(frames, strength, aggressive):
    """numpy transcription of proc_filter.h for one plane."""
    count = len(frames)
    is_float = frames[0].dtype == np.float32
    frames = [f.astype(np.float64 if is_float else np.int64) for f in frames]
    out = []
    for n in range(count):
        def frame(k):
            return frames[min(max(n + k, 0), count - 1)]

        s = frame(0)
        p1, n1, p2 = frame(-1), frame(1), frame(-2)
        others = []
        if strength >= 2:
            others.append(frame(2))
        if strength >= 3:
            others += [frame(-3), frame(3)]

        if aggressive:
            d1 = p2 - s
            d2 = np.where(d1 < 0, -d1, 0)
            d1 = np.where(d1 < 0, 0, d1)
            for o in others:
                d = o - s
                pos = d >= 0
                nd1 = np.where(pos, np.minimum(d, d1), 0)
                nd2 = np.where(pos, 0, np.minimum(-d, d2))
                d1, d2 = nd1, nd2
        else:
            d1 = np.abs(p2 - s)
            for o in others:
                d1 = np.minimum(d1, np.abs(o - s))
            d2 = d1

        if is_float:
            avg = (p1 + n1 + s + s) * 0.25
        else:
            avg = np.maximum((p1 + n1 + 1) // 2 - 1, 0)
            avg = (avg + s + 1) // 2
        ul = np.maximum(np.minimum(p1, n1) - d1, s)
        ll = np.minimum(np.maximum(p1, n1) + d2, s)
        out.append(np.minimum(ul, np.maximum(ll, avg)))
    return out


def main():
    args = sys.argv[1:]
    if args:
        core.std.LoadPlugin(args[0])
    rf = core.rdfl.ReduceFlicker

    # 1. Every variant against the reference: all strengths, aggressive or
    #    not, every optimisation level (the C, SSE2, SSE4.1 and AVX2 routines
    #    must agree bit for bit), 8 to 16 bit integer and 32 bit float.
    formats = [vs.YUV420P8, vs.GRAY8, vs.YUV422P10, vs.GRAY12, vs.YUV444P14,
               vs.YUV444P16, vs.GRAYS, vs.RGBS]
    for fmt in formats:
        name = core.get_video_format(fmt).name
        src, planes = flicker_clip(fmt)
        is_float = core.get_video_format(fmt).sample_type == vs.FLOAT
        for strength in (1, 2, 3):
            for aggressive in (0, 1):
                refs = [reference(plane, strength, aggressive) for plane in planes]
                for opt in (0, 1, 2, 3):
                    out = to_arrays(rf(src, strength=strength, aggressive=aggressive, opt=opt))
                    for p, ref in enumerate(refs):
                        if is_float:
                            worst = max(float(np.abs(a[p] - r).max()) for a, r in zip(out, ref))
                            same = worst <= 1e-6
                        else:
                            same = all(np.array_equal(a[p], r) for a, r in zip(out, ref))
                        check(same, "%s strength=%d aggressive=%d opt=%d plane %d matches the reference"
                              % (name, strength, aggressive, opt, p))

    # 2. The flicker is actually reduced: the frame to frame brightness jump
    #    of the static picture must shrink considerably.
    src, _ = flicker_clip(vs.GRAY8)
    means_in = [a[0].mean() for a in to_arrays(src)]
    jump_in = np.mean(np.abs(np.diff(means_in)))
    for strength in (1, 2, 3):
        means_out = [a[0].mean() for a in to_arrays(rf(src, strength=strength))]
        jump_out = np.mean(np.abs(np.diff(means_out)))
        check(jump_out < jump_in * 0.25,
              "strength=%d reduces the brightness flicker (%.2f -> %.2f)" % (strength, jump_in, jump_out))

    # 3. Higher bit depths filter the same picture to (almost) the same result.
    src8, _ = flicker_clip(vs.YUV420P8)
    out8 = to_arrays(rf(src8))
    for fmt, depth in [(vs.YUV420P10, 10), (vs.YUV420P16, 16)]:
        src, _ = flicker_clip(fmt)
        out = to_arrays(rf(src))
        worst = max(int(np.abs((a >> (depth - 8)) - b).max()) for fa, fb in zip(out, out8) for a, b in zip(fa, fb))
        check(worst <= 1, "%d bit output matches 8 bit output within 1 (max diff %d)" % (depth, worst))

    # 4. planes: unprocessed planes are copied from the source.
    src, planes = flicker_clip(vs.YUV444P8)
    out = to_arrays(rf(src, planes=[0, 1, 1]))
    check(all(np.array_equal(a[0], planes[0][n]) for n, a in enumerate(out)), "planes=[0,1,1] copies luma")
    check(all(np.array_equal(a[1], r) for a, r in zip(out, reference(planes[1], 2, 0))), "planes=[0,1,1] filters chroma")
    out = to_arrays(rf(src, planes=[0, 0, 0]))
    check(all(np.array_equal(a[p], planes[p][n]) for n, a in enumerate(out) for p in range(3)),
          "planes=[0,0,0] copies everything")

    # 5. Default parameters are strength=2, aggressive=0, all planes.
    src, planes = flicker_clip(vs.GRAY8)
    check(all(np.array_equal(a[0], r) for a, r in zip(to_arrays(rf(src)), reference(planes[0], 2, 0))),
          "default parameters are strength=2, aggressive=0")

    # 6. Out of order frame requests work, including at the clip edges.
    clip = rf(flicker_clip(vs.YUV420P16)[0], strength=3)
    for n in [13, 3, 0, 11, 7, 1]:
        clip.get_frame(n)
    check(True, "out of order frame requests work")

    # 7. Very short clips work (every neighbour is clamped to the clip).
    short = rf(flicker_clip(vs.GRAY8, frames=2)[0], strength=3)
    check(short.num_frames == 2 and short.get_frame(1) is not None, "a 2 frame clip is filtered")

    # 8. Frame properties are carried over from the source frame.
    src = core.std.SetFrameProps(flicker_clip(vs.GRAY8)[0], MyProp=7)
    check(rf(src).get_frame(5).props["MyProp"] == 7, "frame properties are copied")

    # 9. Invalid input is rejected.
    def rejects(msg, **kwargs):
        try:
            rf(**kwargs)
        except vs.Error:
            check(True, msg)
        else:
            raise SystemExit("FAIL: accepted: " + msg)

    clip = flicker_clip(vs.GRAY8)[0]
    rejects("strength=4 is rejected", clip=clip, strength=4)
    rejects("strength=0 is rejected", clip=clip, strength=0)
    rejects("planes=[2] is rejected", clip=clip, planes=[2])
    rejects("planes with 4 entries is rejected", clip=clip, planes=[1, 1, 1, 1])
    rejects("half precision float is rejected",
            clip=core.std.BlankClip(width=WIDTH, height=HEIGHT, length=FRAMES, format=vs.GRAYH))
    rejects("variable format is rejected",
            clip=core.std.Splice([core.std.BlankClip(width=WIDTH, height=HEIGHT, length=FRAMES, format=vs.GRAY8),
                                  core.std.BlankClip(width=WIDTH, height=HEIGHT, length=FRAMES, format=vs.GRAY16)],
                                 mismatch=True))

    print("all tests passed")


if __name__ == "__main__":
    main()
