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

detect_platform() {
    if [ "$(uname)" = "Darwin" ]; then
        echo "macos"
    elif [ "$(uname)" = "Linux" ]; then
        echo "linux"
    else
        echo "unknown"
    fi
}

CC=$(find_compiler)
if [ -z "$CC" ]; then
    echo "${RED}Error: No C compiler found${NC}" >&2
    echo "Install one of: cc, gcc, clang, tcc" >&2
    exit 1
fi

PLATFORM=$(detect_platform)

echo "${BLUE}=== nil-lisp build ===${NC}"
echo "${BLUE}Compiler:${NC} $CC"
echo "${BLUE}Platform:${NC} $PLATFORM"
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

# Determine linker flags based on platform
LDFLAGS=""
case "$PLATFORM" in
    linux)
        LDFLAGS="-ldl"
        ;;
    macos)
        # macOS doesn't need -ldl, dlopen is in libc
        LDFLAGS=""
        ;;
    *)
        # Try to link with -ldl, fall back if it fails
        LDFLAGS="-ldl"
        ;;
esac

echo "${BLUE}Source:${NC} $SRC"
echo "${BLUE}Output:${NC} $OUT"
if [ -n "$LDFLAGS" ]; then
    echo "${BLUE}LDFLAGS:${NC} $LDFLAGS"
fi
echo "${BLUE}Building...${NC}"

if ! $CC $CFLAGS "$SRC" -o "$OUT" $LDFLAGS 2>/tmp/build.log; then
    # If build failed with -ldl, try without it
    if [ "$PLATFORM" != "macos" ] && grep -q "unable to find library" /tmp/build.log 2>/dev/null; then
        echo "${YELLOW}Note: -ldl not found, trying without it...${NC}"
        if ! $CC $CFLAGS "$SRC" -o "$OUT"; then
            echo "${RED}✗ Build failed${NC}" >&2
            cat /tmp/build.log >&2
            exit 1
        fi
    else
        echo "${RED}✗ Build failed${NC}" >&2
        cat /tmp/build.log >&2
        exit 1
    fi
fi

echo "${GREEN}✓ Build successful${NC}"
echo ""

# Show sizes
if [ -f "$OUT" ]; then
    if command -v stat >/dev/null 2>&1; then
        # Try BSD stat first, then GNU stat
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
