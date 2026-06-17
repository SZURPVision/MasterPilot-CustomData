#include <masterpilot/customdata-embedded-encode.h>
#include <masterpilot/customdata-embedded-decode.h>
#include <masterpilot/customdata-common.h>
#include <masterpilot/proto/test/test.pb.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ── 测试宏 ────────────────────────────────────────────────────── */

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL line %d: " #cond "\n", __LINE__); \
        abort(); \
    } \
} while(0)

/* ── 共享常量 ──────────────────────────────────────────────────── */

#define TU 60
#define ASSEMBLY_SIZE (256 * (TU - MP_HEADER_SIZE))  /* 14336, 容纳 256 个最大切片 */

/* ── encoder 帧收集 ────────────────────────────────────────────── */

static uint8_t  tx_buf[TU * 100];
static uint16_t tx_frame_count;

static void enc_put_cb(void *user, uint16_t offset, const uint8_t *block, uint16_t size)
{
    (void)user;
    memcpy(tx_buf + tx_frame_count * TU + offset, block, size);
}

static void enc_send_cb(void *user)
{
    (void)user;
    tx_frame_count++;
}

/* ── decoder 数据存取 ──────────────────────────────────────────── */

static uint8_t assembly[ASSEMBLY_SIZE];

static void dec_data_put(void *ctx, mp_coordinate_t coord, const uint8_t *data, uint16_t size)
{
    (void)ctx;
    memcpy(assembly + coord.offset, data, size);
}

static const uint8_t *dec_data_get(void *ctx, mp_coordinate_t coord)
{
    (void)ctx;
    return assembly + coord.offset;
}

/* ── decoder 消息接收 ──────────────────────────────────────────── */

/* nanopb 解码直接写入此处, on_message 仅标记 */
static TestMessage decoded_msg;
static bool         msg_received;

static void on_message(void *msg, void *user)
{
    (void)user;
    (void)msg;   /* 已通过 output_config.msg 指向 decoded_msg, 无需再次拷贝 */
    msg_received = true;
}

/* ── bytes 比较辅助 ────────────────────────────────────────────── */

static bool bytes_equal(const pb_bytes_array_t *pb, const uint8_t *expect, size_t expect_len)
{
    if (!pb) return expect_len == 0;
    return pb->size == expect_len && memcmp(pb->bytes, expect, expect_len) == 0;
}

static bool bytes_equal_bytes(const pb_bytes_array_t *a, const pb_bytes_array_t *b)
{
    if (!a && !b) return true;
    if (!a || !b) return false;
    return a->size == b->size && memcmp(a->bytes, b->bytes, a->size) == 0;
}

/* ── 编码 + 解码 helper ────────────────────────────────────────── */

static bool encode_and_feed(
    mp_pb_decoder_inst_t *dec,
    uint8_t               sender_id,
    uint8_t               package_serial,
    const TestMessage    *msg
)
{
    mp_pb_encoder_inst_t enc = {
        .user           = NULL,
        .config         = (mp_config_t){ .transmission_unit = TU },
        .sender_id      = sender_id,
        .data_put_cb    = enc_put_cb,
        .send_signal_cb = enc_send_cb
    };
    tx_frame_count = 0;
    bool ok = mp_pb_encode(&enc, TestMessage_fields, msg, package_serial);
    if (!ok) return false;

    for (uint16_t i = 0; i < tx_frame_count; i++) {
        ok = mp_pb_decode_feed(dec, tx_buf + i * TU, TU);
        if (!ok) return false;
    }
    return true;
}

/* ── 测试用例 ──────────────────────────────────────────────────── */

static void test_small_packet(void)
{
    TestMessage msg = TestMessage_init_zero;
    msg.id = 42;
    uint8_t hello[] = "hello";
    msg.data.size = 5;
    memcpy(msg.data.bytes, hello, 5);

    mp_pb_decoder_inst_t dec = {
        .config        = (mp_config_t){ .transmission_unit = TU },
        .output_config = {
            .fields     = TestMessage_fields,
            .msg        = &decoded_msg,
            .on_message = on_message,
            .user       = NULL
        }
    };
    mp_pb_decode_init(&dec, dec_data_put, dec_data_get);

    msg_received = false;
    bool ok = encode_and_feed(&dec, 0, 0, &msg);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(msg_received);

    ASSERT_TRUE(decoded_msg.id == 42);
    ASSERT_TRUE(bytes_equal((const pb_bytes_array_t *)&decoded_msg.data, hello, 5));
    ASSERT_TRUE(decoded_msg.extra.size == 0);
}

static void test_large_bytes_cross_tu(void)
{
    /* 构造 200 字节 data — 编码后超过 max_payload, 跨至少两个切片 */
    uint8_t large_data[200];
    for (int i = 0; i < 200; i++)
        large_data[i] = (uint8_t)(i ^ 0xA5);

    TestMessage msg = TestMessage_init_zero;
    msg.id = 1;
    msg.data.size = 200;
    memcpy(msg.data.bytes, large_data, 200);
    msg.extra.size = 10;
    memset(msg.extra.bytes, 0xFF, 10);

    mp_pb_decoder_inst_t dec = {
        .config        = (mp_config_t){ .transmission_unit = TU },
        .output_config = {
            .fields     = TestMessage_fields,
            .msg        = &decoded_msg,
            .on_message = on_message,
            .user       = NULL
        }
    };
    mp_pb_decode_init(&dec, dec_data_put, dec_data_get);

    msg_received = false;
    bool ok = encode_and_feed(&dec, 0, 1, &msg);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(msg_received);

    ASSERT_TRUE(decoded_msg.id == 1);
    ASSERT_TRUE(bytes_equal((const pb_bytes_array_t *)&decoded_msg.data, large_data, 200));
    ASSERT_TRUE(bytes_equal_bytes((const pb_bytes_array_t *)&decoded_msg.extra, (const pb_bytes_array_t *)&msg.extra));
}

static void test_multi_packet(void)
{
    mp_pb_decoder_inst_t dec = {
        .config        = (mp_config_t){ .transmission_unit = TU },
        .output_config = {
            .fields     = TestMessage_fields,
            .msg        = &decoded_msg,
            .on_message = on_message,
            .user       = NULL
        }
    };
    mp_pb_decode_init(&dec, dec_data_put, dec_data_get);

    /* 包 0 */
    TestMessage m0 = TestMessage_init_zero;
    m0.id = 100;
    uint8_t d0[] = "first";
    m0.data.size = 5;
    memcpy(m0.data.bytes, d0, 5);

    msg_received = false;
    bool ok = encode_and_feed(&dec, 0, 0, &m0);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(msg_received);
    ASSERT_TRUE(decoded_msg.id == 100);
    ASSERT_TRUE(bytes_equal((const pb_bytes_array_t *)&decoded_msg.data, d0, 5));

    /* 包 1 */
    TestMessage m1 = TestMessage_init_zero;
    m1.id = 200;
    uint8_t d1[] = "second";
    m1.data.size = 6;
    memcpy(m1.data.bytes, d1, 6);

    msg_received = false;
    ok = encode_and_feed(&dec, 0, 1, &m1);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(msg_received);
    ASSERT_TRUE(decoded_msg.id == 200);
    ASSERT_TRUE(bytes_equal((const pb_bytes_array_t *)&decoded_msg.data, d1, 6));

    /* 包 2 */
    TestMessage m2 = TestMessage_init_zero;
    m2.id = 255;
    uint8_t d2[] = "third";
    m2.data.size = 5;
    memcpy(m2.data.bytes, d2, 5);

    msg_received = false;
    ok = encode_and_feed(&dec, 0, 2, &m2);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(msg_received);
    ASSERT_TRUE(decoded_msg.id == 255);
    ASSERT_TRUE(bytes_equal((const pb_bytes_array_t *)&decoded_msg.data, d2, 5));
}

static void test_decode_failure(void)
{
    mp_pb_decoder_inst_t dec = {
        .config        = (mp_config_t){ .transmission_unit = TU },
        .output_config = {
            .fields     = TestMessage_fields,
            .msg        = &decoded_msg,
            .on_message = on_message,
            .user       = NULL
        }
    };
    mp_pb_decode_init(&dec, dec_data_put, dec_data_get);

    /* 构造非法帧：eop=1, payload 为随机垃圾, 触发 COMPLETE → pb_decode 失败 */
    uint8_t bad_frame[TU];
    mp_header_meta_t bad_hdr = {
        .package_serial     = 0,
        .slice_serial       = 0,
        .sender_id          = 0,
        .eop                = true,
        .slice_payload_size = 10  /* < max_payload, 触发 termination 计算 */
    };
    memcpy(bad_frame, MP_PACKED_HEADER_TO_ARRAY(mp_header_pack(bad_hdr)), MP_HEADER_SIZE);
    /* payload 区域填随机垃圾 */
    for (int i = MP_HEADER_SIZE; i < TU; i++)
        bad_frame[i] = (uint8_t)(i * 17 + 3);

    msg_received = false;
    bool ok = mp_pb_decode_feed(&dec, bad_frame, TU);
    /* 预期解码失败 → feed 返回 false, on_message 未被调用 */
    ASSERT_TRUE(!ok);
    ASSERT_TRUE(!msg_received);
}

/* ── main ──────────────────────────────────────────────────────── */

int main(void)
{
    test_small_packet();
    test_large_bytes_cross_tu();
    test_multi_packet();
    test_decode_failure();

    printf("ALL TESTS PASSED\n");
    return 0;
}