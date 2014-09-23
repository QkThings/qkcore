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

#ifndef QKCORE_LIB_H
#define QKCORE_LIB_H

#include <QtCore/qglobal.h>
#include "qkutils.h"

const QkUtils::Version QKCORE_LIB_VERSION(0, 0, 1);

#if defined(QKLIB_LIBRARY)
#  define QKLIBSHARED_EXPORT Q_DECL_EXPORT
#else
#  define QKLIBSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // QKCORE_LIB_H
