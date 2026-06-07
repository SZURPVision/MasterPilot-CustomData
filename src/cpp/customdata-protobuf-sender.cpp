#include <masterpilot/customdata-protobuf-sender.hpp>
#include <masterpilot/customdata-core.h>
#include <absl/strings/cord.h>
#include <cassert>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace masterpilot::customdata
{

class SenderStream final : public google::protobuf::io::ZeroCopyOutputStream
{

public:
    SenderStream(mp_sender_t& sender)
    {
        _total_avaliable = MP_BlockWriter_Begin(&_writer, &sender);
    }
    ~SenderStream() override
    {
        if (_written > 0)
        {
            if (_last_returned_size > 0) 
                MP_BlockWriter_CommitBytes(&_writer, _last_returned_size);


            MP_BlockWriter_Commit(&_writer, _written);
        }
        else
            MP_BlockWriter_Rollback(&_writer);
    }
    bool Next(void **data, int* size) override
    {
        if (_written >= _total_avaliable) return false; 

        if (_last_returned_size > 0)
        {
            MP_BlockWriter_CommitBytes(&_writer, _last_returned_size);
            _last_returned_size = 0;
        }

        uint8_t* ptr = nullptr;
        auto avaliable = MP_BlockWriter_Acquire(&_writer, &ptr);

        avaliable = avaliable > (_total_avaliable - _written) ? _total_avaliable - _written : avaliable;

        *data = ptr;
        *size = avaliable;


        _last_returned_size = avaliable;
        _written += avaliable;
        return true;
    }
    void BackUp(int count) override
    {
        assert(count >0 && count <= _last_returned_size);

        _written -= count;
        _last_returned_size -= count;
    }
    int64_t ByteCount() const override
    { return _written; }

private:
    int _written = 0;
    int _last_returned_size = 0;
    int _total_avaliable;
    mp_block_writer_t _writer;
};

struct Sender::Impl
{
    std::vector<uint8_t> buffer;
    mp_sender_t sender;

    Impl(const std::uint16_t mtu, const int buffer_count) :
        buffer(mtu*buffer_count),
        sender {
            .config = 
            {
                .buffer = buffer.data(),
                .buffer_count = static_cast<uint8_t>(buffer_count),
                .mtu = mtu
            }
        }
    {}

};

static uint16_t MP_ConsumerTrampoline(const uint8_t* block, const uint16_t block_length, void* user)
{
    auto* consumer = static_cast<Sender::Consumer*>(user);
    auto consumed = (*consumer)({block,block_length});
    return static_cast<uint16_t>(consumed);
}

Sender::Sender(const std::uint16_t mtu, const int buffer_count, const Consumer& consumer) : _consumer(consumer), _pimpl(std::make_unique<Sender::Impl>(mtu,buffer_count))
{ }

bool Sender::Feed(const google::protobuf::Message& msg)
{
    SenderStream stream {_pimpl->sender};
    return msg.SerializeToZeroCopyStream(&stream);
}

bool Sender::Flush()
{
    return MP_Send(&_pimpl->sender, MP_ConsumerTrampoline, (void*)(&_consumer));
}

}