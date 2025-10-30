#ifndef INLINE_CONTROL_H
#define INLINE_CONTROL_H

#if defined(NO_INLINE_HINTS)
#  if defined(__clang__) || defined(__GNUC__)
#    define RAYTRACER_INLINE __attribute__((noinline))
#  elif defined(_MSC_VER)
#    define RAYTRACER_INLINE __declspec(noinline)
#  else
#    define RAYTRACER_INLINE
#  endif
#else
#  define RAYTRACER_INLINE inline
#endif

#endif
