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
9
20
5
x
99
y
100
(10 20 30)
15
1
(20)
true
true
true
false
true
true
true
false
1
0
double
100
(1 2 3)
(20)
10
true
id
42
add-10
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
    echo "$EXPECTED" | head -10
    echo "..."
    echo ""
    echo "Got:"
    echo "$ACTUAL" | head -10
    echo "..."
    exit 1
fi
