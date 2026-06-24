using Xunit;
using MasterPilot.Communicator.CustomData;
using MasterPilot.Communicator.CustomData.Bindings;
using static MasterPilot.Communicator.CustomData.Bindings.CoreMethods;
using System.Buffers;

namespace CustomData.Tests;

static class TxEncoderExtensions
{
	public static TxWritter<List<byte[]>> CreateWritter(
		this TxEncoder encoder, List<byte[]> frames)
	{
		return encoder.CreateWritter(frames, static (span, s) => s.Add(span.ToArray()));
	}
}

public class TxEncoderTests
{
	const ushort Tu = 100;  // max_payload = 96
	const byte SenderId = 3;

	[Fact]
	public void Single_slice_packet_has_eop_true()
	{
		var frames = new List<byte[]>();
		var encoder = new TxEncoder(Tu, SenderId);

		var payload = new byte[50]; // < 96 → single slice
		new Random(42).NextBytes(payload);

		using (var writer = encoder.CreateWritter(frames))
			writer.Write(payload);

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
		var encoder = new TxEncoder(Tu, SenderId);

		var payload = new byte[200]; // > 96 → multi-slice
		new Random(43).NextBytes(payload);

		using (var writer = encoder.CreateWritter(frames))
			writer.Write(payload);

		var maxP = mp_max_payload(new mp_config_t { transmission_unit = Tu });
		var expectedSlices = (payload.Length + maxP - 1) / maxP;
		Assert.Equal(expectedSlices, frames.Count);

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
		var encoder = new TxEncoder(Tu, SenderId);

		using (var w1 = encoder.CreateWritter(frames))
			w1.Write(new byte[10]);

		using (var w2 = encoder.CreateWritter(frames))
			w2.Write(new byte[20]);

		var hdr0 = mp_header_unpack(PackedFromSpan(frames[0]));
		var hdr1 = mp_header_unpack(PackedFromSpan(frames[1]));

		Assert.Equal((byte)(hdr0.package_serial + 1), hdr1.package_serial);
	}


	[Fact]
	public void Exact_multiple_of_max_payload()
	{
		var frames = new List<byte[]>();
		var encoder = new TxEncoder(Tu, SenderId);

		var maxP = mp_max_payload(new mp_config_t { transmission_unit = Tu });
		var payload = new byte[maxP * 3]; // 精确整数倍
		new Random(44).NextBytes(payload);

		using (var writer = encoder.CreateWritter(frames))
			writer.Write(payload);

		// 3 data slices + Dispose 触发空 eop 末片 = 4 帧
		Assert.Equal(4, frames.Count);

		var assembled = new byte[payload.Length];
		for (int i = 0; i < 3; i++)
		{
			var frame = frames[i];
			var header = mp_header_unpack(PackedFromSpan(frame));
			Assert.True(header.eop is 0);
			Assert.Equal(maxP, header.slice_payload_size);
			frame.AsSpan(4, maxP).CopyTo(assembled.AsSpan(i * maxP));
		}
		// 末帧: eop=1, payload=0
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
		var encoder = new TxEncoder(Tu, SenderId);

		var payload = new byte[150]; // 2 slices: 96 + 54
		new Random(45).NextBytes(payload);

		using (var writer = encoder.CreateWritter(frames))
		{
			writer.Write(payload.AsSpan(0, 30));
			writer.Write(payload.AsSpan(30, 90));
			writer.Write(payload.AsSpan(120));
		}

		Assert.Equal(2, frames.Count);

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
