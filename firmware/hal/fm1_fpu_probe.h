/* SPDX-License-Identifier: GPL-3.0-only */
/* S0 diagnostic API: only this translation unit uses the r3 FPU. Production DSP remains Q15. */
#pragma once
#include <stdint.h>
#define FPU_PROBE_COUNT 1024u
typedef struct {
    uint32_t ticks[4], bits[4];
} fpu_probe_result_t;
extern fpu_probe_result_t fpu_probe_result[2];
extern volatile uint32_t fpu_probe_irq_active, fpu_probe_irq_count;
extern volatile uint32_t fpu_probe_irq_bits;
extern uint32_t fpu_probe_sequence_bits[8];
void fpu_probe_bench(uint32_t core);
void fpu_probe_irq(void);
uint32_t fpu_probe_sequence(void);
