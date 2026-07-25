#include "can_filter.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_filter_words_require_extended_data_frames_for_destination(void)
{
    uint32_t identifier = 0u;
    uint32_t mask = 0u;

    assert(can_filter_for_destination(1u, &identifier, &mask));
    assert(identifier == 0x01000004u);
    assert(mask == 0x1f000006u);

    assert(can_filter_for_destination(31u, &identifier, &mask));
    assert(identifier == 0x1f000004u);
    assert(mask == 0x1f000006u);
}

static void test_filter_rejects_invalid_arguments(void)
{
    uint32_t value = 0u;
    assert(!can_filter_for_destination(32u, &value, &value));
    assert(!can_filter_for_destination(1u, NULL, &value));
    assert(!can_filter_for_destination(1u, &value, NULL));
}

int main(void)
{
    test_filter_words_require_extended_data_frames_for_destination();
    test_filter_rejects_invalid_arguments();
    puts("CAN filter tests passed");
    return 0;
}
