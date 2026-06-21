using System.Buffers.Binary;

namespace MasterPilot.Communicator.CustomData.Bindings;

static partial class CoreMethods
{
	public static void PackedHeaderToSpan(uint packed, Span<byte> destination)
    {
        BinaryPrimitives.WriteUInt32LittleEndian(destination, packed);
    }

    public static uint PackedHeaderFromSpan(ReadOnlySpan<byte> span)
    {
        return BinaryPrimitives.ReadUInt32LittleEndian(span);
    }
}