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
