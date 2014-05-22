#ifndef QKPROTOCOL_H
#define QKPROTOCOL_H

#include "qkpacket.h"


/******************************************************************************/
typedef enum qk_error
{
    QK_ERR_COMM_TIMEOUT = 0,
    QK_ERR_CODE_UNKNOWN = 255,
    QK_ERR_UNABLE_TO_SEND_MESSAGE,
    QK_ERR_UNSUPPORTED_OPERATION,
    QK_ERR_INVALID_BOARD,
    QK_ERR_INVALID_DATA_OR_ARG,
    QK_ERR_BOARD_NOT_CONNECTED,
    QK_ERR_INVALID_SAMP_FREQ,
    QK_ERR_SAMP_OVERLAP
} qk_error_t;






#endif // QK_PROTOCOL_H
