#!/usr/bin/env bash
set -euo pipefail

TMPDIR="$(mktemp -d)"
trap "rm -rf $TMPDIR" EXIT

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

if [ $# -ge 1 ]; then
    BIN="$(realpath "$1")"
else
    BIN="$(realpath ./mp-customdata-tools)"
fi

cd "$TMPDIR"

# ── single packet ──
"$BIN" config -m 100 -c 4 -t send > send.sess
"$BIN" config -m 100 -c 4 -t recv > recv.sess

echo -n "Hello World" | "$BIN" send -s send.sess > encoded.bin
"$BIN" recv -s recv.sess < encoded.bin > decoded.bin
test "$(cat decoded.bin)" = "Hello World" && pass "single-packet" || fail "single-packet"

# ── multi-packet cross-session ──
rm -f send.sess recv.sess encoded.bin decoded.bin
"$BIN" config -m 100 -c 4 -t send > send.sess
"$BIN" config -m 100 -c 4 -t recv > recv.sess
echo -n "ABC"   | "$BIN" send -s send.sess > pkgs.bin
echo -n "DEFGH" | "$BIN" send -s send.sess >> pkgs.bin
echo -n "IJ"    | "$BIN" send -s send.sess >> pkgs.bin
"$BIN" recv -s recv.sess < pkgs.bin > decoded.bin
test "$(cat decoded.bin)" = "ABCDEFGHIJ" && pass "multi-packet" || fail "multi-packet"

# ── exact-multiple boundary (need bc>=5: 3 data blocks + 1 terminal + 1 isolation) ──
rm -f send.sess recv.sess encoded.bin decoded.bin
"$BIN" config -m 100 -c 5 -t send > send.sess
"$BIN" config -m 100 -c 5 -t recv > recv.sess
dd if=/dev/urandom of=exact.bin bs=96 count=3 2>/dev/null
"$BIN" send -s send.sess < exact.bin > encoded.bin
"$BIN" recv -s recv.sess < encoded.bin > decoded.bin
diff exact.bin decoded.bin && pass "exact-multiple" || fail "exact-multiple"

# ── binary round-trip ──
rm -f send.sess recv.sess encoded.bin decoded.bin
"$BIN" config -m 64 -c 16 -t send > send.sess
"$BIN" config -m 64 -c 16 -t recv > recv.sess
dd if=/dev/urandom of=random.bin bs=512 count=1 2>/dev/null
"$BIN" send -s send.sess < random.bin > encoded.bin
"$BIN" recv -s recv.sess < encoded.bin > decoded.bin
diff random.bin decoded.bin && pass "binary-round-trip" || fail "binary-round-trip"

# ── empty input ──
rm -f send.sess recv.sess encoded.bin decoded.bin
"$BIN" config -m 100 -c 4 -t send > send.sess
"$BIN" config -m 100 -c 4 -t recv > recv.sess
echo -n "" | "$BIN" send -s send.sess > encoded.bin 2>/dev/null || true
"$BIN" recv -s recv.sess < encoded.bin > decoded.bin 2>/dev/null || true
test ! -s decoded.bin -o "$(cat decoded.bin)" = "" && pass "empty-input" || fail "empty-input"

echo "=== ALL TESTS PASSED ==="