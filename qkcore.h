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

typedef QMap<int, QkNode*> QkNodeMap;

class QKLIBSHARED_EXPORT QkCore : public QObject
{
    Q_OBJECT
    friend class QkProtocol;
public:

    QkCore(QObject *parent = 0);

    static QString errorMessage(int errCode);

    QkNode* node(int address = 0);
    QkNodeMap nodes();
    QkProtocol* protocol() { return m_protocol; }
    bool isRunning();


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
