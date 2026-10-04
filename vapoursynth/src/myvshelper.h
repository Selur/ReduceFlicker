/*
myvshelper.h: Copyright (C) 2016  Oka Motofumi

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


#ifndef MY_VAPOURSYNTH_HELPER_H
#define MY_VAPOURSYNTH_HELPER_H

#include <cstdint>
#include <string>
#include <VapourSynth4.h>
#include "arch.h"


static F_INLINE void validate(bool cond, const char* msg)
{
    if (cond) {
        throw std::string(msg);
    }
}

static F_INLINE bool is_half_precision(const VSVideoInfo& vi)
{
    return vi.format.sampleType == stFloat && vi.format.bitsPerSample == 16;
}


template <typename T>
static F_INLINE T get_prop(const VSAPI*, const VSMap*, const char*, int, int*);

template <>
F_INLINE int32_t
get_prop<int32_t>(const VSAPI* api, const VSMap* in, const char* name, int idx,
                  int* e)
{
    return static_cast<int32_t>(api->mapGetInt(in, name, idx, e));
}

template <>
F_INLINE int64_t
get_prop<int64_t>(const VSAPI* api, const VSMap* in, const char* name, int idx,
                  int* e)
{
    return api->mapGetInt(in, name, idx, e);
}

template <>
F_INLINE bool
get_prop<bool>(const VSAPI* api, const VSMap* in, const char* name, int idx,
               int* e)
{
    return api->mapGetInt(in, name, idx, e) != 0;
}

template <>
F_INLINE float
get_prop<float>(const VSAPI* api, const VSMap* in, const char* name, int idx,
                int* e)
{
    return static_cast<float>(api->mapGetFloat(in, name, idx, e));
}

template <>
F_INLINE double
get_prop<double>(const VSAPI* api, const VSMap* in, const char* name, int idx,
                 int* e)
{
    return api->mapGetFloat(in, name, idx, e);
}

template <>
F_INLINE const char*
get_prop<const char*>(const VSAPI* api, const VSMap* in, const char* name,
                      int idx, int* e)
{
    return api->mapGetData(in, name, idx, e);
}

template <typename T>
static inline T
get_arg(const char* name, const T default_value, int index, const VSMap* in,
        const VSAPI* api)
{
    int err = 0;
    T ret = get_prop<T>(api, in, name, index, &err);
    if (err) {
        ret = default_value;
    }
    return ret;
}

#endif
