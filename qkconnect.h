#ifndef QKCONNECT_H
#define QKCONNECT_H

#include <QObject>
#include <QtSerialPort/QSerialPort>
#include <QStringList>
#include <QReadWriteLock>
#include <QQueue>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "qkcore.h"
#include "qkutils.h"

class QReadWriteLock;
class QkConnection;

class QkConnWorker : public QObject
{
Q_OBJECT
public:
    QkConnWorker(QkConnection *conn);

    bool isConnected() { return m_connected; }

signals:
    void frameReady(QkFrame);
    void connected(int);
    void disconnected(int);
    void finished();
    void error(QString);

public slots:
    virtual void run() = 0;
    void quit();
    void sendFrame(const QkFrame &frame);

protected:
    QkConnection *connection() { return m_conn; }

protected:
    QkFrameQueue m_outputFramesQueue;
//    QkFrameQueue m_inputFrames;
    bool m_quit;
    bool m_connected;

    QMutex m_mutex;
    QWaitCondition m_condition;

private:
    QkConnection *m_conn;

};

class QkConnection : public QObject
{
    Q_OBJECT
    Q_ENUMS(Type)
public:
    enum Type
    {
        tUnknown,
        tSerial,
        tTCP
    };
    enum Status
    {
        sConnecting,
        sConnected,
        sDisconnected
    };

    class Descriptor
    {
    public:
        Type type;
        QMap<QString,QVariant> parameters;
        bool operator==(Descriptor &other);
    };

    QkConnection(QObject *parent = 0);
    ~QkConnection();

    QkCore *qk() { return m_qk; }
    Descriptor descriptor() { return m_descriptor; }
    QIODevice *io() { return m_io; }

    int id() { return m_id; }
    QkConnWorker* worker() { return m_worker; }
    bool isConnected();

    void setSearchOnConnect(bool enabled) { m_searchOnConnect = enabled; }

    bool operator==(QkConnection &other);

    virtual bool sameAs(const Descriptor &desc) = 0;

signals:
    void error(QString message);
    void status(int, QkConnection::Status);
    void connected(int);
    void disconnected(int);

public slots:
    void open();
    void close();

private slots:
    void slotConnected();
    void slotDisconnected();

protected:
    Descriptor m_descriptor;
    QIODevice *m_io;
    QkCore *m_qk;
    QThread *m_workerThread;
    QkConnWorker *m_worker;

private:
    static int nextId;
    int m_id;
    bool m_searchOnConnect;
};

class QkConnectionManager : public QObject
{
    Q_OBJECT
public:
    explicit QkConnectionManager(QObject *parent = 0);
    ~QkConnectionManager();

    void setSearchOnConnect(bool search) { m_searchOnConnect = search; }
    bool searchOnConnect() { return m_searchOnConnect; }

    QList<QkConnection*> connections();
    QkConnection* defaultConnection();
    QkConnection* connection(const QkConnection::Descriptor &descriptor);
    QkConnection* connection(int id);

signals:
    void connectionAdded(QkConnection *c);
    void connectionRemoved(QkConnection *c);
    void error(QString message);

public slots:
//    bool validate(const QkConnection::Descriptor &connDesc);
    QkConnection* addConnection(const QkConnection::Descriptor &desc);
    void removeConnection(const QkConnection::Descriptor &desc);

private slots:
//    void slotConnected(int id);

private:
    QList<QkConnection*> m_connections;
    bool m_searchOnConnect;
};

//class QkSerialConnection : public QkConnection
//{
//    Q_OBJECT
//public:
//    enum Parameter
//    {
//        pPortName,
//        pBaudRate,
//        pCOUNT
//    };
//    QkSerialConnection(QString portName = QString(), int baudRate = 38400, QObject *parent = 0);

//public slots:
//    bool open();
//    void close();
//    void setPortName(const QString &portName);
//    void setBaudRate(int baudRate);

//protected slots:
//    void slotDataReady();
//    void slotInputFrameReady();

//protected:
//    void sendFrame(const QkFrame &frame);

//private:
//    class Comm
//    {
//    public:
//        QByteArray frame;
//        volatile bool txdata;
//        volatile bool rxdata;
//        volatile bool frameReady;
//        volatile bool seq;
//        volatile bool dle;
//        volatile bool valid;
//        volatile int count;
//    };
//    void parseIncomingData(quint8 data);
//    void create();
//    Comm m_comm;
//    QSerialPort *m_sp;
//    QString m_portName;
//    int m_baudRate;

//    QkSerialConnectionWorker *m_worker;
//};



#endif // QKCONNECT_H
