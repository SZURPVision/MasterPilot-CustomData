using System.Runtime.CompilerServices;
using MasterPilot.Communicator.CustomData.Bindings;
using static MasterPilot.Communicator.CustomData.Bindings.CoreMethods;

namespace MasterPilot.Communicator.CustomData;

internal sealed class RxCoordinator
{
    const int SlotCount = 1 << (MP_SENDER_ID_BITS + MP_PACKAGE_ID_BITS);

    internal struct Slot
    {
        public mp_rx_slice_state_t State;
        public byte[]? Payload;
        public byte Gen;

        public int PayloadLength => Payload?.Length ?? 0;

        public void EnsurePayloadCapacity(int needed)
        {
            if (PayloadLength < needed)
                Array.Resize(ref Payload, needed);
        }
    }

    struct SenderMeta
    {
        public byte Pkg;
        public byte Gen;
    }

    readonly Slot[] _slots = new Slot[SlotCount];
    readonly SenderMeta[] _senderMetas = new SenderMeta[1 << MP_SENDER_ID_BITS];

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    static int SlotIndex(byte senderId, byte packageId) =>
        (senderId << MP_PACKAGE_ID_BITS) | packageId;

    void GcClockHook(ref Slot slot, byte senderId)
    {
        ref var meta = ref _senderMetas[senderId];

        if (slot.State.received_count > 0 && slot.Gen != meta.Gen)
            slot.State = default;
        slot.Gen = meta.Gen;
    }

    public void GcHook(byte senderId, byte packageId)
    {
        ref var meta = ref _senderMetas[senderId];
        if (packageId + (MP_PACKAGE_ID_MAX / 2) < meta.Pkg)
            meta.Gen++;
        meta.Pkg = packageId;
    }

    internal ref Slot this[mp_coordinate_t coord]
    {
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        get
        {
            ref var slot = ref _slots[SlotIndex(coord.sender_id, coord.package_id)];
            GcClockHook(ref slot, coord.sender_id);
            return ref slot;
        }
    }
}