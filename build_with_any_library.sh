#!/bin/sh
set -e

# ====================== Configuration ======================
SRCDIR="${SRCDIR:-.}"
OUTDIR="${OUTDIR:-.}"
PROG="nlisp"

CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -g}"
LIBS=""

# Color output
if [ -t 1 ]; then
    GREEN='\033[0;32m'
    BLUE='\033[0;34m'
    YELLOW='\033[1;33m'
    RED='\033[0;31m'
    NC='\033[0m'
else
    GREEN='' BLUE='' YELLOW='' RED='' NC=''
fi

# ====================== Usage ======================
usage() {
    echo "Usage: $0 [LIBRARY ...]"
    echo ""
    echo "Examples:"
    echo "  $0                    # basic build"
    echo "  $0 m                  # with math library"
    echo "  $0 X11                # with X11"
    echo "  $0 X11 ncurses m      # multiple libraries"
    echo ""
    exit 1
}

if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    usage
fi

# Add requested libraries
for lib in "$@"; do
    case "$lib" in
        X11)      LIBS="$LIBS -lX11" ;;
        Xext)     LIBS="$LIBS -lXext" ;;
        ncurses)  LIBS="$LIBS -lncurses" ;;
        curses)   LIBS="$LIBS -lncurses" ;;
        SDL2)     LIBS="$LIBS -lSDL2" ;;
        curl)     LIBS="$LIBS -lcurl" ;;
        sqlite3)  LIBS="$LIBS -lsqlite3" ;;
        pthread)  LIBS="$LIBS -lpthread" ;;
        m)        LIBS="$LIBS -lm" ;;
        *)        LIBS="$LIBS -l$lib" ;;
    esac
done

# ====================== Platform-specific handling ======================
# On OpenBSD (and macOS/FreeBSD), dlopen/dlsym are in libc → do NOT add -ldl
# On Linux, we need -ldl
if [ "$(uname)" = "Linux" ]; then
    LIBS="$LIBS -ldl"
fi

echo "${BLUE}=== nil-lisp build with libraries ===${NC}"
echo "${BLUE}OS:${NC} $(uname)"
echo "${BLUE}Compiler:${NC} $(command -v cc)"
echo "${BLUE}Extra libraries:${NC} ${LIBS:-(none)}"
echo "${BLUE}CFLAGS:${NC} $CFLAGS"
echo ""

# Find source file
SRC=""
for f in nil-lisp-ffi.c main.c nlisp.c; do
    if [ -f "$SRCDIR/$f" ]; then
        SRC="$SRCDIR/$f"
        break
    fi
done

if [ -z "$SRC" ]; then
    echo "${RED}Error: No source file found!${NC}" >&2
    echo "Looking for: nil-lisp-ffi.c, main.c or nlisp.c" >&2
    exit 1
fi

mkdir -p "$OUTDIR"
OUT="$OUTDIR/$PROG"

echo "${BLUE}Source:${NC} $SRC"
echo "${BLUE}Output:${NC} $OUT"
echo "${BLUE}Building...${NC}"

if cc $CFLAGS "$SRC" -o "$OUT" $LIBS 2>/tmp/build.log; then
    echo "${GREEN}✓ Build successful!${NC}"
else
    echo "${RED}✗ Build failed${NC}" >&2
    cat /tmp/build.log >&2
    exit 1
fi

# Show sizes
if command -v wc >/dev/null 2>&1 && [ -f "$OUT" ]; then
    SIZE=$(wc -c < "$OUT")
    SRCSIZE=$(wc -c < "$SRC")
    SRCLINES=$(wc -l < "$SRC")
    echo ""
    echo "${BLUE}Binary size:${NC} $SIZE bytes"
    echo "${BLUE}Source size:${NC} $SRCSIZE bytes ($SRCLINES lines)"
fi

echo ""
echo "${GREEN}✓ Ready:${NC} ./$PROG"
echo "   Example: ./$PROG test.nl"

exit 0
