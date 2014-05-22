#ifndef QKPROTOCOL_H
#define QKPROTOCOL_H

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



#include "qkcore_lib.h"
#include "qkprotocol.h"

#include <stdint.h>

#include <QObject>


#define QK_COMM_WAKEUP      0x00
#define QK_COMM_FLAG        0x55	// Flag
#define QK_COMM_DLE			0xDD	// Data Link Escape
#define QK_COMM_NACK		0x00
#define QK_COMM_ACK         0x02
#define QK_COMM_OK          0x03
#define QK_COMM_ERR         0xFF
#define QK_COMM_TIMEOUT		0xFE

#define QK_PACKET_CODE_WAKEUP           0xF5
#define QK_PACKET_CODE_ACK              0x03

#define QK_PACKET_CODE_OK               0x01
#define QK_PACKET_CODE_ERR              0xFF
#define QK_PACKET_CODE_TIMEOUT          0xFE
#define QK_PACKET_CODE_SAVE             0x04
#define QK_PACKET_CODE_RESTORE          0x05
#define QK_PACKET_CODE_SEARCH           0x06
#define QK_PACKET_CODE_START            0x0A
#define QK_PACKET_CODE_STOP             0x0F
#define QK_PACKET_CODE_HELLO            0x0E

#define QK_PACKET_CODE_GETNODE          0x10
#define QK_PACKET_CODE_GETMODULE        0x11
#define QK_PACKET_CODE_GETDEVICE        0x12
#define QK_PACKET_CODE_GETQK            0x15
#define QK_PACKET_CODE_GETSAMP          0x16
#define QK_PACKET_CODE_GETSTATUS        0x17
#define QK_PACKET_CODE_GETDATA          0x18
#define QK_PACKET_CODE_GETCALENDAR      0x19
#define QK_PACKET_CODE_GETINFOACTION    0x2A
#define QK_PACKET_CODE_GETINFODATA      0x2D
#define QK_PACKET_CODE_GETINFOCONFIG    0x2C
#define QK_PACKET_CODE_GETINFOEVENT     0x2E
#define QK_PACKET_CODE_DEVICEFOUND      0x1A
#define QK_PACKET_CODE_COMMFOUND        0x1B
#define QK_PACKET_CODE_SETQK            0x33
#define QK_PACKET_CODE_SETNAME          0x34
#define QK_PACKET_CODE_SETSAMP          0x36
#define QK_PACKET_CODE_SETCALENDAR      0x39
#define QK_PACKET_CODE_SETCONFIG        0x3C
#define QK_PACKET_CODE_ACTUATE          0x3A
#define QK_PACKET_CODE_SETBAUD          0x40
#define QK_PACKET_CODE_SETFREQ          0x41
#define QK_PACKET_CODE_INFOQK           0xB1
#define QK_PACKET_CODE_INFOSAMP         0xB2
#define QK_PACKET_CODE_INFOBOARD        0xB5
#define QK_PACKET_CODE_INFOCOMM         0xB6
#define QK_PACKET_CODE_INFODEVICE       0xB7
#define QK_PACKET_CODE_INFOACTION       0xBA
#define QK_PACKET_CODE_INFODATA         0xBD
#define QK_PACKET_CODE_INFOEVENT        0xBE
#define QK_PACKET_CODE_INFOCONFIG       0xBC
#define QK_PACKET_CODE_CALENDAR         0xD1
#define QK_PACKET_CODE_STATUS           0xD5
#define QK_PACKET_CODE_DATA             0xD0
#define QK_PACKET_CODE_EVENT            0xDE
#define QK_PACKET_CODE_STRING           0xDF

#define QK_PACKET_FLAGMASK_CTRL_SRC        0x0070
#define QK_PACKET_FLAGMASK_CTRL_NOTIF      0x0008
#define QK_PACKET_FLAGMASK_CTRL_FRAG       0x0004
#define QK_PACKET_FLAGMASK_CTRL_LASTFRAG   0x0002
#define QK_PACKET_FLAGMASK_CTRL_ADDRESS    0x0001
#define QK_PACKET_FLAGMASK_CTRL_DEST       0x0700

#define SIZE_FLAGS_CTRL     2
#define SIZE_FLAGS_NETWORK  1
#define SIZE_ID             1
#define SIZE_CODE           1
#define SIZE_ADDR16         2
#define SIZE_ADDR64         8

class QkCore;
class QkBoard;

class QKLIBSHARED_EXPORT QkFrame
{
public:
    QByteArray data;
    quint64 timestamp;
};

class QKLIBSHARED_EXPORT QkPacket
{
public:
    class Descriptor
    {
    public:
        uint64_t address;
        uint8_t  code;
        uint8_t  boardType;

        QString setname_str;
        int getnode_address;
        int setconfig_idx;
        int action_id;
    };
    class QKLIBSHARED_EXPORT Builder {
    public:
        static bool build(QkPacket *packet, const Descriptor &desc, QkBoard *board = 0);
        static bool validate(Descriptor *pd);
        static void parse(const QkFrame &frame, QkPacket *packet);
    };

    int address;
    struct {
     int ctrl;
     int network;
    } flags;
    int code;
    QByteArray data;
    int checksum;
    int headerLength;
    int id;
    quint64 timestamp;

    QString codeFriendlyName();
    int source();
    void calculateHeaderLenght();
    void process(QkCore *qk);

    static int requestId();

private:
    static int m_nextId;
};

class QKLIBSHARED_EXPORT QkAck
{
public:
    enum Type {
        NACK,
        OK,
        ERROR
    };
    static QkAck fromInt(int ack);
    int arg;
    int err;
    int id;
    int code;
    int type;
    int toInt();
    bool operator ==(const QkAck &other)
    {
        return ((id == other.id && type == other.type) ? true : false);
    }
};

class QkProtocol : public QObject
{
    Q_OBJECT
public:
    QkProtocol(QkCore *qk, QObject *parent = 0);
    QkAck ack() { return m_ack; }

signals:
    void packetProcessed();
    void error(int errCode, int errArg);

public slots:
    QkAck sendPacket(QkPacket *packet, bool wait = false, int retries = 0);
    void processFrame(const QkFrame &frame);

private:
    void processPacket();
    QkAck waitForACK(int packetId, int timeout = 2000);
    //QkAck waitForACK(int timeout = 2000);

    QkCore *m_qk;
    QkPacket m_packet;
    QkAck m_ack;

    QList<QkAck> m_receivedAcks;
    QkAck m_frameAck;
};



#endif // QK_PROTOCOL_H
