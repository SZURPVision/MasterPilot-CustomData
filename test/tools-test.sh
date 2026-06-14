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
"$BIN" config -m 100 -t tx > send.sess
"$BIN" config -m 100 -t rx > recv.sess

echo -n "Hello World" | "$BIN" tx -s send.sess > encoded.bin
"$BIN" rx -s recv.sess < encoded.bin > decoded.bin
test "$(cat decoded.bin)" = "Hello World" && pass "single-packet" || fail "single-packet"

# ── multi-packet cross-session ──
rm -f tx.sess rx.sess encoded.bin decoded.bin
"$BIN" config -m 100 -t tx > send.sess
"$BIN" config -m 100 -t rx > recv.sess
echo -n "ABC"   | "$BIN" tx -s send.sess > pkgs.bin
echo -n "DEFGH" | "$BIN" tx -s send.sess >> pkgs.bin
echo -n "IJ"    | "$BIN" tx -s send.sess >> pkgs.bin
"$BIN" rx -s recv.sess < pkgs.bin > decoded.bin
test "$(cat decoded.bin)" = "ABCDEFGHIJ" && pass "multi-packet" || fail "multi-packet"

# ── exact-multiple boundary (need bc>=5: 3 data + 1 terminal + 1 isolation) ──
rm -f tx.sess rx.sess encoded.bin decoded.bin
"$BIN" config -m 100 -t tx > send.sess
"$BIN" config -m 100 -t rx > recv.sess
dd if=/dev/urandom of=exact.bin bs=96 count=3 2>/dev/null
"$BIN" tx -s send.sess < exact.bin > encoded.bin
"$BIN" rx -s recv.sess < encoded.bin > decoded.bin
diff exact.bin decoded.bin && pass "exact-multiple" || fail "exact-multiple"

# ── binary round-trip ──
rm -f tx.sess rx.sess encoded.bin decoded.bin
"$BIN" config -m 64 -t tx > send.sess
"$BIN" config -m 64 -t rx > recv.sess
dd if=/dev/urandom of=random.bin bs=512 count=1 2>/dev/null
"$BIN" tx -s send.sess < random.bin > encoded.bin
"$BIN" rx -s recv.sess < encoded.bin > decoded.bin
diff random.bin decoded.bin && pass "binary-round-trip" || fail "binary-round-trip"

# ── tiny stdin buffer (byte-at-a-time) ──
rm -f tx.sess rx.sess encoded.bin decoded.bin
"$BIN" config -m 100 -t tx > send.sess
"$BIN" config -m 100 -t rx > recv.sess
dd if=/dev/urandom of=tiny.bin bs=97 count=1 2>/dev/null  # 1 byte short of 2-block
"$BIN" tx -s send.sess -b 1 < tiny.bin > encoded.bin
"$BIN" rx -s recv.sess < encoded.bin > decoded.bin
diff tiny.bin decoded.bin && pass "tiny-stdin-buf (-b 1)" || fail "tiny-stdin-buf (-b 1)"

# ── slice-reorder (packet with terminal frame: exact multiple → shuffle all 6 blocks) ──
rm -f tx.sess rx.sess encoded.bin decoded.bin
MTU=100
"$BIN" config -m $MTU -t tx > send.sess
"$BIN" config -m $MTU -t rx > recv.sess
# 480 = 5 * 96 → exact multiple, Commit appends terminal frame → 6 blocks total
dd if=/dev/urandom of=reorder.bin bs=96 count=5 2>/dev/null
"$BIN" tx -s send.sess < reorder.bin > encoded.bin
mkdir blk_dir
split -b $MTU encoded.bin blk_dir/blk_
ls blk_dir/blk_* | shuf | xargs cat > shuffled.bin
"$BIN" rx -s recv.sess < shuffled.bin > decoded.bin
diff reorder.bin decoded.bin && pass "slice-reorder" || fail "slice-reorder"
rm -rf blk_dir

# ── duplicate-blocks (bitmap dedup: duplicate some blocks, receiver ignores dupes) ──
rm -f tx.sess rx.sess encoded.bin decoded.bin
"$BIN" config -m 100 -t tx > send.sess
"$BIN" config -m 100 -t rx > recv.sess
dd if=/dev/urandom of=dup.bin bs=130 count=1 2>/dev/null  # 2 blocks: 96 + 34
"$BIN" tx -s send.sess < dup.bin > encoded.bin
# Split and duplicate block 0 (first block)
mkdir dup_dir
split -b 100 encoded.bin dup_dir/blk_
# Duplicate first block
first_blk=$(ls dup_dir/blk_* | head -1)
cat "$first_blk" $(ls dup_dir/blk_*) > duped.bin
"$BIN" rx -s recv.sess < duped.bin > decoded.bin
diff dup.bin decoded.bin && pass "duplicate-blocks" || fail "duplicate-blocks"
rm -rf dup_dir

# ── mixed-reorder (multi-packet, each packet's blocks shuffled, packets in order) ──
rm -f tx.sess rx.sess
MTU=64
"$BIN" config -m $MTU -t tx > send.sess
"$BIN" config -m $MTU -t rx > recv.sess
dd if=/dev/urandom of=pktA.bin bs=100 count=1 2>/dev/null
dd if=/dev/urandom of=pktB.bin bs=200 count=1 2>/dev/null
dd if=/dev/urandom of=pktC.bin bs=50  count=1 2>/dev/null
cat pktA.bin pktB.bin pktC.bin > combined.bin

"$BIN" tx -s send.sess < pktA.bin > pktA.raw
"$BIN" tx -s send.sess < pktB.bin > pktB.raw
"$BIN" tx -s send.sess < pktC.bin > pktC.raw

# Shuffle blocks within each packet
for pkt in pktA pktB pktC; do
    mkdir "${pkt}_blks"
    split -b $MTU "${pkt}.raw" "${pkt}_blks/blk_"
    ls "${pkt}_blks/blk_"* | shuf | xargs cat > "${pkt}.shuf"
    rm -rf "${pkt}_blks"
done

cat pktA.shuf pktB.shuf pktC.shuf > mixed.bin
"$BIN" rx -s recv.sess < mixed.bin > decoded.bin
diff combined.bin decoded.bin && pass "mixed-reorder" || fail "mixed-reorder"

# ── large stdin buffer ──
rm -f tx.sess rx.sess encoded.bin decoded.bin
"$BIN" config -m 100 -t tx > send.sess
"$BIN" config -m 100 -t rx > recv.sess
dd if=/dev/urandom of=large.bin bs=500 count=1 2>/dev/null
"$BIN" tx -s send.sess -b 2000 < large.bin > encoded.bin
"$BIN" rx -s recv.sess < encoded.bin > decoded.bin
diff large.bin decoded.bin && pass "large-stdin-buf (-b 2000)" || fail "large-stdin-buf (-b 2000)"

# ── multi-sender interleaved ──
rm -f tx.sess rx.sess
MTU=80
"$BIN" config -m $MTU -t tx > send.sess
"$BIN" config -m $MTU -t rx > recv.sess
# sender0: pkg0 "AAA", pkg1 "BBBB", pkg2 "CC"
echo -n "AAA"  | "$BIN" tx -s send.sess -S 0 > p0.bin
echo -n "BBBB" | "$BIN" tx -s send.sess -S 0 > p1.bin
echo -n "CC"   | "$BIN" tx -s send.sess -S 0 > p2.bin
# sender1: pkg0 "DDDDD"
echo -n "DDDDD" | "$BIN" tx -s send.sess -S 1 > ps1.bin
# sender0: pkg3 "EE"
echo -n "EE"    | "$BIN" tx -s send.sess -S 0 > p3.bin
cat p0.bin p1.bin p2.bin ps1.bin p3.bin > ms_all.bin
"$BIN" rx -s recv.sess < ms_all.bin > ms_out.bin
test "$(cat ms_out.bin)" = "AAABBBBCCDDDDDEE" && pass "multi-sender" || fail "multi-sender"

# ── empty input ──
rm -f tx.sess rx.sess encoded.bin decoded.bin
"$BIN" config -m 100 -t tx > send.sess
"$BIN" config -m 100 -t rx > recv.sess
echo -n "" | "$BIN" tx -s send.sess > encoded.bin 2>/dev/null || true
"$BIN" rx -s recv.sess < encoded.bin > decoded.bin 2>/dev/null || true
test ! -s decoded.bin -o "$(cat decoded.bin)" = "" && pass "empty-input" || fail "empty-input"

echo "=== ALL TESTS PASSED ==="