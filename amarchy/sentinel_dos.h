#ifndef AMARCHY_SENTINEL_DOS_H
#define AMARCHY_SENTINEL_DOS_H

/* Publishes n as the global variable name (a rig case reads it after the run). */
void am_sentinel(const char *name, long n);
/* The same for a text value. */
void am_sentinel_str(const char *name, const char *value);

#endif
