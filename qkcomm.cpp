#include "qkcomm.h"


QkComm::QkComm(QkCore *qk, QkNode *parentNode) :
    QkBoard(qk)
{
    m_parentNode = parentNode;
    m_type = btComm;
}
