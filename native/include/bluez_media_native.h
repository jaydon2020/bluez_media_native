#pragma once

#include <stdint.h>

#if _WIN32
#define BLUEZ_MEDIA_EXPORT __declspec(dllexport)
#else
#define BLUEZ_MEDIA_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// A very short-lived native function.
//
// For very short-lived functions, it is fine to call them on the main isolate.
// They block Dart execution while running, so only use this pattern for work
// that is guaranteed to complete quickly.
BLUEZ_MEDIA_EXPORT int sum(int a, int b);

// A longer-lived native function that occupies the calling thread.
//
// Do not call this kind of function on the main isolate in Flutter apps. Use a
// helper isolate for blocking native work.
BLUEZ_MEDIA_EXPORT int sum_long_running(int a, int b);

#ifdef __cplusplus
}
#endif
