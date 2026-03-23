#!/bin/sh

set -e

# Configuration
SRCDIR="${SRCDIR:-.}"
OUTDIR="${OUTDIR:-.}"
PROG="nlisp"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra}"

if [ -t 1 ]; then
    GREEN='\033[0;32m'
    BLUE='\033[0;34m'
    YELLOW='\033[1;33m'
    NC='\033[0m'
else
    GREEN=''
    BLUE=''
    YELLOW=''
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
    echo "${YELLOW}Error: No C compiler found${NC}" >&2
    echo "Install one of: cc, gcc, clang, tcc" >&2
    exit 1
fi

echo "${BLUE}=== nil-lisp build ===${NC}"
echo "${BLUE}Compiler:${NC} $CC"
echo "${BLUE}Source dir:${NC} $SRCDIR"
echo "${BLUE}Output dir:${NC} $OUTDIR"
echo "${BLUE}Flags:${NC} $CFLAGS"
echo ""

# Ensure output directory exists
mkdir -p "$OUTDIR"

# Build
SRC="$SRCDIR/main.c"
OUT="$OUTDIR/$PROG"

if [ ! -f "$SRC" ]; then
    echo "${YELLOW}Error: $SRC not found${NC}" >&2
    exit 1
fi

echo "${BLUE}Building...${NC}"
$CC $CFLAGS "$SRC" -o "$OUT"

echo "${GREEN}✓ Build successful${NC}"
echo ""

# Show sizes
if [ -f "$OUT" ]; then
    SIZE=$(stat -f%z "$OUT" 2>/dev/null || stat -c%s "$OUT" 2>/dev/null || echo "?")
    echo "${BLUE}Binary size:${NC} $SIZE bytes"
    
    SRCSIZE=$(stat -f%z "$SRC" 2>/dev/null || stat -c%s "$SRC" 2>/dev/null || echo "?")
    echo "${BLUE}Source size:${NC} $SRCSIZE bytes"
fi

echo ""
echo "${GREEN}Usage:${NC} ./$PROG"
