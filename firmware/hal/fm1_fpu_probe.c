/* SPDX-License-Identifier: GPL-3.0-only */
/* A deliberately isolated float translation unit. Timer4 runs at 24 MHz. No float calls
 * enter the audio render. IRQ preservation is tested with a live register dependency
 * across TIMER5 interrupts that execute an independent float expression. Hardware
 * measurements, including at least one observed IRQ, are required to claim success. */
#include "fm1_fpu_probe.h"
#include "fm1_time.h"
extern float sqrtf(float);
extern float sinf(float);
fpu_probe_result_t fpu_probe_result[2];
volatile uint32_t fpu_probe_irq_active, fpu_probe_irq_count, fpu_probe_irq_bits;
uint32_t fpu_probe_sequence_bits[8];
static volatile float probe_a = 1.00001f, probe_b = 1.00002f;
static uint32_t float_bits(float x) { union { float f; uint32_t u; } b; b.f = x; return b.u; }

void fpu_probe_bench(uint32_t core)
{
    uint32_t op, i, start;
    float x;
    fpu_probe_result_t *r = &fpu_probe_result[core & 1u];
    for (op = 0; op < 4u; op++) {
        x = 0.75f;
        start = fm1_ticks();
        switch (op) {
        case 0: for (i = 0; i < FPU_PROBE_COUNT; i++) x = x * probe_a; break;
        case 1: for (i = 0; i < FPU_PROBE_COUNT; i++) x = x / probe_b; break;
        case 2: for (i = 0; i < FPU_PROBE_COUNT; i++) x = sqrtf(x + probe_a); break;
        default: for (i = 0; i < FPU_PROBE_COUNT; i++) x = sinf(x + probe_b); break;
        }
        r->ticks[op] = fm1_ticks() - start;
        r->bits[op] = float_bits(x);
    }
}

/* Volatile inputs prevent constant folding. Eight live accumulators exercise the normal
 * register ABI used for float on pi32v2; the C ISR must preserve callee-saved registers
 * and fm1_isr.S must preserve caller-saved registers and PSR. */
uint32_t __attribute__((noinline)) fpu_probe_sequence(void)
{
    uint32_t i;
    float a = .125f, b = .25f, c = .375f, d = .5f;
    float e = .625f, f = .75f, g = .875f, h = 1.f;
    for (i = 0; i < 4096u; i++) {
        a *= probe_a; b /= probe_b; c += .0001f; d -= .00001f;
        e *= probe_b; f /= probe_a; g += .0002f; h -= .00002f;
    }
    fpu_probe_sequence_bits[0] = float_bits(a); fpu_probe_sequence_bits[1] = float_bits(b);
    fpu_probe_sequence_bits[2] = float_bits(c); fpu_probe_sequence_bits[3] = float_bits(d);
    fpu_probe_sequence_bits[4] = float_bits(e); fpu_probe_sequence_bits[5] = float_bits(f);
    fpu_probe_sequence_bits[6] = float_bits(g); fpu_probe_sequence_bits[7] = float_bits(h);
    return float_bits(a) ^ float_bits(b) ^ float_bits(c) ^ float_bits(d) ^
           float_bits(e) ^ float_bits(f) ^ float_bits(g) ^ float_bits(h);
}

void fpu_probe_irq(void)
{
    if (fpu_probe_irq_active) {
        float x = probe_a, y = probe_b;
        fpu_probe_irq_bits = float_bits((x * 7.25f + y) / (y + .5f));
        fpu_probe_irq_count++;
    }
}
