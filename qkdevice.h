#ifndef QKDEVICE_H
#define QKDEVICE_H

#include <QObject>
#include <QVector>
#include <QQueue>
#include <QVariant>
#include "qkboard.h"

class QKLIBSHARED_EXPORT QkDevice : public QkBoard
{
    Q_OBJECT
    Q_ENUMS(SamplingMode TriggerClock)
public:
    enum Info
    {
        diSampling = ((1<<0) << 3),
        diData = ((1<<1) << 3),
        diEvent = ((1<<2) << 3),
        diAction = ((1<<3) << 3)
    };
    enum SamplingMode
    {
        smSingle,
        smContinuous,
        smTriggered
    };
    enum TriggerClock
    {
        tc1Sec,
        tc10Sec,
        tc1Min,
        tc10Min,
        tc1Hour
    };
    class SamplingInfo
    {
    public:
        SamplingInfo()
        {
            mode = smContinuous;
            frequency = 1;
            triggerClock = tc10Sec;
            triggerScaler = 1;
            N = 5;
        }
        SamplingMode mode;
        int frequency;
        TriggerClock triggerClock;
        int triggerScaler;
        int N;
    };
    class QKLIBSHARED_EXPORT Data
    {
    public:
        enum Type
        {
            dtInt,
            dtFloat
        };
        Data();
        void _setLabel(const QString &label);
        void _setValue(float value, quint64 timestamp = 0);
        QString label();
        float value();
        quint64 timestamp();
    private:
        QString m_label;
        float m_value;
        quint64 m_timestamp;
    };

    class QKLIBSHARED_EXPORT Event {
    public:
        void _setLabel(const QString &label);
        void _setArgs(QList<float> args);
        void _setMessage(const QString &msg);
        QString label();
        QList<float> args();
        QString message();
    private:
        QString m_label;
        QList<float> m_args;
        QString m_text;
    };

    class QKLIBSHARED_EXPORT Action {
    public:
        enum Type {
            atBool,
            atInt,
            atText
        };
        void _setId(int id);
        void _setLabel(const QString &label);
        void _setType(Type type);
        void _setValue(QVariant value);
        int id();
        QString label();
        Type type();
        QVariant value();
    private:
        int m_id;
        QString m_label;
        Type m_type;
        QVariant m_value;
    };

    QkDevice(QkCore *qk, QkNode *parentNode);

    static QString samplingModeString(SamplingMode mode);
    static QString triggerClockString(TriggerClock clock);

    QkAck update();

    void setSamplingInfo(const SamplingInfo &info);
    void setSamplingFrequency(int freq);
    void setSamplingMode(QkDevice::SamplingMode mode);
    void _setSamplingInfo(SamplingInfo info);
    void _setData(QVector<Data> data);
    void _setDataType(Data::Type type);
    void _setDataValue(int idx, float value, quint64 timestamp = 0);
    void _setDataLabel(int idx, const QString &label);
    void _setActions(QVector<Action> actions);
    void _setEvents(QVector<Event> events);
    void _logEvent(const QkDevice::Event &event);

    QQueue<QkDevice::Event>* eventsFired();

    SamplingInfo samplingInfo();
    Data::Type dataType();
    QVector<Data> data();
    QVector<Action> actions();
    QVector<Event> events();

    int actuate(unsigned int id, QVariant value);

protected:
    void setup();

private:
    enum {
        _maxEventsLogged = 100
    };
    SamplingInfo m_samplingInfo;
    QVector<Data> m_data;
    QVector<Action> m_actions;
    QVector<Event> m_events;
    Data::Type m_dataType;

    //TODO queue for data
    QQueue<QkDevice::Event> m_eventsFired;

};

Q_DECLARE_METATYPE(QkDevice::Event) //TODO remove this

#endif // QKDEVICE_H
