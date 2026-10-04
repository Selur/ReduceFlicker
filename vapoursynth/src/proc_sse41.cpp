/*
proc_sse41.cpp: Copyright (C) 2016  Oka Motofumi

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


#define RF_SSE2
#define RF_SSE41
#include "proc_simd_impl.h"


proc_filter_t get_proc_sse41(int strength, bool aggressive, int bps)
{
    switch (bps) {
    case 8:  return select_simd<uint8_t, __m128i, USE_SSE41>(strength, aggressive);
    case 16: return select_simd<uint16_t, __m128i, USE_SSE41>(strength, aggressive);
    case 32: return select_simd<float, __m128, USE_SSE41>(strength, aggressive);
    default: return nullptr;
    }
}
