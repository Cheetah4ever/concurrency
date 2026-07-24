# Concurrency

A small C++ project for learning the fundamental differences between threads
and processes. It contains four focused examples and a CLI that performs the
same file-counting job using either threads or child processes.

## Build

Requirements:

- A C++20 compiler
- POSIX threads
- POSIX process APIs (`fork`, `pipe`, and `waitpid`)

Build the CLI:

```sh
make
```

Build all four examples:

```sh
make examples
```

## CLI

```text
./concurrency (-p | -t) -e <words|lines|characters> -i1 <file> -i2 <file>
```

Options:

- `-p`: execute using two child processes
- `-t`: execute using two threads
- `-e`: choose the function to execute: `words`, `lines`, or `characters`
- `-i1`: first input file
- `-i2`: second input file

Choose exactly one of `-p` and `-t`. The other three options are mandatory.

Examples:

```sh
./concurrency -t -e words -i1 first.txt -i2 second.txt
./concurrency -p -e lines -i1 first.txt -i2 second.txt
./concurrency -t -e characters -i1 first.txt -i2 second.txt
```

Every successful run reports both the result and the elapsed wall-clock time:

```text
42 total words
Execution time: 0.381 ms
```

The timer starts after argument validation and stops after both workers have
finished. It therefore measures thread/process creation, file processing,
communication, and synchronization, but not command-line parsing.

## Learning examples

- `s59.cpp`: creates and joins two POSIX threads.
- `s60.cpp`: creates two child processes and waits for them.
- `s61.cpp`: demonstrates a data race on a shared counter.
- `s62.cpp`: protects the shared counter with a mutex.

`s62.cpp` pairs every `pthread_mutex_lock` with
`pthread_mutex_unlock`. The main CLI needs no mutex because its workers write
to separate result variables and combine them only after completion.

The names in `s61.cpp` and `s62.cpp` come from the original exercise. Despite
the function name `count_words`, these two examples count characters.

Read [concurrency.md](concurrency.md) for the concepts, program design, code
walkthrough, and debugging guide.
