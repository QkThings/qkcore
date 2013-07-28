#ifndef QK_H
#define QK_H

#include "qklib_global.h"
#include "qk_comm.h"

#include <QObject>
#include <QVector>
#include <QVariant>

namespace Qk
{
    class Packet;
    class Ack;
    class Comm;
    class PacketBuilder;
}

using namespace Qk;

class QkCore;

class QkBoard : public QObject
{
    Q_OBJECT
public:
    class QkInfo {
    public:
        int version_major;
        int version_minor;
        int baudRate;
        int flags;
    };
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
    class Config {
    public:
        Config(const QString &label = QString());
        void _set(const QString label, ConfigType type, QVariant value, double min = 0, double max = 0);
        bool set(QVariant value, double min = 0, double max = 0);
    private:
        QString m_label;
        ConfigType m_type;
        QVariant m_value;
        double m_min, m_max;
    };

    QkBoard(QkCore *qk, int address = 0);

    void _setAddress(int address);
    void _setName(const QString &name);
    void _setFirmwareVersion(int version);
    void _setQkInfo(const QkInfo &qkInfo);
    void _setConfigs(QList<Config> configs);
    void update();
    void save();

    QString getName();
    int getFirmwareVersion();
    QkInfo getQkInfo();
    QVector<Config> getConfigs();

private:
    QkCore *m_qk;
    int m_address;
    QString m_name;
    int m_firmwareVersion;
    QkInfo m_qkInfo;
    QVector<Config> m_configs;
};

class QkGateway : public QkBoard {
    Q_OBJECT
public:
    QkGateway(QkCore *qk);
};

class QkNetwork : public QkBoard {
    Q_OBJECT
public:
    QkNetwork(QkCore *qk);
};

class QkModule : public QkBoard {
    Q_OBJECT
public:
    QkModule(QkCore *qk);

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
        QString getLabel();
        double getValue();
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
        QString getLabel();
        ActionType getType();
        QList<int> getArgs();
        QString getText();
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
        QString getLabel();
        QList<int> getArgs();
        QString getText();
    private:
        QString m_label;
        QList<int> m_args;
        QString m_text;
    };

    QkDevice(QkCore *qk);

    static SamplingMode getSamplingModeEnum(const QString &name);
    static TriggerClock getTriggerClockEnum(const QString &name);

    void setSamplingInfo(const SamplingInfo &info);
    void setSamplingFrequency(int freq);
    void _setData(QVector<Data> data);
    void _setEvents(QVector<Event> events);

    SamplingInfo getSamplingInfo();
    int getSamplingFrequency();


private:
    SamplingInfo m_samplingInfo;
    QVector<Data> m_data;
    QVector<Event> m_events;

};

class QkNode {
public:
    QkModule *module;
    QkDevice *device;
};


class Qk::Packet {
public:
    int address;
    int flags;
    int code;
    QByteArray data;
    int checksum;
    int headerLength;
};
class Qk::Ack {
public:
    int code;
    int err;
    int arg;
};
class Qk::Comm {
public:
    QByteArray frame;
    Packet packet;
    volatile bool txdata;
    volatile bool rxdata;
    volatile bool seq;
    volatile bool resp;
    volatile bool dle;
    volatile bool valid;
    volatile int count;
    volatile Ack ack;
};
class Qk::PacketBuilder {
public:
    static bool build(Packet *p, PacketDescriptor *pd);
    static bool validate(PacketDescriptor *pd);
};


class QKLIBSHARED_EXPORT QkCore : public QObject
{
    Q_OBJECT
public:

    QkCore();
    ~QkCore();

    static QString version();
    static QString errorMessage(int errCode);
    static QString codeFriendlyName(int code);

    /**** API ***************************************/
    QkNode* getNode(int address = 0);
    QList<QkNode*> getNodes();
    QkNetwork* getNetwork();
    QkGateway* getGateway();
    /************************************************/

    void _updateNode(int address = 0);
    void _updateNetwork();
    void _updateGateway();

    void _saveNode(int address = 0);
    void _saveNetwork();
    void _saveGateway();

    void _setSendBytesCallback(void (*c)(quint8 *buf, int count));
    void _setProcessBytesCallback(void (*c)(quint8 *buf, int count));

    void comm_sendPacket(Packet *p);

signals:
    void comm_sendBytes(quint8 *buf, int count);
    void dataReceived(int address);
    void eventReceived(int address, QkDevice::Event *e);
    void debugString(int address, QString str);
    void error(int errCode);

public slots:
    /**** API ***************************************/
    void search();
    void start();
    void stop();
    /************************************************/
    void comm_processBytes(quint8 *buf, int count);

private:
    void _comm_processPacket(Packet *p);
    void _comm_processByte(quint8 rxByte);

    Comm m_comm;
};

#endif // QK_H
