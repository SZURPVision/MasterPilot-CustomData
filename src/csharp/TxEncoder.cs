using System.Buffers.Binary;
using System.Runtime.CompilerServices;
using MasterPilot.Communicator.CustomData.Bindings;
using static MasterPilot.Communicator.CustomData.Bindings.CoreMethods;

namespace MasterPilot.Communicator.CustomData;

file sealed class Stream(
    mp_config_t config,
    mp_coordinate_t cursor,
    Action<ReadOnlySpan<byte>> transmit
) : System.IO.Stream
{
    const ushort HeaderSize = (ushort)MP_HEADER_SIZE;

    readonly byte[] _buffer = GC.AllocateUninitializedArray<byte>(config.transmission_unit);
    mp_coordinate_t _cursor = cursor;
    ushort _fill;

    public override void Write(ReadOnlySpan<byte> buffer)
    {
        var maxPayload = mp_max_payload(config);
        var off = 0;

        while (off < buffer.Length)
        {
            var take = Math.Min(buffer.Length - off, maxPayload - _fill);
            buffer.Slice(off, take).CopyTo(_buffer.AsSpan(HeaderSize + _fill));
            _fill = (ushort)(_fill + take);
            off += take;

            if (_fill == maxPayload)
                EmitSlice(eop: false);
        }
    }

    internal void Complete()
    {
        EmitSlice(eop: true);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    void EmitSlice(bool eop)
    {
        var slice = mp_tx_prepare(config, _cursor, _fill, eop);
        var packed = mp_header_pack(slice.header);

        PackedHeaderToSpan(packed,_buffer);
		// 不用跑clear, 纯函数算出的长度和Complete信号准确, 允许垃圾数据填充
        // _buffer.AsSpan(HeaderSize + _fill, _buffer.Length - HeaderSize - _fill).Clear();

        transmit(_buffer);

        _cursor = slice.next;
        _fill = 0;
    }

    public override bool CanRead => false;
    public override bool CanSeek => false;
    public override bool CanWrite => true;
    public override long Length => throw new InvalidOperationException();
    public override long Position { get => throw new InvalidOperationException(); set => throw new InvalidOperationException(); }
    public override void Flush() { }
    public override int Read(byte[] buffer, int offset, int count) => throw new InvalidOperationException();
    public override long Seek(long offset, SeekOrigin origin) => throw new InvalidOperationException();
    public override void SetLength(long value) => throw new InvalidOperationException();
    public override void Write(byte[] buffer, int offset, int count) => Write(new(buffer, offset, count));
}

public abstract class TxEncoder(ushort transmissionUnit, byte senderId)
{
    byte _currentPackageId;

    protected abstract void Transmit(ReadOnlySpan<byte> fullTU);

    public bool Send(Action<System.IO.Stream> write)
    {
        var stream = CreateStream();
        try
        {
            write(stream);
            ((Stream)stream).Complete();
            return true;
        }
        catch
        {
            return false;
        }
        finally
        {
            stream.Dispose();
        }
    }

    System.IO.Stream CreateStream()
    {
        var cursor = new mp_coordinate_t
        {
            sender_id = senderId,
            package_id = _currentPackageId++,
            offset = 0
        };

        var config = new mp_config_t
        {
            transmission_unit = transmissionUnit
        };

        return new Stream(config, cursor, Transmit);
    }
}