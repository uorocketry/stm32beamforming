#ifndef FAULTS_H
#define FAULTS_H

#include "diagnostic_record.h"

void firmware_fail(firmware_fault_t fault) __attribute__((noreturn));
void hard_fault_handler(void) __attribute__((noreturn));
void nmi_handler(void) __attribute__((noreturn));
void unexpected_interrupt_handler(void) __attribute__((noreturn));

#endif
