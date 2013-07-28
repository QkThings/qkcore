#include "qk.h"
#include "qk_utils.h"

#include <QDebug>

static void calculate_hdr_length(Qk::Packet *packet);

void QkBoard::update()
{

}

void QkBoard::save()
{
    Qk::Packet p;
    Qk::PacketDescriptor pd;
    pd.address = m_address;
    pd.code = QK_COMM_SAVE;
    Qk::PacketBuilder::build(&p, &pd);
    m_qk->comm_sendPacket(&p);
}


QkCore::QkCore()
{

}

QkCore::~QkCore()
{

}

void QkCore::search()
{
    Qk::Packet p;
    Qk::PacketDescriptor pd;
    pd.code = QK_COMM_SEARCH;
    Qk::PacketBuilder::build(&p, &pd);
    comm_sendPacket(&p);
}

void QkCore::start()
{
    Qk::Packet p;
    Qk::PacketDescriptor pd;
    pd.code = QK_COMM_START;
    Qk::PacketBuilder::build(&p, &pd);
    comm_sendPacket(&p);
}

void QkCore::stop()
{
    Qk::Packet p;
    Qk::PacketDescriptor pd;
    pd.code = QK_COMM_STOP;
    Qk::PacketBuilder::build(&p, &pd);
    comm_sendPacket(&p);
}

void QkCore::comm_sendPacket(Packet *p)
{
    QByteArray frame;

    if(p->address != 0) {
        p->flags |= QK_PACKET_FLAGMASK_ADDRESS;
        p->flags |= QK_PACKET_FLAGMASK_16BITADDR;
    }

    frame.append(p->flags & 0xFF);
    if(p->flags & QK_PACKET_FLAGMASK_EXTEND) {
        frame.append(p->flags >> 8);
    }

    frame.append(p->code);
    frame.append(p->data);

    emit comm_sendBytes((quint8*)frame.data(), frame.count());
}

void QkCore::comm_processBytes(quint8 *buf, int count)
{
    qDebug() << "comm_processBytes()";
    while(count--) {
        _comm_processByte(*buf++);
    }
}

void QkCore::_comm_processPacket(Packet *p)
{
    /*QkNode *selNode = 0;
    QkBoard *selBoard = 0;
    QkDevice *selDevice = 0;
    QkModule *selModule = 0;*/

}

void QkCore::_comm_processByte(quint8 rxByte)
{
    Packet *packet = &(m_comm.packet);
    rxByte &= 0xFF;

    if(rxByte == QK_COMM_FLAG) {
        if(!m_comm.dle) {
            if(!m_comm.rxdata) {
                m_comm.packet.address = 0;
                m_comm.packet.flags = 0;
                m_comm.packet.data.clear();
                m_comm.rxdata = true;
                m_comm.valid = true;
                m_comm.resp = false;
                m_comm.count = 0;
            } else {
                if(m_comm.count > 0 && m_comm.valid) {
                    _comm_processPacket(&(m_comm.packet));
                    m_comm.rxdata = false;
                    m_comm.valid = false;
                    m_comm.count = 0;
                    if(!m_comm.seq)
                        m_comm.resp = true;
                    else
                        m_comm.resp = false;
                }
            }
            return;
        }
    }
    else if(rxByte == QK_COMM_DLE) {
        if(m_comm.valid) {
            if(!m_comm.dle) {
                m_comm.dle = true;
                return;
            }
        }
    }

    if(m_comm.valid) {

        if(m_comm.count == 0) {
            m_comm.packet.flags = rxByte;
            calculate_hdr_length(packet);
        }
        else if((packet->flags & QK_PACKET_FLAGMASK_EXTEND) && m_comm.count == 1) {
            packet->flags |= rxByte << 8;
        }
        else if((packet->flags & QK_PACKET_FLAGMASK_ADDRESS) &&
                m_comm.count >= QK_PACKET_ADDR16_OFFSET(packet) &&
                m_comm.count < QK_PACKET_CODE_OFFSET(packet))
        {
            if((packet->flags & QK_PACKET_FLAGMASK_16BITADDR)) {
                packet->address |= rxByte << (m_comm.count - QK_PACKET_ADDR16_OFFSET(packet));
            }
            else {
                //TODO 64bit addresses not supported yet
            }
        }
        else if(m_comm.count == QK_PACKET_CODE_OFFSET(packet)) {
            packet->code = rxByte;
            qDebug() << "code =" << packet->code;
        }
        else {
            m_comm.packet.data.append(rxByte);
        }

        m_comm.count++;
    }

    m_comm.dle = false;
}

bool Qk::PacketBuilder::build(Packet *p, PacketDescriptor *pd)
{
    if(!validate(pd))
        return false;

    p->address = pd->address;
    p->code = pd->code;
    return true;
}

bool Qk::PacketBuilder::validate(PacketDescriptor *pd)
{
    bool ok = false;

    switch(pd->code)
    {
    default: ok = true;
    }
    return ok;
}

QString QkCore::version()
{
    return QString().sprintf("%d.%d.%d", QKLIB_VERSION_MAJOR,
                                         QKLIB_VERSION_MINOR,
                                         QKLIB_VERSION_PATCH);
}

QString QkCore::errorMessage(int errCode)
{
    errCode &= 0xFF;
    switch(errCode) {
    case QK_ERR_CODE_UNKNOWN: return tr("Code unknown"); break;
    case QK_ERR_INVALID_BOARD: return tr("Invalid board"); break;
    case QK_ERR_INVALID_DATA_OR_ARG: return tr("Invalid data or arguments"); break;
    case QK_ERR_BOARD_NOT_CONNECTED: return tr("Board not connected"); break;
    case QK_ERR_INVALID_SAMP_FREQ: return tr("Invalid sampling frequency"); break;
    case QK_ERR_COMM_TIMEOUT: return tr("Communication timeout"); break;
    case QK_ERR_UNSUPPORTED_OPERATION: return tr("Unsupported operation"); break;
    case QK_ERR_UNABLE_TO_SEND_MESSAGE: return tr("Unable to send message"); break;
    case QK_ERR_SAMP_OVERLAP: return tr("Sampling overlap"); break;
    default:
        return tr("? (unknown error code)"); break;
    }
}

QString QkCore::codeFriendlyName(int code)
{
    code &= 0xFF;
    switch((quint8)code)
    {
    case QK_COMM_SEQBEGIN:
        return "SEQBEGIN";
        break;
    case QK_COMM_SEQEND:
        return "SEQEND";
        break;
    case QK_COMM_GET_NODE:
        return "GET_NODE";
        break;
    case QK_COMM_GET_MODULE:
        return "GET_MODULE";
        break;
    case QK_COMM_GET_DEVICE:
        return "GET_DEVICE";
    case QK_COMM_GET_NETWORK:
        return "GET_NETWORK";
        break;
    case QK_COMM_INFO_QK:
        return "INFO_QK";
        break;
    case QK_COMM_INFO_SAMP:
        return "INFO_SAMP";
        break;
    case QK_COMM_INFO_BOARD:
        return "INFO_BOARD";
        break;
    case QK_COMM_INFO_MODULE:
        return "INFO_MODULE";
        break;
    case QK_COMM_INFO_DEVICE:
        return "INFO_DEVICE";
        break;
    case QK_COMM_INFO_NETWORK:
        return "INFO_NETWORK";
        break;
    case QK_COMM_INFO_GATEWAY:
        return "INFO_GATEWAY";
        break;
    case QK_COMM_INFO_DATA:
        return "INFO_DATA";
        break;
    case QK_COMM_INFO_EVENT:
        return "INFO_EVENT";
        break;
    case QK_COMM_INFO_CONFIG:
        return "INFO_CONFIG";
        break;
    case QK_COMM_DATA:
        return "DATA";
        break;
    case QK_COMM_EVENT:
        return "EVENT";
        break;
    case QK_COMM_STATUS:
        return "STATUS";
        break;
    case QK_COMM_START:
        return "START";
        break;
    case QK_COMM_STOP:
        return "STOP";
        break;
    case QK_COMM_STRING:
        return "STRING";
        break;
    case QK_COMM_OK:
        return "OK";
        break;
    case QK_COMM_ERR:
        return "ERR";
    case QK_COMM_TIMEOUT:
        return "TIMEOUT";
        break;
    default: return "???";
    }
}

static void calculate_hdr_length(Qk::Packet *packet) {
    int extend, frag, addr, addr16bit;

    extend = flag(packet->flags, QK_PACKET_FLAGMASK_EXTEND);
    frag = flag(packet->flags, QK_PACKET_FLAGMASK_FRAG);
    addr = flag(packet->flags, QK_PACKET_FLAGMASK_ADDRESS);
    addr16bit = flag(packet->flags, QK_PACKET_FLAGMASK_16BITADDR);

    if(extend || frag)
      packet->headerLength = 3; // flags(2) + code(1)
    else
      packet->headerLength = 2; // flags(1) + code(1)

    if(addr) {
      if(addr16bit)
        packet->headerLength += 2; // 16bit address
      else
        packet->headerLength += 8; // 64bit address
    }
}

