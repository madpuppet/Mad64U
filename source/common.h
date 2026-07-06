#pragma once

#if defined(_WIN64)
#include "windows.h"
#endif

#include <iostream>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

#define VERSION "1.000"

// common types
typedef unsigned char u8;
typedef signed char i8;
typedef unsigned short u16;
typedef signed short i16;
typedef unsigned int u32;
typedef signed int i32;
typedef unsigned long long  u64;
typedef long long           i64;

extern void Log(const char* pFormat, ...);

inline int Max(int x, int y) { return x > y ? x : y; }
inline int Min(int x, int y) { return x < y ? x : y; }
inline int Abs(int x) { return x < 0 ? -x : x; }
inline float Clamp(float val, float min, float max) { return val < min ? min : (val > max ? max : val); }

inline size_t HashU16(size_t old, u16 value)
{
    return (old * 0x1234567 + value);
}
inline size_t HashU8(size_t old, u8 value)
{
    return (old * 0x1234567 + value * 0x7654321);
}

struct Rect
{
    float x, y, w, h;
    bool Contains(float xx, float yy) { return xx >= x && xx <= (x + w) && yy >= y && yy <= (y + h); }
};

struct Recti
{
    int x, y, w, h;
    bool Contains(int xx, int yy) { return xx >= x && xx < (x + w) && yy >= y && yy < (y + h); }
    bool Overlaps(const Recti& o) { return x < (o.x + o.w) && (x + w) >= o.x && y < (o.y + o.h) && (y + h); }
    SDL_FRect AsSDLFRect() const { return SDL_FRect((float)x, (float)y, (float)w, (float)h); }
};

struct Vec2i
{
    int x, y;
};
