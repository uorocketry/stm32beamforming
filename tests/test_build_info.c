#include "build_info.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t expected_build_id(const char *revision, uint8_t node_id)
{
    uint32_t hash = 2166136261u;
    for (const char *character = revision; *character != '\0'; ++character) {
        hash = (hash ^ (uint8_t)*character) * 16777619u;
    }
    hash = (hash ^ node_id) * 16777619u;
    return hash;
}

int main(void)
{
    assert(firmware_node_id == 7u);
    assert(firmware_build_id() == expected_build_id("test-revision", 7u));
    puts("build info tests passed");
    return 0;
}
