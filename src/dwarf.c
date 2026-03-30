// dwarf.c - DWARF debug info lookups via libdw
//
// Two lookups are provided for a given code address:
//
//   1. Source location  (.debug_line state machine)
//      dwarf_getsrc_die() runs the line-number program for a CU and
//      returns the Dwarf_Line entry whose address range covers `addr`.
//      dwarf_linesrc() / dwarf_lineno() then extract the filename and
//      1-based line number from that entry.
//
//   2. Function name  (.debug_info DIE tree)
//      We walk every compilation unit and call dwarf_haspc() on each
//      DW_TAG_subprogram DIE to find the one whose address range covers
//      `addr`, then read its DW_AT_name with dwarf_diename().
//
// Both searches scan all CUs linearly.  For a learning debugger that's
// fine; a production tool would build an interval tree at open time.

#include "dwarf.h"
#include <elfutils/libdw.h>
#ifndef DW_TAG_subprogram
#define DW_TAG_subprogram 0x2e
#endif
#include <fcntl.h>

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

/**
 * @brief Internal session state for DWARF lookups.
 *
 * Holds the libdw handle and the underlying file descriptor.  libdw does
 * not take ownership of the fd passed to dwarf_begin(), so we keep it here
 * and close it ourselves in dwarf_close().
 *
 * The struct body is intentionally defined only in this translation unit.
 * Callers receive a dwarf_session_t* (declared in dwarf.h as an incomplete
 * type) and cannot inspect or modify the fields directly.
 */
struct dwarf_session {
    Dwarf *dw;   /**< libdw handle; owns all parsed DWARF memory. */
    int    fd;   /**< file descriptor for the open ELF binary.    */
};

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

/**
 * @brief Opens an ELF binary and initialises a libdw session for DWARF queries.
 *
 * Opens the file at @p path read-only, then calls dwarf_begin() to parse
 * its DWARF sections.  Both the fd and the Dwarf* are stored in a heap-
 * allocated dwarf_session_t that the caller owns and must release with
 * dwarf_close().
 *
 * @param path  Path to the ELF binary to inspect.
 * @return      A new session on success, or NULL if the file could not be
 *              opened or contains no DWARF debug information.
 */
dwarf_session_t *dwarf_open(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("dwarf_open: open");
        return NULL;
    }

    Dwarf *dw = dwarf_begin(fd, DWARF_C_READ);
    if (!dw) {
        fprintf(stderr, "dwarf_open: no DWARF info in '%s' "
                        "(compile with -g and -no-pie)\n", path);
        close(fd);
        return NULL;
    }

    dwarf_session_t *s = malloc(sizeof(*s));
    if (!s) { dwarf_end(dw); close(fd); return NULL; }
    s->dw = dw;
    s->fd = fd;
    return s;
}

/**
 * @brief Releases all resources associated with a DWARF session.
 *
 * Calls dwarf_end() to free libdw's internal state, then closes the file
 * descriptor and frees the session struct.  Safe to call with NULL.
 *
 * @param s  The session to close.  May be NULL (no-op).
 */
void dwarf_close(dwarf_session_t *s) {
    if (!s) return;
    dwarf_end(s->dw);
    close(s->fd);
    free(s);
}

// ---------------------------------------------------------------------------
// Source-location lookup  (.debug_line)
// ---------------------------------------------------------------------------

/**
 * @brief Resolves a code address to a source filename and line number.
 *
 * Iterates every Compilation Unit (CU) in .debug_info.  For each CU, runs
 * the .debug_line state machine via dwarf_getsrc_die() to find the line-
 * table row whose address range contains @p addr.  Stops at the first hit.
 *
 * The strings written to *srcfile_out point into libdw-owned memory and are
 * valid for the lifetime of the session.  The caller must not free them.
 *
 * How the CU iteration works:
 *   dwarf_nextcu() advances a byte-offset cursor through .debug_info.
 *   Each CU starts with a fixed-size header; the root DIE begins immediately
 *   after it at offset (off + hdr_size).  dwarf_offdie() turns that raw
 *   offset into a Dwarf_Die handle that dwarf_getsrc_die() can consume.
 *
 * @param dw           libdw handle from the session.
 * @param addr         The runtime code address to look up.
 * @param srcfile_out  Receives a pointer to the source filename, or NULL.
 * @param line_out     Receives the 1-based line number, or 0 if not found.
 */
static void lookup_src(Dwarf *dw, uint64_t addr,
                        const char **srcfile_out, int *line_out) {
    *srcfile_out = NULL;
    *line_out    = 0;

    Dwarf_Off off = 0, next_off;
    size_t hdr_size;

    // dwarf_nextcu walks the chain of CUs in .debug_info.
    // On each call it fills `next_off` with the offset of the *following* CU
    // so we can iterate with off = next_off.
    while (dwarf_nextcu(dw, off, &next_off, &hdr_size, NULL, NULL, NULL) == 0) {
        Dwarf_Die cudie;
        // Turn the raw offset into a DIE handle for the CU root.
        if (!dwarf_offdie(dw, off + hdr_size, &cudie)) {
            off = next_off;
            continue;
        }

        // dwarf_getsrc_die runs the .debug_line state machine for this CU
        // and returns the entry covering `addr`, or NULL if none does.
        Dwarf_Line *line = dwarf_getsrc_die(&cudie, (Dwarf_Addr)addr);
        if (line) {
            *srcfile_out = dwarf_linesrc(line, NULL, NULL);
            dwarf_lineno(line, line_out);
            return;
        }

        off = next_off;
    }
}

// ---------------------------------------------------------------------------
// Function-name lookup  (.debug_info DW_TAG_subprogram)
// ---------------------------------------------------------------------------

/**
 * @brief Resolves a code address to the name of the enclosing function.
 *
 * Iterates every CU in .debug_info, then walks each CU's immediate children
 * looking for DW_TAG_subprogram DIEs (one per function).  For each one,
 * dwarf_haspc() checks whether @p addr falls within the function's
 * DW_AT_low_pc..DW_AT_high_pc range.  On a match, dwarf_diename() reads
 * DW_AT_name and returns it.
 *
 * Only top-level children of the CU root are inspected — nested functions
 * (rare in C) and inlined subprograms are not searched.
 *
 * The returned pointer points into libdw-owned memory and is valid for the
 * lifetime of the session.  The caller must not free it.
 *
 * @param dw    libdw handle from the session.
 * @param addr  The runtime code address to look up.
 * @return      The function name (DW_AT_name), or NULL if not found.
 */
static const char *lookup_func(Dwarf *dw, uint64_t addr) {
    Dwarf_Off off = 0, next_off;
    size_t hdr_size;

    while (dwarf_nextcu(dw, off, &next_off, &hdr_size, NULL, NULL, NULL) == 0) {
        Dwarf_Die cudie;
        if (!dwarf_offdie(dw, off + hdr_size, &cudie)) {
            off = next_off;
            continue;
        }

        // Walk the CU's children (top-level DIEs = functions, global vars, …)
        Dwarf_Die child;
        if (dwarf_child(&cudie, &child) != 0) {
            off = next_off;
            continue;
        }

        do {
            if (dwarf_tag(&child) == DW_TAG_subprogram) {
                // dwarf_haspc returns 1 if addr is inside this function's range
                if (dwarf_haspc(&child, (Dwarf_Addr)addr) > 0) {
                    const char *name = dwarf_diename(&child);
                    if (name) return name;
                }
            }
        } while (dwarf_siblingof(&child, &child) == 0);

        off = next_off;
    }
    return NULL;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * @brief Looks up the function name and source location for a code address.
 *
 * Runs both lookup_src() and lookup_func() independently and packages their
 * results into a dwarf_location_t.  Either or both fields may be NULL/0 if
 * the address falls outside the binary's debug info (e.g. inside libc).
 *
 * All string pointers in the returned struct point into libdw-owned memory
 * and are valid for the lifetime of @p s.  Do not free them.
 *
 * @param s     The open DWARF session.  If NULL, returns a zeroed struct.
 * @param addr  The runtime code address to look up.
 * @return      A dwarf_location_t with whichever fields could be resolved.
 */
dwarf_location_t dwarf_lookup(dwarf_session_t *s, uint64_t addr) {
    dwarf_location_t loc = { NULL, NULL, 0 };
    if (!s) return loc;

    lookup_src(s->dw, addr, &loc.srcfile, &loc.line);
    loc.function = lookup_func(s->dw, addr);
    return loc;
}