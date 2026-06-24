using Xunit;
using MasterPilot.Communicator.CustomData;
using MasterPilot.Communicator.CustomData.Bindings;
using static MasterPilot.Communicator.CustomData.Bindings.CoreMethods;

namespace CustomData.Tests;

public class RxDecoderTests
{
    const ushort Tu = 100; // max_payload = 96

    static byte[] BuildFrame(byte senderId, byte packageId, byte sliceIdx, ushort payloadSize, bool eop, ReadOnlySpan<byte> payload)
    {
        var frame = new byte[Tu];
        var hdr = new mp_header_meta_t
        {
            sender_id = senderId,
            package_serial = packageId,
            slice_serial = sliceIdx,
            eop = (byte)(eop ? 1 : 0),
            slice_payload_size = payloadSize
        };
        PackedHeaderToSpan(mp_header_pack(hdr), frame.AsSpan());
        payload[..payloadSize].CopyTo(frame.AsSpan(4));
        return frame;
    }

    [Fact]
    public void Single_slice_packet_is_decoded()
    {
        var received = new List<byte[]>();
        var decoder = new RxDecoder(Tu);
        decoder.OnPackage += data => received.Add(data.ToArray());

        var payload = new byte[50];
        new Random(42).NextBytes(payload);

        var frame = BuildFrame(1, 0, 0, 50, true, payload);
        decoder.Feed(frame);

        Assert.Single(received);
        Assert.Equal(payload, received[0]);
    }

    [Fact]
    public void Multi_slice_packet_is_assembled_in_order()
    {
        var received = new List<byte[]>();
        var decoder = new RxDecoder(Tu);
        decoder.OnPackage += data => received.Add(data.ToArray());

        var maxP = mp_max_payload(new mp_config_t { transmission_unit = Tu });
        var payload = new byte[maxP * 2 + 30]; // 2 full + 1 partial
        new Random(43).NextBytes(payload);

        FeedInOrder(decoder, 0, 0, payload, maxP);

        Assert.Single(received);
        Assert.Equal(payload, received[0]);
    }

    [Fact]
    public void Out_of_order_slices_are_assembled()
    {
        var received = new List<byte[]>();
        var decoder = new RxDecoder(Tu);
        decoder.OnPackage += data => received.Add(data.ToArray());

        var maxP = mp_max_payload(new mp_config_t { transmission_unit = Tu });
        var payload = new byte[maxP * 2 + 40]; // 2 full + 1 partial (non-exact)
        new Random(44).NextBytes(payload);

        // Feed in reverse order, only last slice has eop
        decoder.Feed(BuildFrame(0, 0, 2, 40, true,  payload.AsSpan(maxP * 2, 40)));
        decoder.Feed(BuildFrame(0, 0, 1, maxP, false, payload.AsSpan(maxP, maxP)));
        decoder.Feed(BuildFrame(0, 0, 0, maxP, false, payload.AsSpan(0, maxP)));

        Assert.Single(received);
        Assert.Equal(payload, received[0]);
    }

    [Fact]
    public void Duplicate_slice_is_ignored()
    {
        var received = new List<byte[]>();
        var decoder = new RxDecoder(Tu);
        decoder.OnPackage += data => received.Add(data.ToArray());

        var maxP = mp_max_payload(new mp_config_t { transmission_unit = Tu });
        var payload = new byte[maxP * 2]; // 2 full slices
        new Random(45).NextBytes(payload);

        // Feed slice 0 twice (duplicate), then slice 1
        decoder.Feed(BuildFrame(0, 0, 0, maxP, false, payload.AsSpan(0, maxP)));
        decoder.Feed(BuildFrame(0, 0, 0, maxP, false, payload.AsSpan(0, maxP))); // duplicate
        decoder.Feed(BuildFrame(0, 0, 1, maxP, true,  payload.AsSpan(maxP, maxP)));

        Assert.Single(received);
        Assert.Equal(payload, received[0]);
    }

    [Fact]
    public void Cross_package_isolation()
    {
        var received = new List<byte[]>();
        var decoder = new RxDecoder(Tu);
        decoder.OnPackage += data => received.Add(data.ToArray());

        var maxP = mp_max_payload(new mp_config_t { transmission_unit = Tu });
        var pkg0 = new byte[maxP * 2]; // 2 slices
        var pkg1 = new byte[30];       // 1 slice
        new Random(46).NextBytes(pkg0);
        new Random(47).NextBytes(pkg1);

        // Interleave: pkg0 slice 0, pkg1, pkg0 slice 1
        decoder.Feed(BuildFrame(0, 0, 0, maxP, false, pkg0.AsSpan(0, maxP)));
        decoder.Feed(BuildFrame(0, 1, 0, 30, true, pkg1.AsSpan(0, 30)));
        decoder.Feed(BuildFrame(0, 0, 1, maxP, true, pkg0.AsSpan(maxP, maxP)));

        Assert.Equal(2, received.Count);
        Assert.Equal(pkg1, received[0]); // pkg1 先完成
        Assert.Equal(pkg0, received[1]); // pkg0 后完成
    }

    [Fact]
    public void Sender_isolation()
    {
        var received = new List<byte[]>();
        var decoder = new RxDecoder(Tu);
        decoder.OnPackage += data => received.Add(data.ToArray());

        var pkgA = new byte[50];
        var pkgB = new byte[60];
        new Random(48).NextBytes(pkgA);
        new Random(49).NextBytes(pkgB);

        decoder.Feed(BuildFrame(0, 0, 0, 50, true, pkgA));
        decoder.Feed(BuildFrame(1, 0, 0, 60, true, pkgB));

        Assert.Equal(2, received.Count);
        Assert.Equal(pkgA, received[0]);
        Assert.Equal(pkgB, received[1]);
    }

    [Fact]
    public void Eop_with_zero_payload()
    {
        var received = new List<byte[]>();
        var decoder = new RxDecoder(Tu);
        decoder.OnPackage += data => received.Add(data.ToArray());

        var maxP = mp_max_payload(new mp_config_t { transmission_unit = Tu });
        var payload = new byte[maxP]; // exact one slice, eop=1, payload=maxP
        new Random(50).NextBytes(payload);

        // Simulate exact multiple: full slice eop false, then eop+zero slice
        decoder.Feed(BuildFrame(0, 0, 0, maxP, false, payload.AsSpan(0, maxP)));
        decoder.Feed(BuildFrame(0, 0, 1, 0, true, ReadOnlySpan<byte>.Empty));

        Assert.Single(received);
        Assert.Equal(payload, received[0]);
    }

    static void FeedInOrder(RxDecoder decoder, byte senderId, byte packageId, ReadOnlySpan<byte> payload, int maxP)
    {
        var off = 0;
        byte sliceIdx = 0;
        while (off < payload.Length)
        {
            var size = (ushort)Math.Min(maxP, payload.Length - off);
            var eop = off + size >= payload.Length;
            decoder.Feed(BuildFrame(senderId, packageId, sliceIdx, size, eop, payload.Slice(off, size)));
            off += size;
            sliceIdx++;
        }
    }
}