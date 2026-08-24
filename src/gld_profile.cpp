#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstddef>
#include <cstdio>
#include <cstring>

#include "gld_tracy_config.h"
#include "../third_party/tracy/public/tracy/Tracy.hpp"
#include "../third_party/tracy/public/tracy/TracyC.h"

#include "gld_profile.h"
#include "gld_diag.h"

static INIT_ONCE g_profileOnce = INIT_ONCE_STATIC_INIT;
static volatile LONG g_profileState = 0; /* 0=off, 1=starting, 2=started, 3=stopped */
static char g_profileProgram[768] = "GLDirect/unknown";
static char g_profileAppInfo[2048];

static_assert(sizeof(GLDProfileSourceLocation) ==
              sizeof(___tracy_source_location_data),
              "GLDirect/Tracy source location ABI mismatch");
static_assert(offsetof(GLDProfileSourceLocation, name) ==
              offsetof(___tracy_source_location_data, name),
              "GLDirect/Tracy source name ABI mismatch");
static_assert(offsetof(GLDProfileSourceLocation, color) ==
              offsetof(___tracy_source_location_data, color),
              "GLDirect/Tracy source color ABI mismatch");

static void gldProfileLeafName(const char *path, char *out, size_t outSize,
                               bool removeExtension)
{
    const char *leaf;
    char *dot;

    if (!out || outSize == 0)
        return;
    out[0] = '\0';
    if (!path || !path[0])
        return;

    leaf = std::strrchr(path, '\\');
    if (!leaf)
        leaf = std::strrchr(path, '/');
    leaf = leaf ? leaf + 1 : path;
    strncpy_s(out, outSize, leaf, _TRUNCATE);

    if (removeExtension) {
        dot = std::strrchr(out, '.');
        if (dot && dot != out)
            *dot = '\0';
    }
}

static void gldProfileParentName(const char *path, char *out, size_t outSize)
{
    char parent[MAX_PATH];
    char *slash;

    if (!out || outSize == 0)
        return;
    out[0] = '\0';
    if (!path || !path[0])
        return;

    strncpy_s(parent, sizeof(parent), path, _TRUNCATE);
    slash = std::strrchr(parent, '\\');
    if (!slash)
        slash = std::strrchr(parent, '/');
    if (!slash)
        return;
    *slash = '\0';
    gldProfileLeafName(parent, out, outSize, false);
}

static BOOL CALLBACK gldProfileStartOnce(PINIT_ONCE, PVOID, PVOID *)
{
    char exePath[MAX_PATH] = "";
    char wrapperPath[MAX_PATH] = "";
    char exeName[MAX_PATH] = "game";
    char gameFolder[MAX_PATH] = "unknown";
    HMODULE wrapperModule = NULL;
    const char *architecture;

    InterlockedExchange(&g_profileState, 1);

    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    /* A Tracy worker executes code from this image.  A graphics wrapper has
     * no callback before FreeLibrary takes the loader lock, and joining that
     * worker from DLL_PROCESS_DETACH deadlocks.  Pinning after the first WGL
     * call makes its lifetime match the game process and prevents worker code
     * from ever being unmapped underneath the thread. */
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCSTR>(&gldProfileStartup),
                           &wrapperModule)) {
        GetModuleFileNameA(wrapperModule, wrapperPath, MAX_PATH);
    }

    gldProfileLeafName(exePath, exeName, sizeof(exeName), true);
    gldProfileParentName(exePath, gameFolder, sizeof(gameFolder));
    if (!exeName[0])
        strcpy_s(exeName, "game");
    if (!gameFolder[0])
        strcpy_s(gameFolder, "unknown");

#if defined(_WIN64)
    architecture = "x64";
#else
    architecture = "x86";
#endif

    _snprintf_s(g_profileProgram, sizeof(g_profileProgram), _TRUNCATE,
                "GLDirect/%s/%s/%s", gameFolder, exeName, architecture);
    _snprintf_s(g_profileAppInfo, sizeof(g_profileAppInfo), _TRUNCATE,
                "GLDirect embedded profiler\n"
                "Game: %s\n"
                "Wrapper: %s\n"
                "Architecture: %s\n"
                "Process ID: %lu\n"
                "Tracy: 0.13.1 (on-demand, localhost-only, no sampling)\n"
                "Lifetime: process-pinned after first WGL call",
                exePath[0] ? exePath : "(unavailable)",
                wrapperPath[0] ? wrapperPath : "(unavailable)",
                architecture, (unsigned long)GetCurrentProcessId());

    /* Manual lifetime is required for an injected DLL.  Nothing Tracy-owned
     * is constructed until this point, safely outside DllMain. */
    ___tracy_startup_profiler();
    TracySetProgramName(g_profileProgram);
    TracyCAppInfo(g_profileAppInfo, std::strlen(g_profileAppInfo));
    InterlockedExchange(&g_profileState, 2);
    gldDiagLog("Tracy: profiler active as %s (on-demand, localhost-only)",
               g_profileProgram);
    return TRUE;
}

extern "C" void gldProfileStartup(void)
{
    if (InterlockedCompareExchange(&g_profileState, 0, 0) == 0)
        InitOnceExecuteOnce(&g_profileOnce, gldProfileStartOnce, NULL, NULL);
}

extern "C" void gldProfileShutdown(void)
{
    if (InterlockedCompareExchange(&g_profileState, 3, 2) == 2)
        ___tracy_shutdown_profiler();
}

extern "C" int gldProfileIsStarted(void)
{
    return InterlockedCompareExchange(&g_profileState, 0, 0) == 2;
}

extern "C" const char *gldProfileProgramName(void)
{
    return g_profileProgram;
}

extern "C" GLDProfileZone
gldProfileZoneBegin(const GLDProfileSourceLocation *source)
{
    GLDProfileZone zone = { 0, 0 };

    if (source && gldProfileIsStarted()) {
        const ___tracy_source_location_data *tracySource =
            reinterpret_cast<const ___tracy_source_location_data *>(source);
        TracyCZoneCtx tracyZone = ___tracy_emit_zone_begin(tracySource, 1);
        zone.id = tracyZone.id;
        zone.active = tracyZone.active;
    }
    return zone;
}

extern "C" void gldProfileZoneEnd(GLDProfileZone *zone)
{
    if (zone && zone->active && gldProfileIsStarted()) {
        TracyCZoneCtx tracyZone = { zone->id, zone->active };
        ___tracy_emit_zone_end(tracyZone);
        zone->active = 0;
    }
}

extern "C" void gldProfileZoneValue(GLDProfileZone *zone, uint64_t value)
{
    if (zone && zone->active && gldProfileIsStarted()) {
        TracyCZoneCtx tracyZone = { zone->id, zone->active };
        ___tracy_emit_zone_value(tracyZone, value);
    }
}

extern "C" void gldProfileFrameMark(void)
{
    if (gldProfileIsStarted())
        ___tracy_emit_frame_mark("GLDirect frame");
}

extern "C" void gldProfileMessage(const char *message)
{
    if (message && message[0] && gldProfileIsStarted())
        ___tracy_emit_messageC(message, std::strlen(message), 0xE8A030u, 0);
}
