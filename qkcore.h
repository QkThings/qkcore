#ifndef QK_H
#define QK_H

#include "qkcore_global.h"
#include "qklib_constants.h"
#include "qkprotocol.h"
#include "qkpacket.h"
#include "qkboard.h"
#include "qkdevice.h"
#include "qkcomm.h"
#include <stdint.h>

#include <QDebug>
#include <QObject>
#include <QVector>
#include <QVariant>
#include <QMutex>
#include <QQueue>

class QkCore;
class QkNode;
class QkDevice;
class QkComm;
class QkPacket;

class QKLIBSHARED_EXPORT QkCore : public QObject
{
    Q_OBJECT
public:

    QkCore(QObject *parent = 0);

    static QString version();
    static QString errorMessage(int errCode);

    /**** API ***************************************/
    QkNode* node(int address = 0);
    QMap<int, QkNode*> nodes();
    /************************************************/

    void _updateNode(int address = 0);
    void _saveNode(int address = 0);

    bool isRunning();

    void setProtocolTimeout(int timeout);
    void setEventLogging(bool enabled);

    QkAck comm_sendPacket(QkPacket *packet, bool wait = false);

    QkAck waitForACK();

    QQueue<QByteArray>* framesToSend();

signals:
    void comm_frameReady();
    void packetProcessed();
    void commFound(int address);
    void commUpdated(int address);
    void deviceFound(int address);
    void deviceUpdated(int address);

    void infoChanged(int address, QkBoard::Type boardType, int mask);

    void dataReceived(int address);
    void eventReceived(int address);
    void debugReceived(int address, QString str);
    void error(int errCode, int errArg);
    void ack(QkAck ack);

public slots:
    /**** API ***************************************/
    QkAck hello();
    QkAck search();
    QkAck getNode(int address = 0);
    QkAck start(int address = 0);
    QkAck stop(int address = 0);
    /************************************************/
    void comm_processFrame(QkFrame frame);

private:
    void _comm_parseFrame(QkFrame frame, QkPacket *packet);
    void _comm_processPacket(QkPacket *p);
    QkAck _comm_wait(int packetID, int timeout);

    bool m_running;
    bool m_eventLogging;
    QMap<int, QkNode*> m_nodes;
    QkProtocol m_protocol;
    QMutex m_mutex;

    QQueue<QByteArray> m_framesToSend;
    QList<QkAck> m_receivedAcks;
    QkAck m_frameAck;
};

#endif // QK_H
