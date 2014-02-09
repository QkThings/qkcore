#ifndef QK_H
#define QK_H

#include "qklib_global.h"
#include "qk_defs.h"
#include "qk_comm.h"

#include <QDebug>
#include <QObject>
#include <QVector>
#include <QVariant>
#include <QMutex>

namespace Qk
{
    class Packet;
    class Ack;
    class Comm;
    class PacketBuilder;
    class Info;
}

namespace QkUtils
{
    union _IntFloatConverter {
        float f_value;
        int i_value;
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
class QkModule;

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

    /*bool flag_extend;
    bool flag_fragment;
    bool flag_lastFragment;
    bool flag_address;
    bool flag_16bitaddr;*/

    QString codeFriendlyName();
    int source();
};

class Qk::Comm
{
public:
    class Ack {
    public:
        int code;
        int arg;
        int err;
    };
    int defaultTimeout;
    bool sequence;
    Qk::Packet fragmentedPacket;
    Qk::Packet incomingPacket;
    Ack ack;
};

class QKLIBSHARED_EXPORT Qk::PacketBuilder {
public:
    static bool build(Packet *packet, const PacketDescriptor &desc, QkBoard *board = 0);
    static bool validate(PacketDescriptor *pd);
    static void parse(const QByteArray &frame, Packet *packet);
};

class QKLIBSHARED_EXPORT QkBoard : public QObject
{
    Q_OBJECT
public:
    enum Type {
        btHost,
        btModule,
        btDevice,
        btNetwork,
        btGateway
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
    Comm::Ack save();
    Comm::Ack update();

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

class QkGateway : public QkBoard {
    Q_OBJECT
public:
    QkGateway(QkCore *qk);

protected:

};

class QkNetwork : public QkBoard {
    Q_OBJECT
public:
    QkNetwork(QkCore *qk);

protected:
    void setup();
};

class QkModule : public QkBoard {
    Q_OBJECT
public:
    QkModule(QkCore *qk, QkNode *parentNode);

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
        void _setValue(float value);
        QString label();
        float value();
    private:
        QString m_label;
        float m_value;
    };
    enum ActionType {
        atBool,
        atNumber,
        atText
    };
    class QKLIBSHARED_EXPORT Action {
    public:
        Action(const QString &label = QString());
        void _setLabel(const QString &label);
        void _setType(ActionType type);
        void _setArgs(QList<float> args);
        void _setMessage(const QString &msg);
        QString label();
        ActionType type();
        QList<float> args();
        QString text();
    private:
        QString m_label;
        ActionType m_type;
        QList<float> m_args;
        QString  m_text;
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

    QkDevice(QkCore *qk, QkNode *parentNode);

    //static SamplingMode samplingModeEnum(const QString &name);
    //static TriggerClock triggerClockEnum(const QString &name);
    static QString samplingModeString(SamplingMode mode);
    static QString triggerClockString(TriggerClock clock);

    Comm::Ack update();

    void setSamplingInfo(const SamplingInfo &info);
    void setSamplingFrequency(int freq);
    void setSamplingMode(QkDevice::SamplingMode mode);
    void _setSamplingInfo(SamplingInfo info);
    void _setData(QVector<Data> data);
    void _setDataType(Data::Type type);
    void _setDataValue(int idx, float value);
    void _setDataLabel(int idx, const QString &label);
    void _setActions(QVector<Action> actions);
    void _setEvents(QVector<Event> events);

    SamplingInfo samplingInfo();
    Data::Type dataType();
    QVector<Data> data();
    QVector<Action> actions();
    QVector<Event> events();

protected:
    void setup();

private:
    SamplingInfo m_samplingInfo;
    QVector<Data> m_data;
    QVector<Action> m_actions;
    QVector<Event> m_events;
    Data::Type m_dataType;

};

Q_DECLARE_METATYPE(QkDevice::Event)

class QKLIBSHARED_EXPORT QkNode
{
public:
    QkNode(QkCore *qk, int address);

    QkModule *module();
    QkDevice *device();
    void setModule(QkModule *module);
    void setDevice(QkDevice *device);

    int address();
private:
    QkCore *m_qk;
    int m_address;
    QkModule *m_module;
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
    QkNetwork* network();
    QkGateway* gateway();
    /************************************************/

    void _updateNode(int address = 0);
    void _updateNetwork();
    void _updateGateway();

    void _saveNode(int address = 0);
    void _saveNetwork();
    void _saveGateway();

    bool isRunning();

    void setCommTimeout(int timeout);

    Comm::Ack comm_sendPacket(Packet *p, bool wait = false);

signals:
    void comm_sendFrame(QByteArray frame);
    void packetProcessed();
    void gatewayFound();
    void networkFound();
    void moduleFound(int address);
    void deviceFound(int address);
    void deviceUpdated(int address);

    void infoChanged(int address, QkBoard::Type boardType, int mask);

    void dataReceived(int address);
    void eventReceived(int address, QkDevice::Event e);
    void debugString(int address, QString str);
    void error(int errCode, int errArg);
    void ack(Qk::Comm::Ack ack);

public slots:
    /**** API ***************************************/
    Comm::Ack search();
    Comm::Ack getNode(int address = 0);
    Comm::Ack start(int address = 0);
    Comm::Ack stop(int address = 0);
    /************************************************/
    void comm_processFrame(QByteArray frame);

private:
    void _comm_parseFrame(QByteArray frame, Qk::Packet *packet);
    void _comm_processPacket(Packet *p);
    Comm::Ack _comm_wait(int timeout);

    bool m_running;
    QkGateway *m_gateway;
    QkNetwork *m_network;
    QMap<int, QkNode*> m_nodes;
    Qk::Comm m_comm;
    QMutex m_mutex;
};

#endif // QK_H
