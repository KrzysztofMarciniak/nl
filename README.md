# nil-lisp: A Tiny Lisp Interpreter in C99

## Features

- Lightweight and fast (single C file)
- Interactive REPL with clean output
- Full support for lists (`car`, `cdr`, `list`, etc.)
- `lambda`, `define`, `if`, arithmetic, comparisons
- Powerful FFI 
- Cross-platform (Linux, OpenBSD, FreeBSD, macOS)
- Easy to extend

## Quick Start

### 1. Build

You have two options:

1.1 - default, only libc.

``` sh
chmod +x ./make.sh;
./make.sh
```

1.2 - compile with any library, for example math (m) and x11 (X11)

``` sh
chmod +x ./build_with_any_library.sh;
./build_with_any_library.sh m X11
```
### 2. Example

Example files are:

``` newlisp
./nlisp test.nl
./nlisp test_echo.nl
./nlisp test_syscall.nl
```
