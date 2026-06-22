#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <google/protobuf/message.h>
#include <masterpilot/customdata-protobuf-tx.hpp>
#include <masterpilot/customdata-protobuf-rx.hpp>

namespace masterpilot::customdata::test
{

/** @brief 聚合 TxEncoder 输出的完整帧, 供 RxDecoder 消费 */
class FrameBuffer
{
public:
    FrameBuffer(std::uint16_t tu) :_tu(tu)
    { }

    void observer(std::span<const std::uint8_t> frame)
    {
        _frames.insert(_frames.end(), frame.begin(), frame.end());
    }

    void drain(RxDecoder& decoder)
    {
        for (std::size_t i = 0; i < _frames.size(); i += _tu)
            decoder.Push(std::span{_frames}.subspan(i, _tu));
        _frames.clear();
    }

private:
    const std::uint16_t _tu = 0;
    std::vector<std::uint8_t> _frames;
};

/** @brief 编码 → 帧传输 → 解码 一次性往返测试工具 */
template<std::derived_from<google::protobuf::Message> T, size_t TU>
struct Harness
{
    FrameBuffer         fb { TU };
    TxEncoder<TU>       encoder;
    RxDecoder           decoder;
    std::optional<T>    last;

    Harness(std::uint8_t sender_id)
        : encoder(sender_id,
                  [this](auto frame) { fb.observer(frame); })
        , decoder(TU, RxDecoder::Observer<T>{
                  [this](const T& m) { last = m; }})
    { }

    bool roundtrip(const T& msg)
    {
        last.reset();
        if (!encoder.Push(msg)) return false;
        fb.drain(decoder);
        return last.has_value();
    }
};

}