#ifndef QK_H
#define QK_H

#include "qklib_global.h"
#include "qk_comm.h"
#include "qk_defs.h"

#include <QObject>
#include <QVector>
#include <QVariant>
#include <QDebug>

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
    int getValue(int count, int *idx, const QByteArray &data, bool sigExt = false);
    QString getString(int *idx, const QByteArray &data);
    QString getString(int count, int *idx, const QByteArray &data);
}

using namespace Qk;

class QkCore;
class QkNode;

class Qk::Info
{
public:
    int version_major;
    int version_minor;
    int version_patch;
    int baudRate;
    int flags;
};

class QkBoard : public QObject
{
    Q_OBJECT
public:
    enum BoardType {
        btHost,
        btModule,
        btDevice,
        btNetwork,
        btGateway
    };
    enum ConfigType {
        ctIntDec,
        ctIntHex,
        ctFloat,
        ctBool,
        ctCombo,
        ctTime,
        ctDate,
        ctDateTime
    };
    class Config
    {
    public:
        void _set(const QString label, ConfigType type, QVariant value, double min = 0, double max = 0);
        bool set(QVariant value, double min = 0, double max = 0);
    private:
        QString m_label;
        ConfigType m_type;
        QVariant m_value;
        double m_min, m_max;
    };

    QkBoard(QkCore *qk);

    void _setAddress(int address);
    void _setName(const QString &name);
    void _setFirmwareVersion(int version);
    void _setQkInfo(const Qk::Info &qkInfo);
    void _setConfigs(QVector<Config> configs);
    void update();
    void save();

    QString name();
    int firmwareVersion();
    Qk::Info qkInfo();
    QVector<Config> configs();

protected:
    BoardType m_type;
    QkNode *m_parentNode;

private:
    QString m_name;
    int m_fwVersion;
    Qk::Info m_qkInfo;
    QVector<Config> m_configs;
    QkCore *m_qk;
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

class QkDevice : public QkBoard {
    Q_OBJECT
public:
    enum SamplingMode {
        smContinuous,
        smTriggered
    };
    enum TriggerClock {
        tcSingle,
        tc1Sec,
        tc10Sec,
        tc1Min,
        tc10Min,
        tc1Hour
    };
    class SamplingInfo {
    public:
        SamplingMode mode;
        int frequency;
        TriggerClock triggerClock;
        int triggerScaler;
        int N;
    };
    class Data {
    public:
        Data(const QString &label = QString(), double value = 0.0);
        void _setLabel(const QString &label);
        void _setValue(double value);
        QString label();
        double value();
    private:
        QString m_label;
        double m_value;
    };
    enum ActionType {
        atBool,
        atNumber,
        atText
    };
    class Action {
    public:
        Action(const QString &label = QString());
        void _setLabel(const QString &label);
        void _setType(ActionType type);
        void _setArgs(QList<int> args);
        void _setMessage(const QString &msg);
        QString label();
        ActionType type();
        QList<int> args();
        QString text();
    private:
        QString m_label;
        ActionType m_type;
        QList<int> m_args;
        QString  m_text;
    };

    class Event {
    public:
        Event(const QString &label = QString());
        void _setLabel(const QString &label);
        void _setArgs(QList<int> args);
        void _setMessage(const QString &msg);
        QString label();
        QList<int> args();
        QString text();
    private:
        QString m_label;
        QList<int> m_args;
        QString m_text;
    };

    QkDevice(QkCore *qk, QkNode *parentNode);

    static SamplingMode getSamplingModeEnum(const QString &name);
    static TriggerClock getTriggerClockEnum(const QString &name);

    void setSamplingInfo(const SamplingInfo &info);
    void setSamplingFrequency(int freq);
    void _setData(QVector<Data> data);
    void _setEvents(QVector<Event> events);

    SamplingInfo getSamplingInfo();
    int getSamplingFrequency();

protected:
    void setup();

private:
    SamplingInfo m_samplingInfo;
    QVector<Data> m_data;
    QVector<Event> m_events;

};

class QkNode {
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
    Ack ack;
};

class QKLIBSHARED_EXPORT Qk::Packet
{
public:
    int address;
    int flags;
    int code;
    QByteArray data;
    int checksum;
    int headerLength;

    QString codeFriendlyName();
    QkBoard::BoardType source();
};

/*class Qk::Utils
{
public:
    static int getValue(int count, int *idx, const QByteArray &data);
    static QString getString(int count, int *idx, const QByteArray &data);
};*/

class QKLIBSHARED_EXPORT Qk::PacketBuilder {
public:
    static bool build(Packet *p, PacketDescriptor *pd);
    static bool validate(PacketDescriptor *pd);
    static void parse(const QByteArray &frame, Packet *packet);
};


class QKLIBSHARED_EXPORT QkCore : public QObject
{
    Q_OBJECT
public:

    QkCore();
    ~QkCore();

    static QString version();
    static QString errorMessage(int errCode);

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

    void setCommTimeout(int timeout);

    void comm_sendPacket(Packet *p);

signals:
    void comm_sendFrame(QByteArray frame);
    void gatewayDetected(int address);
    void networkDetected(int address);
    void moduleDetected(int address);
    void deviceDetected(int address);

    void dataReceived(int address);
    void eventReceived(int address, QkDevice::Event *e);
    void debugString(int address, QString str);
    void error(int errCode);

public slots:
    /**** API ***************************************/
    int search();
    int start(int address = 0);
    int stop(int address = 0);
    /************************************************/
    void comm_processFrame(QByteArray frame);

private:
    void _comm_parseFrame(QByteArray frame, Qk::Packet *packet);
    void _comm_processPacket(Packet *p);
    int _comm_wait(int timeout);

    QkGateway *m_gateway;
    QkNetwork *m_network;
    QMap<int, QkNode*> m_nodes;
    Qk::Comm m_comm;
};

#endif // QK_H
