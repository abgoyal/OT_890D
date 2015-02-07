



#ifndef _YUSU_IOCTL_H
#define _YUSU_IOCTL_H

typedef struct
{
    UINT32 offset;
    UINT32 value;
    UINT32 mask;
}_Reg_Data;

//below is control message
#define YUSU_SET_ASM_REG  0x00
#define YUSU_GET_ASM_REG  0x01
#define YUSU_SET_ANA_REG  0x02
#define YUSU_GET_ANA_REG  0x03

#define YUSU_SET_MMAP_INDEX 0x10
#define YUSU_GET_ASM_BUFFER_SIZE 0x11
#define YUSU_SET_SPEAKER_VOL 0x12
#define YUSU_SET_SPEAKER_ON 0x13
#define YUSU_SET_SPEAKER_OFF 0x14
#define YUSU_SET_HEADSET 0x15

#define YUSU_OPEN_OUTPUT_STREAM 0x20
#define YUSU_CLOSE_OUTPUT_STREAM 0x21
#define YUSU_SET_OUTPUT_ATTRIBUTE 0x22
#define YUSU_OUPUT_STREAM_START    0x23
#define YUSU_OUPUT_STREAM_STANDBY 0x24

#define YUSU_OPEN_INPUT_STREAM 0x30
#define YUSU_CLOSE_INPUT_STREAM 0x31
#define YUSU_INPUT_STREAM_START    0x33
#define YUSU_INPUT_STREAM_STANDBY 0x34

#define YUSU_Set_2IN1_SPEAKER 0x41
#define YUSU_Set_AUDIO_STATE   0x42
#define YUSU_Get_AUDIO_STATE   0x43


#define YUSU_INIT_STREAM 0x51

#define YUSU_ASSERT_IOCTL 0xFE
#define YUSU_STOP_IOCTL 0xFF

#endif
