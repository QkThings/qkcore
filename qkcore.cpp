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
#include "qkcore_constants.h"
#include "qkutils.h"

#include "qknode.h"
#include "qkprotocol.h"

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
    reset();
}

QkCore::~QkCore()
{
    delete m_protocol;
}

void QkCore::reset()
{
    QList<QkNode*> nodes = m_nodes.values();
    qDeleteAll(nodes.begin(), nodes.end());
    m_nodes.clear();
    m_running = false;
    m_ready = false;
}


bool QkCore::isReady()
{
    return m_ready;
}

bool QkCore::isRunning()
{
    return m_running;
}


bool QkCore::waitForReady(int timeout)
{
    qDebug() << __FUNCTION__;

    emit status(sWaitForReady);

    QEventLoop eventLoop;
    QTimer timer;
    QElapsedTimer elapsedTimer;

    connect(&timer, SIGNAL(timeout()), &eventLoop, SLOT(quit()));
    connect(m_protocol, SIGNAL(packetProcessed()), &eventLoop, SLOT(quit()));

    timer.start(timeout);
    elapsedTimer.restart();

    while(!m_ready && !elapsedTimer.hasExpired(timeout))
        eventLoop.exec();

    if(elapsedTimer.hasExpired(timeout))
        qDebug() << "failed to receive READY";
    else
        emit status(sReady);

    return m_ready;
}

QkNode* QkCore::node(int address)
{
    return m_nodes.value(address);
}

QMap<int, QkNode*> QkCore::nodes()
{
    return m_nodes;
}

int QkCore::hello()
{
    qDebug() << __FUNCTION__;
    QkPacket::Descriptor pd;
    pd.address = 0;
    pd.code = QK_PACKET_CODE_HELLO;

    const int timeout = 100;
    const int retries = 20;
    return m_protocol->sendPacket(pd, true, timeout, retries).toInt();
}

int QkCore::search()
{
    qDebug() << __FUNCTION__;
    emit status(sSearching);
    //TODO clear all nodes
    QkPacket::Descriptor pd;
    pd.address = 0;
    pd.code = QK_PACKET_CODE_SEARCH;
    return m_protocol->sendPacket(pd).toInt();
}

int QkCore::getNode(int address)
{
    QkPacket::Descriptor pd;
    pd.address = address;
    pd.code = QK_PACKET_CODE_GETNODE;
    pd.getnode_address = address;
    return m_protocol->sendPacket(pd).toInt();
}

int QkCore::start(int address)
{
    QkPacket::Descriptor pd;
    pd.address = address;
    pd.code = QK_PACKET_CODE_START;
    QkAck ack = m_protocol->sendPacket(pd);
    if(ack.result == QkAck::OK) {
        m_running = true;
        emit status(sStarted);
    }
    return ack.toInt();
}


int QkCore::stop(int address)
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
    return ack.toInt();
}

void QkCore::slotStatus(QkCore::Status status)
{

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

