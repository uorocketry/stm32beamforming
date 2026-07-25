#include "build_info.h"
#include "firmware_config.h"

#ifndef FIRMWARE_REVISION
#define FIRMWARE_REVISION "unknown"
#endif

const char firmware_revision[] = FIRMWARE_REVISION;
const volatile uint8_t firmware_node_id = BEAMFORMER_NODE_ID;

uint32_t firmware_build_id(void)
{
    uint32_t hash = 2166136261u;

    for (const char *character = firmware_revision; *character != '\0'; ++character) {
        hash = (hash ^ (uint8_t)*character) * 16777619u;
    }

    hash = (hash ^ firmware_node_id) * 16777619u;

    return hash;
}
