#include <cstdint>
#include <google/protobuf/io/zero_copy_stream.h>
#include <masterpilot/customdata-common.h>
#include <masterpilot/customdata-protobuf-tx.hpp>
#include <masterpilot/customdata-stream-tx.h>
#include <vector>

namespace masterpilot::customdata
{

class TxStream final : public google::protobuf::io::ZeroCopyOutputStream
{
public:
    TxStream(
        mp_config_t                 config,
        mp_coordinate_t             coord,
        const TxEncoder::Observer&  observer
    ) :
        _max_payload(mp_max_payload(config)) ,
        _buffer(_max_payload) ,
        _observer(observer),
        _stream({
            .config     = config,
            .cursor     = coord,
            .fill       = 0,
            .downstream = { .push = trampoline, .user = this }
        })
    {}

    bool Next(void** data, int* size) override
    {
        flush_prev();
        *data = _buffer.data();
        *size = static_cast<int>(_buffer.size());
        _bytes_used   = static_cast<int>(_buffer.size());
        _next_called  = true;
        return true;
    }

    void BackUp(int count) override
    {
        _bytes_used -= count;
    }

    int64_t ByteCount() const override
    {
        return _accumulated;
    }

    void finalize()
    {
        flush_prev();
        mp_tx_stream_finalize(&_stream);
    }

private:
    const std::uint16_t              _max_payload;
    std::vector<uint8_t>             _buffer;
    mp_tx_stream_t                   _stream;
    const TxEncoder::Observer&       _observer;

    int64_t _accumulated  = 0;
    int     _bytes_used   = 0;
    bool    _next_called  = false;

    void flush_prev()
    {
        if (_next_called)
        {
            const int used = _bytes_used;
            if (used > 0)
            {
                mp_tx_stream_feed(&_stream, _buffer.data(),
                                  static_cast<uint16_t>(used));
                _accumulated += used;
            }
        }
    }

    static void trampoline(
        void*                    user,
        mp_tx_action_t           act,
        const mp_header_meta_t*  nullable_header,
        const uint8_t*           nullable_data,
        uint16_t                 size
    )
    {
        auto& self = *static_cast<TxStream*>(user);

        if (nullable_data != nullptr && size > 0)
        {
            /* 攒数据 */
            self._observer(
                std::span<const uint8_t>(nullable_data, size),
                self._stream.cursor.package_id,
                false
            );
        }

        if (nullable_header != nullptr)
        {
            /* 封包发送 */
            self._observer(
                std::span<const uint8_t>(),
                self._stream.cursor.package_id,
                true
            );
        }
    }
};

TxEncoder::TxEncoder(
    const std::uint16_t tu,
    const std::uint8_t sender_id,
    const Observer& next
) : _next(next), _sender_id(sender_id), _tu(tu)
{ }

bool TxEncoder::Push(const google::protobuf::Message& msg)
{
    TxStream stream(
        { .transmission_unit = _tu },
        { .sender_id = _sender_id,
          .package_id = _current_package_id++,
          .offset = 0 },
        _next
    );

    const bool ok = msg.SerializeToZeroCopyStream(&stream);

    if (ok) stream.finalize();

    return ok;
}

}