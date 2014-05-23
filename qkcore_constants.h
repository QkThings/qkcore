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
    featEEPROM = MASK(1, 0),
    featRTC = MASK(1, 1),
    featClockSwitching = MASK(1, 2),
    featPowerManagement = MASK(1, 3)
};

}

#endif // QKCORE_CONSTANTS_H
