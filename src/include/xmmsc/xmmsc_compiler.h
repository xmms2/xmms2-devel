#ifndef XMMSC_COMPILER_H
#define XMMSC_COMPILER_H

#ifdef _MSC_VER
#define inline __inline
#endif

#if defined (__GNUC__) && __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ > 1)
#define XMMS_DEPRECATED __attribute__((deprecated))
#else
#define XMMS_DEPRECATED
#endif

#endif
