# Tiny Debugger

A small, Linux debugger for **x86_64 ELF binaries**, built using `ptrace`.

Tiny Debugger provides a minimal but functional debugging environment with breakpoints, stack inspection, register viewing, and memory examination.

---

## Features

### Current Features

- Automatic breakpoint at `main()` (resolved using `nm`)
- Software breakpoints via `INT3 (0xCC)`
- Single-step execution
- Continue execution
- Register inspection (`RIP`, `RSP`, `RBP`)
- Frame-pointer based backtrace
- Stack frame dump (`RSP → RBP`)
- Arbitrary memory examination
- Interactive command-line REPL
- Basic signal reporting (SIGTRAP, SIGSEGV, etc.)

---

## Requirements

- Linux (x86_64)
- GCC
- `nm` (from binutils)

---

## Building

Build the debugger and example test program:

```bash
make
```

This will:
- Create the `obj/` directory
- Build `tinydbg`
- Build the example `test` program with required flags

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

Or debug your own program (must be compiled correctly):

```bash
gcc -g -O0 -fno-omit-frame-pointer -no-pie program.c -o program
./tinydbg ./program
```

### Required Compilation Flags

- `-g` — include debug symbols  
- `-O0` — disable optimizations  
- `-fno-omit-frame-pointer` — required for backtraces  
- `-no-pie` — PIE binaries are not currently supported  

---

## Commands

```
regs         - show RIP, RSP, RBP
bt           - show backtrace (call stack)
locals       - dump current stack frame
x <addr> [n] - examine memory (n qwords, default 8, max 256)
step         - execute one instruction
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
  │   └── commands.h
  │
  ├── src/
  │   ├── debugger.c
  │   ├── commands.c
  │   ├── memory.c
  │   ├── breakpoint.c
  │   └── backtrace.c
  │
  ├── obj/        (generated)
  ├── test.c
  ├── Makefile
  └── tinydbg     (generated)
```

---

## Limitations

- Linux only
- x86_64 only
- Non-PIE binaries only
- Backtrace requires frame pointers
- No symbol resolution in backtrace (addresses only)
- No source-level debugging
- No multi-thread support

