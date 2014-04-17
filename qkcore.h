#ifndef QK_H
#define QK_H

#include "qklib_global.h"
#include "qk_defs.h"
#include "qk_comm.h"
#include "stdint.h"

#include <QDebug>
#include <QObject>
#include <QVector>
#include <QVariant>
#include <QMutex>
#include <QQueue>

namespace Qk
{
    class Frame;
    class Packet;
    class Ack;
    class Protocol;
    class PacketBuilder;
    class Info;
}

namespace Utils
{
    union _IntFloatConverter {
        float f_value;
        int i_value;
        uint8_t bytes[4];
    };
    void fillValue(int value, int count, int *idx, QByteArray &data);
    void fillString(const QString &str, int count, int *idx, QByteArray &data);
    void fillString(const QString &str, int *idx, QByteArray &data);
    int getValue(int count, int *idx, const QByteArray &data, bool sigExt = false);
    QString getString(int *idx, const QByteArray &data);
    QString getString(int count, int *idx, const QByteArray &data);
    float floatFromBytes(int value);
    int bytesFromFloat(float value);
}

using namespace Qk;

class QkCore;
class QkNode;
class QkBoard;
class QkGateway;
class QkNetwork;
class QkDevice;
class QkComm;

class QKLIBSHARED_EXPORT Qk::Info
{
public:
    Info()
    {
        version_major = 0;
        version_minor = 0;
        version_patch = 0;
        baudRate = 0;
        flags = 0;
    }

    int version_major;
    int version_minor;
    int version_patch;
    int baudRate;
    int flags;
    QString versionString();
};

class QKLIBSHARED_EXPORT Qk::Frame
{
public:
    QByteArray data;
    quint64 timestamp;
};

class QKLIBSHARED_EXPORT Qk::Packet
{
public:
    int address;
    struct {
     int ctrl;
     int network;
    } flags;
    int code;
    QByteArray data;
    int checksum;
    int headerLength;
    int id;
    quint64 timestamp;

    QString codeFriendlyName();
    int source();

    static int requestId();
private:
    static int m_nextId;
};

class Qk::Ack
{
public:
    enum Type {
        NACK,
        OK,
        ERROR
    };
    static Ack fromInt(int ack);
    int arg;
    int err;
    int id;
    int code;
    int type;
    int toInt();
    bool operator ==(const Qk::Ack &other)
    {
        return ((id == other.id && type == other.type) ? true : false);
    }
};

class Qk::Protocol
{
public:
    int timeout;
    bool sequence;
    Qk::Packet fragmentedPacket;
    Qk::Packet incomingPacket;
    Qk::Ack ack;
};


class QKLIBSHARED_EXPORT Qk::PacketBuilder {
public:
    static bool build(Packet *packet, const PacketDescriptor &desc, QkBoard *board = 0);
    static bool validate(PacketDescriptor *pd);
    static void parse(const Frame &frame, Packet *packet);
};

class QKLIBSHARED_EXPORT QkBoard : public QObject
{
    Q_OBJECT
public:
    enum Type {
        btComm = 1,
        btDevice = 2
    };
    enum Info
    {
        biQk = (1<<0),
        biBoard = (1<<1),
        biConfig = (1<<2)
    };

    class QKLIBSHARED_EXPORT Config
    {
    public:
        enum Type {
            ctIntDec,
            ctIntHex,
            ctFloat,
            ctBool,
            ctCombo,
            ctTime,
            ctDateTime
        };
        void _set(const QString label, Type type, QVariant value, double min = 0, double max = 0);
        void setValue(QVariant value);
        QVariant value();
        QString label();
        Type type();
    private:
        QString m_label;
        Type m_type;
        QVariant m_value;
        double m_min, m_max;
    };

    QkBoard(QkCore *qk);

    void _setInfoMask(int mask, bool overwrite = false);

    void _setName(const QString &name);
    void _setFirmwareVersion(int version);
    void _setQkInfo(const Qk::Info &qkInfo);
    void _setConfigs(QVector<Config> configs);
    void setConfigValue(int idx, QVariant value);

    Ack save();
    Ack update();

    int address();
    QString name();
    int firmwareVersion();
    Qk::Info qkInfo();
    QVector<Config> configs();

protected:
    Type m_type;
    QkNode *m_parentNode;
    QkCore *m_qk;
private:
    QString m_name;
    int m_fwVersion;
    Qk::Info m_qkInfo;
    QVector<Config> m_configs;
    int m_filledInfoMask;
};

class QkComm : public QkBoard {
    Q_OBJECT
public:
    QkComm(QkCore *qk, QkNode *parentNode);

protected:
    void setup();

private:

};

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

    Ack update();

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
    void _appendEvent(const QkDevice::Event &event);

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
    SamplingInfo m_samplingInfo;
    QVector<Data> m_data;
    QVector<Action> m_actions;
    QVector<Event> m_events;
    Data::Type m_dataType;

    //TODO queue for data
    QQueue<QkDevice::Event> m_eventsFired;

};

Q_DECLARE_METATYPE(QkDevice::Event) //TODO remove this

class QKLIBSHARED_EXPORT QkNode
{
public:
    QkNode(QkCore *qk, int address);

    QkComm *module();
    QkDevice *device();
    void setModule(QkComm *module);
    void setDevice(QkDevice *device);

    int address();
private:
    QkCore *m_qk;
    int m_address;
    QkComm *m_module;
    QkDevice *m_device;
};


class QKLIBSHARED_EXPORT QkCore : public QObject
{
    Q_OBJECT
public:

    QkCore(QObject *parent = 0);

    static QString version();
    static QString errorMessage(int errCode);
    //static float floatFromBytes(int value);
    //static int bytesFromFloat(float value);

    /**** API ***************************************/
    QkNode* node(int address = 0);
    QMap<int, QkNode*> nodes();
    /************************************************/

    void _updateNode(int address = 0);
    void _saveNode(int address = 0);

    bool isRunning();

    void setCommTimeout(int timeout);

    Ack comm_sendPacket(Packet *packet, bool wait = false);

    Ack waitForACK(int id);

    QQueue<QByteArray>* framesToSend();

signals:
    //void comm_sendFrame(QByteArray frame); //TODO obsolete (use queue instead)
    void comm_frameReady();
    void packetProcessed();
    void moduleFound(int address);
    void deviceFound(int address);
    void deviceUpdated(int address);

    void infoChanged(int address, QkBoard::Type boardType, int mask);

    void dataReceived(int address);
    //void eventReceived(int address, QkDevice::Event e); //TODO obsolete (use queue instead)
    void eventReceived(int address);
    void debugString(int address, QString str);
    void error(int errCode, int errArg);
    void ack(Qk::Ack ack);

public slots:
    /**** API ***************************************/
    Ack search();
    Ack getNode(int address = 0);
    Ack start(int address = 0);
    Ack stop(int address = 0);
    /************************************************/
    void comm_processFrame(Qk::Frame frame);

private:
    void _comm_parseFrame(Qk::Frame frame, Qk::Packet *packet);
    void _comm_processPacket(Packet *p);
    Ack _comm_wait(int packetID, int timeout);

    bool m_running;
    QMap<int, QkNode*> m_nodes;
    Qk::Protocol m_protocol;
    QMutex m_mutex;

    QQueue<QByteArray> m_framesToSend;
    QList<Qk::Ack> m_acks;
    Qk::Ack m_frameAck;
};

#endif // QK_H
