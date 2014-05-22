#ifndef QKCORE_LIB_H
#define QKCORE_LIB_H

#include <QtCore/qglobal.h>

#define QKLIB_VERSION_MAJOR 0
#define QKLIB_VERSION_MINOR 0
#define QKLIB_VERSION_PATCH 1

#if defined(QKLIB_LIBRARY)
#  define QKLIBSHARED_EXPORT Q_DECL_EXPORT
#else
#  define QKLIBSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // QKCORE_LIB_H
