#ifndef __MP_CUSTOMDATA_STREAM_RX_H_
#define __MP_CUSTOMDATA_STREAM_RX_H_

#include "customdata-rx.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma region RX Coordinator

typedef struct {
    void *ctx;

    mp_rx_slice_state_t *(*state_get)(void *ctx, mp_coordinate_t coord);

    void (*watermark_put)(void *ctx, mp_coordinate_t coord, uint32_t wm);
    uint32_t (*watermark_get)(void *ctx, mp_coordinate_t coord);

    void (*payload_put)(void *ctx, mp_coordinate_t coord,
                        const uint8_t *payload, uint16_t size);
    const uint8_t *(*payload_get)(void *ctx, mp_coordinate_t coord);

} mp_rx_coordinator_t;

#pragma endregion

#pragma region RX Data Callback

typedef void (*mp_rx_data_cb)(
    void          *user,
    const uint8_t *data,
    uint32_t       offset,
    uint16_t       size,
    bool           eop
);

#pragma endregion

#pragma region RX Stream

typedef struct {
    mp_config_t          config;
    mp_rx_coordinator_t  coordinator;
    mp_rx_data_cb        on_data;
    void                *user;
} mp_rx_stream_t;

void mp_rx_stream_init(
    mp_rx_stream_t      *s,
    mp_config_t          config,
    mp_rx_coordinator_t  coordinator,
    mp_rx_data_cb        on_data,
    void                *user
);

void mp_rx_stream_feed(
    mp_rx_stream_t *s,
    const uint8_t  *frame,
    uint16_t        frame_size
);

#pragma endregion

#ifdef __cplusplus
}
#endif

#endif