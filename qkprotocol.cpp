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

#include "qkprotocol.h"
#include "qkcore.h"
#include "qkboard.h"
#include "qkdevice.h"
#include "qkcomm.h"
#include "qknode.h"

#include "qkutils.h"
#include "qkcore_constants.h"

#include <QDebug>
#include <QDateTime>
#include <QThread>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QTimer>
#include <QStringList>
#include <QMutex>
#include <QWaitCondition>

using namespace QkUtils;

int QkPacket::m_nextId = 0;

int QkAck::toInt()
{
    return (int)result + (arg << 8) + ( err << 16);
}

QkAck QkAck::fromInt(int ack)
{
    QkAck res;
    res.result = (Result)(ack & 0xFF);
    res.arg = (ack >> 8) & 0xFF;
    res.err = (ack >> 16) & 0xFF;
    return res;
}

QkProtocolWorker::QkProtocolWorker(QObject *parent) :
    QObject(parent)
{
    m_quit = false;
}

void QkProtocolWorker::quit()
{
    m_quit = true;
}

void QkProtocolWorker::run()
{
    QkFrame frame;
    QEventLoop eventLoop;

    while(!m_quit)
    {
        eventLoop.processEvents();

        m_mutex.lock();
        if(m_outputPacketsQueue.count() > 0)
        {
            QkPacket packet = m_outputPacketsQueue.dequeue();
            m_mutex.unlock();

//            qDebug() << "sendPacket dequeue";

            frame.data.clear();
            frame.data.append(packet.flags.ctrl & 0xFF);
            frame.data.append((packet.flags.ctrl >> 8) & 0xFF);
            frame.data.append(packet.id);
            frame.data.append(packet.code);
            frame.data.append(packet.data);

            QkAck ack;

//            do
//            {
                emit frameReady(frame);
                if(packet.tx.waitACK)
                    ack = waitForACK(packet.id, packet.tx.timeout);
//            }
//            while(packet.tx.waitACK && packet.tx.retries-- && ack.result == QkAck::NACK);
        }
        else
            m_mutex.unlock();
    }
    eventLoop.processEvents();

    emit finished();

    eventLoop.processEvents();
}

void QkProtocolWorker::sendPacket(QkPacket packet)
{
    QMutexLocker locker(&m_mutex);

//    qDebug() << "sendPacket enqueue";
    m_outputPacketsQueue.enqueue(packet);

    if(m_outputPacketsQueue.count() > 8)
        qDebug() << "OUTPUT PACKETS QUEUE IS GETTING FULL" << m_outputPacketsQueue.count();
}

void QkProtocolWorker::parseFrame(QkFrame frame)
{
    static QkPacket packet;
    static QkPacket fragment;

    QkPacket::Builder::parse(frame, &packet);

    if(packet.flags.ctrl & QK_PACKET_FLAGMASK_CTRL_FRAG)
    {
        fragment.data.append(packet.data);

        //FIXME create elapsedTimer to timeout lastFragment reception
        if(packet.flags.ctrl & QK_PACKET_FLAGMASK_CTRL_LASTFRAG)
        {
            packet.data = fragment.data;
            fragment.data.clear();
        }
        else
            return;
    }
    processPacket(packet);
    emit packetReady(packet);
}

void QkProtocolWorker::processPacket(QkPacket packet)
{
    QkPacket *p = &packet;
    QkAck ack;

    int i_data = 0;
    switch(p->code)
    {
    case QK_PACKET_CODE_ACK:
        ack.id = getValue(1, &i_data, p->data);
        ack.code = getValue(1, &i_data, p->data);
        ack.result = getValue(1, &i_data, p->data);
        if(ack.result == QkAck::ERROR)
        {
            ack.err = getValue(1, &i_data, p->data);
            ack.arg = getValue(1, &i_data, p->data);
        }
        m_acks.prepend(ack);
        break;
    default: ;
    }
}

QkAck QkProtocolWorker::waitForACK(int packetId, int timeout)
{
    QEventLoop loop;
    QTimer loopTimer;
    QElapsedTimer elapsedTimer;
    QkAck ack;

    connect(&loopTimer, SIGNAL(timeout()), &loop, SLOT(quit()));
    connect(this, SIGNAL(packetReady(QkPacket)), &loop, SLOT(quit()));
//    connect(this, SIGNAL(packetProcessed()), &loop, SLOT(quit()));

    elapsedTimer.start();

    while(ack.result == QkAck::NACK && !elapsedTimer.hasExpired(timeout))
    {
        loopTimer.start(20);
        loop.exec();

        foreach(const QkAck &receivedAck, m_acks)
        {
            if(receivedAck.id == packetId)
            {
                ack = receivedAck;
                break;
            }
        }
    }

    m_acks.removeOne(ack);

    loopTimer.stop();
    if(elapsedTimer.hasExpired(timeout))
    {
        qDebug() << "QkProtocolWorker timeout!";
    }

    return ack;
}

QkProtocol::QkProtocol(QkCore *qk) :
    QObject(qk)
{
    m_qk = qk;

    m_workerThread = new QThread(this);
    m_protocolWorker = new QkProtocolWorker();
    m_protocolWorker->moveToThread(m_workerThread);

    connect(m_workerThread, SIGNAL(started()), m_protocolWorker, SLOT(run()), Qt::DirectConnection);
    connect(m_protocolWorker, SIGNAL(finished()), m_workerThread, SLOT(quit()), Qt::DirectConnection);

    connect(this, SIGNAL(packetReady(QkPacket)),
            m_protocolWorker, SLOT(sendPacket(QkPacket)), Qt::DirectConnection);

    connect(m_protocolWorker, SIGNAL(packetReady(QkPacket)),
            this, SLOT(processPacket(QkPacket)), Qt::DirectConnection);

    m_workerThread->start();
}

QkProtocol::~QkProtocol()
{
    m_protocolWorker->quit();
    m_workerThread->wait();
    delete m_protocolWorker;
}

//void QkProtocol::processFrame(const QkFrame &frame)
//{
//    static QkPacket packet;
//    static QkPacket fragment;

//    QkPacket::Builder::parse(frame, &packet);

//    if(packet.flags.ctrl & QK_PACKET_FLAGMASK_CTRL_FRAG)
//    {
//        fragment.data.append(packet.data);

//        //FIXME create elapsedTimer to timeout lastFragment reception
//        if(packet.flags.ctrl & QK_PACKET_FLAGMASK_CTRL_LASTFRAG)
//        {
//            packet.data = fragment.data;
//            fragment.data.clear();
//        }
//        else
//            return;
//    }

//    processPacket(&packet);
//}



QkAck QkProtocol::sendPacket(QkPacket::Descriptor descriptor, bool wait, int timeout, int retries)
{
    QkAck ack;
    QkPacket packet;
    QkPacket::Builder::build(&packet, descriptor);

//    do
//    {
    qDebug() << "sendPacket" << packet.codeFriendlyName() << QString().sprintf("code:%02X id=%d", packet.code, packet.id);

    packet.tx.waitACK = wait;
    packet.tx.timeout = timeout;
    packet.tx.retries = retries;

    emit packetReady(packet);
    if(wait)
        ack = waitForACK(packet.id, 3000);
//    } while(wait && retries-- && ack.result == QkAck::NACK);

    return ack;
}

QkAck QkProtocol::waitForACK(int packetId, int timeout)
{
    QEventLoop loop;
    QTimer loopTimer;
    QElapsedTimer elapsedTimer;

    connect(&loopTimer, SIGNAL(timeout()), &loop, SLOT(quit()));
    connect(this, SIGNAL(packetProcessed()), &loop, SLOT(quit()));

    elapsedTimer.start();

    QkAck ack;

    while(ack.result == QkAck::NACK && !elapsedTimer.hasExpired(timeout))
    {
        loopTimer.start(50);
        loop.exec();
        foreach(const QkAck &receivedAck, m_acks)
        {
            if(receivedAck.id == packetId)
            {
                elapsedTimer.restart();
                ack = receivedAck;
                break;
            }
        }
    }
    loopTimer.stop();

    if(elapsedTimer.hasExpired(timeout))
    {
        qDebug() << "QkProtocol timeout!";
    }

    m_acks.removeOne(ack);

    return ack;
}

void QkProtocol::processPacket(QkPacket packet)
{
    QkBoard *selBoard = 0;
    QkNode *selNode = 0;
    QkComm *selComm = 0;
    QkDevice *selDevice = 0;
    bool boardCreated = false;

    QkPacket *p = &packet;
    QkCore *qk = m_qk;

    qDebug() << __FUNCTION__ <<  p->codeFriendlyName() << QString().sprintf("addr:%04X code:%02X",p->address,p->code);

    selNode = qk->node(p->address);
    if(selNode == 0)
    {
        selNode = new QkNode(qk, p->address);
        qk->m_nodes.insert(p->address, selNode);
    }

    switch(p->source())
    {
    case QkBoard::btComm:
        if(selNode->comm() == 0)
        {
            selNode->setComm(new QkComm(qk, selNode));
            boardCreated = true;
        }
        selComm = selNode->comm();
        selBoard = selNode->comm();
        break;
    case QkBoard::btDevice:
        if(selNode->device() == 0)
        {
            selDevice = new QkDevice(qk, selNode);
            selNode->setDevice(selDevice);
            boardCreated = true;
        }
        selDevice = selNode->device();
        selBoard = (QkBoard*)(selDevice);
        break;
    default:
        qWarning() << __FUNCTION__ << "unkown packet source";
    }

    if(selBoard == 0)
    {
        qWarning() << __FUNCTION__ << "selBoard == 0";
        return;
    }

    int i, j, size, fwVersion, ncfg, ndat, nact, nevt, eventID, nargs;
    int year, month, day, hours, minutes, seconds;
    double min = 0.0, max = 0.0;
    float dataValue;
    QString debugStr;
    QkInfo qkInfo;

    QkBoard::ConfigArray configs;
    QkBoard::Config::Type configType;

    QkDevice::SamplingInfo sampInfo;
    QkDevice::DataArray data;
    QkDevice::Data::Type dataType;
    QkDevice::EventArray events;
    QkDevice::Event eventRx;
    QkDevice::ActionArray actions;

    QString label, name;
    QVariant varValue;
    QStringList items;
    QDateTime dateTime;

    QList<float> eventArgs;
    float eventArg;
    bool bufferJustCreated = false;
    bool infoChangedEmit = false;
    int infoChangedMask = 0;
    QkAck ackRx;

    int i_data = 0;
    switch(p->code)
    {
    case QK_PACKET_CODE_ACK:
        ackRx.id = getValue(1, &i_data, p->data);
        ackRx.code = getValue(1, &i_data, p->data);
        ackRx.result = getValue(1, &i_data, p->data);
        if(ackRx.result == QkAck::ERROR)
        {
            ackRx.err = getValue(1, &i_data, p->data);
            ackRx.arg = getValue(1, &i_data, p->data);
        }
        m_acks.prepend(ackRx);
        qDebug() << " ACK received:" << QString().sprintf("id:%d code:%02X result:%d", ackRx.id, ackRx.code, ackRx.result);
        break;
    case QK_PACKET_CODE_READY:
        qk->m_ready = true;
        break;
    case QK_PACKET_CODE_INFOQK:
        qkInfo.version = Version(getValue(1, &i_data, p->data),
                                 getValue(1, &i_data, p->data),
                                 getValue(1, &i_data, p->data));
        qkInfo.baudRate = getValue(4, &i_data, p->data);
        qkInfo.flags = getValue(4, &i_data, p->data);
        selBoard->_setQkInfo(qkInfo);
        selBoard->_setInfoMask((int)QkBoard::biQk);
        break;
    case QK_PACKET_CODE_INFOBOARD:
        fwVersion = getValue(2, &i_data, p->data);
        name = getString(QK_BOARD_NAME_SIZE, &i_data, p->data);
        selBoard->_setFirmwareVersion(fwVersion);
        selBoard->_setName(name);
        selBoard->_setInfoMask((int)QkBoard::biBoard);
        break;
    case QK_PACKET_CODE_INFOCONFIG:
        ncfg = getValue(1, &i_data, p->data);
        configs = QVector<QkBoard::Config>(ncfg);
        for(i=0; i<ncfg; i++)
        {
            configType = (QkBoard::Config::Type) getValue(1, &i_data, p->data);
            label = getString(QK_LABEL_SIZE, &i_data, p->data);
            switch(configType)
            {
            case QkBoard::Config::ctBool:
                varValue = QVariant((bool) getValue(1, &i_data, p->data));
                break;
            case QkBoard::Config::ctIntDec:
                varValue = QVariant((int) getValue(4, &i_data, p->data, true));
                min = (double) getValue(4, &i_data, p->data, true);
                max = (double) getValue(4, &i_data, p->data, true);
                break;
            case QkBoard::Config::ctIntHex:
                varValue = QVariant((unsigned int) getValue(4, &i_data, p->data, true));
                min = (double) getValue(4, &i_data, p->data, true);
                max = (double) getValue(4, &i_data, p->data, true);
                break;
            case QkBoard::Config::ctFloat:
                varValue = QVariant(floatFromBytes(getValue(4, &i_data, p->data, true)));
                min = (double) getValue(4, &i_data, p->data, true);
                max = (double) getValue(4, &i_data, p->data, true);
                break;
            case QkBoard::Config::ctDateTime:
                year = 2000+getValue(1, &i_data, p->data);
                month = getValue(1, &i_data, p->data);
                day = getValue(1, &i_data, p->data);
                hours = getValue(1, &i_data, p->data);
                minutes = getValue(1, &i_data, p->data);
                seconds = getValue(1, &i_data, p->data);
                dateTime = QDateTime(QDate(year,month,day),QTime(hours,minutes,seconds));
                varValue = QVariant(dateTime);
                break;
            case QkBoard::Config::ctTime:
                hours = getValue(1, &i_data, p->data);
                minutes = getValue(1, &i_data, p->data);
                seconds = getValue(1, &i_data, p->data);
                dateTime = QDateTime(QDate::currentDate(),QTime(hours,minutes,seconds));
                varValue = QVariant(dateTime);
                break;
            case QkBoard::Config::ctCombo:
                size = getValue(1, &i_data, p->data);
                items.clear();
                for(i=0; i<size; i++)
                {
                    items.append(getString(&i_data, p->data));
                }
                varValue = QVariant(items);
                break;
            }
            configs[i]._set(label, configType, varValue, min, max);
        }
        selBoard->_setConfigs(configs);
        selBoard->_setInfoMask((int)QkBoard::biConfig);
        break;
    case QK_PACKET_CODE_INFOSAMP:
        sampInfo.frequency = getValue(4, &i_data, p->data);
        sampInfo.mode = (QkDevice::SamplingMode) getValue(1, &i_data, p->data);
        sampInfo.triggerClock = (QkDevice::TriggerClock) getValue(1, &i_data, p->data);
        sampInfo.triggerScaler = getValue(1, &i_data, p->data);
        sampInfo.N = getValue(4, &i_data, p->data);
        selDevice->_setSamplingInfo(sampInfo);
        selDevice->_setInfoMask((int)QkDevice::diSampling);
        break;
    case QK_PACKET_CODE_INFODATA:
        ndat = getValue(1, &i_data, p->data);
        data = QVector<QkDevice::Data>(ndat);
        dataType = (QkDevice::Data::Type)getValue(1, &i_data, p->data);
        for(i=0; i<ndat; i++)
        {
            data[i]._setLabel(getString(QK_LABEL_SIZE, &i_data, p->data));
        }
        selDevice->_setDataType(dataType);
        selDevice->_setData(data);
        selDevice->_setInfoMask((int)QkDevice::diData);
        break;
    case QK_PACKET_CODE_INFOEVENT:
        nevt = getValue(1, &i_data, p->data);
        events = QVector<QkDevice::Event>(nevt);
        for(i=0; i<nevt; i++)
        {
            events[i]._setLabel(getString(QK_LABEL_SIZE, &i_data, p->data));
        }
        selDevice->_setEvents(events);
        selDevice->_setInfoMask((int)QkDevice::diEvent);
        break;
    case QK_PACKET_CODE_INFOACTION:
        nact = getValue(1, &i_data, p->data);
        actions = QVector<QkDevice::Action>(nact);
        for(i = 0; i < nact; i++)
        {
            actions[i]._setType((QkDevice::Action::Type)getValue(1, &i_data, p->data));
            actions[i]._setLabel(getString(QK_LABEL_SIZE, &i_data, p->data));
            switch(actions[i].type())
            {
            case QkDevice::Action::atBool:
                actions[i]._setValue(QVariant((bool) getValue(1, &i_data, p->data)));
                break;
            case QkDevice::Action::atInt:
                actions[i]._setValue(QVariant((int) getValue(4, &i_data, p->data)));
                break;
            }
        }
        selDevice->_setActions(actions);
        selDevice->_setInfoMask((int)QkDevice::diAction);
        break;
    case QK_PACKET_CODE_DATA:
        ndat = getValue(1, &i_data, p->data);
        dataType = (QkDevice::Data::Type)getValue(1, &i_data, p->data);
        if(selDevice->data().size() != ndat)
        {
            qWarning() << __FUNCTION__ << "data count doesn't match buffer size";
            data.resize(ndat);
            selDevice->_setData(data);
            selDevice->_setDataType(dataType);
            bufferJustCreated = true;
            infoChangedEmit = true;
            infoChangedMask |= (int)QkDevice::diData;
        }
        for(i=0; i<ndat; i++)
        {
            if(dataType == QkDevice::Data::dtInt)
                dataValue = getValue(4, &i_data, p->data, true);
            else
                dataValue = floatFromBytes(getValue(4, &i_data, p->data, true));
            selDevice->_setDataValue(i, dataValue, p->timestamp);
            if(bufferJustCreated)
                selDevice->_setDataLabel(i, QString().sprintf("D%d",i));
        }
        selDevice->_logData(selDevice->data());
        break;
    case QK_PACKET_CODE_EVENT:
        eventID = getValue(1, &i_data, p->data);
        if(eventID >= selDevice->events().size())
        {
            qWarning() << __FUNCTION__ << "event id greater than buffer capacity";
            events.resize(eventID+1);
            for(i=0; i < events.count(); i++)
                events[i]._setLabel(QString().sprintf("E%d",i));
            selDevice->_setEvents(events);
            bufferJustCreated = true;
            infoChangedEmit = true;
            infoChangedMask |= (int)QkDevice::diEvent;
        }
        eventRx._setLabel(selDevice->events()[eventID].label());
        nargs = getValue(1, &i_data, p->data);
        eventArgs.clear();
        for(i=0; i<nargs; i++)
        {
            eventArg = floatFromBytes(getValue(4, &i_data, p->data, true));
            eventArgs.append(eventArg);
        }
        eventRx._setArgs(eventArgs);
        eventRx._setMessage(getString(&i_data, p->data));
//        if(!m_eventLogging)
//            selDevice->eventsFired()->clear();
        selDevice->_logEvent(eventRx);
        break;
    case QK_PACKET_CODE_STRING:
        debugStr = getString(&i_data, p->data);
        break;
//    case QK_PACKET_CODE_OK: //FIXME not tested
//        m_protocol.ack.code = QK_COMM_OK;
//        m_protocol.ack.arg = getValue(1, &i_data, p->data);
//        break;
//    case QK_PACKET_CODE_ERR: //FIXME not tested
//        m_protocol.ack.code = QK_COMM_ERR;
//        m_protocol.ack.err = getValue(4, &i_data, p->data);
//        m_protocol.ack.arg = getValue(4, &i_data, p->data);
//        break;
    default:
        //m_protocol->ack().type = QkAck::ERROR;
        //m_protocol->ack().err = QK_ERR_CODE_UNKNOWN;
        //m_protocol->ack().arg = p->code;
        qWarning() << __FUNCTION__ << "packet code not recognized" << QString().sprintf("(%02X)", p->code);
    }

//    switch(p->code)
//    {
//    case QK_PACKET_CODE_SEQBEGIN:
//    case QK_PACKET_CODE_SEQEND:
//    case QK_PACKET_CODE_OK:
//    case QK_PACKET_CODE_ERR:
//        break;
//    default:
//        if(!(p->flags.ctrl & QK_PACKET_FLAGMASK_CTRL_NOTIF))
//        {
//            if(!m_protocol.sequence && m_protocol.ack.err != QK_ERR_CODE_UNKNOWN)
//            {
//                m_protocol.ack.code = QK_COMM_ACK;
//                m_protocol.ack.arg = p->code;
//            }
//        }
//    }

//    if(infoChangedEmit)
//        emit infoChanged(p->address, (QkBoard::Type)p->source(), infoChangedMask);

    switch(p->code)
    {
    case QK_PACKET_CODE_ACK:
        emit ack(ackRx);
        if(ackRx.code == QK_PACKET_CODE_SEARCH)
        {
            switch(p->source())
            {
            case QkBoard::btComm: emit commFound(selNode->address()); break;
            case QkBoard::btDevice: emit deviceFound(selNode->address()); break;
            }
        }
        else if((ackRx.code == QK_PACKET_CODE_GETNODE && p->source() == QkBoard::btComm) ||
                ackRx.code == QK_PACKET_CODE_GETMODULE)
        {
            emit commUpdated(selNode->address());
        }
        else if((ackRx.code == QK_PACKET_CODE_GETNODE && p->source() == QkBoard::btDevice) ||
                ackRx.code == QK_PACKET_CODE_GETDEVICE)
        {
            emit deviceUpdated(selNode->address());
        }

        break;
    case QK_PACKET_CODE_DATA:
        emit dataReceived(selNode->address(), selDevice->data());
        break;
    case QK_PACKET_CODE_EVENT:
        //emit eventReceived(selNode->address(), firedEvent);
        emit eventReceived(selNode->address(), eventRx);
        break;
    case QK_PACKET_CODE_STRING:
        emit debugReceived(selNode->address(), debugStr);
        break;
//    case QK_PACKET_CODE_ERR:
//        emit error(m_protocol.ack.err, m_protocol.ack.arg);
//        break;
    default: ;
    }

    emit packetProcessed();
}


bool QkPacket::Builder::build(QkPacket *packet, const Descriptor &desc)
{
    qDebug() << "build packet with code" << QString().sprintf("%02X", desc.code & 0xFF);

    int i_data;
    QkDevice *device = 0;

    packet->flags.ctrl = 0;
    packet->address = desc.address;
    packet->id = QkPacket::requestId();
    packet->code = desc.code;
    packet->data.clear();

    QkBoard *board = desc.board;

    if(board != 0)
    {
        switch(desc.boardType)
        {
        case QkBoard::btDevice:
            device = (QkDevice *) board;
            break;
        default:
            qDebug() << "cant build packets for that board type";
        }
    }

    QkDevice::SamplingInfo sampInfo;
    QVector<QkBoard::Config> configs;
    QVariant configValue;
    QkDevice::Action *act;

    i_data = 0;
    switch(desc.code)
    {
    case QK_PACKET_CODE_GETNODE:
        fillValue(desc.getnode_address, 4, &i_data, packet->data);
        break;
    case QK_PACKET_CODE_SETNAME:
        fillString(desc.setname_str, QK_BOARD_NAME_SIZE, &i_data, packet->data);
        break;
    case QK_PACKET_CODE_SETCONFIG:
        configs = board->configs();
        fillValue(1, 1, &i_data, packet->data);
        fillValue(desc.setconfig_idx, 1, &i_data, packet->data);
        configValue = configs[desc.setconfig_idx].value();
        switch(configs[desc.setconfig_idx].type())
        {
        case QkBoard::Config::ctBool:
            fillValue(configValue.toInt(), 1, &i_data, packet->data);
            break;
        case QkBoard::Config::ctIntDec:
            fillValue(configValue.toInt(), 4, &i_data, packet->data);
            break;
        case QkBoard::Config::ctIntHex:
            fillValue(configValue.toUInt(), 4, &i_data, packet->data);
            break;
        case QkBoard::Config::ctFloat:
            fillValue(bytesFromFloat(configValue.toFloat()), 4, &i_data, packet->data);
            break;
        case QkBoard::Config::ctDateTime:
            fillValue(configValue.toDateTime().date().year()-2000, 1, &i_data, packet->data);
            fillValue(configValue.toDateTime().date().month(), 1, &i_data, packet->data);
            fillValue(configValue.toDateTime().date().day(), 1, &i_data, packet->data);
            fillValue(configValue.toDateTime().time().hour(), 1, &i_data, packet->data);
            fillValue(configValue.toDateTime().time().minute(), 1, &i_data, packet->data);
            fillValue(configValue.toDateTime().time().second(), 1, &i_data, packet->data);
            break;
        case QkBoard::Config::ctTime:
            fillValue(configValue.toTime().hour(), 1, &i_data, packet->data);
            fillValue(configValue.toTime().minute(), 1, &i_data, packet->data);
            fillValue(configValue.toTime().second(), 1, &i_data, packet->data);
            break;
        case QkBoard::Config::ctCombo:
            qDebug() << "Config::ctCombo";
            break;
        default:
            qDebug() << "Config type unknown";
        }
        break;
    case QK_PACKET_CODE_SETSAMP:
        sampInfo = device->samplingInfo();
        fillValue(sampInfo.frequency, 4, &i_data, packet->data);
        fillValue(sampInfo.mode, 1, &i_data, packet->data);
        fillValue((int)(sampInfo.triggerClock), 1, &i_data, packet->data);
        fillValue((int)(sampInfo.triggerScaler), 1, &i_data, packet->data);
        fillValue(sampInfo.N, 4, &i_data, packet->data);
        break;
    case QK_PACKET_CODE_ACTUATE:
        act = &(device->actions()[desc.action_id]);
        fillValue(desc.action_id, 1, &i_data, packet->data);
        fillValue((int)(act->type()), 1, &i_data, packet->data);
        switch (act->type())
        {
        case QkDevice::Action::atBool:
            fillValue((int)(act->value().toBool()), 1, &i_data, packet->data);
            break;
        case QkDevice::Action::atInt:
            fillValue(act->value().toInt(), 4, &i_data, packet->data);
            break;
        }
        break;
    }

    return true;
}

bool QkPacket::Builder::validate(Descriptor *pd)
{
    bool ok = false;

    switch(pd->code)
    {
    default: ok = true;
    }

    return ok;
}

void QkPacket::Builder::parse(const QkFrame &frame, QkPacket *packet)
{
    int i_data = 0;

    QByteArray data = frame.data;

    packet->calculateHeaderLenght();

    packet->checksum = (int) data.at(data.length() - 1);

    packet->flags.ctrl = getValue(2, &i_data, data);
    packet->address = 0;
    packet->timestamp = frame.timestamp;

    packet->code = getValue(1, &i_data, data);

    packet->data.clear();
    packet->data.append(data.right(data.count() - i_data));
    packet->data.truncate(packet->data.count() - 1); // remove checksum byte
}



int QkPacket::requestId()
{
    m_nextId = (m_nextId+1) % 256;
    return m_nextId;
}

int QkPacket::source()
{
    return (QkBoard::Type) ((flags.ctrl >> 4) & 0x07);
}

void QkPacket::calculateHeaderLenght()
{
    headerLength = SIZE_FLAGS_CTRL + SIZE_CODE;

    if(!(flags.ctrl & QK_PACKET_FLAGMASK_CTRL_NOTIF))
      headerLength += SIZE_ID;

    if(flags.ctrl & QK_PACKET_FLAGMASK_CTRL_ADDRESS)
      headerLength += SIZE_FLAGS_NETWORK;
}

QString QkPacket::codeFriendlyName()
{
    switch((quint8)code)
    {
    case QK_PACKET_CODE_HELLO:
        return "HELLO";
    case QK_PACKET_CODE_ACK:
        return "ACK";
    case QK_PACKET_CODE_SEARCH:
        return "SEARCH";
    case QK_PACKET_CODE_GETNODE:
        return "GET_NODE";
    case QK_PACKET_CODE_GETMODULE:
        return "GET_MODULE";
    case QK_PACKET_CODE_GETDEVICE:
        return "GET_DEVICE";
    case QK_PACKET_CODE_SETNAME:
        return "SET_NAME";
    case QK_PACKET_CODE_SETCONFIG:
        return "SET_CONFIG";
    case QK_PACKET_CODE_SETSAMP:
        return "SET_SAMP";
    case QK_PACKET_CODE_INFOQK:
        return "INFO_QK";
    case QK_PACKET_CODE_INFOSAMP:
        return "INFO_SAMP";
    case QK_PACKET_CODE_INFOBOARD:
        return "INFO_BOARD";
    case QK_PACKET_CODE_INFOCOMM:
        return "INFO_MODULE";
    case QK_PACKET_CODE_INFODEVICE:
        return "INFO_DEVICE";
    case QK_PACKET_CODE_INFODATA:
        return "INFO_DATA";
    case QK_PACKET_CODE_INFOEVENT:
        return "INFO_EVENT";
    case QK_PACKET_CODE_INFOACTION:
        return "INFO_ACTION";
    case QK_PACKET_CODE_INFOCONFIG:
        return "INFO_CONFIG";
    case QK_PACKET_CODE_DATA:
        return "DATA";
    case QK_PACKET_CODE_EVENT:
        return "EVENT";
    case QK_PACKET_CODE_STATUS:
        return "STATUS";
    case QK_PACKET_CODE_START:
        return "START";
    case QK_PACKET_CODE_STOP:
        return "STOP";
    case QK_PACKET_CODE_STRING:
        return "STRING";
    case QK_PACKET_CODE_OK:
        return "OK";
    case QK_PACKET_CODE_ERR:
        return "ERR";
    case QK_PACKET_CODE_TIMEOUT:
        return "TIMEOUT";
    default:
        return QString().sprintf("%02X", code);
    }
}
