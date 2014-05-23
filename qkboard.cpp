#include "qkboard.h"
#include "qkcore.h"
#include "qknode.h"

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

void QkBoard::_setQkInfo(const QkInfo &qkInfo)
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

QkInfo QkBoard::qkInfo()
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

QkAck QkBoard::update()
{
    int i;
    QkAck ack;
    QkPacket::Descriptor pd;

    pd.board = this;
    pd.boardType = m_type;
    pd.address = address();

    pd.code = QK_PACKET_CODE_SETNAME;
    pd.setname_str = name();
    ack = m_qk->protocol()->sendPacket(pd);
    if(ack.result != QkAck::OK)
    {
        qDebug() << "failed to set name" << ack.result;
        return ack;
    }

    pd.code = QK_PACKET_CODE_SETCONFIG;
    for(i = 0; i < configs().count(); i++)
    {
        pd.setconfig_idx = i;
        ack = m_qk->protocol()->sendPacket(pd);
        if(ack.result != QkAck::OK)
            return ack;
    }

    pd.code = QK_PACKET_CODE_SETSAMP;
    ack = m_qk->protocol()->sendPacket(pd);
    if(ack.result != QkAck::OK)
        return ack;

    return ack;
}

QkAck QkBoard::save()
{
    QkPacket::Descriptor pd;
    pd.boardType = m_type;
    pd.address = address();
    pd.code = QK_PACKET_CODE_SAVE;
    return m_qk->protocol()->sendPacket(pd, false);
}
