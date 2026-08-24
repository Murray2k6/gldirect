#ifndef GLD_TRACY_CONFIG_H
#define GLD_TRACY_CONFIG_H

/*
 * Keep the Tracy feature set identical in the client implementation and in
 * every translation unit that calls its API.  This wrapper is injected into
 * games, so profiling must be low-impact when no viewer is attached and must
 * never expose a listener outside the local machine.
 */
#define TRACY_ENABLE
#define TRACY_ON_DEMAND
#define TRACY_ONLY_LOCALHOST
#define TRACY_NO_CODE_TRANSFER
#define TRACY_NO_SAMPLING

/* Tracy's documented delayed/manual start avoids constructing workers under
 * the loader lock.  Once started, gld_profile.cpp pins this injected graphics
 * wrapper for process lifetime; joining worker threads from DllMain would
 * deadlock on the Windows loader lock. */
#define TRACY_DELAYED_INIT
#define TRACY_MANUAL_LIFETIME

#endif /* GLD_TRACY_CONFIG_H */
