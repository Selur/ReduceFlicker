/*
proc_c.cpp: Copyright (C) 2016  Oka Motofumi

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


#include "proc_table.h"
#include "proc_filter.h"


template <typename T0, typename T1>
static proc_filter_t select_c(int strength, bool aggressive)
{
    switch (strength) {
    case 1:  return aggressive ? proc_a_c<T0, T1, 1> : proc_c<T0, T1, 1>;
    case 2:  return aggressive ? proc_a_c<T0, T1, 2> : proc_c<T0, T1, 2>;
    default: return aggressive ? proc_a_c<T0, T1, 3> : proc_c<T0, T1, 3>;
    }
}


proc_filter_t get_proc_c(int strength, bool aggressive, int bps)
{
    switch (bps) {
    case 8:  return select_c<uint8_t, int>(strength, aggressive);
    case 16: return select_c<uint16_t, int>(strength, aggressive);
    case 32: return select_c<float, float>(strength, aggressive);
    default: return nullptr;
    }
}
