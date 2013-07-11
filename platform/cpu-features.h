/*
 * Copyright (C) 2013 Roger Shen  rogershen@pptv.com
 *
 */

#ifndef CPU_FEATURES_H
#define CPU_FEATURES_H

#include <sys/cdefs.h>
#include <stdint.h>

__BEGIN_DECLS

typedef enum {
    ANDROID_CPU_FAMILY_UNKNOWN = 0,
    ANDROID_CPU_FAMILY_ARM,
    ANDROID_CPU_FAMILY_X86,
    ANDROID_CPU_FAMILY_MIPS,

    ANDROID_CPU_FAMILY_MAX  /* do not remove */

} AndroidCpuFamily;

/* Return family of the device's CPU */
extern AndroidCpuFamily   android_getCpuFamily(void);

enum {
    ANDROID_CPU_ARM_FEATURE_ARMv7       = (1 << 0),
    ANDROID_CPU_ARM_FEATURE_VFPv3       = (1 << 1),
    ANDROID_CPU_ARM_FEATURE_NEON        = (1 << 2),
    ANDROID_CPU_ARM_FEATURE_LDREX_STREX = (1 << 3),
    ANDROID_CPU_ARM_FEATURE_VFP       = (1 << 4),
    ANDROID_CPU_ARM_FEATURE_VFPv3d16       = (1 << 5),
};

enum {
    ANDROID_CPU_X86_FEATURE_SSSE3  = (1 << 0),
    ANDROID_CPU_X86_FEATURE_POPCNT = (1 << 1),
    ANDROID_CPU_X86_FEATURE_MOVBE  = (1 << 2),
};

extern uint64_t android_getCpuFeatures(void);

/* Return the number of CPU cores detected on this device. */
extern int android_getCpuCount(void);

extern int android_getCpuFreq(void);

extern int android_getCpuArchNumber(void);


__END_DECLS

#endif /* CPU_FEATURES_H */
