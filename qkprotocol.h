/*
 * QkThings LICENSE
 * The open source framework and modular platform for smart devices.
 * Copyright (C) 2014 <http://qkthings.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
#include <stdint.h>

#include <QObject>
#include <QQueue>
#include <QReadWriteLock>
#include <QMutex>
#include <QWaitCondition>

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
#define QK_PACKET_CODE_READY            0x0D

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

#include "qkdevice.h"

class QkCore;
class QkBoard;
class QkProtocol;

class QKLIBSHARED_EXPORT QkFrame
{
public:
    QByteArray data;
    quint64 timestamp;
};

Q_DECLARE_METATYPE(QkFrame)
typedef QQueue<QkFrame> QkFrameQueue;

class QKLIBSHARED_EXPORT QkPacket
{
public:
    class Descriptor
    {
    public:
        Descriptor()
        {
            board = 0;
        }
        uint64_t address;
        uint8_t  code;
        uint8_t  boardType;
        QkBoard *board;

        QString setname_str;
        int getnode_address;
        int setconfig_idx;
        int action_id;
    };
    class Transmission
    {
    public:
        Transmission()
        {
            waitACK = true;
            timeout = 500;
            retries = 0;
        }
        bool waitACK;
        int timeout;
        int retries;
    };

    class QKLIBSHARED_EXPORT Builder {
    public:
        static bool build(QkPacket *packet, const Descriptor &desc);
        static bool validate(Descriptor *pd);
        static void parse(const QkFrame &frame, QkPacket *packet);
    };

    QkPacket()
    {
        address = 0;
        flags.ctrl = 0;
        flags.network = 0;
        code = 0;
    }

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
    Transmission tx;

    QString codeFriendlyName();
    int source();
    void calculateHeaderLenght();
    void process(QkCore *qk);

    static int requestId();

private:
    static int m_nextId;
};

Q_DECLARE_METATYPE(QkPacket)
typedef QQueue<QkPacket> QkPacketQueue;

class QKLIBSHARED_EXPORT QkAck
{
public:
    enum Result {
        ACK_NACK = 0,
        ACK_OK = 1,
        ACK_ERROR = 255
    };
    QkAck()
    {
        result = ACK_NACK;
    }

    static QkAck fromInt(int ack);
    int id;
    int result;
    int arg;
    int err;
    int code;
    int toInt();
    bool operator ==(const QkAck &other)
    {
        return ((id == other.id && result == other.result) ? true : false);
    }
};

class QkProtocolWorker : public QObject
{
    Q_OBJECT
public:
    QkProtocolWorker(QObject *parent = 0);

signals:
    void finished();
    void frameReady(QkFrame);
    void packetReady(QkPacket);

public slots:
    void run();
    void quit();
    void sendPacket(QkPacket packet);
    void parseFrame(QkFrame frame);

private:
    QkAck waitForACK(int packetId, int timeout = 500);
    void processPacket(QkPacket packet);
    QkPacketQueue m_outputPacketsQueue;
    QList<QkAck> m_acks;
    bool m_quit;

    QMutex m_mutex;
    QWaitCondition m_condition;
};

class QkProtocol : public QObject
{
    Q_OBJECT
public:
    QkProtocol(QkCore *qk);
    ~QkProtocol();
//    QkAck ack() { return m_ack; }
    //QkFrameQueue *outputFramesQueue() { return &m_outputFramesQueue; }
    //QReadWriteLock* outputFramesLock() { return &m_outputFramesLock; }

    QkCore *qk() { return m_qk; }

    QkAck sendPacket(QkPacket::Descriptor descriptor,
                     bool wait = true,
                     int timeout = 2000,
                     int retries = 0);

    QkProtocolWorker *worker() { return m_protocolWorker; }

signals:
    //void outputFrameReady(QkFrameQueue*);
    //void infoChanged(int address, QkBoard::Type boardType, int mask); // ??
    void commFound(int address);
    void commUpdated(int address);
    void deviceFound(int address);
    void deviceUpdated(int address);
    void dataReceived(int address, QkDevice::DataArray data);
    void eventReceived(int address, QkDevice::Event event);
    void debugReceived(int address, QString str);
    void packetReady(QkPacket);
    void packetProcessed();
    void ack(QkAck ack);
    void error(int errCode, int errArg);

public slots:
    //void processFrame(const QkFrame &frame);
    void processPacket(QkPacket packet);



private:
    void setupSignals();
    QkAck waitForACK(int packetId, int timeout = 500);
    //QkAck waitForACK(int timeout = 2000);

    QkCore *m_qk;
    QList<QkAck> m_acks;

    QThread *m_workerThread;
    QkProtocolWorker *m_protocolWorker;
//    QkFrameQueue m_outputFramesQueue;
//    QReadWriteLock m_outputFramesLock;
};



#endif // QK_PROTOCOL_H
