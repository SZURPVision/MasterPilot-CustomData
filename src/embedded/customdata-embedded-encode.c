#include "masterpilot/customdata-stream-tx.h"
#include <masterpilot/customdata-common.h>
#include <masterpilot/customdata-embedded-encode.h>
#include <stdbool.h>
#include <pb_encode.h>
#include <stdint.h>


static bool pb_ostream_pipeline(pb_ostream_t *stream, const pb_byte_t *buf, size_t count)
{
    mp_pb_encoder_inst_t* instance = (typeof(instance))stream->state;
    mp_tx_stream_feed(
        &instance->mp_stream,
        buf,
        count
    );
    return true;
}

static void mp_tx_push_pipeline(
    void                    *user,
    mp_tx_action_t          act,
    const mp_header_meta_t* nullable_header,
    const uint8_t           *nullable_data,
    uint16_t                size
)
{
    (void)act; //无论什么情况, 都可以用这段判断操作搞定.
    mp_pb_encoder_inst_t* instance = (typeof(instance))user;

    if(nullable_data && size)
    {
        instance->data_put_cb(
            instance->user,
            instance->accumulated + MP_HEADER_SIZE, //预留header位置.
            nullable_data,
            size
        );
        instance->accumulated += size;
    }
    if(nullable_header)
    {
        instance->data_put_cb(
            instance->user,
            0,
            MP_PACKED_HEADER_TO_ARRAY(mp_header_pack(*nullable_header)),
            MP_HEADER_SIZE
        );
        //send
        instance->send_signal_cb(instance->user);
        instance->accumulated = 0;
    }
}


bool mp_pb_encode(mp_pb_encoder_inst_t *instance, const pb_msgdesc_t *fields, const void *message, uint8_t package_serial)
{
    // 初始化instance内部状态.
    instance->mp_stream = (mp_tx_stream_t){
        .cursor = {
            .sender_id = instance->sender_id,
            .package_id = package_serial,
            .offset = 0
        },
        .config = instance->config,
        .downstream = {
            .push = mp_tx_push_pipeline,
            .user = instance
        }
    };
    instance->accumulated = 0;

    // nanopb编码流触发
    pb_ostream_t stream = {
        .callback = pb_ostream_pipeline,
        .bytes_written = SIZE_MAX,
        .state = instance
    };
    bool result = pb_encode(&stream, fields, message);
    if(!result) return false;

    // finalize
    mp_tx_stream_finalize(&instance->mp_stream);
    return true;
}