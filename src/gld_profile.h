/*
 * Small C ABI around Tracy.  Most of GLDirect is C, while Tracy's client is
 * C++; keeping that boundary here avoids leaking C++ or profiler headers into
 * the renderer and makes every call a cheap no-op before startup/after stop.
 */
#ifndef GLD_PROFILE_H
#define GLD_PROFILE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GLDProfileSourceLocation {
    const char *name;
    const char *function;
    const char *file;
    uint32_t line;
    uint32_t color;
} GLDProfileSourceLocation;

typedef struct GLDProfileZone {
    uint32_t id;
    int32_t active;
} GLDProfileZone;

/* Startup is idempotent and intentionally deferred until the first WGL call,
 * outside DllMain's loader lock. */
void gldProfileStartup(void);
/* Available to an explicit, non-DllMain owner.  GLDirect pins itself after
 * startup, so ordinary game/OS teardown intentionally does not call this. */
void gldProfileShutdown(void);
int  gldProfileIsStarted(void);
const char *gldProfileProgramName(void);

GLDProfileZone gldProfileZoneBegin(const GLDProfileSourceLocation *source);
void gldProfileZoneEnd(GLDProfileZone *zone);
void gldProfileZoneValue(GLDProfileZone *zone, uint64_t value);
void gldProfileFrameMark(void);
void gldProfileMessage(const char *message);

#ifdef __cplusplus
}
#endif

#define GLD_PROFILE_ZONE_BEGIN(variable, displayName)                         \
    static const GLDProfileSourceLocation gldProfileSource_##variable = {    \
        (displayName), __FUNCTION__, __FILE__, (uint32_t)__LINE__, 0u        \
    };                                                                        \
    GLDProfileZone variable =                                                \
        gldProfileZoneBegin(&gldProfileSource_##variable)

#define GLD_PROFILE_ZONE_END(variable) gldProfileZoneEnd(&(variable))
#define GLD_PROFILE_ZONE_VALUE(variable, value) \
    gldProfileZoneValue(&(variable), (uint64_t)(value))

#endif /* GLD_PROFILE_H */
