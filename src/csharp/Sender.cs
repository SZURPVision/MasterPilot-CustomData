using System.Collections.Specialized;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using MasterPilot.Communicator.CustomData.Bindings;

namespace MasterPilot.Communicator.CustomData;
using static CoreMethods;

public class Sender : IDisposable
{
	readonly GCHandle bufferHandle;
	unsafe internal readonly mp_sender_t* pSender;
	readonly Lock streamLock = new();
	internal bool disposed;

	/// <summary>
	/// 发送的消费方法
	/// </summary>
	/// <param name="block"></param>
	/// <returns>已消费长度</returns>
	public delegate ushort Consumer(ReadOnlySpan<byte> block);
	public Sender(int mtu, int bufferCount)
	{
		bufferHandle = GCHandle.Alloc(new byte[mtu*bufferCount],GCHandleType.Pinned);
		unsafe
		{
			pSender = (mp_sender_t*)NativeMemory.AllocZeroed((nuint)sizeof(mp_sender_t));
			pSender->config = new()
			{
				buffer = (byte*)bufferHandle.AddrOfPinnedObject(),
				buffer_count = (byte)bufferCount,
				mtu = (ushort)mtu
			};
		}
	}
	public void Dispose()
	{
		lock(streamLock)
		{
			if(disposed) return;
			if(bufferHandle.IsAllocated) bufferHandle.Free();
			unsafe
			{
				NativeMemory.Free(pSender);
			}
			disposed = true;
		}
	}
	public Stream BeginWrite()
	{
		lock(streamLock)
		{
			ObjectDisposedException.ThrowIf(disposed, this);
			unsafe
			{
				return new SenderStream(streamLock, this);
			}
		}
	}

	public bool Flush(Consumer consumer)
	{
		var handle = GCHandle.Alloc(consumer);
		bool sent;
		unsafe
		{
			var pConsumer = (void*)GCHandle.ToIntPtr(handle);
			lock(streamLock)
			{
				ObjectDisposedException.ThrowIf(disposed, this);
				sent = MP_Send(pSender, &ConsumerCallback, pConsumer) != 0;
			}
		}
		handle.Free();
		return sent;

		[UnmanagedCallersOnly(CallConvs = [ typeof(CallConvCdecl)])]
		unsafe static ushort ConsumerCallback(byte* block, ushort length, void* user)
		{
			var handle = GCHandle.FromIntPtr((nint)user);
			var consumer = (Consumer)handle.Target!;
			return consumer(new(block,length));
		}
	}
}

file class SenderStream : Stream
{
	unsafe readonly mp_block_writer_t* pWriter;
	readonly Sender owner;
	readonly Lock syncLock;
	readonly ushort avaliable;
	ushort written = 0;
	public SenderStream(Lock syncLock, Sender owner)
	{
		lock(syncLock)
		{
			unsafe
			{
				pWriter = (mp_block_writer_t*)NativeMemory.AllocZeroed((nuint)sizeof(mp_block_writer_t));
				avaliable = MP_BlockWriter_Begin(pWriter,owner.pSender);
			}
		}
		this.owner = owner;
		this.syncLock = syncLock;
	}
	bool disposed = false;
	protected override void Dispose(bool disposing)
	{
		lock(syncLock)
		{
			if(disposed) return;
			unsafe
			{
				MP_BlockWriter_Commit(pWriter,written);
				NativeMemory.Free(pWriter);
			}
			disposed = true;
		}
	}
	~SenderStream() => Dispose(false);
	public override void Write(ReadOnlySpan<byte> buffer)
	{
		lock(syncLock)
		{
			ThrowIfDisposed();
			unsafe
			{
				fixed(byte* pBuffer = buffer)
				written += MP_BlockWriter_Write(pWriter,pBuffer,(ushort)buffer.Length);
			}
		}
	}
	public void ThrowIfDisposed()
	{
		ObjectDisposedException.ThrowIf(disposed, this);
		ObjectDisposedException.ThrowIf(owner.disposed, owner);
	}
	public override bool CanRead => false;
	public override bool CanSeek => false;
	public override bool CanWrite => !disposed && !owner.disposed;
	public override long Length => avaliable;
	public override long Position
	{
		get => throw new NotSupportedException("SenderStream does not support seeking.");
		set => throw new NotSupportedException("SenderStream does not support seeking.");
	}
	public override int Read(byte[] buffer, int offset, int count) => throw new NotSupportedException("SenderStream does not support reading.");
	public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException("SenderStream does not support seeking.");
	public override void SetLength(long value) => throw new NotSupportedException("SenderStream does not support SetLength.");
	public override void Flush() => throw new NotSupportedException("SenderStream does not support flushing");
	public override void Write(byte[] buffer, int offset, int count) => Write(buffer.AsSpan(offset,count));
}