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

#include "qkdevice.h"
#include "qkcore.h"

#include <QDebug>
#include <QMetaEnum>

QkDevice::QkDevice(QkCore *qk, QkNode *parentNode) :
    QkBoard(qk)
{
    qRegisterMetaType<QkDevice::DataArray>();
    qRegisterMetaType<QkDevice::Event>();

    m_parentNode = parentNode;
    m_type = btDevice;
    m_events.clear();
    m_data.clear();
    m_actions.clear();
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

int QkDevice::update()
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
    m_data[idx]._setLabel(label);
}

void QkDevice::_logData(const DataArray &data)
{
    m_dataLog.append(data);
    while(m_dataLog.count() > _dataLogMax)
        m_dataLog.removeFirst();
}

void QkDevice::_setActions(ActionArray actions)
{
    m_actions = actions;
}

void QkDevice::_setEvents(QVector<Event> events)
{
    m_events = events;
}

void QkDevice::_logEvent(const QkDevice::Event &event)
{
    m_eventLog.append(event);
    while(m_eventLog.count() > _eventLogMax)
        m_eventLog.removeFirst();
}

QQueue<QkDevice::Event> QkDevice::eventLog()
{
    return m_eventLog;
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

QkDevice::ActionArray QkDevice::actions()
{
    return m_actions;
}

QVector<QkDevice::Event> QkDevice::events()
{
    return m_events;
}

int QkDevice::actuate(int id, QVariant value)
{
    qDebug() << __FUNCTION__;
    if(id >= m_actions.count())
        return -1;

    m_actions[id]._setValue(value);

    QkPacket packet;
    QkPacket::Descriptor desc;

    desc.boardType = m_type;
    desc.board = this;

    desc.code = QK_PACKET_CODE_ACTUATE;
    desc.action_id = id;

    QkAck ack = m_qk->protocol()->sendPacket(desc);
    if(ack.result != QkAck::OK)
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
