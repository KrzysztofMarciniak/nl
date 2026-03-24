OUT="nlisp"
PROG="nlisp"

if [ ! -f "$OUT" ]; then
    echo "${RED}Error: Binary not found at $OUT${NC}" >&2
    echo "Run build first: ./make.sh" >&2
    exit 1
fi

INSTALL_PATH="/usr/local/bin/$PROG"
echo "${BLUE}Installing to $INSTALL_PATH...${NC}"

if command -v sudo >/dev/null 2>&1; then
    ROOT_CMD="sudo"
elif command -v doas >/dev/null 2>&1; then
    ROOT_CMD="doas"
else
    echo "${RED}Error: Neither sudo nor doas found${NC}" >&2
    echo "Run manually: cp $OUT $INSTALL_PATH && chmod 755 $INSTALL_PATH" >&2
    exit 1
fi

$ROOT_CMD cp "$OUT" "$INSTALL_PATH"
$ROOT_CMD chmod 755 "$INSTALL_PATH"

echo "${GREEN}✓ Installed to $INSTALL_PATH${NC}"
