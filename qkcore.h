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
class QkConnection;

typedef QMap<int, QkNode*> QkNodeMap;

class QKLIBSHARED_EXPORT QkCore : public QObject
{
    Q_OBJECT
    friend class QkProtocol;
public:
    enum Status {
        sSearching,
        sStarted,
        sStopped
    };

    QkCore(QkConnection *conn, QObject *parent = 0);
    ~QkCore();

    static QString errorMessage(int errCode);

    QkNode* node(int address = 0);
    QkNodeMap nodes();
    QkConnection *connection() { return m_conn; }
    QkProtocol* protocol() { return m_protocol; }
    bool isRunning();

signals:
    void status(QkCore::Status);

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
    QkConnection *m_conn;


};

#endif // QK_H
