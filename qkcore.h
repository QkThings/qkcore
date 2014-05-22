#ifndef QK_H
#define QK_H

#include "qkcore_lib.h"
#include "qkcore_constants.h"
#include "qkprotocol.h"
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


    QkNode* node(int address = 0);
    QMap<int, QkNode*> nodes();

    bool isRunning();

    //QkAck waitForACK();

    QkProtocol* protocol() { return m_protocol; }

    //QQueue<QByteArray>* framesToSend();
    void _updateNode(int address = 0);
    void _saveNode(int address = 0);

signals:
    void commFound(int address);
    void commUpdated(int address);
    void deviceFound(int address);
    void deviceUpdated(int address);

    void infoChanged(int address, QkBoard::Type boardType, int mask);

    void dataReceived(int address);
    void eventReceived(int address);
    void debugReceived(int address, QString str);
    void ack(QkAck ack);

public slots:
    QkAck hello();
    QkAck search();
    QkAck getNode(int address = 0);
    QkAck start(int address = 0);
    QkAck stop(int address = 0);


private:
    bool m_running;
    QMap<int, QkNode*> m_nodes;
    QkProtocol *m_protocol;
};

#endif // QK_H
