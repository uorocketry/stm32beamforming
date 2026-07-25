CC ?= cc
CFLAGS ?= -std=c17 -Wall -Wextra -Werror -pedantic -O2
CPPFLAGS += -I../app/include

TARGETS := test_protocol test_diagnostics

.PHONY: all test clean

all: $(TARGETS)

test_protocol: test_protocol.c ../app/src/beamforming_protocol.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

test_diagnostics: test_diagnostics.c ../app/src/diagnostic_record.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

test: $(TARGETS)
	./test_protocol
	./test_diagnostics

clean:
	rm -f $(TARGETS)
