#include "qkcore.h"
#include "qklib_constants.h"
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

using namespace std;
using namespace QkUtils;

//static void calculate_hdr_length(Qk::Packet *packet);


QString Qk::Info::versionString()
{
    return QString().sprintf("%d.%d.%d", version_major,
                                          version_minor,
                                          version_patch);
}









QkCore::QkCore(QObject *parent) :
    QObject(parent)
{
    m_running = false;
    m_nodes.clear();
    setProtocolTimeout(2000);
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
    QkPacket p;
    QkPacket::Descriptor pd;
    pd.address = 0;
    pd.code = QK_PACKET_CODE_HELLO;
    QkPacket::Builder::build(&p, pd);

    int retries = 10;
    int backup_timeout = m_protocol.timeout;
    m_protocol.timeout = 100;
    QkAck ack;
    while(ack.type != QkAck::OK && retries--)
    {
        ack = comm_sendPacket(&p, true);
    }
    m_protocol.timeout = backup_timeout;
    return ack;
}

QkAck QkCore::search()
{
    qDebug() << __FUNCTION__;
    //TODO clear all nodes
    QkPacket p;
    QkPacket::Descriptor pd;
    pd.address = 0;
    pd.code = QK_PACKET_CODE_SEARCH;
    QkPacket::Builder::build(&p, pd);
    return comm_sendPacket(&p, true);
}

QkAck QkCore::getNode(int address)
{
    QkPacket p;
    QkPacket::Descriptor pd;
    pd.address = 0;
    pd.code = QK_PACKET_CODE_GETNODE;
    pd.getnode_address = address;
    QkPacket::Builder::build(&p, pd);
    return comm_sendPacket(&p, true);
}

QkAck QkCore::start(int address)
{
    QkPacket p;
    QkPacket::Descriptor pd;
    pd.address = address;
    pd.code = QK_PACKET_CODE_START;
    QkPacket::Builder::build(&p, pd);
    QkAck ack = comm_sendPacket(&p, true);
    if(ack.type == QkAck::OK)
        m_running = true;
    return ack;
}

QkAck QkCore::stop(int address)
{
    QkPacket p;
    QkPacket::Descriptor pd;
    pd.address = address;
    pd.code = QK_PACKET_CODE_STOP;
    QkPacket::Builder::build(&p, pd);
    QkAck ack = comm_sendPacket(&p, true);
    if(ack.type == QkAck::OK)
        m_running = false;
    return ack;
}

bool QkCore::isRunning()
{
    return m_running;
}

void QkCore::setProtocolTimeout(int timeout)
{
    m_protocol.timeout = timeout;
}

void QkCore::setEventLogging(bool enabled)
{
    m_eventLogging = enabled;
}


QkAck QkCore::comm_sendPacket(QkPacket *packet, bool wait)
{
    qDebug() << __FUNCTION__ << packet->codeFriendlyName() << "id:" << packet->id;
    QByteArray frame;

    frame.append(packet->flags.ctrl & 0xFF);
    frame.append((packet->flags.ctrl >> 8) & 0xFF);

    frame.append(packet->id);
    frame.append(packet->code);
    frame.append(packet->data);


    m_framesToSend.enqueue(frame);
    emit comm_frameReady();

    if(wait)
        return _comm_wait(packet->id, m_protocol.timeout);
    else
        return m_protocol.ack;
}

QQueue<QByteArray> *QkCore::framesToSend()
{
    return &m_framesToSend;
}

void QkCore::comm_processFrame(QkFrame frame)
{
    QkPacket *incomingPacket = &(m_protocol.incomingPacket);
    _comm_parseFrame(frame, incomingPacket);

    if(incomingPacket->flags.ctrl & QK_PACKET_FLAGMASK_CTRL_FRAG)
    {
        m_protocol.fragmentedPacket.data.append(incomingPacket->data);

        //FIXME create elapsedTimer to timeout lastFragment reception
        if(!(incomingPacket->flags.ctrl & QK_PACKET_FLAGMASK_CTRL_LASTFRAG))
            return;
        else
        {
            incomingPacket->data = m_protocol.fragmentedPacket.data;
            m_protocol.fragmentedPacket.data.clear();
        }
    }
    _comm_processPacket(incomingPacket);
}

void QkCore::_comm_parseFrame(QkFrame frame, QkPacket *packet)
{
    QkPacket::Builder::parse(frame, packet);
}

void QkCore::_comm_processPacket(QkPacket *p)
{
    qDebug() << __FUNCTION__ << "addr =" << p->address << "code =" << p->codeFriendlyName();

    QkBoard *selBoard = 0;
    QkNode *selNode = 0;
    QkComm *selComm = 0;
    QkDevice *selDevice = 0;
    bool boardCreated = false;



    QMutexLocker locker(&m_mutex);

    switch(p->source())
    {
    case QkBoard::btComm:
        selNode = node(p->address);
        if(selNode == 0)
        {
            selNode = new QkNode(this, p->address);
            m_nodes.insert(p->address, selNode);
        }
        if(selNode->comm() == 0)
        {
            selNode->setComm(new QkComm(this, selNode));
            boardCreated = true;
        }
        selComm = selNode->comm();
        selBoard = selNode->comm();
        break;
    case QkBoard::btDevice:
        selNode = node(p->address);
        if(selNode == 0)
        {
            selNode = new QkNode(this, p->address);
            m_nodes.insert(p->address, selNode);
        }
        if(selNode->device() == 0)
        {
            selDevice = new QkDevice(this, selNode);
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
    Qk::Info qkInfo;
    QVector<QkBoard::Config> configs;
    QkBoard::Config::Type configType;
    QVector<QkDevice::Data> data;
    QkDevice::Data::Type dataType;
    QVector<QkDevice::Event> events;
    QkDevice::Event firedEvent;
    QVector<QkDevice::Action> actions;
    QString label, name;
    QVariant varValue;
    QStringList items;
    QDateTime dateTime;
    QkDevice::SamplingInfo sampInfo;
    QList<float> eventArgs;
    float eventArg;
    bool bufferJustCreated = false;
    bool infoChangedEmit = false;
    int infoChangedMask = 0;
    QkAck receivedAck;

    int i_data = 0;
    switch(p->code)
    {
    case QK_PACKET_CODE_ACK:
        receivedAck.id = getValue(1, &i_data, p->data);
        receivedAck.code = getValue(1, &i_data, p->data);
        receivedAck.type = getValue(1, &i_data, p->data);
        if(receivedAck.type == QkAck::ERROR)
        {
            receivedAck.err = getValue(1, &i_data, p->data);
            receivedAck.arg = getValue(1, &i_data, p->data);
        }
        m_frameAck = receivedAck;
        m_receivedAcks.prepend(receivedAck);
        qDebug() << "ACK" << "id" << receivedAck.id << "code" << receivedAck.code << "type" << receivedAck.type;
        break;
    case QK_PACKET_CODE_INFOQK:
        qkInfo.version_major = getValue(1, &i_data, p->data);
        qkInfo.version_minor = getValue(1, &i_data, p->data);
        qkInfo.version_patch = getValue(1, &i_data, p->data);
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
        qDebug() << "ncfg" << ncfg;
        configs = QVector<QkBoard::Config>(ncfg);
        for(i=0; i<ncfg; i++)
        {
            configType = (QkBoard::Config::Type) getValue(1, &i_data, p->data);
            label = getString(QK_LABEL_SIZE, &i_data, p->data);
            qDebug() << "config label" << label;
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
        qDebug() << "ndat =" << ndat;
        for(i=0; i<ndat; i++)
        {
            data[i]._setLabel(getString(QK_LABEL_SIZE, &i_data, p->data));
            qDebug() << QString().sprintf("data[%d]=%s",i,data[i].label().toStdString().data());
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
        qDebug() << "nact" << nact;
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
        firedEvent._setLabel(selDevice->events()[eventID].label());
        nargs = getValue(1, &i_data, p->data);
        eventArgs.clear();
        for(i=0; i<nargs; i++)
        {
            eventArg = floatFromBytes(getValue(4, &i_data, p->data, true));
            eventArgs.append(eventArg);
        }
        firedEvent._setArgs(eventArgs);
        firedEvent._setMessage(getString(&i_data, p->data));
//        if(!m_eventLogging)
//            selDevice->eventsFired()->clear();
        selDevice->_logEvent(firedEvent);
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
        m_protocol.ack.type = QkAck::ERROR;
        m_protocol.ack.err = QK_ERR_CODE_UNKNOWN;
        m_protocol.ack.arg = p->code;
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

    if(infoChangedEmit)
        emit infoChanged(p->address, (QkBoard::Type)p->source(), infoChangedMask);

    switch(p->code)
    {
    case QK_PACKET_CODE_ACK:
        emit ack(receivedAck);
        if(receivedAck.code == QK_PACKET_CODE_SEARCH)
        {
            switch(p->source())
            {
            case QkBoard::btComm: emit commFound(selNode->address()); break;
            case QkBoard::btDevice: emit deviceFound(selNode->address()); break;
            }
        }
        else if((receivedAck.code == QK_PACKET_CODE_GETNODE && p->source() == QkBoard::btComm) ||
                receivedAck.code == QK_PACKET_CODE_GETMODULE)
        {
            emit commUpdated(selNode->address());
        }
        else if((receivedAck.code == QK_PACKET_CODE_GETNODE && p->source() == QkBoard::btDevice) ||
                receivedAck.code == QK_PACKET_CODE_GETDEVICE)
        {
            emit deviceUpdated(selNode->address());
        }

        break;
//    case QK_PACKET_CODE_SEQEND:
//        qDebug() << "m_comm.ack.arg" << QString().sprintf("%02X", m_protocol.ack.arg);
//        switch(m_protocol.ack.arg)
//        {
//        case QK_PACKET_CODE_COMMFOUND:
//            qDebug() << "QkModule found:" << QString().sprintf("%04X",selNode->address());
//            emit moduleFound(selNode->address());
//            break;
//        case QK_PACKET_CODE_DEVICEFOUND:
//            qDebug() << "QkDevice found:" << QString().sprintf("%04X",selNode->address());
//            emit deviceFound(selNode->address());
//            break;
//        case QK_PACKET_CODE_GETDEVICE:
//            emit deviceUpdated(selNode->address());
//            break;
//        }
//        break;
    case QK_PACKET_CODE_DATA:
        emit dataReceived(selNode->address());
        break;
    case QK_PACKET_CODE_EVENT:
        //emit eventReceived(selNode->address(), firedEvent);
        emit eventReceived(selNode->address());
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

QkAck QkCore::waitForACK()
{
    m_frameAck.type = QkAck::NACK;
    qDebug() << __FUNCTION__;

    int timeout = 2000;

    QEventLoop loop;
    QTimer loopTimer;
    QElapsedTimer elapsedTimer;

    //loopTimer.setSingleShot(true);
    connect(&loopTimer, SIGNAL(timeout()), &loop, SLOT(quit()));
    connect(this, SIGNAL(packetProcessed()), &loop, SLOT(quit()));

    elapsedTimer.start();

    while(m_frameAck.type == QkAck::NACK && !elapsedTimer.hasExpired(timeout))
    {
        loopTimer.start(100);
        loop.exec();
    }

    loopTimer.stop();

    if(elapsedTimer.hasExpired(timeout))
    {
        error(QK_ERR_COMM_TIMEOUT, timeout);
    }

    return m_frameAck;
}

QkAck QkCore::_comm_wait(int packetID, int timeout)
{
    timeout = 2000;

    QEventLoop loop;
    QTimer loopTimer;
    QElapsedTimer elapsedTimer;
    int interval = timeout/4;

    //loopTimer.setSingleShot(true);
    connect(&loopTimer, SIGNAL(timeout()), &loop, SLOT(quit()));
    connect(this, SIGNAL(packetProcessed()), &loop, SLOT(quit()));

    elapsedTimer.start();

    //while(!elapsedTimer.hasExpired(timeout) && m_protocol.ack.type == Qk::Ack::NACK)
    QkAck ack;
    ack.type = QkAck::NACK;

//   ack.code = Qk::Ack::OK;
//   return ack;

    bool nack = true;
//    while(nack && !elapsedTimer.hasExpired(timeout))
    while(ack.type == QkAck::NACK && !elapsedTimer.hasExpired(timeout))
    {
        loopTimer.start(timeout);
        loop.exec();
        foreach(const QkAck &receivedAck, m_receivedAcks)
        {
            if(receivedAck.id == packetID)
            {
                ack = receivedAck;
                nack = false;
                break;
            }
        }
    }
    qDebug() << "  elapsed time:" << elapsedTimer.elapsed();
    loopTimer.stop();

    if(elapsedTimer.hasExpired(timeout))
    {
        qDebug() << "COMM TIMEOUT" << "packetID" << packetID;
        qDebug() << "acks ="  << m_receivedAcks.count();
        foreach(const QkAck &receivedAck, m_receivedAcks)
            qDebug() << "ack packetID" << receivedAck.id;
//        emit error(QK_ERR_COMM_TIMEOUT, timeout);
    }

    m_receivedAcks.removeOne(ack);

    return ack;
}



QString QkCore::version()
{
    return QString().sprintf("%d.%d.%d", QKLIB_VERSION_MAJOR,
                                         QKLIB_VERSION_MINOR,
                                         QKLIB_VERSION_PATCH);
}

QString QkCore::errorMessage(int errCode)
{
    errCode &= 0xFF;
    switch(errCode) {
    case QK_ERR_CODE_UNKNOWN: return tr("Code unknown"); break;
    case QK_ERR_INVALID_BOARD: return tr("Invalid board"); break;
    case QK_ERR_INVALID_DATA_OR_ARG: return tr("Invalid data or arguments"); break;
    case QK_ERR_BOARD_NOT_CONNECTED: return tr("Board not connected"); break;
    case QK_ERR_INVALID_SAMP_FREQ: return tr("Invalid sampling frequency"); break;
    case QK_ERR_COMM_TIMEOUT: return tr("Communication timeout"); break;
    case QK_ERR_UNSUPPORTED_OPERATION: return tr("Unsupported operation"); break;
    case QK_ERR_UNABLE_TO_SEND_MESSAGE: return tr("Unable to send message"); break;
    case QK_ERR_SAMP_OVERLAP: return tr("Sampling overlap"); break;
    default:
        return tr("Error code unknown") + QString().sprintf(" (%d)",errCode); break;
    }
}




//static void calculate_hdr_length(Qk::Packet *packet)
//{

//    packet->headerLength = 2+1; // flags(2) + code(1)
//    packet->headerLength = SIZE_FLAGS_CTRL + SIZE_CODE;

//    if(!(packet->flags.ctrl & QK_PACKET_FLAGMASK_CTRL_NOTIF))
//      packet->headerLength += SIZE_ID;

//    if(packet->flags.ctrl & QK_PACKET_FLAGMASK_CTRL_ADDRESS)
//    {
//      packet->headerLength += SIZE_FLAGS_NETWORK;
//    }
//}



