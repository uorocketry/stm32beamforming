CC ?= cc
CFLAGS ?= -std=c17 -Wall -Wextra -Werror -pedantic -O2
CPPFLAGS += -I../app/include

TARGETS := test_protocol test_diagnostics test_can_protocol test_can_queue test_can_control test_can_filter test_build_info

.PHONY: all test test_invalid_node_config clean

all: $(TARGETS)

test_protocol: test_protocol.c ../app/src/beamforming_protocol.c ../app/include/beamforming_protocol.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -o $@

test_diagnostics: test_diagnostics.c ../app/src/diagnostic_record.c ../app/include/diagnostic_record.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -o $@

test_can_protocol: test_can_protocol.c ../app/src/can_protocol.c ../app/include/can_protocol.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -o $@

test_can_queue: test_can_queue.c ../app/src/can_queue.c ../app/include/can_queue.h ../app/include/can_protocol.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -o $@

test_can_control: test_can_control.c ../app/src/can_control.c ../app/src/beamforming_protocol.c ../app/include/can_control.h ../app/include/can_protocol.h ../app/include/beamforming_protocol.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -o $@

test_can_filter: test_can_filter.c ../app/src/can_filter.c ../app/include/can_filter.h ../app/include/can_protocol.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -o $@

test_build_info: test_build_info.c ../app/src/build_info.c ../app/include/build_info.h ../app/include/firmware_config.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -DFIRMWARE_REVISION=\"test-revision\" -DBEAMFORMER_NODE_ID=7 $(filter %.c,$^) -o $@

test: $(TARGETS) test_invalid_node_config
	./test_protocol
	./test_diagnostics
	./test_can_protocol
	./test_can_queue
	./test_can_control
	./test_can_filter
	./test_build_info

test_invalid_node_config:
	@if printf '#include "firmware_config.h"\n' \
		| $(CC) $(CPPFLAGS) -DBEAMFORMER_NODE_ID=31 -x c -fsyntax-only - >/dev/null 2>&1; then \
		echo "invalid node configuration unexpectedly compiled"; \
		exit 1; \
	else \
		echo "invalid node configuration rejected"; \
	fi

clean:
	rm -f $(TARGETS)
