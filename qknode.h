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

#include "qkcore_lib.h"
#include <QObject>

class QkCore;
class QkComm;
class QkDevice;

class QKLIBSHARED_EXPORT QkNode
{
public:
    QkNode(QkCore *qk, int address);

    QkComm *comm();
    QkDevice *device();
    void setComm(QkComm *comm);
    void setDevice(QkDevice *device);

    int address();
private:
    QkCore *m_qk;
    QkComm *m_comm;
    QkDevice *m_device;
    int m_address;
};

#endif // QKNODE_H
