#include "build_info.h"

#ifndef FIRMWARE_REVISION
#define FIRMWARE_REVISION "unknown"
#endif

const char firmware_revision[] = FIRMWARE_REVISION;

uint32_t firmware_build_id(void)
{
    uint32_t hash = 2166136261u;

    for (const char *character = firmware_revision; *character != '\0'; ++character) {
        hash = (hash ^ (uint8_t)*character) * 16777619u;
    }

    return hash;
}
