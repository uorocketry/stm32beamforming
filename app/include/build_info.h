#ifndef BUILD_INFO_H
#define BUILD_INFO_H

#include <stdint.h>

extern const char firmware_revision[];
uint32_t firmware_build_id(void);

#endif
