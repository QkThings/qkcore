#ifndef QKBOARD_H
#define QKBOARD_H

#include "qkcore_global.h"
#include "qkpacket.h"
#include <QObject>
#include <QVariant>
#include <QVector>

class QkCore;

namespace Qk
{
    class Info;
}

class QkNode;
class QkCore;

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

    QkAck save();
    QkAck update();

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


#endif // QKBOARD_H
