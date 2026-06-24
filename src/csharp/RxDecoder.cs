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

	/// <summary>
	/// 喂入通信层给的数据.
	/// </summary>
	/// <param name="frame"></param>
	/// <exception cref="InvalidOperationException"></exception>
	public void Feed(ReadOnlySpan<byte> frame)
	{
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


		switch (step.e)
		{
			case MP_RX_STREAM_DUPLICATE:
				return;

			case MP_RX_STREAM_COMPLETE
			or MP_RX_STREAM_NEW_SLICE_IN_ORDER
			or MP_RX_STREAM_NEW_SLICE_OUT_OF_ORDER:
				slot.EnsurePayloadCapacity(coord.offset + payloadSize);
				payload[..payloadSize].CopyTo(slot.Payload.AsSpan(coord.offset));
				slot.State = step.next_state;

				if (step.e == MP_RX_STREAM_COMPLETE)
				{
					var span = slot.Payload.AsSpan(0, step.next_state.termination);
					OnPackage?.Invoke(span);
					slot = slot with { State = default };
				}
				return;

			default:
				throw new InvalidOperationException("This C# wrapper is out-of-date");
		}

	}
}