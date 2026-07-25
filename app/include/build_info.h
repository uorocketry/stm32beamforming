#ifndef BUILD_INFO_H
#define BUILD_INFO_H

#include <stdint.h>

extern const char firmware_revision[];
extern const volatile uint8_t firmware_node_id;
uint32_t firmware_build_id(void);

#endif
