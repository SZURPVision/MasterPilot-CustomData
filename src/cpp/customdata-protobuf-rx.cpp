#include <array>
#include <cassert>
#include <functional>
#include <iterator>
#include <memory>
#include <algorithm>
#include <span>
#include <vector>

#include <google/protobuf/message.h>
#include <masterpilot/customdata-common.h>
#include <masterpilot/customdata-protobuf-rx.hpp>
#include <masterpilot/customdata-rx.h>
#include <masterpilot/customdata-stream-rx.h>

namespace masterpilot::customdata
{


class Coordinator
{
private:
    struct Slot
    {
        mp_rx_slice_state_t state {};
        std::vector<uint8_t> data;
        uint8_t gen = 0;   /** sender generation, used for GC */
    };

    Slot& operator[](mp_coordinate_t coord)
    {
        auto& slot = _slots[(coord.sender_id << MP_PACKAGE_ID_BITS) | coord.package_id];
        gc_clock_hook(slot, coord.sender_id);
        return slot;
    }

public:
    Coordinator() = default;

    void gc_clock_hook(Slot& slot, uint8_t sender_id)
    {
        auto& meta = _sender_meta[sender_id];

        if (slot.state.received_count > 0
            && slot.gen != meta.gen)
            slot.state = {};
        slot.gen = meta.gen;
    }
    /**
     * @brief 推进 sender generation, 用于 GC.
     *        在 on_event 入口处显式调用.
     */
    void gc_hook(uint8_t sender_id, uint8_t package_id)
    {
        auto& meta = _sender_meta[sender_id];
        if (package_id + (MP_PACKAGE_ID_MAX / 2) < meta.pkg)
            meta.gen++;
        meta.pkg = package_id;
    }

    const mp_rx_slice_state_t& get_state(mp_coordinate_t coord)
    {
        return (*this)[coord].state;
    }

    void set_state(mp_coordinate_t coord, const mp_rx_slice_state_t& state)
    {
        (*this)[coord].state = state;
    }

    void put_payload(mp_coordinate_t coord, std::span<const uint8_t> payload)
    {
        if(payload.empty()) return;

        auto& slot = (*this)[coord];

        const size_t needed = coord.offset + payload.size();
        if (slot.data.size() < needed) 
            slot.data.resize(needed);

        auto target_it = std::next(slot.data.begin(), coord.offset);
        std::copy(payload.begin(),payload.end(),target_it);
    }

    std::span<const uint8_t> get_payload(mp_coordinate_t coord)
    {
        auto& data = (*this)[coord].data;
        auto begin_it = std::next(data.begin(), coord.offset);
        return { begin_it, data.end() };
    }

    operator mp_rx_coordinator_t()
    {
        return mp_rx_coordinator_t{
            .ctx = this,
            .state_get = +[](void* ctx, mp_coordinate_t coord) -> const mp_rx_slice_state_t*
            {
                return &static_cast<Coordinator*>(ctx)->get_state(coord);
            },
            .state_put = +[](void* ctx, mp_coordinate_t coord, const mp_rx_slice_state_t* state)
            {
                static_cast<Coordinator*>(ctx)->set_state(coord, *state);
            },
            .payload_put = +[](void* ctx, mp_coordinate_t coord, const uint8_t* data, uint16_t size)
            {
                static_cast<Coordinator*>(ctx)->put_payload(coord, {data, size});
            },
            .payload_get = +[](void* ctx, mp_coordinate_t coord) -> const uint8_t*
            {
                auto view = static_cast<Coordinator*>(ctx)->get_payload(coord);
                return view.data();
            }
        };
    }

private:
    std::array<Slot, 1u << (MP_SENDER_ID_BITS + MP_PACKAGE_ID_BITS)> _slots;
    struct
    {
        uint8_t pkg;
        uint8_t gen;
    } _sender_meta[1 << MP_SENDER_ID_BITS];
};


struct RxDecoder::Impl
{
    Coordinator _coordinator;

    mp_rx_stream_t _stream;

    std::function<void(MsgPtr)> _observer;

    const google::protobuf::Message& _prototype;

    Impl(int tu,
         std::function<void(MsgPtr)> observer,
         const google::protobuf::Message& prototype
        )
        : _observer(std::move(observer))
        , _prototype(prototype)
    {
        _stream = mp_rx_stream_t{
            .config = {
                .transmission_unit = static_cast<uint16_t>(tu)
            },
            .coordinator = _coordinator,
            .on_event = +[](void* user, mp_rx_stream_event_t e,
                           const mp_coordinate_t* coord,
                           const mp_rx_coordinator_t* coordinator,
                           const mp_rx_slice_state_t* state,
                           uint32_t old_watermark,
                           const uint8_t* payload, uint16_t payload_size) {
                (void)old_watermark;
                auto& self = *static_cast<Impl*>(user);
                auto& specified_coordinator = *static_cast<Coordinator*>(coordinator->ctx);

                // 推进 sender generation, 触发 GC clock
                specified_coordinator.gc_hook(
                    coord->sender_id,
                    coord->package_id
                );

                // 有payload存payload. 因为直接用了大数组, 按位置放进去, 是免疫乱序的
                if (payload && payload_size > 0)
                    coordinator->payload_put(coordinator->ctx, *coord, payload, payload_size);

                // 有了上面的内存连续铺垫, 理论组装完毕后的信号在这里直接解码
                if (e == MP_RX_STREAM_COMPLETE)
                {

                    MsgPtr p_msg(self._prototype.New()); 

                    mp_coordinate_t normalized_coord
                    {
                        .sender_id = coord->sender_id,
                        .package_id = coord->package_id,
                        .offset = 0
                    };

                    auto payload_view = specified_coordinator.get_payload(normalized_coord);

                    assert(payload_view.size() >= state->termination);

                    const bool ok = p_msg->ParseFromArray(
                        payload_view.data(),
                        static_cast<int>(state->termination));

                    if (ok) self._observer(std::move(p_msg));

                    specified_coordinator.set_state(normalized_coord, {});
                }
            },
            .user = this
        };
    }
};


std::unique_ptr<RxDecoder::Impl> RxDecoder::MakeImpl(
    int tu,
    std::function<void(MsgPtr)> observer,
    const google::protobuf::Message& prototype)
{
    return std::make_unique<Impl>(tu, std::move(observer), prototype);
}

/* ========================================================================= */
/* RxDecoder 成员函数                                                        */
/* ========================================================================= */

RxDecoder::~RxDecoder() = default;

void RxDecoder::Push(std::span<const uint8_t> block)
{
    mp_rx_stream_feed(&_pimpl->_stream, block.data(),
                      static_cast<uint16_t>(block.size()));
}

}