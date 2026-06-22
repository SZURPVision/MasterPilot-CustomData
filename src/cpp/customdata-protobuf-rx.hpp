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
            [next = std::move(next)](const google::protobuf::Message& msg) {
                next(static_cast<const T&>(msg));
            },
            T::default_instance()
        )
    { }


    ~RxDecoder();

    RxDecoder (const RxDecoder&) = delete;
    RxDecoder operator=(const RxDecoder&) = delete;

    /**
     * @brief 流式压入待解码消息分片
    */
    void Push(std::span<const uint8_t> block);

private:
    struct Impl;
    std::unique_ptr<Impl> _pimpl;

    /// @note observer 在回调返回后 Arena 即准备好复用当前消息引用, 因此 observer 必须同步消费
    RxDecoder(
        int tu,
        std::function<void(const google::protobuf::Message&)> erased_observer,
        const google::protobuf::Message& prototype);
};


}
