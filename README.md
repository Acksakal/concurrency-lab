# concurrency-lab

A hands-on study of concurrency: processes, POSIX threads, races, locks, real multi-core behaviour, and how PostgreSQL runs a query in parallel.
Interactive HTML pages explain each idea; small C programs (C90 + GNU extensions) let you run it yourself.

## Pages (`docs/`)

Open any file in a browser; no server or internet connection is needed. Suggested order:

| # | Page | What it covers |
|---|------|----------------|
| 1 | [concurrency.html](docs/concurrency.html) | Concurrent vs parallel, cores and SMT, scheduling, `fork()` in six languages |
| 2 | [threads.html](docs/threads.html) | POSIX threads: create/join, data races, mutexes, deadlock, condition variables, thread lifetime, thread-safe functions |
| 3 | [thread-paths.html](docs/thread-paths.html) | A step-by-step game where you are `main()` and choose the schedule; ends with a condition-variable bug and its fix |
| 4 | [thread-ryzen.html](docs/thread-ryzen.html) | Real experiments on your own CPU: where threads run, races on real cores, a memory-reordering result no interleaving explains |
| 5 | [postgres-funnel.html](docs/postgres-funnel.html) | PostgreSQL parallel query and the leader bottleneck, as a simulation |
| 6 | [pg-play.html](docs/pg-play.html) | PostgreSQL's processes as actors in a short pixel-art play, from server start to a parallel query |

To publish them as a website: GitHub → Settings → Pages → Deploy from a branch → `main`, folder `/docs`.

## C programs (`c/`)

| Program | Shows | Page |
|---------|-------|------|
| `01-processes/fork.c` | `fork()` creates a second process; output order is not fixed | 1 |
| `02-threads/hello_threads.c` | Creating and joining threads; output order between threads is not fixed | 2 |
| `03-race/race.c` | Two threads, one counter, no lock: lost updates | 2 |
| `03-race/mutex.c` | The same counter protected by a mutex: always correct | 2 |
| `03-race/single_count.c` | The same work in one thread, to compare speed with `mutex.c` | — |
| `04-deadlock/deadlock.c` | Two locks taken in opposite orders: hangs forever | 2 |
| `04-deadlock/deadlock_fixed.c` | One lock order for everyone: finishes | 2 |
| `05-condvar/condvar.c` | Producer/consumer with a condition variable | 2 |
| `05-condvar/counter_original.c` | A counter with a wait window; the waiting thread can oversleep | 3 |
| `05-condvar/counter_fixed.c` | The fix: signal as soon as the condition changes | 3 |
| `06-hardware/ryzen_lab.c` | Timeline, race and store-buffering experiments; its output feeds page 4 | 4 |

## Build and run

Needs `gcc` and `make` (on Cygwin, install the `gcc-core` and `make` packages).

```sh
make                       # builds everything into build/
./build/race               # run any program; repeat it, results vary
timeout 3 ./build/deadlock # never finishes on its own; exit status 124 = stopped by timeout
./build/ryzen_lab > out.txt   # then open docs/thread-ryzen.html and load out.txt
make clean
```

All programs use `-std=gnu90 -Wall -Wextra -pedantic -pthread`, and build with no warnings.
`race` is built at `-O0` on purpose; at `-O2` the lost updates often hide, but the program is still wrong.
`ryzen_lab` and `single_count` are built at `-O2`, as on their pages.

## Notes

- **The `.c` files are the source of truth.** Most of them also appear inside a page. `make check-sync` (needs `python3`) verifies that each page still shows exactly the code in `c/`.
- **Results depend on the machine.** Thread order, lost updates and timings differ between runs, operating systems and CPUs; the pages explain what may and may not happen.
- **ThreadSanitizer does not support Cygwin**; to try `-fsanitize=thread`, use Linux.
- Tested with GCC 13.3 on Ubuntu 24.04; written for Cygwin 3.6 on Windows 10.
