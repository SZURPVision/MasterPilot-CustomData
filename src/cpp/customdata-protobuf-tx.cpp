#include <cstdint>
#include <cstring>
#include <google/protobuf/io/zero_copy_stream.h>
#include <masterpilot/customdata-common.h>
#include <masterpilot/customdata-protobuf-tx.hpp>
#include <masterpilot/customdata-tx.h>
#include <span>

namespace masterpilot::customdata
{

/*
 * ZeroCopyOutputStream 适配器.
 *
 * protobuf 的 ZeroCopyOutputStream 是"索取指针 + 回退确认"模型:
 *   Next() → 获取写指针, BackUp(count) → 确认实际写入量
 *
 * 与此相对, core stream API (mp_tx_stream_feed) 是"数据驱动推送"模型,
 * 需要预先知道数据大小才能正确管理 fill 计数器.
 *
 * ZeroCopyOutputStream的设计真是坑b完了:
 *   mp_tx_stream_feed 内部在 TU 不满时仅积攒数据不发送,
 *   而 ZeroCopyOutputStream 的 flush 逻辑会重置 cursor,
 *   导致未发送的积攒数据被覆盖.
 *
 * 本实现绕过 core stream API, 直接在 ZeroCopyOutputStream 层面
 * 使用 mp_tx_prepare / mp_header_pack 等纯函数进行 slice 管理.
 */

class TxStream final : public google::protobuf::io::ZeroCopyOutputStream
{
public:
    TxStream(
        mp_config_t                     config,
        mp_coordinate_t                 coord,
        std::span<uint8_t>              buffer,
        const TxEncoderCore::Observer&  observer
    ) :
        _config(config),
        _cursor(coord),
        _max_payload(mp_max_payload(config)),
        _hdr_buf(buffer.first(MP_HEADER_SIZE)),
        _payload_buf(buffer.subspan(MP_HEADER_SIZE)),
        _observer(observer)
    {}

    bool Next(void** data, int* size) override
    {
        /*
         * protobuf 的 CodedOutputStream 在需要新缓冲区时直接再次调用 Next(),
         * 不会在中途调用 BackUp(). 因此非首次 Next() 意味着上次的缓冲区已被
         * 完全消耗 (_payload_buf.size() 字节), 直接按满量提交 slice.
         */
        if (_next_called)
            commit_slice(false, static_cast<int>(_payload_buf.size()));

        *data = _payload_buf.data();
        *size = static_cast<int>(_payload_buf.size());
        _next_called = true;
        _written = 0;  /* 重置; BackUp() 会在最后一个缓冲区填充实际值 */
        return true;
    }

    void BackUp(int count) override
    {
        _written = static_cast<int>(_payload_buf.size()) - count;
    }

    int64_t ByteCount() const override
    {
        return _accumulated;
    }

    void finalize()
    {
        if (_next_called)
        {
            /*
             * 若 BackUp() 已被调用则 _written > 0, 表示缓冲区未满.
             * 否则缓冲区被完全消耗, 取全大小.
             */
            const int size = _written > 0
                ? _written
                : static_cast<int>(_payload_buf.size());
            commit_slice(true, size);
        }
    }

private:
    void commit_slice(bool eop, int size)
    {
        auto slice = mp_tx_prepare(
            _config, _cursor,
            static_cast<uint16_t>(size),
            eop
        );

        /* wire header 写入 hdr 区 (小端序写入, 不受平台大小端影响) */
        auto packed = mp_header_pack(slice.header);
        std::memcpy(_hdr_buf.data(), MP_PACKED_HEADER_TO_ARRAY(packed), MP_HEADER_SIZE);

        /* 帧大小恒为 TU, 不满部分由调用方随机填充 */
        _observer(std::span{
            _hdr_buf.data(),
            _hdr_buf.size() + _payload_buf.size()
        });

        _cursor = slice.next;
        _accumulated += size;
    }

    const mp_config_t                    _config;
    mp_coordinate_t                      _cursor;
    const std::uint16_t                  _max_payload;
    const std::span<uint8_t>             _hdr_buf;
    const std::span<uint8_t>             _payload_buf;
    const TxEncoderCore::Observer&       _observer;

    int64_t  _accumulated  = 0;
    int      _written      = 0;
    bool     _next_called  = false;
};

TxEncoderCore::TxEncoderCore(
    const std::uint16_t tu,
    const std::uint8_t sender_id,
    Observer next
) : _next(std::move(next)), _sender_id(sender_id), _tu(tu)
{ }

bool TxEncoderCore::Push(const google::protobuf::Message& msg, std::span<uint8_t> buffer)
{
    TxStream stream(
        { .transmission_unit = _tu },
        { .sender_id = _sender_id,
          .package_id = _current_package_id++,
          .offset = 0 },
        buffer,
        _next
    );

    const bool ok = msg.SerializeToZeroCopyStream(&stream);

    if (ok) stream.finalize();

    return ok;
}

}