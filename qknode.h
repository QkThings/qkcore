#ifndef QKNODE_H
#define QKNODE_H

#include "qkcore_global.h"
#include <QObject>

class QkCore;
class QkComm;
class QkDevice;

class QKLIBSHARED_EXPORT QkNode
{
public:
    QkNode(QkCore *qk, int address);

    QkComm *comm();
    QkDevice *device();
    void setComm(QkComm *comm);
    void setDevice(QkDevice *device);

    int address();
private:
    QkCore *m_qk;
    QkComm *m_comm;
    QkDevice *m_device;
    int m_address;
};

#endif // QKNODE_H
