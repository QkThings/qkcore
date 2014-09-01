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

#include "qkcore.h"
#include "qkconnserial.h"

#include <QDebug>
#include <QtSerialPort/QSerialPortInfo>
#include <QStringList>
#include <QIODevice>
#include <QQueue>
#include <QEventLoop>
#include <QTimer>


int QkConnection::nextId = 0;

QkConnWorker::QkConnWorker(QkConnection *conn)
{
    m_conn = conn;
    m_quit = false;
    m_connected = false;
}

void QkConnWorker::quit()
{
    m_quit = true;
}

void QkConnWorker::sendFrame(const QkFrame &frame)
{
    QMutexLocker locker(&m_mutex);
//    qDebug() << "sendFrame enqueue";
    m_outputFramesQueue.enqueue(frame);
}


QkConnection::QkConnection(QObject *parent) :
    QObject(parent)
{
    m_qk = new QkCore(this);
    m_id = QkConnection::nextId++;
    m_searchOnConnect = false;

    connect(this, SIGNAL(connected(int)), this, SLOT(slotConnected()));
    connect(this, SIGNAL(disconnected(int)), this, SLOT(slotDisconnected()));
}

QkConnection::~QkConnection()
{
    close();
    delete m_qk;
}

bool QkConnection::isConnected()
{
    if(m_worker != 0)
        return m_worker->isConnected();
    return false;
}

void QkConnection::open()
{
    emit status(m_id, sConnecting);
    m_qk->reset();
    if(m_workerThread != 0)
        m_workerThread->start();
}

void QkConnection::close()
{
    if(m_workerThread != 0 && m_worker != 0)
    {
        m_worker->quit();
        m_workerThread->quit();
        m_workerThread->wait();
        qDebug() << "closed!";
    }
    else
        qDebug() << "connWorker not found";
}

void QkConnection::slotConnected()
{
    emit status(m_id, sConnected);
    if(m_searchOnConnect)
    {
        if(qk()->waitForReady())
            qk()->search();
    }
}

void QkConnection::slotDisconnected()
{
    emit status(m_id, sDisconnected);
}

bool QkConnection::Descriptor::operator==(Descriptor &other)
{
    return (type == other.type &&
            parameters == other.parameters ? true : false);
}


bool QkConnection::operator==(QkConnection &other)
{
    return (descriptor().type == other.descriptor().type &&
            descriptor().parameters == other.descriptor().parameters ? true : false);
}


QkConnectionManager::QkConnectionManager(QObject *parent) :
    QObject(parent)
{
    m_searchOnConnect = false;
}

QkConnectionManager::~QkConnectionManager()
{
    qDeleteAll(m_connections.begin(), m_connections.end());
}

QList<QkConnection*> QkConnectionManager::connections()
{
    return m_connections;
}

QkConnection* QkConnectionManager::defaultConnection()
{
    return (m_connections.count() > 0 ? m_connections[0] : 0);
}

//void QkConnectionManager::slotConnected(int id)
//{
//    QkConnection *conn = connection(id);

//    if(m_searchOnConnect)
//    {
//        QkAck ack = conn->qk()->hello();
//        if(ack.result != QkAck::NACK)
//            conn->qk()->search();
//    }
//}

QkConnection* QkConnectionManager::addConnection(const QkConnection::Descriptor &desc)
{
    QkConnection *conn;

    if(connection(desc) != 0)
    {
        emit error(tr("Connection already exists."));
        return 0;
    }

    switch(desc.type)
    {
    case QkConnection::tSerial:
        conn = new QkConnSerial(desc.parameters["portName"].toString(),
                                desc.parameters["baudRate"].toInt(),
                                this);
        break;
    default:
        qDebug() << "Connection type unknown" << desc.type;
        return 0;
    }

    conn->setSearchOnConnect(m_searchOnConnect);


    connect(conn, SIGNAL(error(QString)), this, SIGNAL(error(QString)));
//    connect(conn, SIGNAL(connected(int)), this, SLOT(slotConnected(int)));

    m_connections.append(conn);
    emit connectionAdded(conn);

    conn->open();
}

void QkConnectionManager::removeConnection(const QkConnection::Descriptor &desc)
{
    QkConnection *conn = connection(desc);
    if(conn != 0)
    {
        conn->close();
        emit connectionRemoved(conn);
        m_connections.removeOne(conn);
    }
}

QkConnection* QkConnectionManager::connection(const QkConnection::Descriptor &desc)
{
    foreach(QkConnection *conn, m_connections)
        if(conn->sameAs(desc))
            return conn;
    return 0;
}

QkConnection* QkConnectionManager::connection(int id)
{
    foreach(QkConnection *conn, m_connections)
        if(conn->id() == id)
            return conn;
    return 0;
}

