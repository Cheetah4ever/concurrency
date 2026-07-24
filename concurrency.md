# Threads and Processes

## What concurrency means

Concurrency means that the lifetimes of two or more tasks overlap. A scheduler
may alternate between them on one CPU core, or they may run simultaneously on
different cores. Parallelism specifically means simultaneous execution;
concurrency does not require it.

## The fundamental difference

The fundamental difference is **memory and resource ownership**:

- Threads are multiple execution paths inside one process. They share the
  process's address space and most process resources.
- Processes are independent running programs. Each process has its own virtual
  address space and process resources.

This one distinction explains most of their practical differences.

| Concern | Threads | Processes |
|---|---|---|
| Memory | Share globals, heap, and process address space | Have separate virtual address spaces |
| Local variables | Each thread has its own stack | Each process has its own complete memory image |
| Communication | Directly read and write shared objects | Require IPC such as pipes, sockets, or shared memory |
| Synchronization | Mutexes, atomics, condition variables | IPC protocols, semaphores, or shared-memory locks |
| Isolation | A bad memory write can damage the whole process | A crash is normally contained to one process |
| Creation/switching | Usually lighter | Usually heavier |
| Failure | One fatal thread failure ends the process | One child can fail while the parent survives |

After `fork()`, the child initially appears to contain the parent's memory, but
the operating system implements this using copy-on-write. Changes to ordinary
variables in the child are not changes to the parent's variables.

## Design

The CLI gives the same logical job to two different execution models:

```text
                         +--> input 1 --> count --+
arguments --> p or t ----|                        +--> add --> output
                         +--> input 2 --> count --+
```

The command requires:

```text
(-p | -t) -e <function> -i1 <file> -i2 <file>
```

The selected function is one of:

- `words`: formatted extraction separates whitespace-delimited words.
- `lines`: `std::getline` counts lines.
- `characters`: `std::ifstream::get` counts every byte read.

### Thread design

`run_threads` creates two `std::thread` objects. Each thread calls
`count_file` for one input and writes to a different result variable. The main
thread joins both threads before adding their results.

Because the two workers never update the same counter, the CLI needs no mutex.
This is an important design principle: avoiding shared mutable state is often
safer than adding locks around it.

In particular, the first thread writes only to `first`, the second writes only
to `second`, and the main thread reads those variables only after both calls to
`join`. A mutex would be necessary if both workers incremented one shared
counter, as they do in `s61.cpp` and `s62.cpp`.

### Process design

`run_processes` creates two pipes and forks two children. Each child counts one
file and writes a small `ChildResult` through its pipe. The parent reads both
results and calls `waitpid` to reap both children.

The pipes are necessary because changing a counter in a child would only
change the child's private memory. The parent cannot obtain that result through
an ordinary global variable.

## The example programs

### `s59.cpp`: basic threads

Two calls to `pthread_create` start the same `print` function with different
arguments. `pthread_join` prevents `main` from finishing before the workers.
The order of `hello` and `world` is nondeterministic.

### `s60.cpp`: basic processes

Two calls to `fork` create two children. Each child prints a different message
and uses `_exit`. The original parent calls `waitpid` for both children.

Every `fork` continues execution in both parent and child. The `if (p == 0)`
branches and `_exit` calls prevent a child from accidentally creating more
children.

### `s61.cpp`: a race condition

Both threads execute `total_words++`. Incrementing is a read-modify-write
operation, not one indivisible action. Interleaving the operations can lose
updates, so the result is undefined and may vary between runs.

### `s62.cpp`: a mutex

The mutex ensures only one thread at a time executes `total_words++`. This
removes the data race, although locking once for every character is expensive.
A better design is for each thread to count locally, then combine the two
results after joining, as the CLI does.

## Reading the CLI code

The important functions in `main.cpp` are:

- `parse_options`: validates the execution mode, function, and input paths.
- `count_file`: contains the work and is independent of concurrency.
- `run_threads`: adapts the work to `std::thread`.
- `start_child` and `collect_child`: manage `fork`, pipes, and `waitpid`.
- `run_processes`: combines the two process results.

Separating the counting function from the concurrency mechanism makes the code
easier to test and makes the thread/process comparison fair.

## Timing the execution

The CLI uses `std::chrono::steady_clock` to measure elapsed wall-clock time.
This clock is monotonic, so changes to the computer's calendar clock cannot
make an interval jump forward or backward.

Timing begins immediately before `run_threads` or `run_processes` and ends
after both workers have completed. The reported duration therefore includes:

- creating threads or processes;
- reading and counting both files;
- thread joining or pipe communication;
- waiting for both child processes.

It excludes argument parsing and printing the final result. Output is reported
in milliseconds with three digits after the decimal point:

```text
42 total words
Execution time: 0.381 ms
```

One run is not a reliable benchmark because file-system caching and system load
vary. Run each mode several times with the same inputs and compare a median or
average. Timing demonstrates observed performance; it does not change the
fundamental memory and ownership differences between threads and processes.

## How to debug concurrent programs

First compile with warnings and debug symbols:

```sh
c++ -std=c++20 -Wall -Wextra -Wpedantic -g -O0 -pthread main.cpp -o concurrency
```

Then reduce the problem:

1. Run the counting logic with small, known files.
2. Compare `-t` and `-p` using exactly the same inputs.
3. Log the process ID and thread ID near suspicious operations.
4. Check that every thread is joined.
5. Check that every child is reaped with `waitpid`.
6. Check that unused pipe ends are closed in both parent and child.
7. For shared thread data, identify which lock protects each variable.
8. Keep a consistent lock order if more than one mutex is acquired.

Useful tools:

- `lldb ./concurrency` on macOS, or `gdb ./concurrency` on Linux.
- Compiler sanitizers for threads:

  ```sh
  c++ -std=c++20 -g -O1 -fsanitize=thread -pthread main.cpp -o concurrency-tsan
  ```

- Address and undefined-behaviour sanitizers:

  ```sh
  c++ -std=c++20 -g -O1 -fsanitize=address,undefined -pthread main.cpp -o concurrency-asan
  ```

- `ps`, `pgrep`, and `lsof` to inspect processes and open descriptors.

A hang usually suggests a deadlock, an unjoined thread, a missing pipe close,
or a parent waiting for a child that cannot finish. A changing or unexpectedly
small result usually suggests a data race.

## Choosing between them

Prefer threads when tasks need efficient access to shared in-process data and
you can control synchronization carefully. Prefer processes when isolation,
fault containment, privilege separation, or independent program lifetimes are
more important.

Neither model is universally better. Choose based on ownership, communication,
failure isolation, and debugging complexity.
