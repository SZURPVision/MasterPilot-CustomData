using System.Runtime.InteropServices;
using MasterPilot.Communicator.CustomData.Bindings;

namespace MasterPilot.Communicator.CustomData;
using static CoreMethods;

public class Receiver : IDisposable
{
	readonly GCHandle bufferHandle;
	internal unsafe readonly mp_receiver_t* pReceiver;
	readonly Lock streamLock = new();
	public Receiver(int mtu, int bufferCount)
	{
        bufferHandle = GCHandle.Alloc(new byte[mtu * bufferCount], GCHandleType.Pinned);
		unsafe
		{
			pReceiver = (mp_receiver_t*)NativeMemory.AllocZeroed((nuint)sizeof(mp_receiver_t));
			pReceiver->config = new()
			{
				buffer = (byte*)bufferHandle.AddrOfPinnedObject(),
				buffer_count = (byte)bufferCount,
				mtu = (ushort)mtu
			};
		}
	}
	public Stream BeginRead()
	{
		lock(streamLock)
		{
			ObjectDisposedException.ThrowIf(disposed, this);
			unsafe
			{
				return new ReceiverStream(streamLock, this);
			}
		}
	}
	public bool Push(ReadOnlySpan<byte> block)
	{
		lock(streamLock)
		{
			ObjectDisposedException.ThrowIf(disposed, this);
			unsafe
			{
				var mtu = pReceiver->config.mtu;
				if(block.Length != mtu) throw new ArgumentException($"Unexpected mtu({block.Length}) of block.");
				fixed(byte* d = &block.GetPinnableReference())
				{
					var received = MP_Receive(pReceiver,d);
					return received == mtu;
				}
			}
		}
	}
	internal bool disposed = false;
	public void Dispose()
	{
		lock(streamLock)
		{
			if(disposed) return;

			if(bufferHandle.IsAllocated) bufferHandle.Free();
			unsafe
			{
				NativeMemory.Free(pReceiver);
			}
			disposed = true;
		}
	}
}


file class ReceiverStream : Stream
{
    bool disposed;
    unsafe readonly mp_block_reader_t* pReader;
	readonly ushort avaliable;
	readonly Lock syncLock;
	readonly Receiver owner; //保证pReceiver生命周期

	public unsafe ReceiverStream(Lock syncLock, Receiver owner)
	{
		pReader = (mp_block_reader_t*)NativeMemory.AllocZeroed((nuint)sizeof(mp_block_reader_t));

		this.syncLock = syncLock;
		this.owner = owner;

		lock(syncLock) avaliable = MP_BlockReader_Begin(pReader,owner.pReceiver);
	}
	~ReceiverStream() => Dispose(false);
    protected override void Dispose(bool disposing)
    {
		lock(syncLock)
		{
			if(disposed) return;

			unsafe
			{
				MP_BlockReader_Finish(pReader);
				NativeMemory.Free(pReader);
			}

			disposed = true;
		}
    }

    public override int Read(Span<byte> buffer)
    {
		lock(syncLock)
		{
			ThrowIfDisposed();
			unsafe
			{
				fixed (byte* dst = buffer)
				return MP_BlockReader_Read(pReader, dst, (ushort)buffer.Length);
			}
		}
    }


    void ThrowIfDisposed()
	{
		ObjectDisposedException.ThrowIf(disposed,this);
		ObjectDisposedException.ThrowIf(owner.disposed,owner);
	}

    public override bool CanRead => !disposed && !owner.disposed;
    public override bool CanSeek => false;
    public override bool CanWrite => false;
    public override long Length => avaliable;

    public override long Position
    {
        get => throw new NotSupportedException("ReceiverStream does not support seeking.");
        set => throw new NotSupportedException("ReceiverStream does not support seeking.");
    }

    public override void Flush() { }

    public override long Seek(long offset, SeekOrigin origin) =>
        throw new NotSupportedException("ReceiverStream does not support seeking.");

    public override void SetLength(long value) =>
        throw new NotSupportedException("ReceiverStream does not support SetLength.");

    public override void Write(byte[] buffer, int offset, int count) =>
        throw new NotSupportedException("ReceiverStream is read-only.");

    public override int Read(byte[] buffer, int offset, int count) => Read(buffer.AsSpan(offset, count));
}