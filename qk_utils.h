#ifndef QK_UTILS_H
#define QK_UTILS_H

#define flag(flag,mask) (flag & mask ? 1 : 0)
#define flag_bool(flag,mask) (flag & mask ? true : false)
#define flag_set(flag, mask) (flag |= mask)
#define flag_clr(flag, mask) (flag &= ~mask)

#endif // QK_UTILS_H
