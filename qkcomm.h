#ifndef QKCOMM_H
#define QKCOMM_H

#include <QObject>
#include "qkboard.h"

class QkCore;
class QkNode;

class QkComm : public QkBoard {
    Q_OBJECT
public:
    QkComm(QkCore *qk, QkNode *parentNode);

protected:
    void setup();

private:

};

#endif // QKCOMM_H
