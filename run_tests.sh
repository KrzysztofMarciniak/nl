#!/bin/sh
# Test validator - simple and direct

INTERPRETER="${1:-./nlisp}"
TEST_FILE="${2:-test.nl}"

if [ ! -f "$TEST_FILE" ]; then
    echo "Error: $TEST_FILE not found"
    exit 1
fi

if [ ! -x "$INTERPRETER" ]; then
    echo "Error: $INTERPRETER not executable"
    exit 1
fi

echo "Running nil-lisp test suite..."
echo ""

# Run tests and capture output
ACTUAL=$($INTERPRETER < "$TEST_FILE" 2>/dev/null)

# Expected output (exact)
EXPECTED="3
15
7
20
5
x
y
10
20
30
15
20
true
false
true
false
true
false
1
0
99
2
(1 2 3)
(10 20)
10
(2 3)
10
true
false
nil
id
42
100
get-value
555
const-ten
10
10"

# Simple comparison
if [ "$ACTUAL" = "$EXPECTED" ]; then
    PASS=$(echo "$EXPECTED" | wc -l)
    echo "✓ All $PASS tests passed!"
    echo ""
    echo "================================"
    echo "Results: $PASS/$PASS tests passed"
    echo "================================"
    exit 0
else
    echo "✗ Tests failed - output mismatch"
    echo ""
    echo "Expected:"
    echo "$EXPECTED" | head -5
    echo "..."
    echo ""
    echo "Got:"
    echo "$ACTUAL" | head -5
    echo "..."
    exit 1
fi
