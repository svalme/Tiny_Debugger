# Tiny Debugger

A small Linux debugger for **x86_64 ELF binaries**, built using `ptrace`.

Tiny Debugger provides a minimal but functional debugging environment with breakpoints, stack inspection, register viewing, memory examination, and DWARF-annotated backtraces.

---

## Features

- Automatic breakpoint at `main()` (resolved using `nm`)
- Software breakpoints via `INT3 (0xCC)`
- Single-step execution (`step`) — follows calls into functions
- Step-over execution (`next`) — steps over calls without entering them
- Continue execution
- Register inspection (`RIP`, `RSP`, `RBP`)
- Frame-pointer based backtrace with DWARF annotations (function name, source file, line number)
- Stack frame dump (`RSP → RBP`)
- Arbitrary memory examination
- Interactive command-line REPL
- Basic signal reporting (SIGTRAP, SIGSEGV, etc.)

---

## Requirements

- Linux (x86_64)
- GCC
- `nm` (from binutils)
- `libdw` (from elfutils): `sudo apt install libdw-dev`

---

## Building

Build the debugger and example test program:

```bash
make
```

To clean build artifacts:

```bash
make clean
```

---

## Running

Debug the included example program:

```bash
./tinydbg ./test
```

Or debug your own program:

```bash
gcc -g -O0 -fno-omit-frame-pointer -no-pie program.c -o program
./tinydbg ./program
```

### Required Compilation Flags

- `-g` — include debug symbols (required for DWARF annotations)
- `-O0` — disable optimizations
- `-fno-omit-frame-pointer` — required for backtrace unwinding
- `-no-pie` — PIE binaries are not currently supported

---

## Commands

```
regs         - show RIP, RSP, RBP
bt           - show backtrace with function names and source locations
locals       - dump current stack frame
x <addr> [n] - examine memory (n qwords, default 8, max 256)
step         - execute one instruction (steps into calls)
next         - execute one instruction (steps over calls)
cont         - continue execution
quit         - kill program and exit
```

---

## Project Structure

```
tinydbg/
  ├── include/
  │   ├── types.h
  │   ├── memory.h
  │   ├── breakpoint.h
  │   ├── backtrace.h
  │   ├── dwarf.h
  │   └── commands.h
  │
  ├── src/
  │   ├── debugger.c
  │   ├── commands.c
  │   ├── memory.c
  │   ├── breakpoint.c
  │   ├── backtrace.c
  │   └── dwarf.c
  │
  ├── obj/        (generated)
  ├── test.c
  ├── test_multiple_functions.c
  ├── Makefile
  └── tinydbg     (generated)
```

---

## Limitations

- Linux only
- x86_64 only
- Non-PIE binaries only
- Backtrace uses frame-pointer unwinding — unreliable at libc boundaries and mid-prologue (before `push rbp` / `mov rbp, rsp` complete)
- DWARF annotations only cover the current binary; libc frames show raw addresses
- No user-defined breakpoints
- No multi-thread support