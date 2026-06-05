#ifndef __MP_CUSTOM_DATA_NANOPB_H_
#define __MP_CUSTOM_DATA_NANOPB_H_

#include "mp-customdata.h"
#include <pb.h>

#ifdef __cplusplus
extern "C" {
#endif

bool MP_Encode(mp_sender_t *sender, const pb_msgdesc_t *fields, const void *message);
bool MP_Decode(mp_receiver_t *receiver, const pb_msgdesc_t *fields, void *message);

#ifdef __cplusplus
}
#endif

#endif