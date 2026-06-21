using System.Buffers.Binary;
using MasterPilot.Communicator.CustomData.Bindings;

namespace MasterPilot.Communicator.CustomData;
using static mp_rx_stream_event_t;
using static Bindings.CoreMethods;

public sealed class RxDecoder(int transmissionUnit)
{
    readonly mp_config_t _config = new() { transmission_unit = (ushort)transmissionUnit };
    readonly RxCoordinator _coordinator = new();

    public event Action<ReadOnlySpan<byte>>? OnPackage;

    public void Feed(ReadOnlySpan<byte> frame)
    {
        if (frame.Length < (int)MP_HEADER_SIZE)
            return;

        var packed = PackedHeaderFromSpan(frame);
        var header = mp_header_unpack(packed);

        var maxPayload = mp_max_payload(_config);
        var payload = frame[(int)MP_HEADER_SIZE..];
        var payloadSize = Math.Min(header.slice_payload_size, (ushort)payload.Length);

        var coord = mp_rx_header_to_coordinate(_config, header);

        _coordinator.GcHook(coord.sender_id, coord.package_id);

        ref var slot = ref _coordinator[coord];
        var oldState = slot.State;
        var step = mp_rx_calc_slice_step(oldState, header.slice_serial, payloadSize, maxPayload, header.eop is not 0);

		switch(step.e)
		{
			case MP_RX_STREAM_DUPLICATE:
				break;

			case MP_RX_STREAM_COMPLETE:
				// 取 payload 和 termination，然后清零状态
				var fullPayload = slot.Payload;
				var termination = step.next_state.termination;
				slot = default;

				if (fullPayload is not null)
				{
					var span = fullPayload.AsSpan();
					if (termination > 0 && termination <= span.Length)
						span = span[..termination];
					OnPackage?.Invoke(span);
				}

				break;

			// 不管是不是乱序, 都被coordinator进行数轴排序
			case MP_RX_STREAM_NEW_SLICE_IN_ORDER or MP_RX_STREAM_NEW_SLICE_OUT_OF_ORDER:
				if (payloadSize > 0)
				{
					slot.EnsurePayloadCapacity(coord.offset + payloadSize);
					payload[..payloadSize].CopyTo(slot.Payload.AsSpan(coord.offset));
				}
				slot.State = step.next_state;

				break;

			default:
				throw new InvalidOperationException("This C# wrapper is out-of-date");
		}

    }
}