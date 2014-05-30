#include "qkconnserial.h"
#include "qkcore.h"
#include "qkprotocol.h"
#include "qkconnect.h"

#include <QDebug>
#include <QDateTime>
#include <QEventLoop>


QkConnSerialWorker::QkConnSerialWorker(QkConnSerial *conn) :
    QkConnWorker(conn)
{

}

void QkConnSerialWorker::run()
{
    QEventLoop eventLoop;

    m_sp = new QSerialPort(this);
    m_protocol = new Protocol();

    connect(m_sp, SIGNAL(readyRead()), this, SLOT(slotReadyRead()));

    QkConnection::Descriptor desc = connection()->descriptor();

    QString portName = desc.parameters.value("portName").toString();
    int baudRate = desc.parameters.value("baudRate").toInt();

    m_sp->setPortName(portName);
    m_sp->setBaudRate(baudRate);
    if(m_sp->open(QIODevice::ReadWrite))
    {
        m_sp->setBaudRate(baudRate);
        m_sp->setParity(QSerialPort::NoParity);
        m_sp->setFlowControl(QSerialPort::NoFlowControl);
        m_sp->setDataBits(QSerialPort::Data8);
        m_sp->setRequestToSend(false);
        m_sp->clear();
        qDebug() << "connection opened:" << m_sp->portName() << m_sp->baudRate();
    }
    else
    {
        emit error(tr("Failed to open serial port ") + m_sp->portName() + m_sp->errorString());
        return;
    }

    m_connected = true;
    emit connected(connection()->id());

    while(!m_quit)
    {
        eventLoop.processEvents();

        m_mutex.lock();
        if(m_outputFramesQueue.count() > 0)
        {
            const QByteArray &frame = m_outputFramesQueue.dequeue().data;
            m_mutex.unlock();

            qDebug() << "sendFrame dequeue";

            int i;
            quint8 chBuf;

            const char flagByte = QK_COMM_FLAG;
            const char dleByte = QK_COMM_DLE;

            m_sp->write(&flagByte, 1);
            for(i = 0; i < frame.count(); i++)
            {
                chBuf = frame[i] & 0xFF;
                if(chBuf == QK_COMM_FLAG || chBuf == QK_COMM_DLE)
                    m_sp->write(&dleByte, 1);
                m_sp->write((char*) &chBuf, 1);
            }
            m_sp->write(&flagByte, 1);
        }
        else
            m_mutex.unlock();
    }

    qDebug() << "close connection";

    m_sp->close();
    m_connected = false;

    emit disconnected(connection()->id());
    emit finished();

    eventLoop.processEvents();
}

void QkConnSerialWorker::slotReadyRead()
{
    QByteArray data = m_sp->readAll();
    char *bufPtr = data.data();
    int countBytes = data.count();

    while(countBytes--)
    {
        parseSerialData((quint8)*bufPtr++);
        if(FLAG(m_protocol->ctrlFlags, Protocol::cfFrameReady))
        {
            QkFrame frame;
            frame.data = m_protocol->frame;
            frame.timestamp = QDateTime::currentMSecsSinceEpoch();
            emit frameReady(frame);
            FLAG_CLR(m_protocol->ctrlFlags, Protocol::cfFrameReady);
        }
    }
}

void QkConnSerialWorker::parseSerialData(quint8 data)
{
    switch(data)
    {
    case QK_COMM_FLAG:
        if(!FLAG(m_protocol->ctrlFlags, Protocol::cfDataLinkEscape))
        {
            if(!FLAG(m_protocol->ctrlFlags, Protocol::cfReceiving))
            {
                m_protocol->frame.clear();
                FLAG_SET(m_protocol->ctrlFlags, Protocol::cfReceiving);
                FLAG_SET(m_protocol->ctrlFlags, Protocol::cfValid);
            }
            else
            {
                if(FLAG(m_protocol->ctrlFlags, Protocol::cfValid) &&
                   m_protocol->frame.count() > 0)
                {
                    FLAG_SET(m_protocol->ctrlFlags, Protocol::cfFrameReady);
                    FLAG_CLR(m_protocol->ctrlFlags, Protocol::cfReceiving);
                    FLAG_CLR(m_protocol->ctrlFlags, Protocol::cfValid);
                }
            }
            return;
        }
        break;
    case QK_COMM_DLE:
        if(FLAG(m_protocol->ctrlFlags, Protocol::cfValid))
        {
            if(!FLAG(m_protocol->ctrlFlags, Protocol::cfDataLinkEscape))
            {
                FLAG_SET(m_protocol->ctrlFlags, Protocol::cfDataLinkEscape);
                return;
            }
        }
        break;
    default: ;
    }

    if(FLAG(m_protocol->ctrlFlags, Protocol::cfValid))
        m_protocol->frame.append(data);

    FLAG_CLR(m_protocol->ctrlFlags, Protocol::cfDataLinkEscape);
}

QkConnSerial::QkConnSerial(QObject *parent)
{
    QkConnSerial("portName", 38400, parent);
}

QkConnSerial::QkConnSerial(const QString &portName, int baudRate, QObject *parent) :
    QkConnection(parent)
{
    qDebug() << "QkConnSerial" << portName << baudRate;

    m_descriptor.type = QkConnection::tSerial;
    m_descriptor.parameters["portName"] = portName;
    m_descriptor.parameters["baudRate"] = baudRate;

    m_workerThread = new QThread(this);
    m_worker = new QkConnSerialWorker(this);

    m_worker->moveToThread(m_workerThread);

    //TODO try to use queued connections by connecting before moveToThread

    connect(m_workerThread, SIGNAL(started()), m_worker, SLOT(run()), Qt::DirectConnection);
    connect(m_worker, SIGNAL(finished()), m_workerThread, SLOT(quit()), Qt::DirectConnection);

    connect(m_worker, SIGNAL(connected(int)), this, SIGNAL(connected(int)), Qt::DirectConnection);
    connect(m_worker, SIGNAL(disconnected(int)), this, SIGNAL(disconnected(int)), Qt::DirectConnection);

    QkProtocol *protocol = m_qk->protocol();
    QkProtocolWorker *protocolWorker = protocol->worker();

    connect(protocolWorker, SIGNAL(frameReady(QkFrame)), m_worker, SLOT(sendFrame(QkFrame)), Qt::DirectConnection);
    connect(m_worker, SIGNAL(frameReady(QkFrame)), protocolWorker, SLOT(parseFrame(QkFrame)), Qt::DirectConnection);
}


void QkConnSerial::setPortName(const QString &portName)
{
    m_descriptor.parameters["portName"] = portName;
}

void QkConnSerial::setBaudRate(int baudRate)
{
    m_descriptor.parameters["baudRate"] = baudRate;
}

bool QkConnSerial::sameAs(const Descriptor &desc)
{
    if( desc.type == QkConnection::tSerial &&
       (desc.parameters.keys().contains("portName") &&
        desc.parameters["portName"] == m_descriptor.parameters["portName"]))
        return true;

    return false;
}
