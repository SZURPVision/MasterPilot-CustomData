#define MP_SENDER_MTU 1500
#define MP_SENDER_BUFFER_COUNT 16
#define MP_RECEIVER_MTU 300
#define MP_RECEIVER_BUFFER_COUNT 64

#include <hero.pb.h>
#include <mp-customdata-nanopb.h>
#include <mp-customdata.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

mp_sender_t sender;
mp_receiver_t receiver;
HeroDataPacketToClient msg;
int main()
{
    for (int i = 0; i < 10; i++)
    {
        memset(&msg, 0, HeroDataPacketToClient_size);
        msg.has_camera_frame = true;
        HeroDataPacketToClient_camera_frame_t *frame = &msg.camera_frame;
        char* head = (char*)(&frame->bytes);
        for (int j = 0; j < 50; j++)
        {
            size_t len = sprintf(head, "[%d-%d] Ciallo~\n",i,j);
            frame->size += len;
            head += len;
        }
        if(!MP_Encode(&sender, HeroDataPacketToClient_fields, &msg))
        {
            return 1;
        }
    }

    memset(&msg, 0, HeroDataPacketToClient_size);
    while(MP_Decode(&receiver, HeroDataPacketToClient_fields, &msg))
    {
        HeroDataPacketToClient_camera_frame_t *frame = &msg.camera_frame;
        for(int i = 0; i < frame->size; i ++)
        {
            putchar(frame->bytes[i]);
        }
        memset(&msg, 0, HeroDataPacketToClient_size);
    }
    return 0;
}