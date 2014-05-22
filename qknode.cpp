#include "qknode.h"


QkNode::QkNode(QkCore *qk, int address)
{
    m_qk = qk;
    m_address = address;
    m_comm = 0;
    m_device = 0;
}

void QkNode::setComm(QkComm *comm)
{
    m_comm = comm;
}

void QkNode::setDevice(QkDevice *device)
{
    m_device = device;
}


QkComm* QkNode::comm()
{
    return m_comm;
}

QkDevice* QkNode::device()
{
    return m_device;
}

int QkNode::address()
{
    return m_address;
}

