#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <google/protobuf/message.h>

namespace masterpilot::customdata
{

/*
 * @brief 发包编码器
*/
class TxEncoderCore
{

public:
    /**
     * @brief 观察者. 大小恒定为 tu, 可以直接拷到发送缓冲区.
     * @param frame 完整帧数据块
    */
    using Observer = std::function<void (
        const std::span<const uint8_t> frame
    )>;

    /**
     * @brief 发包编码器构造函数
     * @param tu 传输单元大小. 机器人 -> 自定义客户端 为固定值300, 自定义客户端 -> 机器人 为固定值30
     * @param sender_id 发送者id, 范围0~7
     * @param next 下游观察者
    */
    TxEncoderCore(
        const std::uint16_t tu,
        const std::uint8_t sender_id,
        Observer next
    );

    TxEncoderCore (const TxEncoderCore&) = delete;
    TxEncoderCore operator=(const TxEncoderCore&) = delete;

    /**
     * @brief 传入要编码的消息, 自动编码到缓冲区
     * @param msg 要编码的消息
     * @param buffer 临时缓冲区, 需要不小于tu.
     * @note 如果你更希望发送链路无拷贝, 可以直接把目标缓冲区传入, observer仅作为发送信号
     * @return 编码是否成功
    */
    bool Push(const google::protobuf::Message& msg, std::span<std::uint8_t> buffer);

private:
   Observer _next;
   const std::uint8_t _sender_id;
   const std::uint16_t _tu;
   std::atomic<uint8_t> _current_package_id;

};

/**
 * @brief 发包编码器的固定TU版本, 可以自动使用栈分配
 * 
 * @tparam TU 传输单元大小
 */
template<std::size_t TU=0>
class TxEncoder final
{
    static_assert(TU <= 4096, "TU is too large, stack alloc is dangerous" );
    public:
    TxEncoder(
        const std::uint8_t sender_id,
        TxEncoderCore::Observer next
    ) : _core(TU,sender_id,next)
    { }

    bool Push(const google::protobuf::Message& msg)
    {
        std::array<std::uint8_t, TU> stack_alloc_buffer;
        return _core.Push(msg, stack_alloc_buffer);
    }

    private:
    TxEncoderCore _core;
};

template<>
class TxEncoder<0> final : public TxEncoderCore
{ };

}