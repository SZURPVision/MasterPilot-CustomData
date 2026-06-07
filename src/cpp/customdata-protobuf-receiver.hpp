#pragma once

#include <concepts>
#include <cstdint>
#include <google/protobuf/message.h>
#include <memory>
#include <optional>
#include <span>
namespace masterpilot::customdata
{

/*
 * @brief 接收解码器
*/
class Receiver
{
public:
    /*
     * @brief 接收解码器构造函数
     * @copydoc Sender::Sender
    */
    Receiver(const int mtu, const int buffer_count);

    /*
     * @brief 压入缓冲区
     * @param block 来自于通信层的缓冲区数据块
     * @return 是否成功
    */
    bool Push(std::span<const uint8_t> block);

    /*
     * @brief 从缓冲区解码出消息包, 并释放部分缓冲区
     * @return 解码出来的强类型包. 若失败则返回空
    */
    template<std::derived_from<google::protobuf::Message> T>
    std::optional<T> Pop()
    {
        T msg;
        if(TryParseFromReader(msg)) return msg;
        return {};
    }

private:
    struct Impl;
    std::unique_ptr<Impl> _pimpl;

    bool TryParseFromReader(google::protobuf::Message& msg);
};

}