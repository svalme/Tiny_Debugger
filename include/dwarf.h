// dwarf.h - DWARF debug info: address -> function name + source location
#ifndef DWARF_H
#define DWARF_H

#include "types.h"
#include <dwarf.h>

// Opaque handle wrapping a libdw Dwarf* session.
// Open once at startup, pass around, close at exit.
typedef struct dwarf_session dwarf_session_t;

// Result of a single address lookup. All string pointers point into
// libdw-owned memory and are valid for the lifetime of the session.
typedef struct {
    const char *function;   // e.g. "main"          (NULL if not found)
    const char *srcfile;    // e.g. "test.c"         (NULL if not found)
    int         line;       // 1-based line number   (0 if not found)
} dwarf_location_t;

// Open the named ELF binary for DWARF queries.
// Returns NULL and prints a message if the file has no debug info.
dwarf_session_t *dwarf_open(const char *path);

// Look up the function name and source location for a code address.
// Returns a zeroed struct if nothing is found.
dwarf_location_t dwarf_lookup(dwarf_session_t *s, uint64_t addr);

// Release all resources.
void dwarf_close(dwarf_session_t *s);

#endif // DWARF_H