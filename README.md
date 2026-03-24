# nil-lisp

<p align="center">
  <img src="nil.png" alt="nil-lisp logo" width="200" height="200">
</p>

<p align="center">
  <strong>A minimal, high-performance Lisp interpreter written in pure C99</strong>
</p>

---

## Overview

**nil-lisp** is a lightweight Lisp interpreter designed for simplicity and performance. At just ~600 lines of C99, it provides a complete Lisp environment with support for functional programming, dynamic typing, and powerful foreign function interface (FFI) capabilities. Perfect for embedded scripting, education, or rapid prototyping.

## Features

✨ **Core Language Features**
- Dynamic typing with full type support (numbers, symbols, strings, lists, booleans)
- Interactive REPL for real-time experimentation
- Lexical scoping with environment variables
- First-class functions and lambda expressions
- Complete list operations (`car`, `cdr`, `list`)
- Arithmetic and comparison operators with arbitrary argument count

🔗 **Foreign Function Interface (FFI)**
- Direct C library calls with automatic symbol resolution
- Support for libc and any system library
- Built-in FFI cache for performance
- Cross-platform compatibility

🎯 **Design Philosophy**
- **Minimal** — Under 700 lines of C99, no external dependencies
- **Fast** — Direct interpretation with efficient caching
- **Portable** — Runs on Linux, OpenBSD, FreeBSD, macOS, and more
- **Hackable** — Clean, readable code; easy to extend

## Installation

### Prerequisites

- GCC or Clang compiler
- POSIX-compatible operating system
- Standard C library (libc)

### Build from Source

**Option 1: Default build (libc only)**

```bash
chmod +x ./make.sh
./make.sh
```

**Option 2: With additional libraries**

Link against math library, X11, or any system library:

```bash
chmod +x ./build_with_any_library.sh
./build_with_any_library.sh m X11
```

Replace `m X11` with any libraries your scripts need (space-separated).

### Install to System

```bash
chmod +x ./install.sh
./install.sh
```

This installs the `nlisp` binary to your system (usually `/usr/local/bin`).

### Verify Installation

```bash
nlisp
# Output: nil-lisp v0.1
```

## Usage

### Interactive REPL

Start the interpreter with no arguments:

```bash
nlisp
nil-lisp v0.1
(+ 1 2 3)
6
(define greet (lambda (name) (concat "Hello, " name)))
greet
(greet "World")
"Hello, World"
```

Exit with `Ctrl+D` or `Ctrl+C`.

### Run Script Files

Execute Lisp files:

```bash
nlisp script.nl
nlisp script1.nl script2.nl script3.nl
```

Multiple files are loaded in sequence. Each file is evaluated in the same environment.

### Comments

Lines starting with `;` are ignored:

```lisp
; This is a comment
(+ 1 2) ; Inline comments work too
```

## Examples

### Arithmetic

```lisp
(+ 1 2 3 4)          ; => 10
(* 5 3)              ; => 15
(- 10 3 2)           ; => 5
(/ 20 4)             ; => 5
```

### Variables and Functions

```lisp
(define x 42)
x                    ; => 42

(define square (lambda (n) (* n n)))
(square 7)           ; => 49

(define greet 
  (lambda (name) 
    (concat "Hello, " name "!")))
(greet "Alice")      ; => "Hello, Alice!"
```

### Control Flow

```lisp
(if (< 5 10)
  "five is less than ten"
  "five is greater than or equal to ten")
; => "five is less than ten"

(define abs-val
  (lambda (n)
    (if (< n 0)
      (* n -1)
      n)))
(abs-val -15)        ; => 15
```

### List Operations

```lisp
(define items (list 1 2 3 4 5))
(car items)          ; => 1
(cdr items)          ; => (2 3 4 5)

(list "a" "b" "c")   ; => ("a" "b" "c")
```

### String Operations

```lisp
(strlen "hello")              ; => 5
(concat "Hello" " " "World")  ; => "Hello World"

(define name "Alice")
(concat "Welcome, " name)     ; => "Welcome, Alice"
```

### Comparisons

```lisp
(= 5 5 5)            ; => true
(< 1 2 3 4)          ; => true
(> 10 5 3 1)         ; => true
(= 1 2)              ; => false
```

### FFI — Call C Libraries

Call any C function directly from Lisp:

```lisp
; Call strlen from libc
(ffi "libc" "strlen" "hello")  ; => 5

; Call puts to print a line
(ffi "libc" "puts" "Hello from C!")

; Call sin from libm (if linked with -lm)
(ffi "libm" "sin" 0)           ; => 0
```

**FFI Signature:** `(ffi "<library>" "<function>" arg1 arg2 ... arg6)`

- Up to 6 arguments are supported
- All arguments and return values are converted to `long`
- Arguments can be numbers, strings (converted to pointers), or symbols
- Useful for system calls, math functions, and C library integration

## API Reference

### Data Types

| Type | Example | Notes |
|------|---------|-------|
| **Number** | `42`, `0` | ints |
| **String** | `"hello"`, `"world"` | Enclosed in double quotes |
| **Symbol** | `x`, `foo`, `+` | Identifiers and operators |
| **List** | `(1 2 3)`, `(a b c)` | S-expressions |
| **Boolean** | `true`, `false` | Truth values |
| **Nil** | `nil` | The empty value |

### Special Forms

| Form | Syntax | Description |
|------|--------|-------------|
| **define** | `(define name value)` | Bind a value to a name |
| **lambda** | `(lambda (args...) body)` | Create an anonymous function |
| **if** | `(if condition then-expr else-expr)` | Conditional evaluation |
| **ffi** | `(ffi "lib" "fn" args...)` | Call a C function |

### Operators

#### Arithmetic

- `(+ a b c ...)` — Addition
- `(- a b c ...)` — Subtraction (first argument, then subtract the rest)
- `(* a b c ...)` — Multiplication
- `(/ a b c ...)` — Division (first argument, then divide by the rest)

#### Comparison

- `(= a b c ...)` — Equality (all equal?)
- `(< a b c ...)` — Less than (strictly increasing?)
- `(> a b c ...)` — Greater than (strictly decreasing?)

#### String & List Operations

- `(strlen s)` — Return length of string
- `(concat a b c ...)` — Concatenate strings
- `(list a b c ...)` — Create a list
- `(car lst)` — First element of a list
- `(cdr lst)` — Rest of a list (all but first)

## Performance

nil-lisp is optimized for:

- **Fast startup** — No JIT compilation overhead
- **Low memory** — Minimal allocations, efficient data structures
- **FFI caching** — Library symbols are loaded once and reused

Typical overhead: <1ms for simple expressions.

## Architecture

```
nil-lisp
├── main()           — REPL and file loading
├── read_atom()      — Lexer and parser
├── eval()           — Evaluator with environment
├── ffi_load()       — Dynamic library loading with cache
└── print()          — Output formatting
```

## Limitations

- **No tail-call optimization** — Deep recursion may overflow the stack
- **No garbage collection** — Manual memory management (simple for small scripts)
- **No modules** — All code shares a global environment
- **6-arg FFI limit** — FFI calls support at most 6 arguments
- **No macros** — No compile-time code transformation

These are intentional trade-offs for simplicity and speed.

## FAQ

**Q: Can I use nil-lisp in production?**

A: Yes, for lightweight scripting tasks. For mission-critical applications, consider more mature Lisp implementations like Scheme or Common Lisp.

**Q: Can I extend nil-lisp with custom functions?**

A: Yes! Edit `main.c` to add new operators in the `eval()` function, recompile, and you're done.

**Q: Why is it called "nil-lisp"?**

A: "nil" is the empty list in Lisp, i wanted my interpreter to be as nil as possible while still being actually usable.

