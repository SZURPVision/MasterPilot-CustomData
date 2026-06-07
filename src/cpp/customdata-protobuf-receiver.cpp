#include <masterpilot/customdata-protobuf-receiver.hpp>
#include <masterpilot/customdata-core.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <cassert>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace masterpilot::customdata
{

class ReceiverStream final : public google::protobuf::io::ZeroCopyInputStream
{
public:
    ReceiverStream(mp_receiver_t& receiver)
    {
        _total_available = MP_BlockReader_Begin(&_reader, &receiver);
    }

    ~ReceiverStream() override
    {
        MP_BlockReader_Finish(&_reader);
    }

    bool Next(const void** data, int* size) override
    {
        if (_read >= _total_available)
            return false;

        if (_last_returned_size > 0)
        {
            MP_BlockReader_Advance(&_reader, _last_returned_size);
            _last_returned_size = 0;
        }

        const uint8_t* ptr = nullptr;
        auto available = MP_BlockReader_Acquire(&_reader, &ptr);

        available = available > (_total_available - _read)
                         ? _total_available - _read
                         : available;

        *data = ptr;
        *size = available;

        _last_returned_size = available;
        _read += available;
        return true;
    }

    void BackUp(int count) override
    {
        assert(count > 0 && count <= _last_returned_size);

        _read -= count;
        _last_returned_size -= count;
    }

    bool Skip(int count) override
    {
        if (count < 0)
            return false;
        if (count == 0)
            return true;

        while (count > 0 && _read < _total_available)
        {
            const void* data = nullptr;
            int chunk = 0;
            if (!Next(&data, &chunk))
                return false;

            if (chunk > count)
            {
                BackUp(chunk - count);
                count = 0;
            }
            else
            {
                count -= chunk;
            }
        }

        return count == 0;
    }

    int64_t ByteCount() const override
    {
        return _read;
    }

private:
    int _read = 0;
    int _last_returned_size = 0;
    int _total_available;
    mp_block_reader_t _reader;
};

struct Receiver::Impl
{
    std::vector<uint8_t> buffer;
    mp_receiver_t receiver;

    Impl(const int mtu, const int buffer_count)
        : buffer(mtu * buffer_count)
        , receiver{
              .config =
                  {
                      .buffer = buffer.data(),
                      .buffer_count = static_cast<uint8_t>(buffer_count),
                      .mtu = static_cast<uint16_t>(mtu),
                  },
          }
    { }
};

Receiver::Receiver(const int mtu, const int buffer_count)
    : _pimpl(std::make_unique<Impl>(mtu, buffer_count))
{ }

bool Receiver::Push(std::span<const uint8_t> block)
{
    MP_Receive(&_pimpl->receiver, block.data());
    return _pimpl->receiver.package_complete;
}

bool Receiver::TryParseFromReader(google::protobuf::Message& msg)
{
    ReceiverStream stream{_pimpl->receiver};
    return msg.ParseFromZeroCopyStream(&stream);
}

}