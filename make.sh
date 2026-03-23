#!/bin/sh
set -e

# Configuration
SRCDIR="${SRCDIR:-.}"
OUTDIR="${OUTDIR:-.}"
PROG="nlisp"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra}"

# Color output
if [ -t 1 ]; then
    GREEN='\033[0;32m'
    BLUE='\033[0;34m'
    YELLOW='\033[1;33m'
    RED='\033[0;31m'
    NC='\033[0m'
else
    GREEN=''
    BLUE=''
    YELLOW=''
    RED=''
    NC=''
fi

find_compiler() {
    for cmd in cc gcc clang tcc; do
        if command -v "$cmd" >/dev/null 2>&1; then
            echo "$cmd"
            return 0
        fi
    done
    return 1
}

CC=$(find_compiler)
if [ -z "$CC" ]; then
    echo "${RED}Error: No C compiler found${NC}" >&2
    echo "Install one of: cc, gcc, clang, tcc" >&2
    exit 1
fi

echo "${BLUE}=== nil-lisp build ===${NC}"
echo "${BLUE}Compiler:${NC} $CC"
echo "${BLUE}Source dir:${NC} $SRCDIR"
echo "${BLUE}Output dir:${NC} $OUTDIR"
echo "${BLUE}CFLAGS:${NC} $CFLAGS"
echo ""

# Ensure output directory exists
mkdir -p "$OUTDIR"

# Find source file
SRC=""
if [ -f "$SRCDIR/nil-lisp-ffi.c" ]; then
    SRC="$SRCDIR/nil-lisp-ffi.c"
elif [ -f "$SRCDIR/main.c" ]; then
    SRC="$SRCDIR/main.c"
elif [ -f "$SRCDIR/nlisp.c" ]; then
    SRC="$SRCDIR/nlisp.c"
else
    echo "${RED}Error: No source file found${NC}" >&2
    echo "Looking for: nil-lisp-ffi.c, main.c, or nlisp.c" >&2
    exit 1
fi

OUT="$OUTDIR/$PROG"

echo "${BLUE}Source:${NC} $SRC"
echo "${BLUE}Output:${NC} $OUT"
echo "${BLUE}Building...${NC}"

# Try to build without -ldl first (works on macOS and some Linux systems)
if $CC $CFLAGS "$SRC" -o "$OUT" 2>/tmp/build1.log; then
    # Success without -ldl
    :
elif grep -q "undefined reference.*dlopen\|undefined reference.*dlsym" /tmp/build1.log 2>/dev/null; then
    # Need to link libdl - try adding it
    echo "${YELLOW}Adding -ldl...${NC}"
    if ! $CC $CFLAGS "$SRC" -o "$OUT" -ldl 2>/tmp/build2.log; then
        echo "${RED}✗ Build failed${NC}" >&2
        cat /tmp/build2.log >&2
        exit 1
    fi
else
    # Some other error
    echo "${RED}✗ Build failed${NC}" >&2
    cat /tmp/build1.log >&2
    exit 1
fi

echo "${GREEN}✓ Build successful${NC}"
echo ""

# Show sizes
if [ -f "$OUT" ]; then
    if command -v stat >/dev/null 2>&1; then
        if stat -f%z "$OUT" >/dev/null 2>&1; then
            SIZE=$(stat -f%z "$OUT")
        else
            SIZE=$(stat -c%s "$OUT")
        fi
        echo "${BLUE}Binary size:${NC} $SIZE bytes"
    fi

    if command -v wc >/dev/null 2>&1; then
        SRCSIZE=$(wc -c < "$SRC")
        SRCLINES=$(wc -l < "$SRC")
        echo "${BLUE}Source size:${NC} $SRCSIZE bytes ($SRCLINES lines)"
    fi
fi

echo ""
echo "${GREEN}✓ Ready to use:${NC}"
echo "  ./$PROG"
echo "  ./$PROG < test.nl"
echo "  ./validate.sh ./$PROG test.nl"
