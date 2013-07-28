#ifndef QK_COMM_H
#define QK_COMM_H

#include "stdint.h"

namespace Qk {
    typedef struct PacketDescriptor
    {
        uint64_t address;
        uint8_t  code;
        uint8_t  boardType;
        union {
            struct {

            };
        };
    } PacketDescriptor;
}

/******************************************************************************/
#define QK_COMM_WAKEUP      0x00
#define QK_COMM_FLAG        0x55	// Flag
#define QK_COMM_SEQBEGIN	0xCB	// Sequence begin
#define QK_COMM_SEQEND		0xCE	// Sequence end
#define QK_COMM_DLE			0xDD	// Data Link Escape
#define QK_COMM_NACK		0x00
#define QK_COMM_ACK         0x02
#define QK_COMM_OK			0x01
#define QK_COMM_ERR			0xFF
#define QK_COMM_TIMEOUT		0xFE

#define QK_COMM_SAVE        0x04
#define QK_COMM_RESTORE     0x05

#define QK_COMM_START		0x0A
#define QK_COMM_STOP		0x0F

#define QK_COMM_SEARCH          0x1F
#define QK_COMM_GET_NODE        0x10
#define QK_COMM_GET_MODULE      0x11
#define QK_COMM_GET_DEVICE      0x12
#define QK_COMM_GET_NETWORK     0x13
#define QK_COMM_GET_GATEWAY     0x14
#define QK_COMM_GET_QK          0x15
#define QK_COMM_GET_SAMP        0x16
#define QK_COMM_GET_STATUS      0x17
#define QK_COMM_GET_DATA        0x18
#define QK_COMM_GET_CALENDAR    0x19

#define QK_COMM_GET_INFO_ACTION 0x2A
#define QK_COMM_GET_INFO_DATA   0x2D
#define QK_COMM_GET_INFO_CONFIG 0x2C
#define QK_COMM_GET_INFO_EVENT  0x2E

#define QK_COMM_SET_QK          0x33
#define QK_COMM_SET_NAME        0x34
#define QK_COMM_SET_SAMP		0x36
#define QK_COMM_SET_CALENDAR    0x39
#define QK_COMM_SET_CONFIG      0x3C
#define QK_COMM_ACTUATE         0x3A

#define QK_COMM_SET_BAUD        0x40
#define QK_COMM_SET_FREQ        0x41

#define QK_COMM_INFO_QK         0xB1
#define QK_COMM_INFO_SAMP		0xB2
#define QK_COMM_INFO_BOARD      0xB5
#define QK_COMM_INFO_MODULE		0xB6
#define QK_COMM_INFO_DEVICE		0xB7
#define QK_COMM_INFO_NETWORK    0xB8
#define QK_COMM_INFO_GATEWAY    0xB9
#define QK_COMM_INFO_ACTION     0xBA
#define QK_COMM_INFO_DATA		0xBD
#define QK_COMM_INFO_EVENT		0xBE
#define QK_COMM_INFO_CONFIG		0xBC

#define QK_COMM_CALENDAR        0xD1
#define QK_COMM_SIZES           0xD2
#define QK_COMM_STATUS          0xD5
#define QK_COMM_DATA            0xD0
#define QK_COMM_EVENT           0xDE
#define QK_COMM_STRING          0xDF
/******************************************************************************/
typedef enum qk_error {
  QK_ERR_CODE_UNKNOWN,
  QK_ERR_UNSUPPORTED_OPERATION,
  QK_ERR_INVALID_BOARD,
  QK_ERR_INVALID_DATA_OR_ARG,
  QK_ERR_BOARD_NOT_CONNECTED,
  QK_ERR_INVALID_SAMP_FREQ,
  QK_ERR_COMM_TIMEOUT,
  QK_ERR_UNABLE_TO_SEND_MESSAGE,
  QK_ERR_SAMP_OVERLAP
} qk_error_t;
/******************************************************************************/
#define QK_PACKET_CODE_SIZE         1
#define QK_PACKET_ADDR64BIT_SIZE		8
#define QK_PACKET_ADDR16BIT_SIZE		2

#define QK_PACKET_FLAGMASK_EXTEND       0x0080
#define QK_PACKET_FLAGMASK_SOURCE       0x0070
#define QK_PACKET_FLAGMASK_FRAG         0x0008
#define QK_PACKET_FLAGMASK_LASTFRAG     0x0004
#define QK_PACKET_FLAGMASK_16BITADDR    0x0002
#define QK_PACKET_FLAGMASK_ADDRESS      0x0001
#define QK_PACKET_FLAGMASK_BOARD        0x0700

/******************************************************************************/

#define QK_PACKET_CODE_OFFSET(packet)   (packet->headerLength - QK_PACKET_CODE_SIZE)
#define QK_PACKET_ADDR16_OFFSET(packet) (QK_PACKET_CODE_OFFSET(packet) - QK_PACKET_ADDR16BIT_SIZE)
#define QK_PACKET_ADDR64_OFFSET(packet) (QK_PACKET_CODE_OFFSET(packet) - QK_PACKET_ADDR64BIT_SIZE)

#endif // QK_COMM_H
