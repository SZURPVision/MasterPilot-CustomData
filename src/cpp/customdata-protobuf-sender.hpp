#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <google/protobuf/message.h>

namespace masterpilot::customdata
{



/*
 * @brief 发包编码器
*/
class Sender final
{

public:
    /*
     * @brief 消费者方法, 接受只读内存块, 返回消费的字节数
    */
    using Consumer = std::function<int (const std::span<const uint8_t> block)>;

    /*
     * @brief 发包编码器构造函数
     * @param mtu 通信链路包**固定**大小. 机器人 -> 自定义客户端 为固定值300, 自定义客户端 -> 机器人 为固定值30
     * @param consumer 缓冲区消费者, 调用`Flush`时会间接调用, 接具体的通信发送方法.
    */
    Sender(const std::uint16_t mtu, const int buffer_count, const Consumer& consumer);

    Sender (const Sender&) = delete;
    Sender operator=(const Sender&) = delete;

    /*
     * @brief 传入要编码的消息, 自动编码到缓冲区
     * @param msg 要编码的消息
     * @return 是否成功
    */
    bool Feed(const google::protobuf::Message& msg);

    /*
     * @brief 将缓冲区内容刷到`consumer`, 并释放部分缓冲区空间
     * @return 是否成功
    */
    bool Flush();

private:
   const Consumer& _consumer;

   struct Impl;
   const std::unique_ptr<Impl> _pimpl;

};


}