#include "qk.h"
#include "qk_utils.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QTimer>
#include <QEventLoop>
#include <QStringList>
#include <QDateTime>
#include <QTime>
#include <QDate>

using namespace std;

static void calculate_hdr_length(Qk::Packet *packet);

QString Qk::Info::versionString()
{
    return QString().sprintf("%d.%d.%d", version_major,
                                          version_minor,
                                          version_patch);
}

QkBoard::QkBoard(QkCore *qk)
{
    m_qk = qk;
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

QString QkBoard::name()
{
    return m_name;
}

int QkBoard::firmwareVersion()
{
    return m_fwVersion;
}

void QkBoard::update()
{

}

void QkBoard::save()
{
    Qk::Packet p;
    Qk::PacketDescriptor desc;
    desc.boardType = m_type;
    if(m_parentNode != 0)
        desc.address = m_parentNode->address();
    else
        desc.address = 0;
    desc.code = QK_PACKET_CODE_SAVE;
    Qk::PacketBuilder::build(&p, desc);
    m_qk->comm_sendPacket(&p);
}

QkGateway::QkGateway(QkCore *qk) :
    QkBoard(qk)
{
    m_type = btGateway;
}

QkNetwork::QkNetwork(QkCore *qk) :
    QkBoard(qk)
{
    m_type = btNetwork;
}

QkModule::QkModule(QkCore *qk, QkNode *parentNode) :
    QkBoard(qk)
{
    m_parentNode = parentNode;
    m_type = btModule;
}

QkDevice::QkDevice(QkCore *qk, QkNode *parentNode) :
    QkBoard(qk)
{
    m_parentNode = parentNode;
    m_type = btDevice;
}

void QkDevice::_setSamplingInfo(const SamplingInfo &info)
{
    m_samplingInfo = info;
}

void QkDevice::_setData(QVector<Data> data)
{
    m_data = data;
}

void QkDevice::_setActions(QVector<Action> actions)
{
    m_actions = actions;
}

void QkDevice::_setEvents(QVector<Event> events)
{
    m_events = events;
}

QkDevice::SamplingInfo QkDevice::samplingInfo()
{
    return m_samplingInfo;
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

QkDevice::Data::Data()
{
    m_value = 0.0;
}

void QkDevice::Data::_setLabel(const QString &label)
{
    m_label = label;
}

void QkDevice::Data::_setValue(double value)
{
    m_value = value;
}

QString QkDevice::Data::label()
{
    return m_label;
}

double QkDevice::Data::value()
{
    return m_value;
}

QkNode::QkNode(QkCore *qk, int address)
{
    m_qk = qk;
    m_address = address;
    m_module = 0;
    m_device = 0;
}

void QkNode::setModule(QkModule *module)
{
    m_module = module;
}

void QkNode::setDevice(QkDevice *device)
{
    m_device = device;
}

QkModule* QkNode::module()
{
    return m_module;
}

QkDevice* QkNode::device()
{
    return m_device;
}

int QkNode::address()
{
    return m_address;
}


QkCore::QkCore()
{
    m_gateway = 0;
    m_network = 0;
    m_nodes.clear();
    m_comm.sequence = false;
    setCommTimeout(2000);
}

QkCore::~QkCore()
{

}

QkGateway* QkCore::gateway()
{
    return m_gateway;
}

QkNetwork* QkCore::network()
{
    return m_network;
}

QkNode* QkCore::node(int address)
{
    return m_nodes.value(address);
}

QMap<int, QkNode*> QkCore::nodes()
{
    return m_nodes;
}

int QkCore::search()
{
    Qk::Packet p;
    Qk::PacketDescriptor pd;
    pd.address = 0;
    pd.code = QK_PACKET_CODE_SEARCH;
    Qk::PacketBuilder::build(&p, pd);
    comm_sendPacket(&p);
    return _comm_wait(m_comm.defaultTimeout);
}

int QkCore::start(int address)
{
    Qk::Packet p;
    Qk::PacketDescriptor pd;
    pd.address = address;
    pd.code = QK_PACKET_CODE_START;
    Qk::PacketBuilder::build(&p, pd);
    comm_sendPacket(&p);
    return _comm_wait(m_comm.defaultTimeout);
}

int QkCore::stop(int address)
{
    Qk::Packet p;
    Qk::PacketDescriptor pd;
    pd.address = address;
    pd.code = QK_PACKET_CODE_STOP;
    Qk::PacketBuilder::build(&p, pd);
    comm_sendPacket(&p);
    return _comm_wait(m_comm.defaultTimeout);
}

void QkCore::setCommTimeout(int timeout)
{
    m_comm.defaultTimeout = timeout;
}

void QkCore::comm_sendPacket(Packet *p)
{
    qDebug() << "QkCore::comm_sendPacket()" << p->codeFriendlyName();
    QByteArray frame;

    if(p->address != 0) {
        p->flags |= QK_PACKET_FLAGMASK_ADDRESS;
        p->flags |= QK_PACKET_FLAGMASK_16BITADDR;
    }

    frame.append(p->flags & 0xFF);
    if(p->flags & QK_PACKET_FLAGMASK_EXTEND) {
        frame.append(p->flags >> 8);
    }

    frame.append(p->code);
    frame.append(p->data);

    m_comm.ack.code = QK_COMM_NACK;

    emit comm_sendFrame(frame);
}

void QkCore::comm_processFrame(QByteArray frame)
{
    Qk::Packet *incomingPacket = &(m_comm.incomingPacket);
    _comm_parseFrame(frame, incomingPacket);

    if(incomingPacket->flag_fragment)
    {
        m_comm.fragmentedPacket.data.append(incomingPacket->data);

        if(!incomingPacket->flag_lastFragment) //FIXME create elapsedTimer to timeout lastFragment reception
            return;
        else
        {
            incomingPacket->data = m_comm.fragmentedPacket.data;
            m_comm.fragmentedPacket.data.clear();
        }
    }
    _comm_processPacket(incomingPacket);
}

void QkCore::_comm_parseFrame(QByteArray frame, Packet *packet)
{
    Qk::PacketBuilder::parse(frame, packet);
}

void QkCore::_comm_processPacket(Packet *p)
{
    qDebug() << "QkCore::_comm_processPacket()" << p->codeFriendlyName();

    QkBoard *selBoard = 0;
    QkNode *selNode = 0;
    QkModule *selModule = 0;
    QkDevice *selDevice = 0;

    using namespace QkUtils;

    switch(p->source())
    {
    case QkBoard::btGateway:
        if(m_gateway == 0)
            m_gateway = new QkGateway(this);
        selBoard = m_gateway;
        break;
    case QkBoard::btNetwork:
        if(m_network == 0)
            m_network = new QkNetwork(this);
        selBoard = m_network;
        break;
    case QkBoard::btModule:
        selNode = node(p->address);
        if(selNode == 0)
        {
            selNode = new QkNode(this, p->address);
            m_nodes.insert(p->address, selNode);
        }
        if(selNode->module() == 0)
        {
            selNode->setModule(new QkModule(this, selNode));
        }
        selBoard = selNode->module();
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
        }
        selDevice = selNode->device();
        selBoard = (QkBoard*)(selDevice);
        break;
    default: ;
    }

    if(selBoard == 0)
    {
        qDebug() << "selBoard == 0";
        return;
    }

    int i, j, size, fwVersion, ncfg, ndat, nact, nevt;
    int year, month, day, hours, minutes, seconds;
    double min = 0.0, max = 0.0;
    QString debugStr;
    Qk::Info qkInfo;
    QVector<QkBoard::Config> configs;
    QkBoard::Config::Type configType;
    QVector<QkDevice::Data> data;
    QkDevice::Data::Type dataType;
    QString label, name;
    QVariant varValue;
    QStringList items;
    QDateTime dateTime;
    QkDevice::SamplingInfo sampInfo;

    int i_data = 0;
    switch(p->code)
    {
    case QK_PACKET_CODE_SEQBEGIN:
        m_comm.sequence = true;
        break;
    case QK_PACKET_CODE_SEQEND:
        m_comm.sequence = false;
        m_comm.ack.code = QK_COMM_ACK;
        m_comm.ack.arg = getValue(1, &i_data, p->data);
        break;
    case QK_PACKET_CODE_INFOQK:
        qkInfo.version_major = getValue(1, &i_data, p->data);
        qkInfo.version_minor = getValue(1, &i_data, p->data);
        qkInfo.version_patch = getValue(1, &i_data, p->data);
        qkInfo.baudRate = getValue(4, &i_data, p->data);
        qkInfo.flags = getValue(4, &i_data, p->data);
        selBoard->_setQkInfo(qkInfo);
        break;
    case QK_PACKET_CODE_INFOBOARD:
        fwVersion = getValue(2, &i_data, p->data);
        name = getString(QK_BOARD_NAME_SIZE, &i_data, p->data);
        selBoard->_setFirmwareVersion(fwVersion);
        selBoard->_setName(name);
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
            case QkBoard::Config::ctIntHex:
                varValue = QVariant((int) getValue(4, &i_data, p->data, true));
                min = (double) getValue(4, &i_data, p->data, true);
                max = (double) getValue(4, &i_data, p->data, true);
                break;
            case QkBoard::Config::ctFloat:
                varValue = QVariant((float) getValue(4, &i_data, p->data, true));
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
        break;
    case QK_PACKET_CODE_INFOSAMP:
        sampInfo.frequency = getValue(4, &i_data, p->data);
        sampInfo.mode = (QkDevice::SamplingMode) getValue(1, &i_data, p->data);
        sampInfo.triggerClock = (QkDevice::TriggerClock) getValue(1, &i_data, p->data);
        sampInfo.triggerScaler = getValue(1, &i_data, p->data);
        sampInfo.N = getValue(1, &i_data, p->data);
        selDevice->_setSamplingInfo(sampInfo);
        break;
    case QK_PACKET_CODE_INFODATA:
        ndat = getValue(1, &i_data, p->data);
        data = QVector<QkDevice::Data>(ndat);
        dataType = (QkDevice::Data::Type)getValue(1, &i_data, p->data);
        for(i=0; i<ndat; i++)
        {
            data[i]._setLabel(getString(QK_LABEL_SIZE, &i_data, p->data));
        }
        selDevice->_setData(data);
        break;
    case QK_PACKET_CODE_INFOEVENT:
        break;
    case QK_PACKET_CODE_INFOACTION:
        break;
    case QK_PACKET_CODE_DATA:
        qDebug() << "DATA RECEIVED";
        break;
    case QK_PACKET_CODE_STRING:
        debugStr = getString(&i_data, p->data);
        break;
    case QK_PACKET_CODE_OK: //FIXME not tested
        m_comm.ack.code = QK_COMM_OK;
        m_comm.ack.arg = getValue(1, &i_data, p->data);
        break;
    case QK_PACKET_CODE_ERR: //FIXME not tested
        m_comm.ack.code = QK_COMM_ERR;
        m_comm.ack.err = getValue(4, &i_data, p->data);
        m_comm.ack.arg = getValue(4, &i_data, p->data);
        break;
    default:
        m_comm.ack.code = QK_COMM_ERR;
        m_comm.ack.err = QK_ERR_CODE_UNKNOWN;
        //qDebug() << "Packet code not recognized:" << QString().sprintf("%02X", p->code);
    }

    switch(p->code)
    {
    case QK_PACKET_CODE_SEQBEGIN:
    case QK_PACKET_CODE_SEQEND:
    case QK_PACKET_CODE_OK:
    case QK_PACKET_CODE_ERR:
        break;
    default:
        if(!m_comm.sequence && m_comm.ack.err != QK_ERR_CODE_UNKNOWN)
        {
            m_comm.ack.code = QK_COMM_ACK;
            m_comm.ack.arg = p->code;
        }
    }

    switch(p->code)
    {
    case QK_PACKET_CODE_SEQEND:
        qDebug() << "m_comm.ack.arg" << m_comm.ack.arg;
        switch(m_comm.ack.arg)
        {
        case QK_PACKET_CODE_GETGATEWAY:
            emit gatewayDetected();
            break;
        case QK_PACKET_CODE_GETNETWORK:
            emit networkDetected();
            break;
        case QK_PACKET_CODE_GETMODULE:
            qDebug() << "QkModule detected:" << QString().sprintf("%04X",selNode->address());
            emit moduleDetected(selNode->address());
            break;
        case QK_PACKET_CODE_GETDEVICE:
            qDebug() << "QkDevice detected:" << QString().sprintf("%04X",selNode->address());
            emit deviceDetected(selNode->address());
            break;
        }
        break;
    case QK_PACKET_CODE_DATA:
        emit dataReceived(selNode->address());
        break;
    case QK_PACKET_CODE_EVENT:
        //emit eventReceived(selNode->address());
        break;
    case QK_PACKET_CODE_STRING:
        qDebug() << "emit debugString()";
        emit debugString(selNode->address(), debugStr);
        break;
    case QK_PACKET_CODE_ERR:
        emit error(m_comm.ack.err);
        break;
    default: ;
    }

    emit packetProcessed();

}

int QkCore::_comm_wait(int timeout)
{
    if(timeout == 0)
        return QK_COMM_NACK;

    qDebug() << "_comm_wait() timeout =" << timeout;

    QEventLoop loop;
    QTimer loopTimer;
    QElapsedTimer elapsedTimer;
    int interval = timeout/2;

    //loopTimer.setSingleShot(true);
    connect(&loopTimer, SIGNAL(timeout()), &loop, SLOT(quit()));
    connect(this, SIGNAL(packetProcessed()), &loop, SLOT(quit()));

    loopTimer.start(interval);
    elapsedTimer.start();

    while(!elapsedTimer.hasExpired(timeout) && m_comm.ack.code == QK_COMM_NACK)
    {
        loop.exec(QEventLoop::ExcludeUserInputEvents);
        //loop.processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    loopTimer.stop();
    qDebug() << "elapsed time =" << elapsedTimer.elapsed();

    if(elapsedTimer.hasExpired(timeout))
        error(QK_ERR_COMM_TIMEOUT);

    return m_comm.ack.code;
}

bool Qk::PacketBuilder::build(Packet *packet, const PacketDescriptor &desc)
{
    //if(!validate(pd))
      //  return false;

    packet->flags = 0;
    packet->address = desc.address;
    packet->code = desc.code;
    packet->data.clear();
    return true;
}

bool Qk::PacketBuilder::validate(PacketDescriptor *pd)
{
    bool ok = false;

    switch(pd->code)
    {
    default: ok = true;
    }
    return ok;
}

void Qk::PacketBuilder::parse(const QByteArray &frame, Packet *packet)
{
    int i_data = 0;

    using namespace QkUtils;

    calculate_hdr_length(packet);

    packet->checksum = (int) frame.at(frame.length() - 1);

    packet->flags = getValue(1, &i_data, frame);

    packet->flag_extend = flag_bool(packet->flags, QK_PACKET_FLAGMASK_EXTEND); //FIXME remote these flag_
    packet->flag_fragment = flag_bool(packet->flags, QK_PACKET_FLAGMASK_FRAG);
    packet->flag_lastFragment = flag_bool(packet->flags, QK_PACKET_FLAGMASK_LASTFRAG);
    packet->flag_address = flag_bool(packet->flags, QK_PACKET_FLAGMASK_ADDRESS);
    packet->flag_16bitaddr = flag_bool(packet->flags, QK_PACKET_FLAGMASK_16BITADDR);

    if(packet->flag_extend)
    {
        packet->flags += (getValue(1, &i_data, frame) << 8);
    }

    if(packet->flag_address)
    {
        if(packet->flag_16bitaddr)
        {
            packet->address = getValue(2, &i_data, frame);
        }
        else
        {
            //TODO not supported yet
            packet->address = getValue(8, &i_data, frame);
        }
    }
    else
        packet->address = 0;

    packet->code = getValue(1, &i_data, frame);

    packet->data.clear();
    packet->data.append(frame.right(frame.count() - i_data));
    packet->data.truncate(packet->data.count() - 1); // remove checksum byte
}

int QkUtils::getValue(int count, int *idx, const QByteArray &data, bool sigExt)
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

QString QkUtils::getString(int *idx, const QByteArray &data)
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

QString QkUtils::getString(int count, int *idx, const QByteArray &data)
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
        return tr("(error code unknown)"); break;
    }
}

QkBoard::Type Qk::Packet::source()
{
    return (QkBoard::Type) ((flags >> 4) & 0x07);
}

QString Qk::Packet::codeFriendlyName()
{
    switch((quint8)code)
    {
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
    case QK_PACKET_CODE_GETNETWORK:
        return "GET_NETWORK";
    case QK_PACKET_CODE_INFOQK:
        return "INFO_QK";
    case QK_PACKET_CODE_INFOSAMP:
        return "INFO_SAMP";
    case QK_PACKET_CODE_INFOBOARD:
        return "INFO_BOARD";
    case QK_PACKET_CODE_INFOMODULE:
        return "INFO_MODULE";
    case QK_PACKET_CODE_INFODEVICE:
        return "INFO_DEVICE";
    case QK_PACKET_CODE_INFONETWORK:
        return "INFO_NETWORK";
    case QK_PACKET_CODE_INFOGATEWAY:
        return "INFO_GATEWAY";
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

static void calculate_hdr_length(Qk::Packet *packet) {
    int extend, frag, addr, addr16bit;

    extend = flag(packet->flags, QK_PACKET_FLAGMASK_EXTEND);
    frag = flag(packet->flags, QK_PACKET_FLAGMASK_FRAG);
    addr = flag(packet->flags, QK_PACKET_FLAGMASK_ADDRESS);
    addr16bit = flag(packet->flags, QK_PACKET_FLAGMASK_16BITADDR);

    if(extend || frag)
      packet->headerLength = 3; // flags(2) + code(1)
    else
      packet->headerLength = 2; // flags(1) + code(1)

    if(addr) {
      if(addr16bit)
        packet->headerLength += 2; // 16bit address
      else
        packet->headerLength += 8; // 64bit address
    }
}

