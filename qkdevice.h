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

    typedef QVector<Data> DataArray;
    typedef QQueue<DataArray> DataLog;
    typedef QVector<Event> EventArray;
    typedef QQueue<Event> EventLog;
    typedef QVector<Action> ActionArray;

    QkDevice(QkCore *qk, QkNode *parentNode);

    static QString samplingModeString(SamplingMode mode);
    static QString triggerClockString(TriggerClock clock);

    int update();

    void setSamplingInfo(const SamplingInfo &info);
    void setSamplingFrequency(int freq);
    void setSamplingMode(SamplingMode mode);
    void _setSamplingInfo(SamplingInfo info);
    void _setData(DataArray data);
    void _setDataType(Data::Type type);
    void _setDataValue(int idx, float value, quint64 timestamp = 0);
    void _setDataLabel(int idx, const QString &label);
    void _logData(const DataArray &data);
    void _setActions(ActionArray actions);
    void _setEvents(EventArray events);
    void _logEvent(const Event &event);

    QQueue<QkDevice::Data> dataLog();
    QQueue<QkDevice::Event> eventLog();

    SamplingInfo samplingInfo();
    Data::Type dataType();
    DataArray data();
    ActionArray actions();
    EventArray events();

    int actuate(int id, QVariant value);

protected:
    void setup();

private:
    enum
    {
        _dataLogMax = 128,
        _eventLogMax = 128
    };
    SamplingInfo m_samplingInfo;
    DataArray m_data;
    ActionArray m_actions;
    EventArray m_events;
    Data::Type m_dataType;

    DataLog m_dataLog;
    EventLog m_eventLog;

};

Q_DECLARE_METATYPE(QkDevice::DataArray)
Q_DECLARE_METATYPE(QkDevice::Event)

#endif // QKDEVICE_H
