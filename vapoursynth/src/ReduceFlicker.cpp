/*
ReduceFlicker.cpp: Copyright (C) 2016  Oka Motofumi

Author: Oka Motofumi (chikuzen.mo at gmail dot com)

This file is part of ReduceFlicker.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with the author; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/


#include <algorithm>
#include <cstring>
#include <VSHelper4.h>
#include "ReduceFlicker.h"
#include "myvshelper.h"


template <int STRENGTH>
static void
request_frames(int n, int nf, VSNode* clip, const VSAPI* api,
    VSFrameContext* ctx) noexcept
{
    // Frames beyond the ends of the clip are clamped, so near the edges the
    // same frame is needed several times; request each one only once.
    int wanted[7];
    int count = 0;
    auto request = [&](int f) {
        for (int i = 0; i < count; ++i) {
            if (wanted[i] == f) return;
        }
        wanted[count++] = f;
        api->requestFrameFilter(f, clip, ctx);
    };

    request(n);
    request(std::max(n - 1, 0));
    request(std::max(n - 2, 0));
    request(std::min(n + 1, nf));
    if (STRENGTH > 1) {
        request(std::min(n + 2, nf));
        if (STRENGTH > 2) {
            request(std::max(n - 3, 0));
            request(std::min(n + 3, nf));
        }
    }
}


template <int STRENGTH>
static void
recieve_frames(const VSFrame** curr, const VSFrame** prev,
    const VSFrame** next, int n, int nf, VSNode* clip,
    const VSAPI* api, VSFrameContext* ctx) noexcept
{
    *curr = api->getFrameFilter(n, clip, ctx);
    prev[0] = api->getFrameFilter(std::max(n - 1, 0), clip, ctx);
    prev[1] = api->getFrameFilter(std::max(n - 2, 0), clip, ctx);
    next[0] = api->getFrameFilter(std::min(n + 1, nf), clip, ctx);
    if (STRENGTH > 1) {
        next[1] = api->getFrameFilter(std::min(n + 2, nf), clip, ctx);
        if (STRENGTH > 2) {
            prev[2] = api->getFrameFilter(std::max(n - 3, 0), clip, ctx);
            next[2] = api->getFrameFilter(std::min(n + 3, nf), clip, ctx);
        }
    }
}


template <int STRENGTH>
static void
prepare_pointers(const uint8_t** currp, const uint8_t** prevp,
    const uint8_t** nextp, int& cstride, int* pstride,
    int* nstride, const VSFrame* curr, const VSFrame** prev,
    const VSFrame** next, int plane, const VSAPI* api) noexcept
{
    *currp = api->getReadPtr(curr, plane);
    cstride = static_cast<int>(api->getStride(curr, plane));
    prevp[0] = api->getReadPtr(prev[0], plane);
    pstride[0] = static_cast<int>(api->getStride(prev[0], plane));
    prevp[1] = api->getReadPtr(prev[1], plane);
    pstride[1] = static_cast<int>(api->getStride(prev[1], plane));
    nextp[0] = api->getReadPtr(next[0], plane);
    nstride[0] = static_cast<int>(api->getStride(next[0], plane));
    if (STRENGTH > 1) {
        nextp[1] = api->getReadPtr(next[1], plane);
        nstride[1] = static_cast<int>(api->getStride(next[1], plane));
        if (STRENGTH > 2) {
            prevp[2] = api->getReadPtr(prev[2], plane);
            pstride[2] = static_cast<int>(api->getStride(prev[2], plane));
            nextp[2] = api->getReadPtr(next[2], plane);
            nstride[2] = static_cast<int>(api->getStride(next[2], plane));
        }
    }
}


static proc_filter_t
get_main_proc(arch_t arch, int strength, bool aggressive, const VSVideoFormat& fmt)
{
    // 8: uint8_t, 16: uint16_t, 32: float. SSE2 has a faster signed 16 bit
    // variant for samples that fit into 15 bits.
    int bps = fmt.bytesPerSample * 8;
    if (arch == USE_SSE2 && fmt.sampleType == stInteger && bps == 16
        && fmt.bitsPerSample <= 15) {
        bps = 10;
    }

    switch (arch) {
#if defined(INTEL_X86_CPU)
    case USE_AVX2:  return get_proc_avx2(strength, aggressive, bps);
    case USE_SSE41: return get_proc_sse41(strength, aggressive, bps);
    case USE_SSE2:  return get_proc_sse2(strength, aggressive, bps);
#endif
#if defined(ARM64_CPU)
    case USE_NEON:  return get_proc_neon(strength, aggressive, bps);
#endif
    default:        return get_proc_c(strength, aggressive, bps);
    }
}


ReduceFlicker::
ReduceFlicker(VSNode* c, int s, bool aggressive, int* planes, arch_t arch,
              VSCore* core, const VSAPI* api) :
    strength(s), clip(c)
{
    (void)core;

    vi = api->getVideoInfo(clip);
    validate(!vsh::isConstantVideoFormat(vi), "clip is not constant format.");
    validate(is_half_precision(*vi), "half precision is not supported.");
    validate(vi->format.sampleType == stInteger && vi->format.bitsPerSample > 16,
             "integer samples must be 8 to 16 bits.");

    memcpy(procType, planes, sizeof(int) * 3);

    switch (strength) {
    case 1:
        requestFrames = request_frames<1>;
        recieveFrames = recieve_frames<1>;
        prepareSrcPtrs = prepare_pointers<1>;
        break;
    case 2:
        requestFrames = request_frames<2>;
        recieveFrames = recieve_frames<2>;
        prepareSrcPtrs = prepare_pointers<2>;
        break;
    default:
        requestFrames = request_frames<3>;
        recieveFrames = recieve_frames<3>;
        prepareSrcPtrs = prepare_pointers<3>;
    }

    mainProc = get_main_proc(arch, strength, aggressive, vi->format);
    validate(mainProc == nullptr, "unsupported format.");
}



const VSFrame* ReduceFlicker::
getFrame(int n, VSCore* core, const VSAPI* api, VSFrameContext* ctx)
{
    const int nf = vi->numFrames - 1;
    const VSFrame *curr = nullptr, *prev[3] = {}, *next[3] = {};

    recieveFrames(&curr, prev, next, n, nf, clip, api, ctx);

    const VSVideoFormat* fmt = api->getVideoFrameFormat(curr);

    // Planes that are not processed are shared with the source frame.
    const VSFrame* plane_src[3] = {
        procType[0] ? nullptr : curr,
        procType[1] ? nullptr : curr,
        procType[2] ? nullptr : curr,
    };
    const int plane_idx[3] = { 0, 1, 2 };

    VSFrame* dst = api->newVideoFrame2(fmt, api->getFrameWidth(curr, 0),
                                       api->getFrameHeight(curr, 0),
                                       plane_src, plane_idx, curr, core);

    for (int p = 0; p < fmt->numPlanes; ++p) {
        if (procType[p] == 0) {
            continue;
        }

        int cstride, pstride[3], nstride[3];
        const uint8_t *currp, *prevp[3], *nextp[3];
        prepareSrcPtrs(&currp, prevp, nextp, cstride, pstride, nstride, curr,
                       prev, next, p, api);

        uint8_t* dstp = api->getWritePtr(dst, p);
        int dstride = static_cast<int>(api->getStride(dst, p));
        size_t width = api->getFrameWidth(dst, p);
        size_t height = api->getFrameHeight(dst, p);

        mainProc(dstp, currp, prevp, nextp, dstride, cstride, pstride, nstride,
                 width, height);
    }

    api->freeFrame(curr);
    api->freeFrame(prev[0]);
    api->freeFrame(prev[1]);
    api->freeFrame(next[0]);
    if (strength > 1) {
        api->freeFrame(next[1]);
    }
    if (strength > 2) {
        api->freeFrame(prev[2]);
        api->freeFrame(next[2]);
    }

    return dst;
}
