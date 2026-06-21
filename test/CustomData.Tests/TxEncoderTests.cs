using Xunit;
using MasterPilot.Communicator.CustomData;
using MasterPilot.Communicator.CustomData.Bindings;
using static MasterPilot.Communicator.CustomData.Bindings.CoreMethods;

namespace CustomData.Tests;

sealed class MockTxEncoder(ushort tu, byte senderId, Action<ReadOnlySpan<byte>> onTransmit)
    : TxEncoder(tu, senderId)
{
    protected override void Transmit(ReadOnlySpan<byte> fullTU) => onTransmit(fullTU);
}

public class TxEncoderTests
{
    const ushort Tu = 100;  // max_payload = 96
    const byte SenderId = 3;

    [Fact]
    public void Single_slice_packet_has_eop_true()
    {
        var frames = new List<byte[]>();
        var encoder = new MockTxEncoder(Tu, SenderId, frame => frames.Add(frame.ToArray()));

        var payload = new byte[50]; // < 96 → single slice
        new Random(42).NextBytes(payload);

        encoder.Send(s => s.Write(payload));

        Assert.Single(frames);
        var frame = frames[0];
        Assert.Equal(Tu, frame.Length);

        var header = mp_header_unpack(PackedFromSpan(frame));
        Assert.Equal(SenderId, header.sender_id);
        Assert.True(header.eop is not 0);
        Assert.Equal(payload.Length, header.slice_payload_size);
    }

    [Fact]
    public void Multi_slice_packet_builds_correct_frames()
    {
        var frames = new List<byte[]>();
        var encoder = new MockTxEncoder(Tu, SenderId, frame => frames.Add(frame.ToArray()));

        var payload = new byte[200]; // > 96 → multi-slice
        new Random(43).NextBytes(payload);
        encoder.Send(s => s.Write(payload));

        var maxP = mp_max_payload(new mp_config_t { transmission_unit = Tu });
        var expectedSlices = (payload.Length + maxP - 1) / maxP;
        Assert.Equal(expectedSlices, frames.Count);

        // 逐帧校验 payload 数据拼接
        var assembled = new byte[payload.Length];
        for (int i = 0; i < frames.Count; i++)
        {
            var frame = frames[i];
            Assert.Equal(Tu, frame.Length);

            var header = mp_header_unpack(PackedFromSpan(frame));
            var isLast = i == frames.Count - 1;
            Assert.Equal(isLast ? (byte)1 : (byte)0, header.eop);

            var payloadSize = header.slice_payload_size;
            frame.AsSpan(4, payloadSize).CopyTo(assembled.AsSpan(i * maxP));
        }
        Assert.Equal(payload, assembled);
    }

    [Fact]
    public void Package_id_increments_across_calls()
    {
        var frames = new List<byte[]>();
        var encoder = new MockTxEncoder(Tu, SenderId, frame => frames.Add(frame.ToArray()));

        encoder.Send(s => s.Write(new byte[10]));
        encoder.Send(s => s.Write(new byte[20]));

        var hdr0 = mp_header_unpack(PackedFromSpan(frames[0]));
        var hdr1 = mp_header_unpack(PackedFromSpan(frames.Last()));

        Assert.True(hdr0.package_serial + 1 == hdr1.package_serial
                    || hdr0.package_serial == 0 && hdr1.package_serial == 1);
    }

    [Fact]
    public void Send_returns_false_when_write_throws()
    {
        var transmitted = 0;
        var encoder = new MockTxEncoder(Tu, SenderId, _ => transmitted++);

        var result = encoder.Send(_ => throw new InvalidOperationException("fail"));

        Assert.False(result);
        Assert.Equal(0, transmitted);
    }

    [Fact]
    public void Exact_multiple_of_max_payload()
    {
        var frames = new List<byte[]>();
        var encoder = new MockTxEncoder(Tu, SenderId, frame => frames.Add(frame.ToArray()));

        var maxP = mp_max_payload(new mp_config_t { transmission_unit = Tu });
        var payload = new byte[maxP * 3]; // 精确整数倍
        new Random(44).NextBytes(payload);
        encoder.Send(s => s.Write(payload));

        // 精确整数倍 → 3 data slices + Complete() 触发空eop末片 = 4 帧
        Assert.Equal(4, frames.Count);

        var assembled = new byte[payload.Length];
        for (int i = 0; i < frames.Count - 1; i++) // 3 data slices
        {
            var frame = frames[i];
            var header = mp_header_unpack(PackedFromSpan(frame));
            Assert.True(header.eop is 0);

            var payloadSize = header.slice_payload_size;
            Assert.Equal(maxP, payloadSize);
            frame.AsSpan(4, payloadSize).CopyTo(assembled.AsSpan(i * maxP));
        }
        // 末帧: eop=1, payload=0 (空终止)
        var lastFrame = frames.Last();
        var lastHeader = mp_header_unpack(PackedFromSpan(lastFrame));
        Assert.True(lastHeader.eop is not 0);
        Assert.Equal(0, lastHeader.slice_payload_size);

        Assert.Equal(payload, assembled);
    }

    [Fact]
    public void Fragmented_writes_accumulate()
    {
        var frames = new List<byte[]>();
        var encoder = new MockTxEncoder(Tu, SenderId, frame => frames.Add(frame.ToArray()));

        var payload = new byte[150]; // 2 slices: 96 + 54
        new Random(45).NextBytes(payload);

        encoder.Send(s =>
        {
            s.Write(payload.AsSpan(0, 30));
            s.Write(payload.AsSpan(30, 90));
            s.Write(payload.AsSpan(120));
        });

        Assert.Equal(2, frames.Count);

        // 验证 payload 拼接
        var maxP = mp_max_payload(new mp_config_t { transmission_unit = Tu });
        var assembled = new byte[payload.Length];
        for (int i = 0; i < frames.Count; i++)
        {
            var frame = frames[i];
            var header = mp_header_unpack(PackedFromSpan(frame));
            var payloadSize = header.slice_payload_size;
            frame.AsSpan(4, payloadSize).CopyTo(assembled.AsSpan(i * maxP));
        }
        Assert.Equal(payload, assembled);
    }

    static uint PackedFromSpan(ReadOnlySpan<byte> span) => PackedHeaderFromSpan(span);
}