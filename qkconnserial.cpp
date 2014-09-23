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

#include "qkconnserial.h"
#include "qkcore.h"
#include "qkprotocol.h"
#include "qkconnect.h"

#include <QDebug>
#include <QTimer>
#include <QDateTime>
#include <QEventLoop>


QkConnSerialWorker::QkConnSerialWorker(QkConnSerial *conn) :
    QkConnWorker(conn)
{

}

void QkConnSerialWorker::run()
{
    QEventLoop eventLoop;

    m_sp = new QSerialPort(this);
    m_protocol = new Protocol();

    connect(m_sp, SIGNAL(readyRead()), this, SLOT(slotReadyRead()));

    QkConnection::Descriptor desc = connection()->descriptor();

    QString portName = desc.parameters.value("portName").toString();
    int baudRate = desc.parameters.value("baudRate").toInt();

    m_sp->setPortName(portName);
    m_sp->setBaudRate(baudRate);
    if(m_sp->open(QIODevice::ReadWrite))
    {
        m_sp->setBaudRate(baudRate);
        m_sp->setParity(QSerialPort::NoParity);
        m_sp->setFlowControl(QSerialPort::NoFlowControl);
        m_sp->setDataBits(QSerialPort::Data8);

        if(portName.contains("USB")) //FIXME remove this mega-hack
        {
        m_sp->setDataTerminalReady(false);
        QEventLoop eventLoop;
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, SIGNAL(timeout()), &eventLoop, SLOT(quit()));

        m_sp->setRequestToSend(true);
        timer.start(100);
        eventLoop.exec();
        m_sp->setRequestToSend(false);
        timer.start(100);
        eventLoop.exec();
        }

//        m_sp->setRequestToSend(true);
//        m_sp->setDataTerminalReady(false);

//        m_sp->setRequestToSend(false);
//        m_sp->setDataTerminalReady(false);


        m_sp->clear();
        qDebug() << "connection opened:" << m_sp->portName() << m_sp->baudRate();
    }
    else
    {
        emit error(tr("Failed to open serial port ") + m_sp->portName() + m_sp->errorString());
        return;
    }

    m_connected = true;
    emit connected(connection()->id());

    QString dbgStr;

    while(!m_quit)
    {
        eventLoop.processEvents();

        m_mutex.lock();
        if(m_outputFramesQueue.count() > 0)
        {
            const QByteArray &frame = m_outputFramesQueue.dequeue().data;
            m_mutex.unlock();

            dbgStr = "";

            int i;
            quint8 chBuf;

            const char flagByte = QK_COMM_FLAG;
            const char dleByte = QK_COMM_DLE;

            m_sp->write(&flagByte, 1); dbgStr += QString().sprintf("%02X ", flagByte);
            for(i = 0; i < frame.count(); i++)
            {
                chBuf = frame[i] & 0xFF;
                if(chBuf == QK_COMM_FLAG || chBuf == QK_COMM_DLE)
                {
                    m_sp->write(&dleByte, 1); dbgStr += QString().sprintf("%02X ", dleByte);
                }
                m_sp->write((char*) &chBuf, 1); dbgStr += QString().sprintf("%02X ", chBuf);
            }
            m_sp->write(&flagByte, 1); dbgStr += QString().sprintf("%02X ", flagByte);

            qDebug() << "tx: " << dbgStr;
        }
        else
            m_mutex.unlock();
    }

    m_sp->close();
    m_connected = false;

    emit disconnected(connection()->id());
    emit finished();

    eventLoop.processEvents();
}

void QkConnSerialWorker::slotReadyRead()
{
    QByteArray data;// = m_sp->readAll();
    char *bufPtr;// = data.data();
    int countBytes;// = data.count();
    QString dbgStr;

//    while(countBytes--)
    while(m_sp->bytesAvailable() > 0)
    {
        data = m_sp->readAll();
        bufPtr = data.data();
        countBytes = data.count();

        while(countBytes--)
        {

            //        qDebug() << "rx:" << QString().sprintf("%02X (%c)", *bufPtr & 0xFF, *bufPtr & 0xFF);
            dbgStr.append( QString().sprintf("%02X ", *bufPtr & 0xFF) );
//            dbgStr.append( QString().sprintf("%c", *bufPtr & 0xFF) );
            parseSerialData((quint8)*bufPtr++);
            if(FLAG(m_protocol->ctrlFlags, Protocol::cfFrameReady))
            {
                QkFrame frame;
                frame.data = m_protocol->frame;
                frame.timestamp = QDateTime::currentMSecsSinceEpoch();
                emit frameReady(frame);
                FLAG_CLR(m_protocol->ctrlFlags, Protocol::cfFrameReady);
            }
        }
    }
    qDebug() << "rx: " << dbgStr;
//    qDebug("%s", dbgStr.toStdString().c_str());
}

void QkConnSerialWorker::parseSerialData(quint8 data)
{
    switch(data)
    {
    case QK_COMM_FLAG:
        if(!FLAG(m_protocol->ctrlFlags, Protocol::cfDataLinkEscape))
        {
            if(!FLAG(m_protocol->ctrlFlags, Protocol::cfReceiving))
            {
                m_protocol->frame.clear();
                FLAG_SET(m_protocol->ctrlFlags, Protocol::cfReceiving);
                FLAG_SET(m_protocol->ctrlFlags, Protocol::cfValid);
            }
            else
            {
                if(FLAG(m_protocol->ctrlFlags, Protocol::cfValid) &&
                   m_protocol->frame.count() > 0)
                {
                    FLAG_SET(m_protocol->ctrlFlags, Protocol::cfFrameReady);
                    FLAG_CLR(m_protocol->ctrlFlags, Protocol::cfReceiving);
                    FLAG_CLR(m_protocol->ctrlFlags, Protocol::cfValid);
                }
            }
            return;
        }
        break;
    case QK_COMM_DLE:
        if(FLAG(m_protocol->ctrlFlags, Protocol::cfValid))
        {
            if(!FLAG(m_protocol->ctrlFlags, Protocol::cfDataLinkEscape))
            {
                FLAG_SET(m_protocol->ctrlFlags, Protocol::cfDataLinkEscape);
                return;
            }
        }
        break;
    default: ;
    }

    if(FLAG(m_protocol->ctrlFlags, Protocol::cfValid))
        m_protocol->frame.append(data);

    FLAG_CLR(m_protocol->ctrlFlags, Protocol::cfDataLinkEscape);
}

QkConnSerial::QkConnSerial(QObject *parent)
{
    QkConnSerial("portName", 38400, parent);
}

QkConnSerial::QkConnSerial(const QString &portName, int baudRate, QObject *parent) :
    QkConnection(parent)
{
    m_descriptor.type = QkConnection::tSerial;
    m_descriptor.parameters["portName"] = portName;
    m_descriptor.parameters["baudRate"] = baudRate;

    m_workerThread = new QThread(this);
    m_worker = new QkConnSerialWorker(this);

    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, SIGNAL(started()), m_worker, SLOT(run()), Qt::DirectConnection);
    connect(m_worker, SIGNAL(finished()), m_workerThread, SLOT(quit()), Qt::DirectConnection);

    connect(m_worker, SIGNAL(connected(int)), this, SIGNAL(connected(int)), Qt::DirectConnection);
    connect(m_worker, SIGNAL(disconnected(int)), this, SIGNAL(disconnected(int)), Qt::DirectConnection);

    QkProtocol *protocol = m_qk->protocol();
    QkProtocolWorker *protocolWorker = protocol->worker();

    connect(protocolWorker, SIGNAL(frameReady(QkFrame)), m_worker, SLOT(sendFrame(QkFrame)), Qt::DirectConnection);
    connect(m_worker, SIGNAL(frameReady(QkFrame)), protocolWorker, SLOT(parseFrame(QkFrame)), Qt::DirectConnection);
}


void QkConnSerial::setPortName(const QString &portName)
{
    m_descriptor.parameters["portName"] = portName;
}

void QkConnSerial::setBaudRate(int baudRate)
{
    m_descriptor.parameters["baudRate"] = baudRate;
}

bool QkConnSerial::sameAs(const Descriptor &desc)
{
    if( desc.type == QkConnection::tSerial &&
       (desc.parameters.keys().contains("portName") &&
        desc.parameters["portName"] == m_descriptor.parameters["portName"]))
        return true;

    return false;
}
