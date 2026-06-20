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
    RxDecoder(const int tu, Observer<T> next);

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

    /** @brief 类型擦除工厂, 定义在cpp中 */
    static std::unique_ptr<Impl> MakeImpl(
        int tu,
        std::function<void(MsgPtr)> observer,
        const google::protobuf::Message& prototype
    );
};

/*
 * 模板构造函数 — 头文件内定义, 完成 T → MsgPtr 的一次性类型擦除
 */
template<std::derived_from<google::protobuf::Message> T>
RxDecoder::RxDecoder(const int tu, Observer<T> next)
    : _pimpl(MakeImpl(tu,
        [next = std::move(next)](MsgPtr msg) {
            next(*msg);
        },
        T::default_instance()
    ))
{ }

}
