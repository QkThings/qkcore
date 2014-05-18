#include "qkcore.h"
#include "qk_defs.h"

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

static void calculate_hdr_length(Qk::Packet *packet);

int Qk::Packet::m_nextId = 0;

int Qk::Packet::requestId()
{
    m_nextId = (m_nextId >= 255 ? 0 : m_nextId+1);
    return m_nextId;
}


QString Qk::Info::versionString()
{
    return QString().sprintf("%d.%d.%d", version_major,
                                          version_minor,
                                          version_patch);
}

int Qk::Ack::toInt()
{
    return (int)type + (arg << 8) + ( err << 16);
}

Qk::Ack Qk::Ack::fromInt(int ack)
{
    Ack res;
    res.type = (Type)(ack & 0xFF);
    res.arg = (ack >> 8) & 0xFF;
    res.err = (ack >> 16) & 0xFF;
    return res;
}

QkBoard::QkBoard(QkCore *qk)
{
    m_qk = qk;
    m_name = tr("<unknown>");
    m_fwVersion = 0;
    m_filledInfoMask = 0;
}

void QkBoard::_setInfoMask(int mask, bool overwrite)
{
    if(overwrite)
        m_filledInfoMask = mask;
    else
        m_filledInfoMask |= mask;
}

void QkBoard::_setFirmwareVersion(int version)
{
    m_fwVersion = version;
}

void QkBoard::_setQkInfo(const Qk::Info &qkInfo)
{
    m_qkInfo = qkInfo;
}

void QkBoard::_setName(const QString &name)
{
    m_name = name;
}

void QkBoard::_setConfigs(QVector<Config> configs)
{
    m_configs = configs;
}

void QkBoard::Config::_set(const QString label, Type type, QVariant value, double min, double max)
{
    m_label = label;
    m_type = type;
    m_value = value;
    m_min = min;
    m_max = max;
}

QString QkBoard::Config::label()
{
    return m_label;
}

QkBoard::Config::Type QkBoard::Config::type()
{
    return m_type;
}

void QkBoard::Config::setValue(QVariant value)
{
    m_value = value;
}

QVariant QkBoard::Config::value()
{
    return m_value;
}

Qk::Info QkBoard::qkInfo()
{
    return m_qkInfo;
}

QVector<QkBoard::Config> QkBoard::configs()
{
    return m_configs;
}

int QkBoard::address()
{
    if(m_parentNode != 0)
        return m_parentNode->address();
    else
        return 0;
}

QString QkBoard::name()
{
    return m_name;
}

int QkBoard::firmwareVersion()
{
    return m_fwVersion;
}

void QkBoard::setConfigValue(int idx, QVariant value)
{
    if(idx < 0 || idx >= m_configs.count())
        return;

    m_configs[idx].setValue(value);
}

Qk::Ack QkBoard::update()
{
    int i;
    Qk::Ack ack;
    Qk::Packet p;
    Qk::PacketDescriptor desc;
    desc.boardType = m_type;
    desc.address = address();

    desc.code = QK_PACKET_CODE_SETNAME;
    desc.setname_str = name();
    Qk::PacketBuilder::build(&p, desc, this);
    ack = m_qk->comm_sendPacket(&p, true);
    if(ack.type != Qk::Ack::OK) {
        qDebug() << "failed to set name" << ack.type;
        return ack;
    }

    desc.code = QK_PACKET_CODE_SETCONFIG;
    for(i = 0; i < configs().count(); i++)
    {
        desc.setconfig_idx = i;
        Qk::PacketBuilder::build(&p, desc, this);
        ack = m_qk->comm_sendPacket(&p, true);
        if(ack.type != Qk::Ack::OK)
            return ack;
    }

    desc.code = QK_PACKET_CODE_SETSAMP;
    Qk::PacketBuilder::build(&p, desc, this);
    ack = m_qk->comm_sendPacket(&p, true);
    if(ack.type != Qk::Ack::OK)
        return ack;

    return ack;
}

Qk::Ack QkBoard::save()
{
    Qk::Packet p;
    Qk::PacketDescriptor desc;
    desc.boardType = m_type;
    desc.address = address();
    desc.code = QK_PACKET_CODE_SAVE;
    Qk::PacketBuilder::build(&p, desc);
    return m_qk->comm_sendPacket(&p);
}

QkComm::QkComm(QkCore *qk, QkNode *parentNode) :
    QkBoard(qk)
{
    m_parentNode = parentNode;
    m_type = btComm;
}



QkDevice::QkDevice(QkCore *qk, QkNode *parentNode) :
    QkBoard(qk)
{
    m_parentNode = parentNode;
    m_type = btDevice;
    m_events.clear();
    m_data.clear();
    m_actions.clear();

    qRegisterMetaType<QkDevice::Event>();
}

QString QkDevice::samplingModeString(SamplingMode mode)
{
    int index = QkDevice::staticMetaObject.indexOfEnumerator("SamplingMode");
    QMetaEnum metaEnum = QkDevice::staticMetaObject.enumerator(index);

    QString str = QString(metaEnum.valueToKey(mode));
    str = str.toLower();
    str = str.remove(0, 2);
    return str;
}

QString QkDevice::triggerClockString(TriggerClock clock)
{
    int index = QkDevice::staticMetaObject.indexOfEnumerator("TriggerClock");
    QMetaEnum metaEnum = QkDevice::staticMetaObject.enumerator(index);

    QString str = QString(metaEnum.valueToKey(clock));
    str = str.toLower();
    str = str.remove(0, 2);
    return str;
}

void QkDevice::setSamplingFrequency(int freq)
{
    m_samplingInfo.frequency = freq;
}

void QkDevice::setSamplingMode(QkDevice::SamplingMode mode)
{
    m_samplingInfo.mode = mode;
}

Qk::Ack QkDevice::update()
{
    return QkBoard::update();
}

void QkDevice::_setSamplingInfo(SamplingInfo info)
{
    m_samplingInfo = info;
}

void QkDevice::_setData(QVector<Data> data)
{
    m_data = data;
}

void QkDevice::_setDataType(Data::Type type)
{
    m_dataType = type;
}

void QkDevice::_setDataValue(int idx, float value, quint64 timestamp)
{
    if(idx < 0 || idx > m_data.size())
    {
        qWarning() << __FUNCTION__ << "data index out of bounds";
        m_data.resize(idx+1);
    }
    m_data[idx]._setValue(value, timestamp);
}

void QkDevice::_setDataLabel(int idx, const QString &label)
{
    if(idx < 0 || idx > m_data.size())
    {
        qWarning() << __FUNCTION__ << "data index out of bounds";
        m_data.resize(idx+1);
    }
    m_data[idx]._setLabel(label);
}

void QkDevice::_setActions(QVector<Action> actions)
{
    m_actions = actions;
}

void QkDevice::_setEvents(QVector<Event> events)
{
    m_events = events;
}

void QkDevice::_appendEvent(const QkDevice::Event &event)
{
    m_eventsFired.append(event);
}

QQueue<QkDevice::Event> *QkDevice::eventsFired()
{
    return &m_eventsFired;
}

QkDevice::SamplingInfo QkDevice::samplingInfo()
{
    return m_samplingInfo;
}

QkDevice::Data::Type QkDevice::dataType()
{
    return m_dataType;
}

QVector<QkDevice::Data> QkDevice::data()
{
    return m_data;
}

QVector<QkDevice::Action> QkDevice::actions()
{
    return m_actions;
}

QVector<QkDevice::Event> QkDevice::events()
{
    return m_events;
}

int QkDevice::actuate(unsigned int id, QVariant value)
{
    if(id >= m_actions.count())
        return -1;

    m_actions[id]._setValue(value);

    Qk::Packet packet;
    Qk::PacketDescriptor descriptor;

    descriptor.boardType = m_type;
    descriptor.code = QK_PACKET_CODE_ACTUATE;
    descriptor.action_id = id;

    Qk::PacketBuilder::build(&packet, descriptor, this);
    Qk::Ack ack = m_qk->comm_sendPacket(&packet, true);
    if(ack.type != Qk::Ack::OK)
        return -2;

    return 0;
}

QkDevice::Data::Data()
{
    m_value = 0.0;
}

void QkDevice::Data::_setLabel(const QString &label)
{
    m_label = label;
}

void QkDevice::Data::_setValue(float value, quint64 timestamp)
{
    m_value = value;
    m_timestamp = timestamp;
}

QString QkDevice::Data::label()
{
    return m_label;
}

float QkDevice::Data::value()
{
    return m_value;
}

quint64 QkDevice::Data::timestamp()
{
    return m_timestamp;
}

void QkDevice::Event::_setLabel(const QString &label)
{
    m_label = label;
}

void QkDevice::Event::_setMessage(const QString &msg)
{
    m_text = msg;
}

void QkDevice::Event::_setArgs(QList<float> args)
{
    m_args = args;
}

QString QkDevice::Event::label()
{
    return m_label;
}

QString QkDevice::Event::message()
{
    return m_text;
}

QList<float> QkDevice::Event::args()
{
    return m_args;
}

void QkDevice::Action::_setId(int id)
{
    m_id = id;
}

void QkDevice::Action::_setLabel(const QString &label)
{
    m_label = label;
}

void QkDevice::Action::_setType(QkDevice::Action::Type type)
{
    m_type = type;
}

void QkDevice::Action::_setValue(QVariant value)
{
    m_value = value;
}

int QkDevice::Action::id()
{
    return m_id;
}

QString QkDevice::Action::label()
{
    return m_label;
}

QkDevice::Action::Type QkDevice::Action::type()
{
    return m_type;
}

QVariant QkDevice::Action::value()
{
    return m_value;
}

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


QkCore::QkCore(QObject *parent) :
    QObject(parent)
{
    m_running = false;
    m_nodes.clear();
    m_protocol.sequence = false;
    setCommTimeout(2000);
}

QkNode* QkCore::node(int address)
{
    return m_nodes.value(address);
}

QMap<int, QkNode*> QkCore::nodes()
{
    return m_nodes;
}

Qk::Ack QkCore::search()
{
    //TODO clear all nodes
    Qk::Packet p;
    Qk::PacketDescriptor pd;
    pd.address = 0;
    pd.code = QK_PACKET_CODE_SEARCH;
    Qk::PacketBuilder::build(&p, pd);
    return comm_sendPacket(&p, true);
}

Qk::Ack QkCore::getNode(int address)
{
    Qk::Packet p;
    Qk::PacketDescriptor pd;
    pd.address = 0;
    pd.code = QK_PACKET_CODE_GETNODE;
    pd.getnode_address = address;
    Qk::PacketBuilder::build(&p, pd);
    return comm_sendPacket(&p, true);
}

Qk::Ack QkCore::start(int address)
{
    Qk::Packet p;
    Qk::PacketDescriptor pd;
    pd.address = address;
    pd.code = QK_PACKET_CODE_START;
    Qk::PacketBuilder::build(&p, pd);
    Qk::Ack ack = comm_sendPacket(&p, true);
    if(ack.type == Qk::Ack::OK)
        m_running = true;
    return ack;
}

Qk::Ack QkCore::stop(int address)
{
    Qk::Packet p;
    Qk::PacketDescriptor pd;
    pd.address = address;
    pd.code = QK_PACKET_CODE_STOP;
    Qk::PacketBuilder::build(&p, pd);
    Qk::Ack ack = comm_sendPacket(&p, true);
    if(ack.type == Qk::Ack::OK)
        m_running = false;
    return ack;
}

bool QkCore::isRunning()
{
    return m_running;
}

void QkCore::setCommTimeout(int timeout)
{
    m_protocol.timeout = timeout;
}

//#include <QCoreApplication>

Qk::Ack QkCore::comm_sendPacket(Packet *packet, bool wait)
{
    qDebug() << __FUNCTION__ << packet->codeFriendlyName() << "id:" << packet->id;
    QByteArray frame;

    frame.append(packet->flags.ctrl & 0xFF);
    frame.append((packet->flags.ctrl >> 8) & 0xFF);

    frame.append(packet->id);
    frame.append(packet->code);
    frame.append(packet->data);

    // Wait while the ack from previous sent packet isn't received
//    while(m_comm.ack.code == QK_COMM_NACK && ++timeout < 99999)
//    {
//        QCoreApplication::processEvents();
//    }

//    m_protocol.ack.type = Qk::Ack::NACK;
    m_framesToSend.enqueue(frame);
    emit comm_frameReady();

//    emit comm_sendFrame(frame); //TODO obsolete (use queue instead)

    if(wait)
        return _comm_wait(packet->id, m_protocol.timeout);
    else
        return m_protocol.ack;
}

QQueue<QByteArray> *QkCore::framesToSend()
{
    return &m_framesToSend;
}

void QkCore::comm_processFrame(Qk::Frame frame)
{
    Qk::Packet *incomingPacket = &(m_protocol.incomingPacket);
    _comm_parseFrame(frame, incomingPacket);

    if(incomingPacket->flags.ctrl & QK_PACKET_FLAGMASK_CTRL_FRAG)
    {
        m_protocol.fragmentedPacket.data.append(incomingPacket->data);

        if(!(incomingPacket->flags.ctrl & QK_PACKET_FLAGMASK_CTRL_LASTFRAG)) //FIXME create elapsedTimer to timeout lastFragment reception
            return;
        else
        {
            incomingPacket->data = m_protocol.fragmentedPacket.data;
            m_protocol.fragmentedPacket.data.clear();
        }
    }
    _comm_processPacket(incomingPacket);
}

void QkCore::_comm_parseFrame(Frame frame, Packet *packet)
{
    Qk::PacketBuilder::parse(frame, packet);
}

void QkCore::_comm_processPacket(Packet *p)
{
    qDebug() << __FUNCTION__ << "addr =" << p->address << "code =" << p->codeFriendlyName();

    QkBoard *selBoard = 0;
    QkNode *selNode = 0;
    QkComm *selComm = 0;
    QkDevice *selDevice = 0;
    bool boardCreated = false;

    using namespace Utils;

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

//    switch(p->code)
//    {
//    case QK_PACKET_CODE_DATA:
//    case QK_PACKET_CODE_EVENT:
//    case QK_PACKET_CODE_STRING:
//    case QK_PACKET_CODE_OK:
//    case QK_PACKET_CODE_ERR:
//        if(boardCreated)
//        {
//            switch(p->source())
//            {
//            case QkBoard::btComm:
//                emit moduleFound(p->address);
//            case QkBoard::btDevice:
//                emit deviceFound(p->address);
//                break;
//            default:
//                qDebug() << "source?";
//            }
//        }
//        break;
//    default:;
//    }

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
    Qk::Ack receivedAck;

    int i_data = 0;
    switch(p->code)
    {
    case QK_PACKET_CODE_ACK:
        receivedAck.id = getValue(1, &i_data, p->data);
        receivedAck.code = getValue(1, &i_data, p->data);
        receivedAck.type = getValue(1, &i_data, p->data);
        if(receivedAck.type == Qk::Ack::ERROR)
        {
            receivedAck.err = getValue(1, &i_data, p->data);
            receivedAck.arg = getValue(1, &i_data, p->data);
        }
        m_frameAck = receivedAck;
        m_acks.prepend(receivedAck);
        qDebug() << "ACK" << "id" << receivedAck.id << "code" << receivedAck.code << "type" << receivedAck.type;
        break;
//    case QK_PACKET_CODE_SEQBEGIN:
//        m_protocol.sequence = true;
//        break;
//    case QK_PACKET_CODE_SEQEND:
//        m_protocol.sequence = false;
//        m_protocol.ack.code = QK_COMM_ACK;
//        m_protocol.ack.arg = getValue(1, &i_data, p->data);
//        break;
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
                qDebug() << "config info value HEX" << varValue;
                min = (double) getValue(4, &i_data, p->data, true);
                max = (double) getValue(4, &i_data, p->data, true);
                break;
            case QkBoard::Config::ctFloat:
                varValue = QVariant(Utils::floatFromBytes(getValue(4, &i_data, p->data, true)));
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
        selDevice->_appendEvent(firedEvent);
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
        m_protocol.ack.type = Qk::Ack::ERROR;
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
        emit debugString(selNode->address(), debugStr);
        break;
//    case QK_PACKET_CODE_ERR:
//        emit error(m_protocol.ack.err, m_protocol.ack.arg);
//        break;
    default: ;
    }

    emit packetProcessed();
}

Qk::Ack QkCore::waitForACK(int id)
{
    m_frameAck.type = Qk::Ack::NACK;
    qDebug() << __FUNCTION__;

    int timeout = 2000;

    QEventLoop loop;
    QTimer loopTimer;
    QElapsedTimer elapsedTimer;

    //loopTimer.setSingleShot(true);
    connect(&loopTimer, SIGNAL(timeout()), &loop, SLOT(quit()));
    connect(this, SIGNAL(packetProcessed()), &loop, SLOT(quit()));

    elapsedTimer.start();

    while(m_frameAck.type == Qk::Ack::NACK && !elapsedTimer.hasExpired(timeout))
    {
        loopTimer.start(100);
        loop.exec();
    }

//    while(ack.code == Qk::Ack::NACK && !elapsedTimer.hasExpired(timeout))
//    {
//        loopTimer.start(100);
//        loop.exec();

//        if(m_acks.count() > 0)
//        {
//            if(m_acks.count() > 1)
//                qDebug() << "OOOOPS" << "m_acks.count()" << m_acks.count();
//            ack = m_acks.takeLast();
//            //ack = m_acks.last();
//            qDebug() << "  acknowledged!" << QString().sprintf("id:%d type:%d code:%d", ack.id
//                                                               , ack.type
//                                                               , ack.code);
//            break;
//        }
//    }
    qDebug() << "  elapsed time:" << elapsedTimer.elapsed();
    loopTimer.stop();

    if(elapsedTimer.hasExpired(timeout))
    {
        qDebug() << "WAITFORACK TIMEOUT";
        error(QK_ERR_COMM_TIMEOUT, timeout);
    }

    return m_frameAck;
}

Qk::Ack QkCore::_comm_wait(int packetID, int timeout)
{
//    if(timeout == 0)
//        return m_protocol.ack;
    timeout = 2000;

//    qDebug() << __FUNCTION__;

    QEventLoop loop;
    QTimer loopTimer;
    QElapsedTimer elapsedTimer;
    int interval = timeout/4;

    //loopTimer.setSingleShot(true);
    connect(&loopTimer, SIGNAL(timeout()), &loop, SLOT(quit()));
    connect(this, SIGNAL(packetProcessed()), &loop, SLOT(quit()));

    elapsedTimer.start();

    //while(!elapsedTimer.hasExpired(timeout) && m_protocol.ack.type == Qk::Ack::NACK)
    Qk::Ack ack;
    ack.type = Qk::Ack::NACK;

//   ack.code = Qk::Ack::OK;
//   return ack;

    bool nack = true;
//    while(nack && !elapsedTimer.hasExpired(timeout))
    while(ack.type == Qk::Ack::NACK && !elapsedTimer.hasExpired(timeout))
    {
        loopTimer.start(timeout);
        loop.exec();
        //loop.exec(QEventLoop::ExcludeUserInputEvents);
        //loop.processEvents(QEventLoop::ExcludeUserInputEvents);
        qDebug() << "check ACKs for id..." << packetID;
        foreach(const Qk::Ack &receivedAck, m_acks)
        {
            qDebug() << "  ack" << receivedAck.id;
            if(receivedAck.id == packetID)
            {
                ack = receivedAck;
                qDebug() << "found!" << "type" << ack.type;
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
        qDebug() << "acks ="  << m_acks.count();
        foreach(const Qk::Ack &receivedAck, m_acks)
            qDebug() << "ack packetID" << receivedAck.id;
        error(QK_ERR_COMM_TIMEOUT, timeout);
    }

    m_acks.removeOne(ack);


    return ack;
}

bool Qk::PacketBuilder::build(Packet *packet, const PacketDescriptor &desc, QkBoard *board)
{
    //if(!validate(pd))
        //return false;

    int i_data;
    QkDevice *device = 0;
    using namespace Utils;

    packet->flags.ctrl = 0;
    packet->address = desc.address;
    packet->id = Qk::Packet::requestId();
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
            qDebug() << "config value HEX:" << configValue.toInt();
            fillValue(configValue.toUInt(), 4, &i_data, packet->data);
            break;
        case QkBoard::Config::ctFloat:
            qDebug() << "config value FLOAT:" << configValue.toFloat() << "bytes:" << Utils::bytesFromFloat(configValue.toFloat());
            fillValue(Utils::bytesFromFloat(configValue.toFloat()), 4, &i_data, packet->data);
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

bool Qk::PacketBuilder::validate(PacketDescriptor *pd)
{
    bool ok = false;

    switch(pd->code)
    {
    default: ok = true;
    }

    //if(!ok)
        //qDebug() << "Qk::PacketBuilder::validate() BAD PACKET, code =" << Qk::Packet::codeFriendlyName(pd->code);

    return ok;
}

void Qk::PacketBuilder::parse(const Qk::Frame &frame, Packet *packet)
{
    int i_data = 0;

    using namespace Utils;

    QByteArray data = frame.data;

    calculate_hdr_length(packet);

    packet->checksum = (int) data.at(data.length() - 1);

    packet->flags.ctrl = getValue(2, &i_data, data);
    packet->address = 0;
    packet->timestamp = frame.timestamp;

    packet->code = getValue(1, &i_data, data);

    packet->data.clear();
    packet->data.append(data.right(data.count() - i_data));
    packet->data.truncate(packet->data.count() - 1); // remove checksum byte
}

void Utils::fillValue(int value, int count, int *idx, QByteArray &data)
{
    int i, j = *idx;
    for(i = 0; i < count; i++, j++)
    {
        data.insert(j, (value >> 8*i) & 0xFF);
    }
    *idx = j;
}

void Utils::fillString(const QString &str, int count, int *idx, QByteArray &data)
{
    QString justifiedStr = str.leftJustified(count, '\0', true);
    fillString(justifiedStr, idx, data);
}

void Utils::fillString(const QString &str, int *idx, QByteArray &data)
{
    int j = *idx;
    data.insert(j, str);
    j += str.length()+1;
}

int Utils::getValue(int count, int *idx, const QByteArray &data, bool sigExt)
{   
    int j, value = 0;
    int i = *idx;

    for(j = 0; j < count; j++, i++)
    {
        if(i < data.count())
            value += (data.at(i) & 0xFF) << (8*j); // little endian
    }

    switch(count) // truncate and extend sign bit
    {
    case 1:
        value &= 0xFF;
        if(sigExt & ((value & 0x80) > 0))
            value |= 0xFFFFFF00;
        break;
    case 2:
        value &= 0xFFFF;
        if(sigExt & ((value & 0x8000) > 0))
            value |= 0xFFFF0000;
        break;
    case 4:
        value &= 0xFFFFFFFF;
        break;
    }

    *idx = i;
    return value;
}

QString Utils::getString(int *idx, const QByteArray &data)
{
    int j;
    char c, buf[1024];
    int i = *idx;

    memset(buf, '\0', sizeof(buf));

    for(j=0; j < 1024; j++)
    {
        if(j+1 < data.length())
        {
            c = (char)((quint8)data[i++]);
            if(c == '\0')
                break;
            if((c < 32 || c > 126))
                c = ' ';
            buf[j] = c;
        }
        else
            break;
    }

    *idx = i;
    return QString(buf);
}

QString Utils::getString(int count, int *idx, const QByteArray &data)
{
    int j;
    char c, buf[count+1];
    int i = *idx;

    memset(buf, '\0', sizeof(buf));

    for(j=0; j < count; j++)
    {
        if(j+1 < data.length())
        {
            c = (char)((quint8)data[i++]);
            if((c < 32 || c > 126) && c != '\0')
                c = ' ';
            buf[j] = c;
            //qDebug() << "char:" << j << QString().sprintf("%02X",c) << c << "\n";
        }
        else
            break;
    }
    buf[count] = '\0';
    *idx = i;
    //qDebug() << "buf" << buf;
    return QString(buf);
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

int Qk::Packet::source()
{
    return (QkBoard::Type) ((flags.ctrl >> 4) & 0x07);
}

QString Qk::Packet::codeFriendlyName()
{
    switch((quint8)code)
    {
    case QK_PACKET_CODE_ACK:
        return "ACK";
    case QK_PACKET_CODE_SEARCH:
        return "SEARCH";
    case QK_PACKET_CODE_SEQBEGIN:
        return "SEQBEGIN";
    case QK_PACKET_CODE_SEQEND:
        return "SEQEND";
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

float Utils::floatFromBytes(int value)
{
    Utils::_IntFloatConverter converter;
    converter.i_value = value;
    return converter.f_value;
}

int Utils::bytesFromFloat(float value)
{
    Utils::_IntFloatConverter converter;
    converter.f_value = value;

//    uint8_t tmp[4];
//    for(int i=0; i < 2; i++)
//        tmp[3-i] = converter.bytes[i];
//    for(int i=0; i < 4; i++)
//        converter.bytes[i] = tmp[i];

    return converter.i_value;
}

static void calculate_hdr_length(Qk::Packet *packet)
{

    packet->headerLength = 2+1; // flags(2) + code(1)
    packet->headerLength = SIZE_FLAGS_CTRL + SIZE_CODE;

    if(!(packet->flags.ctrl & QK_PACKET_FLAGMASK_CTRL_NOTIF))
      packet->headerLength += SIZE_ID;

    if(packet->flags.ctrl & QK_PACKET_FLAGMASK_CTRL_ADDRESS)
    {
      packet->headerLength += SIZE_FLAGS_NETWORK;
    }
}



