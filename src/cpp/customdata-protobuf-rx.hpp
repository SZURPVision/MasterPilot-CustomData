#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <google/protobuf/message.h>
#include <memory>
#include <span>

namespace masterpilot::customdata
{


/*
 * @brief 接收解码器
*/
class RxDecoder final
{
public:
    template<std::derived_from<google::protobuf::Message> T>
    using Observer = std::function<void(const T& msg)>;

    /**
     * @brief 接收解码器构造函数
     *
     * @tparam T  protobuf消息类型
     * @param tu 传输单元大小. @see TxEncoder::TxEncoder
     * @param next 下游观察者. 按值传递, 支持lambda右值
     */
    template<std::derived_from<google::protobuf::Message> T>
    RxDecoder(const int tu, Observer<T> next)
        : RxDecoder(tu,
            [next = std::move(next)](MsgPtr msg) {
                next(static_cast<const T&>(*msg));
            },
            T::default_instance()
        )
    { }


    ~RxDecoder();

    RxDecoder (const RxDecoder&) = delete;
    RxDecoder operator=(const RxDecoder&) = delete;

    /**
     * @brief 流式压入待解码消息, 支持碎片化
    */
    void Push(std::span<const uint8_t> block);

private:
    struct Impl;
    std::unique_ptr<Impl> _pimpl;

    using MsgPtr = std::unique_ptr<google::protobuf::Message>;

    // 类型擦除使用
    RxDecoder(int tu, std::function<void(MsgPtr)> erased_observer, const google::protobuf::Message& prototype);
};


}
