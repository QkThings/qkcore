#include "qkcore.h"
#include "qkcore_constants.h"
#include "qkutils.h"

#include "qknode.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QTimer>
#include <QEventLoop>
#include <QStringList>
#include <QDateTime>
#include <QTime>
#include <QDate>
#include <QMetaEnum>
#include <QQueue>

using namespace QkUtils;

QkCore::QkCore(QkConnection *conn, QObject *parent) :
    QObject(parent)
{
    qRegisterMetaType<QkFrame>("QkFrame");
    qRegisterMetaType<QkPacket>("QkPacket");

    m_conn = conn;
    m_protocol = new QkProtocol(this);
    m_nodes.clear();
    m_running = false;
}

QkNode* QkCore::node(int address)
{
    return m_nodes.value(address);
}

QMap<int, QkNode*> QkCore::nodes()
{
    return m_nodes;
}

QkAck QkCore::hello()
{
    qDebug() << __FUNCTION__;
    QkPacket::Descriptor pd;
    pd.address = 0;
    pd.code = QK_PACKET_CODE_HELLO;

    const int timeout = 150;
    const int retries = 20;
    return m_protocol->sendPacket(pd, true, timeout, retries);
}

QkAck QkCore::search()
{
    emit status(sSearching);
    qDebug() << __FUNCTION__;
    //TODO clear all nodes
    QkPacket::Descriptor pd;
    pd.address = 0;
    pd.code = QK_PACKET_CODE_SEARCH;
    return m_protocol->sendPacket(pd);
}

QkAck QkCore::getNode(int address)
{
    QkPacket::Descriptor pd;
    pd.address = address;
    pd.code = QK_PACKET_CODE_GETNODE;
    pd.getnode_address = address;
    return m_protocol->sendPacket(pd);
}

QkAck QkCore::start(int address)
{
    QkPacket::Descriptor pd;
    pd.address = address;
    pd.code = QK_PACKET_CODE_START;
    QkAck ack = m_protocol->sendPacket(pd);
    if(ack.result == QkAck::OK) {
        m_running = true;
        emit status(sStarted);
    }
    return ack;
}


QkAck QkCore::stop(int address)
{
    QkPacket::Descriptor pd;
    pd.address = address;
    pd.code = QK_PACKET_CODE_STOP;
    QkAck ack = m_protocol->sendPacket(pd);
    if(ack.result == QkAck::OK)
    {
        m_running = false;
        emit status(sStopped);
    }
    return ack;
}

bool QkCore::isRunning()
{
    return m_running;
}

QString QkCore::errorMessage(int errCode)
{
    errCode &= 0xFF;
    switch(errCode) {
    case QK_ERR_CODE_UNKNOWN: return tr("Unkown code"); break;
    case QK_ERR_INVALID_BOARD: return tr("Invalid board"); break;
    case QK_ERR_INVALID_DATA_OR_ARG: return tr("Invalid data or arguments"); break;
    case QK_ERR_BOARD_NOT_CONNECTED: return tr("Board is not connected"); break;
    case QK_ERR_INVALID_SAMP_FREQ: return tr("Invalid sampling frequency"); break;
    case QK_ERR_COMM_TIMEOUT: return tr("Communication timeout"); break;
    case QK_ERR_UNSUPPORTED_OPERATION: return tr("Unsupported operation"); break;
    case QK_ERR_UNABLE_TO_SEND_MESSAGE: return tr("Unable to send message"); break;
    case QK_ERR_SAMP_OVERLAP: return tr("Sampling overlap"); break;
    default:
        return tr("Unknown error code") + QString().sprintf(" (%d)",errCode); break;
    }
}

