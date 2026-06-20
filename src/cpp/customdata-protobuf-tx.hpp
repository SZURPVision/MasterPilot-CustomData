#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <span>
#include <google/protobuf/message.h>

namespace masterpilot::customdata
{

/*
 * @brief 发包编码器
*/
class TxEncoder final
{

public:
    /**
     * @brief 观察者类型
     * @param block 可空数据块. 非空表示需要积攒数据块(拷贝)
     * @param package_id 当前包id, 用于在并发情况下做区分, 无并发可忽略
     * @param should_send true表示需要发送并清空数据块
     */
    using Observer = std::function<void (
        const std::span<const uint8_t> block,
        const std::uint8_t package_id,
        bool should_send
    )>;

    /**
     * @brief 发包编码器构造函数
     * @param tu 传输单元大小. 机器人 -> 自定义客户端 为固定值300, 自定义客户端 -> 机器人 为固定值30
     * @param sender_id 发送者id, 范围0~7
     * @param next 下游观察者
    */
    TxEncoder(
        const std::uint16_t tu,
        const std::uint8_t sender_id,
        const Observer& next
    );

    TxEncoder (const TxEncoder&) = delete;
    TxEncoder operator=(const TxEncoder&) = delete;

    /*
     * @brief 传入要编码的消息, 自动编码到缓冲区
     * @param msg 要编码的消息
     * @return 编码是否成功
    */
    bool Push(const google::protobuf::Message& msg);

private:
   const Observer& _next;
   const std::uint8_t _sender_id;
   const std::uint16_t _tu;
   std::atomic<uint8_t> _current_package_id;

};


}