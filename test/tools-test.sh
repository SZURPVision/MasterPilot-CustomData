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

# ── exact-multiple boundary (need bc>=5: 3 data + 1 terminal + 1 isolation) ──
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

# ── tiny stdin buffer (byte-at-a-time) ──
rm -f send.sess recv.sess encoded.bin decoded.bin
"$BIN" config -m 100 -c 5 -t send > send.sess
"$BIN" config -m 100 -c 5 -t recv > recv.sess
dd if=/dev/urandom of=tiny.bin bs=97 count=1 2>/dev/null  # 1 byte short of 2-block
"$BIN" send -s send.sess -b 1 < tiny.bin > encoded.bin
"$BIN" recv -s recv.sess < encoded.bin > decoded.bin
diff tiny.bin decoded.bin && pass "tiny-stdin-buf (-b 1)" || fail "tiny-stdin-buf (-b 1)"

# ── slice-reorder (single packet, intra-packet block shuffle) ──
rm -f send.sess recv.sess encoded.bin decoded.bin
MTU=100
"$BIN" config -m $MTU -c 8 -t send > send.sess
"$BIN" config -m $MTU -c 8 -t recv > recv.sess
# Generate data spanning 5 blocks (5 * 96 = 480 bytes, non-multiple → no terminal frame)
dd if=/dev/urandom of=reorder.bin bs=96 count=5 2>/dev/null
"$BIN" send -s send.sess < reorder.bin > encoded.bin
# Split into mtu-byte blocks, shuffle, reassemble
mkdir blk_dir
split -b $MTU encoded.bin blk_dir/blk_
ls blk_dir/blk_* | shuf | xargs cat > shuffled.bin
"$BIN" recv -s recv.sess < shuffled.bin > decoded.bin
diff reorder.bin decoded.bin && pass "slice-reorder" || fail "slice-reorder"
rm -rf blk_dir

# ── mixed-reorder (multi-packet, each packet's blocks shuffled, packets in order) ──
rm -f send.sess recv.sess
MTU=64
"$BIN" config -m $MTU -c 8 -t send > send.sess
"$BIN" config -m $MTU -c 8 -t recv > recv.sess
dd if=/dev/urandom of=pktA.bin bs=100 count=1 2>/dev/null
dd if=/dev/urandom of=pktB.bin bs=200 count=1 2>/dev/null
dd if=/dev/urandom of=pktC.bin bs=50  count=1 2>/dev/null
cat pktA.bin pktB.bin pktC.bin > combined.bin

"$BIN" send -s send.sess < pktA.bin > pktA.raw
"$BIN" send -s send.sess < pktB.bin > pktB.raw
"$BIN" send -s send.sess < pktC.bin > pktC.raw

# Shuffle blocks within each packet
for pkt in pktA pktB pktC; do
    mkdir "${pkt}_blks"
    split -b $MTU "${pkt}.raw" "${pkt}_blks/blk_"
    ls "${pkt}_blks/blk_"* | shuf | xargs cat > "${pkt}.shuf"
    rm -rf "${pkt}_blks"
done

cat pktA.shuf pktB.shuf pktC.shuf > mixed.bin
"$BIN" recv -s recv.sess < mixed.bin > decoded.bin
diff combined.bin decoded.bin && pass "mixed-reorder" || fail "mixed-reorder"

# ── empty input ──
rm -f send.sess recv.sess encoded.bin decoded.bin
"$BIN" config -m 100 -c 4 -t send > send.sess
"$BIN" config -m 100 -c 4 -t recv > recv.sess
echo -n "" | "$BIN" send -s send.sess > encoded.bin 2>/dev/null || true
"$BIN" recv -s recv.sess < encoded.bin > decoded.bin 2>/dev/null || true
test ! -s decoded.bin -o "$(cat decoded.bin)" = "" && pass "empty-input" || fail "empty-input"

echo "=== ALL TESTS PASSED ==="