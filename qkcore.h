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

#ifndef QKCORE_H
#define QKCORE_H

#include "qkcore_lib.h"
#include "qkcore_constants.h"
#include "qkprotocol.h"
#include "qkboard.h"
#include "qkdevice.h"
#include "qkcomm.h"
#include <stdint.h>

#include <QDebug>
#include <QObject>
#include <QVector>
#include <QVariant>
#include <QMutex>
#include <QQueue>

class QkCore;
class QkNode;
class QkDevice;
class QkComm;
class QkPacket;
class QkConnection;

typedef QMap<int, QkNode*> QkNodeMap;

class QKLIBSHARED_EXPORT QkCore : public QObject
{
    Q_OBJECT
    friend class QkProtocol;
public:
    enum Status
    {
        sWaitForReady,
        sReady,
        sSearching,
        sStarted,
        sStopped
    };

    QkCore(QkConnection *conn, QObject *parent = 0);
    ~QkCore();

    static QString errorMessage(int errCode);

    void reset();
    bool isReady();
    bool isRunning();

    bool waitForReady(int timeout = 5000);

    QkNode* node(int address = 0);
    QkNodeMap nodes();
    QkConnection *connection() { return m_conn; }
    QkProtocol* protocol() { return m_protocol; }


signals:
    void status(QkCore::Status);

public slots:
    int hello();
    int search();
    int getNode(int address = 0);
    int start(int address = 0);
    int stop(int address = 0);

private:
    void slotStatus(QkCore::Status status);

private:
    bool m_ready;
    bool m_running;
    QMap<int, QkNode*> m_nodes;
    QkProtocol *m_protocol;
    QkConnection *m_conn;


};

#endif // QK_H
