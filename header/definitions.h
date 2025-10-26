#ifndef DEFINITIONS__H
#define DEFINITIONS__H

#include <cstdint>
#include <utility>
#include <array>
#include <vector>

typedef int8_t i8;
typedef int16_t i16;
typedef uint8_t u8;
typedef uint16_t u16;
typedef size_t usize;

constexpr u8 operator"" _u8(unsigned long long v) { return static_cast<u8>(v); }
constexpr u16 operator"" _u16(unsigned long long v) { return static_cast<u16>(v); }
constexpr usize operator"" _usize(unsigned long long v) { return static_cast<usize>(v); }

#endif