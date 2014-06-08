#ifndef QKBOARD_H
#define QKBOARD_H

#include "qkcore_lib.h"
#include "qkcore_constants.h"
//#include "qkprotocol.h"
#include "qkutils.h"
#include <QObject>
#include <QVariant>
#include <QVector>

class QkCore;
class QkNode;
class QkCore;

using namespace QkUtils;


class QKLIBSHARED_EXPORT QkInfo
{
public:
    QkInfo()
    {
        version = Version(0,0,0);
        baudRate = 0;
        flags = 0;
    }

    Version version;
    int baudRate;
    int flags;
    int features;
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

    typedef QVector<Config> ConfigArray;

    QkBoard(QkCore *qk);

    void _setInfoMask(int mask, bool overwrite = false);

    void _setName(const QString &name);
    void _setFirmwareVersion(int version);
    void _setQkInfo(const QkInfo &qkInfo);
    void _setConfigs(QVector<Config> configs);
    void setConfigValue(int idx, QVariant value);

    int save();
    int update();

    int address();
    QString name();
    int firmwareVersion();
    QkInfo qkInfo();
    ConfigArray configs();

protected:
    Type m_type;
    QkNode *m_parentNode;
    QkCore *m_qk;
private:
    QString m_name;
    int m_fwVersion;
    QkInfo m_qkInfo;
    QVector<Config> m_configs;
    int m_filledInfoMask;
};


#endif // QKBOARD_H
