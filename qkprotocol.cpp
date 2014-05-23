
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
#include <QMutexLocker>
#include <QMutex>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QTimer>
#include <QStringList>


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

QkProtocol::QkProtocol(QkCore *qk, QObject *parent) :
    QObject(parent)
{
    m_qk = qk;
}

void QkProtocol::processFrame(const QkFrame &frame)
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

    processPacket(&packet);
}

QkAck QkProtocol::sendPacket(QkPacket::Descriptor descriptor, bool wait, int timeout, int retries)
{
    QkAck ack;
    QkPacket packet;
    QkPacket::Builder::build(&packet, descriptor);

    QkFrame frame;
    frame.data.append(packet.flags.ctrl & 0xFF);
    frame.data.append((packet.flags.ctrl >> 8) & 0xFF);
    frame.data.append(packet.id);
    frame.data.append(packet.code);
    frame.data.append(packet.data);

    do
    {
        m_frames.enqueue(frame);
        emit frameReady(&m_frames);
        if(wait)
            ack = waitForACK(packet.id, timeout);
    } while(wait && retries-- && ack.result == QkAck::NACK);

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

    QkAck waitAck;
    waitAck.result = QkAck::NACK;

    while(waitAck.result == QkAck::NACK && !elapsedTimer.hasExpired(timeout))
    {
        loopTimer.start(50);
        loop.exec();
        foreach(const QkAck &receivedAck, m_acks)
        {
            if(receivedAck.id == packetId)
            {
                waitAck = receivedAck;
                break;
            }
        }
    }
    loopTimer.stop();

    if(elapsedTimer.hasExpired(timeout))
    {
//        emit error(QK_ERR_COMM_TIMEOUT, timeout);
    }

    m_acks.removeOne(waitAck);

    return waitAck;
}

//QkAck QkProtocol::waitForACK(int timeout)
//{
//    m_frameAck.type = QkAck::NACK;
//    qDebug() << __FUNCTION__;

//    QEventLoop loop;
//    QTimer loopTimer;
//    QElapsedTimer elapsedTimer;

//    //loopTimer.setSingleShot(true);
//    connect(&loopTimer, SIGNAL(timeout()), &loop, SLOT(quit()));
//    connect(this, SIGNAL(packetProcessed()), &loop, SLOT(quit()));

//    elapsedTimer.start();

//    while(m_frameAck.type == QkAck::NACK && !elapsedTimer.hasExpired(timeout))
//    {
//        loopTimer.start(100);
//        loop.exec();
//    }

//    loopTimer.stop();

//    if(elapsedTimer.hasExpired(timeout))
//    {
//        error(QK_ERR_COMM_TIMEOUT, timeout);
//    }

//    return m_frameAck;
//}


void QkProtocol::processPacket(QkPacket *packet)
{
    QkBoard *selBoard = 0;
    QkNode *selNode = 0;
    QkComm *selComm = 0;
    QkDevice *selDevice = 0;
    bool boardCreated = false;

    QkPacket *p = packet;
    QkCore *qk = m_qk;

    qDebug() << __FUNCTION__ << "addr =" << p->address << "code =" << p->codeFriendlyName();

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
        receivedAck.result = getValue(1, &i_data, p->data);
        if(receivedAck.result == QkAck::ERROR)
        {
            receivedAck.err = getValue(1, &i_data, p->data);
            receivedAck.arg = getValue(1, &i_data, p->data);
        }
        m_acks.prepend(receivedAck);
        qDebug() << "ACK" << "id" << receivedAck.id << "code" << receivedAck.code << "type" << receivedAck.result;
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


bool QkPacket::Builder::build(QkPacket *packet, const Descriptor &desc)
{
    int i_data;
    QkDevice *device = 0;

    packet->flags.ctrl = 0;
    packet->address = desc.address;
    packet->id = QkPacket::requestId();
    packet->code = desc.code;
    packet->data.clear();

    QkBoard *board = desc.board,

    if(board != 0)
    {
        switch(desc.boardType)
        {
        case QkBoard::btDevice:
            device = (QkDevice *) board;
            break;
        default: qDebug() << "cant build packets for that board type (yet!)";
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
            qDebug() << configValue;
            fillValue(configValue.toTime().hour(), 1, &i_data, packet->data);
            fillValue(configValue.toTime().minute(), 1, &i_data, packet->data);
            fillValue(configValue.toTime().second(), 1, &i_data, packet->data);
            break;
        case QkBoard::Config::ctCombo:
            qDebug() << "Config::ctCombo <-- TODO";
            break;
        default:
            qDebug() << "WARNING: Unkown config type" << __LINE__ << __FILE__;
        }
        break;
    case QK_PACKET_CODE_SETSAMP:
        sampInfo = device->samplingInfo();
        qDebug() << "SETSAMP";
        qDebug() << "frequency" << sampInfo.frequency;
        qDebug() << "mode" << sampInfo.mode;
        qDebug() << "clock" << sampInfo.triggerClock;
        qDebug() << "scaler" << sampInfo.triggerScaler;
        qDebug() << "N" << sampInfo.N;
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
    m_nextId = (m_nextId >= 255 ? 0 : m_nextId+1);
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
        return "???";
    }
}
