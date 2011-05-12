/* Rumble_version.h */
#ifndef RUMBLE_VERSION_H
#   define RUMBLE_VERSION_H
#   define RUMBLE_VERSION  0x001E0983   /* Internal version for module checks */
#   define RUMBLE_MAJOR    (RUMBLE_VERSION & 0xFF000000) >> 24
#   define RUMBLE_MINOR    (RUMBLE_VERSION & 0x00FF0000) >> 16
#   define RUMBLE_REV      (RUMBLE_VERSION & 0x0000FFFF)
#endif /* RUMBLE_VERSION_H */
