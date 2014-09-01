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

#include "qkconnect.h"
class QSerialPort;
class QkConnSerial;

class QkConnSerialWorker : public QkConnWorker
{
    Q_OBJECT
public:
    class Protocol
    {
    public:
        enum ControlFlag
        {
            cfTramsmiting = MASK(1, 0),
            cfReceiving = MASK(1, 1),
            cfFrameReady = MASK(1, 2),
            cfDataLinkEscape = MASK(1, 3),
            cfValid = MASK(1, 4)
        };

        Protocol()
        {
            frame.clear();
            ctrlFlags = 0;
            bytesRead = 0;
        }

        QByteArray frame;
        int ctrlFlags;
        int bytesRead;
    };

    QkConnSerialWorker(QkConnSerial *conn);
    void run();

public slots:
    void slotReadyRead();

private:
    void parseSerialData(quint8 data);

    QSerialPort *m_sp;
    Protocol *m_protocol;
};

class QkConnSerial : public QkConnection
{
    Q_OBJECT
public:
    QkConnSerial(QObject *parent = 0);
    QkConnSerial(const QString &portName, int baudRate, QObject *parent = 0);
    void setPortName(const QString &portName);
    void setBaudRate(int baudRate);

    bool sameAs(const Descriptor &desc);


signals:

public slots:

};

#endif // QKCONNSERIAL_H
