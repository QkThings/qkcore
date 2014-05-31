#include "qkconnect.h"

#include "qkcore.h"
#include "qkconnserial.h"

#include <QDebug>
#include <QtSerialPort/QSerialPortInfo>
#include <QStringList>
#include <QIODevice>
#include <QQueue>
#include <QEventLoop>
#include <QTimer>


int QkConnection::nextId = 0;

QkConnWorker::QkConnWorker(QkConnection *conn)
{
    m_conn = conn;
    m_quit = false;
    m_connected = false;
}

void QkConnWorker::quit()
{
    m_quit = true;
}

void QkConnWorker::sendFrame(const QkFrame &frame)
{
    QMutexLocker locker(&m_mutex);
//    qDebug() << "sendFrame enqueue";
    m_outputFramesQueue.enqueue(frame);
}


QkConnection::QkConnection(QObject *parent) :
    QObject(parent)
{
    m_qk = new QkCore(this);
    m_id = QkConnection::nextId++;    
}

QkConnection::~QkConnection()
{
    close();
}

bool QkConnection::isConnected()
{
    if(m_worker != 0)
        return m_worker->isConnected();
    return false;
}

void QkConnection::open()
{
    if(m_workerThread != 0)
        m_workerThread->start();
}

void QkConnection::close()
{
    if(m_workerThread != 0 && m_worker != 0)
    {
        m_worker->quit();
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

bool QkConnection::Descriptor::operator==(Descriptor &other)
{
    return (type == other.type &&
            parameters == other.parameters ? true : false);
}


bool QkConnection::operator==(QkConnection &other)
{
    return (descriptor().type == other.descriptor().type &&
            descriptor().parameters == other.descriptor().parameters ? true : false);
}


QkConnectionManager::QkConnectionManager(QObject *parent) :
    QObject(parent)
{
    m_searchOnConnect = true;
}

QkConnectionManager::~QkConnectionManager()
{
    qDeleteAll(m_connections.begin(), m_connections.end());
}

QList<QkConnection*> QkConnectionManager::connections()
{
    return m_connections;
}

QkConnection* QkConnectionManager::defaultConnection()
{
    return (m_connections.count() > 0 ? m_connections[0] : 0);
}

void QkConnectionManager::slotConnected(int id)
{
    QkConnection *conn = connection(id);

    if(m_searchOnConnect)
    {
        QkAck ack = conn->qk()->hello();
        if(ack.result != QkAck::NACK)
            conn->qk()->search();
    }
}

QkConnection* QkConnectionManager::addConnection(const QkConnection::Descriptor &desc)
{
    QkConnection *conn;

    if(connection(desc) != 0)
    {
        emit error(tr("Connection already exists."));
        return 0;
    }

    switch(desc.type)
    {
    case QkConnection::tSerial:
        conn = new QkConnSerial(desc.parameters["portName"].toString(),
                                desc.parameters["baudRate"].toInt(),
                                this);
        break;
    default:
        qDebug() << "Connection type unknown" << desc.type;
        return 0;
    }

    connect(conn, SIGNAL(error(QString)), this, SIGNAL(error(QString)));
    connect(conn, SIGNAL(connected(int)), this, SLOT(slotConnected(int)));

    m_connections.append(conn);
    emit connectionAdded(conn);

    conn->open();
}

void QkConnectionManager::removeConnection(const QkConnection::Descriptor &desc)
{
    QkConnection *conn = connection(desc);
    if(conn != 0)
    {
        conn->close();
        emit connectionRemoved(conn);
        m_connections.removeOne(conn);
    }
}

QkConnection* QkConnectionManager::connection(const QkConnection::Descriptor &desc)
{
    foreach(QkConnection *conn, m_connections)
        if(conn->sameAs(desc))
            return conn;
    return 0;
}

QkConnection* QkConnectionManager::connection(int id)
{
    foreach(QkConnection *conn, m_connections)
        if(conn->id() == id)
            return conn;
    return 0;
}

