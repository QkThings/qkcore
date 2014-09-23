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

#ifndef QKCORE_CONSTANTS_H
#define QKCORE_CONSTANTS_H

#define QK_FLAGMASK_EVENTNOTIF  (1<<0)
#define QK_FLAGMASK_STATUSNOTIF (1<<1)
#define QK_FLAGMASK_AUTOSAMP    (1<<2)

#define QK_LABEL_SIZE       20
#define QK_BOARD_NAME_SIZE  20


namespace Qk
{
enum Feature
{
    featEEPROM = (1<<0),
    featRTC = (1<<1),
    featClockSwitching = (1<<2),
    featPowerManagement = (1<<3)
};

}

#endif // QKCORE_CONSTANTS_H
