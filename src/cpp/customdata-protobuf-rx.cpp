#include <array>
#include <cassert>
#include <functional>
#include <iterator>
#include <memory>
#include <algorithm>
#include <span>
#include <vector>

#include <google/protobuf/arena.h>
#include <google/protobuf/message.h>
#include <masterpilot/customdata-common.h>
#include <masterpilot/customdata-protobuf-rx.hpp>
#include <masterpilot/customdata-rx.h>

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
    const mp_config_t               _config;
    Coordinator                     _coordinator;
    google::protobuf::Arena         _arena;

    std::function<void(const google::protobuf::Message&)> _observer;

    const google::protobuf::Message& _prototype;

    Impl(int tu,
         std::function<void(const google::protobuf::Message&)> observer,
         const google::protobuf::Message& prototype
        )
        : _config{ .transmission_unit = static_cast<uint16_t>(tu) }
        , _observer(std::move(observer))
        , _prototype(prototype)
    { }

    void Push(std::span<const uint8_t> block)
    {
        if (block.size() < MP_HEADER_SIZE) return;

        /* 1. 解包 header */
        const auto hdr = mp_header_unpack(
            MP_ARRAY_TO_PACKED_HEADER(block.data()));
        const uint16_t max_payload  = mp_max_payload(_config);
        const uint16_t payload_size = static_cast<uint16_t>(
            std::min<size_t>(hdr.slice_payload_size, block.size() - MP_HEADER_SIZE));
        const auto payload = block.subspan(MP_HEADER_SIZE, payload_size);

        /* 2. header → 坐标 */
        const auto coord = mp_rx_header_to_coordinate(_config, hdr);

        // 推进 sender generation, 触发 GC clock
        _coordinator.gc_hook(
            coord.sender_id,
            coord.package_id
        );

        /* 3. 纯函数: slice 状态转移 + 事件分类 */
        const auto& state = _coordinator.get_state(coord);
        const auto  inst  = mp_rx_calc_slice_step(
            state, hdr.slice_serial, payload_size, max_payload, hdr.eop
        );

        /* 4. 分类处理 */
        switch (inst.e)
        {
        case MP_RX_STREAM_DUPLICATE:
            return;

        case MP_RX_STREAM_NEW_SLICE_OUT_OF_ORDER:
        case MP_RX_STREAM_NEW_SLICE_IN_ORDER:
            // 直接用了大数组, 按位置放进去, 免疫乱序
            if (payload_size > 0)
                _coordinator.put_payload(coord, payload);
            _coordinator.set_state(coord, inst.next_state);
            break;

        case MP_RX_STREAM_COMPLETE:
        {
            if (payload_size > 0)
                _coordinator.put_payload(coord, payload);
            _coordinator.set_state(coord, inst.next_state);

            mp_coordinate_t normalized_coord
            {
                .sender_id = coord.sender_id,
                .package_id = coord.package_id,
                .offset = 0
            };

            auto* p_msg = _prototype.New(&_arena);

            auto payload_view = _coordinator.get_payload(normalized_coord);

            assert(payload_view.size() >= inst.next_state.termination);

            const bool ok = p_msg->ParseFromArray(
                payload_view.data(),
                static_cast<int>(inst.next_state.termination));

            if (ok) _observer(*p_msg);

            _coordinator.set_state(normalized_coord, {});
            break;
        }

        default:
            throw std::logic_error("mp_rx_stream_event_t: unknown event");
        }
    }
};



/* ========================================================================= */
/* RxDecoder 成员函数                                                        */
/* ========================================================================= */

RxDecoder::RxDecoder(
    int tu,
    std::function<void(const google::protobuf::Message&)> observer,
    const google::protobuf::Message& prototype)
    : _pimpl(std::make_unique<Impl>(tu, std::move(observer), prototype))
{ }

RxDecoder::~RxDecoder() = default;

void RxDecoder::Push(std::span<const uint8_t> block)
{
    _pimpl->Push(block);
}

}