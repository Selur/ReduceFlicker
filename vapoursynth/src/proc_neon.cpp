/*
proc_neon.cpp: NEON routines for arm64.

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


#include <arm_neon.h>
#include <type_traits>

#include "proc_table.h"
#include "proc_filter.h"


/*
 * One set of primitives per sample type. They follow the C++ routines of
 * proc_filter.h step by step, so the output is identical:
 *
 *   integer: (a + b + 1) / 2 is a rounding halving add, "- 1, clipped at 0" a
 *            saturating subtract. min(prev, next) - d and max(prev, next) + d
 *            are only compared with the current sample afterwards, so the
 *            saturating forms give the same result as the wider C++ types.
 *   float:   same operations in the same order as the C++ routine.
 */
struct NeonU8 {
    typedef uint8_t T;
    typedef uint8x16_t V;
    enum { LANES = 16 };
    static F_INLINE V load(const T* p) { return vld1q_u8(p); }
    static F_INLINE void store(T* p, V v) { vst1q_u8(p, v); }
    static F_INLINE V zero() { return vdupq_n_u8(0); }
    static F_INLINE V vmin(V a, V b) { return vminq_u8(a, b); }
    static F_INLINE V vmax(V a, V b) { return vmaxq_u8(a, b); }
    static F_INLINE V absdiff(V a, V b) { return vabdq_u8(a, b); }
    // max(a - b, 0)
    static F_INLINE V subpos(V a, V b) { return vqsubq_u8(a, b); }
    static F_INLINE V sub(V a, V b) { return vqsubq_u8(a, b); }
    static F_INLINE V add(V a, V b) { return vqaddq_u8(a, b); }
    static F_INLINE V ge(V a, V b) { return vcgeq_u8(a, b); }
    static F_INLINE V select(V mask, V a, V b) { return vbslq_u8(mask, a, b); }
    static F_INLINE V avg(V a, V b, V x)
    {
        return vrhaddq_u8(vqsubq_u8(vrhaddq_u8(a, b), vdupq_n_u8(1)), x);
    }
};

struct NeonU16 {
    typedef uint16_t T;
    typedef uint16x8_t V;
    enum { LANES = 8 };
    static F_INLINE V load(const T* p) { return vld1q_u16(p); }
    static F_INLINE void store(T* p, V v) { vst1q_u16(p, v); }
    static F_INLINE V zero() { return vdupq_n_u16(0); }
    static F_INLINE V vmin(V a, V b) { return vminq_u16(a, b); }
    static F_INLINE V vmax(V a, V b) { return vmaxq_u16(a, b); }
    static F_INLINE V absdiff(V a, V b) { return vabdq_u16(a, b); }
    static F_INLINE V subpos(V a, V b) { return vqsubq_u16(a, b); }
    static F_INLINE V sub(V a, V b) { return vqsubq_u16(a, b); }
    static F_INLINE V add(V a, V b) { return vqaddq_u16(a, b); }
    static F_INLINE V ge(V a, V b) { return vcgeq_u16(a, b); }
    static F_INLINE V select(V mask, V a, V b) { return vbslq_u16(mask, a, b); }
    static F_INLINE V avg(V a, V b, V x)
    {
        return vrhaddq_u16(vqsubq_u16(vrhaddq_u16(a, b), vdupq_n_u16(1)), x);
    }
};

struct NeonF32 {
    typedef float T;
    typedef float32x4_t V;
    enum { LANES = 4 };
    static F_INLINE V load(const T* p) { return vld1q_f32(p); }
    static F_INLINE void store(T* p, V v) { vst1q_f32(p, v); }
    static F_INLINE V zero() { return vdupq_n_f32(0.0f); }
    static F_INLINE V vmin(V a, V b) { return vminq_f32(a, b); }
    static F_INLINE V vmax(V a, V b) { return vmaxq_f32(a, b); }
    static F_INLINE V absdiff(V a, V b) { return vabdq_f32(a, b); }
    static F_INLINE V subpos(V a, V b) { return vmaxq_f32(vsubq_f32(a, b), vdupq_n_f32(0.0f)); }
    static F_INLINE V sub(V a, V b) { return vsubq_f32(a, b); }
    static F_INLINE V add(V a, V b) { return vaddq_f32(a, b); }
    static F_INLINE V ge(V a, V b) { return vreinterpretq_f32_u32(vcgeq_f32(a, b)); }
    static F_INLINE V select(V mask, V a, V b) { return vbslq_f32(vreinterpretq_u32_f32(mask), a, b); }
    static F_INLINE V avg(V a, V b, V x)
    {
        return vmulq_n_f32(vaddq_f32(vaddq_f32(vaddq_f32(a, b), x), x), 0.25f);
    }
};


template <typename N, int STRENGTH>
static F_INLINE typename N::V
filter(const typename N::T* cur, const typename N::T* prv0, const typename N::T* prv1,
       const typename N::T* prv2, const typename N::T* nxt0, const typename N::T* nxt1,
       const typename N::T* nxt2, size_t x)
{
    typedef typename N::V V;
    const V curx = N::load(cur + x);
    V d = N::absdiff(curx, N::load(prv1 + x));
    if (STRENGTH > 1) {
        d = N::vmin(d, N::absdiff(curx, N::load(nxt1 + x)));
    }
    if (STRENGTH > 2) {
        d = N::vmin(d, N::absdiff(curx, N::load(prv2 + x)));
        d = N::vmin(d, N::absdiff(curx, N::load(nxt2 + x)));
    }
    const V pr0 = N::load(prv0 + x);
    const V nx0 = N::load(nxt0 + x);
    const V ul = N::vmax(N::sub(N::vmin(pr0, nx0), d), curx);
    const V ll = N::vmin(N::add(N::vmax(pr0, nx0), d), curx);
    return N::vmin(N::vmax(N::avg(pr0, nx0, curx), ll), ul);
}


// x >= y: d1 = min(x - y, d1), d2 = 0; otherwise d1 = 0, d2 = min(y - x, d2)
template <typename N>
static F_INLINE void
update_diff_neon(typename N::V x, typename N::V y, typename N::V& d1, typename N::V& d2)
{
    typedef typename N::V V;
    const V mask = N::ge(x, y);
    const V zero = N::zero();
    d1 = N::select(mask, N::vmin(N::subpos(x, y), d1), zero);
    d2 = N::select(mask, zero, N::vmin(N::subpos(y, x), d2));
}


template <typename N, int STRENGTH>
static F_INLINE typename N::V
filter_a(const typename N::T* cur, const typename N::T* prv0, const typename N::T* prv1,
         const typename N::T* prv2, const typename N::T* nxt0, const typename N::T* nxt1,
         const typename N::T* nxt2, size_t x)
{
    typedef typename N::V V;
    const V curx = N::load(cur + x);
    const V p1 = N::load(prv1 + x);
    V d1 = N::subpos(p1, curx);
    V d2 = N::subpos(curx, p1);
    if (STRENGTH > 1) {
        update_diff_neon<N>(N::load(nxt1 + x), curx, d1, d2);
    }
    if (STRENGTH > 2) {
        update_diff_neon<N>(N::load(prv2 + x), curx, d1, d2);
        update_diff_neon<N>(N::load(nxt2 + x), curx, d1, d2);
    }
    const V pr0 = N::load(prv0 + x);
    const V nx0 = N::load(nxt0 + x);
    const V ul = N::vmax(N::sub(N::vmin(pr0, nx0), d1), curx);
    const V ll = N::vmin(N::add(N::vmax(pr0, nx0), d2), curx);
    return N::vmin(N::vmax(N::avg(pr0, nx0, curx), ll), ul);
}


/*
 * Rows are processed in whole vectors; the rest of a row is covered by one
 * more vector that ends at the last sample and overlaps the previous one.
 * Nothing is read or written beyond the row, and the destination is never a
 * source, so the overlap is harmless. Rows narrower than one vector go to
 * the C++ routine.
 */
template <typename N, int STRENGTH, bool AGGRESSIVE>
static void
proc_neon(uint8_t* dstp, const uint8_t* currp, const uint8_t** prevp,
          const uint8_t** nextp, int dstride, int cstride, int* pstride,
          int* nstride, size_t width, size_t height) noexcept
{
    typedef typename N::T T;
    const size_t lanes = N::LANES;

    if (width < lanes) {
        if (AGGRESSIVE) {
            proc_a_c<T, typename std::conditional<std::is_floating_point<T>::value, float, int>::type, STRENGTH>(
                dstp, currp, prevp, nextp, dstride, cstride, pstride, nstride, width, height);
        } else {
            proc_c<T, typename std::conditional<std::is_floating_point<T>::value, float, int>::type, STRENGTH>(
                dstp, currp, prevp, nextp, dstride, cstride, pstride, nstride, width, height);
        }
        return;
    }

    const uint8_t* prv0 = prevp[0];
    const uint8_t* prv1 = prevp[1];
    const uint8_t* nxt0 = nextp[0];
    // unused pointers alias a valid row, they are never loaded from
    const uint8_t* nxt1 = STRENGTH > 1 ? nextp[1] : nxt0;
    const uint8_t* prv2 = STRENGTH > 2 ? prevp[2] : prv0;
    const uint8_t* nxt2 = STRENGTH > 2 ? nextp[2] : nxt0;
    const int ns1 = STRENGTH > 1 ? nstride[1] : 0;
    const int ps2 = STRENGTH > 2 ? pstride[2] : 0;
    const int ns2 = STRENGTH > 2 ? nstride[2] : 0;

    const size_t last = width - lanes;

    for (size_t y = 0; y < height; ++y) {
        T* d = reinterpret_cast<T*>(dstp);
        const T* c = reinterpret_cast<const T*>(currp);
        const T* p0 = reinterpret_cast<const T*>(prv0);
        const T* p1 = reinterpret_cast<const T*>(prv1);
        const T* p2 = reinterpret_cast<const T*>(prv2);
        const T* n0 = reinterpret_cast<const T*>(nxt0);
        const T* n1 = reinterpret_cast<const T*>(nxt1);
        const T* n2 = reinterpret_cast<const T*>(nxt2);

        size_t x = 0;
        for (; x <= last; x += lanes) {
            N::store(d + x, AGGRESSIVE ? filter_a<N, STRENGTH>(c, p0, p1, p2, n0, n1, n2, x)
                                       : filter<N, STRENGTH>(c, p0, p1, p2, n0, n1, n2, x));
        }
        if (x < width) {
            N::store(d + last, AGGRESSIVE ? filter_a<N, STRENGTH>(c, p0, p1, p2, n0, n1, n2, last)
                                          : filter<N, STRENGTH>(c, p0, p1, p2, n0, n1, n2, last));
        }

        dstp += dstride;
        currp += cstride;
        prv0 += pstride[0];
        prv1 += pstride[1];
        nxt0 += nstride[0];
        nxt1 += ns1;
        prv2 += ps2;
        nxt2 += ns2;
    }
}


template <typename N>
static proc_filter_t select_neon(int strength, bool aggressive)
{
    switch (strength) {
    case 1:  return aggressive ? proc_neon<N, 1, true> : proc_neon<N, 1, false>;
    case 2:  return aggressive ? proc_neon<N, 2, true> : proc_neon<N, 2, false>;
    default: return aggressive ? proc_neon<N, 3, true> : proc_neon<N, 3, false>;
    }
}


proc_filter_t get_proc_neon(int strength, bool aggressive, int bps)
{
    switch (bps) {
    case 8:  return select_neon<NeonU8>(strength, aggressive);
    case 16: return select_neon<NeonU16>(strength, aggressive);
    case 32: return select_neon<NeonF32>(strength, aggressive);
    default: return nullptr;
    }
}
