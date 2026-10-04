/*
proc_simd_impl.h: Copyright (C) 2016  Oka Motofumi

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


/* Included by the per instruction set translation units after they
 * defined RF_SSE2 / RF_SSE41 / RF_AVX2. */

#ifndef REDUCE_FLICKER_PROC_SIMD_IMPL_H
#define REDUCE_FLICKER_PROC_SIMD_IMPL_H

#include "proc_table.h"
#include "proc_filter.h"


template <typename T, typename V, arch_t ARCH>
static proc_filter_t select_simd(int strength, bool aggressive)
{
    switch (strength) {
    case 1:  return aggressive ? proc_a_simd<T, V, 1, ARCH> : proc_simd<T, V, 1, ARCH>;
    case 2:  return aggressive ? proc_a_simd<T, V, 2, ARCH> : proc_simd<T, V, 2, ARCH>;
    default: return aggressive ? proc_a_simd<T, V, 3, ARCH> : proc_simd<T, V, 3, ARCH>;
    }
}

#endif
