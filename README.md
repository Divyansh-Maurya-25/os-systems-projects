# OS Systems Projects — Shell, Parallel Compression & RAID

> Three systems programming projects in C: a Unix shell built with fork/exec, a multithreaded file compressor using pthreads and zlib, and a RAID mapping table generator covering five RAID levels.

[![C](https://img.shields.io/badge/C-99-blue)](https://en.wikipedia.org/wiki/C99)
[![pthreads](https://img.shields.io/badge/pthreads-POSIX-lightgrey)](https://man7.org/linux/man-pages/man7/pthreads.7.html)

---

## Project 1 — rush: A Unix Shell

**File:** `rush.c`

A working interactive Unix shell (`rush` = Rapid Unix SHell) built from scratch using only POSIX system calls.

### Features
- `rush>` prompt with `getline`-based input loop
- **Built-in commands:** `exit` (clean shutdown), `cd` (chdir), `path` (update executable search path)
- **Output redirection:** `ls > file.txt` — detects `>`, splits command/filename, uses `dup2()` to redirect stdout to file descriptor
- **External commands:** searches all PATH directories with `access(path, X_OK)`, forks a child with `fork()`, executes with `execv()`
- **Error handling:** all errors go to stderr via `write(STDERR_FILENO, ...)` as required by the spec
- **Dynamic token parser:** uses `realloc` to grow the token array on demand — no fixed-size buffers

### Process Model
```
parent (rush)
    │
    ├── getline() → parse → check builtins
    │
    └── fork()
          ├── child:  dup2() (if redirect) → execv() → _exit()
          └── parent: waitpid() → cleanup → next prompt
```

**Core concepts:** `fork`/`exec`/`wait` process model, file descriptor manipulation, `dup2` for redirection, dynamic memory allocation.

---

## Project 2 — Parallel File Compressor (Group Project)

**File:** `serial.c`

**Team:** Divyansh Maurya, Arjan Subedi, Ayush Poudel, Aidan Lauser

A multithreaded `.txt` file compressor using a thread pool with pthreads and zlib deflate compression.

### Architecture

```
compress_directory(dir)
    │
    ├── Collect + sort .txt filenames (lexicographic, qsort)
    ├── Build Job[] and Result[] arrays
    │
    ├── Spawn min(2×cores, 19, file_count) worker threads
    │
    └── Each worker thread:
            while (jobs remain):
                lock → grab job_idx → unlock
                fread file into input_buffer
                deflateReset(&stream)       ← reuse z_stream, no re-init cost
                deflate(Z_FINISH)
                memcpy to results[job_idx].outbuf
            deflateEnd(&stream)
    │
    └── Write archive: [size_i][compressed_bytes_i] × N
        Print: "Compression rate: X.XX%"
```

### Key Optimizations
- **Per-thread `z_stream`** — each thread initializes one zlib stream and reuses it across jobs with `deflateReset()`, avoiding expensive `deflateInit`/`deflateEnd` per file
- **Pre-allocated buffers** — input and output buffers (1MB each) allocated once per thread at startup
- **Mutex-only on job counter** — the critical section is a single integer increment; all I/O and compression are lockless
- **Adaptive thread count** — `num_workers = min(2 × sysconf(_SC_NPROCESSORS_ONLN), 19, file_count)`

**Core concepts:** Thread pool pattern, mutex for shared state, zlib deflate API, `deflateReset` for stream reuse, lexicographic sort with `qsort`.

---

## Project 3 — RAID Mapping Table Generator

**File:** `main.c`

Given a RAID level, number of disks, chunk size, and total stripe rows, generates a tab-separated mapping table showing which logical sector number sits on which disk at each stripe.

### RAID Levels Implemented

| Level | Strategy | Parity |
|---|---|---|
| **RAID 0** | Striping across all disks | None |
| **RAID 01** | Mirror sets, then stripe | Mirrored pairs |
| **RAID 10** | Stripe, then mirror pairs | Mirrored pairs |
| **RAID 4** | Stripe + dedicated parity disk | Fixed last disk |
| **RAID 5** | Stripe + distributed parity | Left-asymmetric rotation |

### RAID 5 Parity Rotation
```c
int parity_pos = (num_disks - 1) - (row % num_disks);  // left-asymmetric
int data_idx   = (disk - parity_pos - 1 + num_disks) % num_disks;
```
Parity rotates left across disks with each new stripe row — the standard left-asymmetric RAID 5 scheme that prevents any single disk from becoming a write bottleneck.

**Core concepts:** RAID storage architecture, striping, mirroring, distributed parity, modular arithmetic for sector addressing.

---

## Setup

```bash
git clone https://github.com/Divyansh-Maurya-25/os-systems-projects.git
cd os-systems-projects
```

```bash
# Project 1 — rush shell
gcc -o rush Project1/rush.c
./rush

# Project 2 — compressor (requires zlib)
gcc -o compressor Project2/serial.c -lpthread -lz
./compressor <directory>

# Project 3 — RAID mapper
gcc -o raid Project3/main.c
echo "5 4 2 8 output.txt" | ./raid
```

---

## File Structure

```
os-systems-projects/
├── Project1/
│   └── rush.c               # Unix shell
├── Project2/
│   └── serial.c             # Parallel compressor
├── Project3/
│   └── main.c               # RAID mapping table generator
└── README.md
```

---

## Course Context

Projects for **COP 4600 — Operating Systems** at the University of South Florida.
