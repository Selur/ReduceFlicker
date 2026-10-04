/*
proc_table.h: Copyright (C) 2016  Oka Motofumi

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


#ifndef REDUCE_FLICKER_PROC_TABLE_H
#define REDUCE_FLICKER_PROC_TABLE_H

#include <cstdint>
#include <cstddef>


typedef void (*proc_filter_t)(
    uint8_t* dstp, const uint8_t* currp, const uint8_t** prevp,
    const uint8_t** nextp, int dstride, int cstride, int* pstride, int* nstride,
    size_t width, size_t height);


/*
 * Each instruction set lives in its own translation unit so it can be
 * compiled with the matching compiler flags. 'bps' selects the sample
 * type: 8 (uint8_t), 10 (int16_t, SSE2 only, for 9-15 bit samples),
 * 16 (uint16_t) or 32 (float). nullptr is returned for unknown combinations.
 */
proc_filter_t get_proc_c(int strength, bool aggressive, int bps);
proc_filter_t get_proc_sse2(int strength, bool aggressive, int bps);
proc_filter_t get_proc_sse41(int strength, bool aggressive, int bps);
proc_filter_t get_proc_avx2(int strength, bool aggressive, int bps);

#endif
