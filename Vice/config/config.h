#ifndef VICE_CONFIG_H
#define VICE_CONFIG_H

#define WINDOWS_COMPILE 1
#define USE_HEADLESSUI 1
#define HAVE_FASTSID 1
#define HAVE_RESID   1

// Standard headers available in modern MSVC.
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDBOOL_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_MEMORY_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_IO_H 1

// Windows uses 32-bit long, including on x64.
#define SIZEOF_UNSIGNED_SHORT 2
#define SIZEOF_UNSIGNED_INT   4
#define SIZEOF_UNSIGNED_LONG  4

// MSVC's default off_t is supplied by sys/types.h.
#define HAVE_OFF_T 1
#define HAVE_OFF_T_IN_SYS_TYPES 1

// Default modern MSVC time_t.
#define HAVE_TIME_T_IN_TIME_H 1
#define SIZEOF_TIME_T 8
#define TIME_T_IS_64BIT 1

// Leave unavailable/disabled features UNDEFINED.
// In particular: USE_GCC, UNIX_COMPILE, WORDS_BIGENDIAN,
// HAVE_UNISTD_H, and optional external libraries.

#define VERSION "3.10"

#endif
