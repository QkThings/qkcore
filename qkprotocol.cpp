
#include "qkprotocol.h"
#include "qkcore.h"
#include "qkboard.h"
#include "qkdevice.h"
#include "qkcomm.h"

#include "qkutils.h"
#include "qkcore_constants.h"

#include <QDebug>
#include <QDateTime>
#include <QMutexLocker>
#include <QMutex>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QTimer>


using namespace QkUtils;

int QkPacket::m_nextId = 0;

int QkAck::toInt()
{
    return (int)type + (arg << 8) + ( err << 16);
}

QkAck QkAck::fromInt(int ack)
{
    QkAck res;
    res.type = (Type)(ack & 0xFF);
    res.arg = (ack >> 8) & 0xFF;
    res.err = (ack >> 16) & 0xFF;
    return res;
}

QkProtocol::QkProtocol(QkCore *qk, QObject *parent) :
    QObject(parent)
{

}

QkAck QkProtocol::sendPacket(QkPacket *packet, bool wait, int retries)
{
    QByteArray frame;

    frame.append(packet->flags.ctrl & 0xFF);
    frame.append((packet->flags.ctrl >> 8) & 0xFF);
    frame.append(packet->id);
    frame.append(packet->code);
    frame.append(packet->data);

    //m_framesToSend.enqueue(frame);
    //emit comm_frameReady();

    if(wait)
        return waitForACK(packet->id);
    else
        return m_ack;
}

void QkProtocol::processFrame(const QkFrame &frame)
{
    QkPacket::Builder::parse(frame, &m_packet);

    if(m_packet.flags.ctrl & QK_PACKET_FLAGMASK_CTRL_FRAG)
    {
        m_packet.data.append(m_packet.data);

        //FIXME create elapsedTimer to timeout lastFragment reception
        if(!(m_packet.flags.ctrl & QK_PACKET_FLAGMASK_CTRL_LASTFRAG))
            return;
    }
    processPacket();
}

void QkProtocol::processPacket()
{

    emit packetProcessed();
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
    ack.type = QkAck::NACK;

    while(ack.type == QkAck::NACK && !elapsedTimer.hasExpired(timeout))
    {
        loopTimer.start(timeout);
        loop.exec();
        foreach(const QkAck &receivedAck, m_receivedAcks)
        {
            if(receivedAck.id == packetId)
            {
                ack = receivedAck;
                break;
            }
        }
    }
    loopTimer.stop();

    if(elapsedTimer.hasExpired(timeout))
    {
//        emit error(QK_ERR_COMM_TIMEOUT, timeout);
    }

    m_receivedAcks.removeOne(ack);

    return ack;
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

bool QkPacket::Builder::build(QkPacket *packet, const Descriptor &desc, QkBoard *board)
{
    int i_data;
    QkDevice *device = 0;

    packet->flags.ctrl = 0;
    packet->address = desc.address;
    packet->id = QkPacket::requestId();
    packet->code = desc.code;
    packet->data.clear();


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
