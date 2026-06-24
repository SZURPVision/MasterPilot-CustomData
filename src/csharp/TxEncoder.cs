using System.Buffers;
using System.Runtime.CompilerServices;
using MasterPilot.Communicator.CustomData.Bindings;
using static MasterPilot.Communicator.CustomData.Bindings.CoreMethods;

namespace MasterPilot.Communicator.CustomData;

public sealed class TxWritter<TState> : IBufferWriter<byte>, IDisposable
{
	const ushort HeaderSize = (ushort)MP_HEADER_SIZE;

	readonly byte[] _buffer;
	readonly mp_config_t _config;
	readonly Action<ReadOnlySpan<byte>, TState> _next;
	readonly TState _state;
	mp_coordinate_t _cursor;
	ushort _fill;

	internal TxWritter(
		mp_config_t config,
		mp_coordinate_t cursor,
		Action<ReadOnlySpan<byte>, TState> next,
		TState state
	)
	{
		_config = config;
		_next = next;
		_buffer = ArrayPool<byte>.Shared.Rent(config.transmission_unit);
		_cursor = cursor;
		_state = state;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	void EmitSlice(bool eop)
	{
		var slice = mp_tx_prepare(_config, _cursor, _fill, eop);
		var packed = mp_header_pack(slice.header);

		PackedHeaderToSpan(packed, _buffer);
		// 不用跑clear, 纯函数算出的长度和Complete信号准确, 允许垃圾数据填充
		// _buffer.AsSpan(HeaderSize + _fill, _buffer.Length - HeaderSize - _fill).Clear();

		_next(_buffer, _state);

		_cursor = slice.next;
		_fill = 0;
	}
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	void CheckSizeAndTryEmitSlice(int size)
	{
		var maxPayload = mp_max_payload(_config);
		if (maxPayload - _fill < size)
			EmitSlice(eop: false);
	}

	public void Dispose()
	{
		EmitSlice(true);
		ArrayPool<byte>.Shared.Return(_buffer);
	}

	public void Advance(int count)
	{
		_fill += (ushort)count;
		if (_fill == mp_max_payload(_config))
			EmitSlice(eop: false);
	}

	public Memory<byte> GetMemory(int sizeHint = 0)
	{
		CheckSizeAndTryEmitSlice(sizeHint);
		return _buffer.AsMemory(HeaderSize + _fill);
	}

	public Span<byte> GetSpan(int sizeHint = 0)
	{
		CheckSizeAndTryEmitSlice(sizeHint);
		return _buffer.AsSpan(HeaderSize + _fill);
	}
}

public class TxEncoder(ushort transmissionUnit, byte senderId)
{
	int _currentPackageId = 0;

	public TxWritter<TState> CreateWritter<TState>(TState state, Action<ReadOnlySpan<byte>, TState> next)
	{
		var cursor = new mp_coordinate_t
		{
			sender_id = senderId,
			package_id = (byte)Interlocked.Increment(ref _currentPackageId),
			offset = 0
		};

		var config = new mp_config_t
		{
			transmission_unit = transmissionUnit
		};

		return new(config, cursor, next, state);
	}
}