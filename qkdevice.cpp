#include "qkdevice.h"
#include "qkcore.h"

#include <QDebug>
#include <QMetaEnum>

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

QkAck QkDevice::update()
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

void QkDevice::_logEvent(const QkDevice::Event &event)
{
    m_eventsFired.append(event);
    while(m_eventsFired.count() > _maxEventsLogged)
        m_eventsFired.removeFirst();
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

int QkDevice::actuate(int id, QVariant value)
{
    if(id >= m_actions.count())
        return -1;

    m_actions[id]._setValue(value);

    QkPacket packet;
    QkPacket::Descriptor descriptor;

    descriptor.boardType = m_type;
    descriptor.code = QK_PACKET_CODE_ACTUATE;
    descriptor.action_id = id;

    QkPacket::Builder::build(&packet, descriptor, this);
    QkAck ack = m_qk->protocol()->sendPacket(&packet, true);
    if(ack.type != QkAck::OK)
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
