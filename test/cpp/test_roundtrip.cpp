#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <masterpilot/proto/test/test.pb.h>
#include <masterpilot/customdata-common.h>
#include "test_harness.hpp"

using namespace masterpilot::customdata;
using namespace masterpilot::customdata::test;

static constexpr std::uint16_t kTu        = 60;
static constexpr std::uint8_t  kSender    = 1;
static constexpr std::size_t   kHeaderLen = 4;

static TestMessage make_msg(std::uint32_t id, std::string data)
{
    TestMessage msg;
    msg.set_id(id);
    msg.set_data(std::move(data));
    return msg;
}

static void test_small_packet()
{
    Harness<TestMessage,kTu> h(kSender);
    bool ok = h.roundtrip(make_msg(42, "hello"));
    assert(ok);
    assert(h.last->id() == 42);
    assert(h.last->data() == "hello");
}

static void test_large_bytes()
{
    Harness<TestMessage,kTu> h( kSender);

    std::string large(200, '\0');
    for (int i = 0; i < 200; ++i)
        large[i] = static_cast<char>(i ^ 0xA5);

    TestMessage msg;
    msg.set_id(1);
    msg.set_data(large);
    msg.set_extra(std::string(10, '\xFF'));

    bool ok = h.roundtrip(msg);
    assert(ok);
    assert(h.last->id() == 1);
    assert(h.last->data() == large);
    assert(h.last->extra() == std::string(10, '\xFF'));
}

static void test_multi_packet()
{
    Harness<TestMessage, kTu> h(kSender);

    bool ok = h.roundtrip(make_msg(100, "first"));
    assert(ok);
    assert(h.last->id() == 100);

    ok = h.roundtrip(make_msg(200, "second"));
    assert(ok);
    assert(h.last->id() == 200);

    ok = h.roundtrip(make_msg(255, "third"));
    assert(ok);
    assert(h.last->id() == 255);
}

static void test_decode_failure()
{
    /* 非法帧: eop + 随机 payload, 触发 COMPLETE 但解码失败 */
    Harness<TestMessage, kTu> h(kSender);

    std::vector<std::uint8_t> bad_frame(kTu);
    mp_header_meta_t hdr{};
    hdr.package_serial     = 0;
    hdr.eop                = true;
    hdr.slice_payload_size = kTu - kHeaderLen - 1; /* < max_payload */

    auto packed = mp_header_pack(hdr);
    std::memcpy(bad_frame.data(), &packed, kHeaderLen);
    for (int i = kHeaderLen; i < kTu; ++i)
        bad_frame[i] = static_cast<std::uint8_t>(i * 17 + 3);

    h.last.reset();
    h.decoder.Push(bad_frame);
    assert(!h.last.has_value());
}

int main()
{
    test_small_packet();
    test_large_bytes();
    test_multi_packet();
    test_decode_failure();
    }