/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Andre Rezemini (the SLOOP dualcore branch, github.com/andrerezemini/sloop-fm1) */
/* The second core as the render sees it: one job at a time.
 *   cpu1_run(fn, arg)  hands a job over: 1 = core 1 took it, 0 = run it yourself (no second core, or held)
 *   cpu1_wait()        0 = the job is done; -1 = core 1 did not finish in time: it is held from now on and
 *                      the caller runs the job itself, once
 *   cpu1_on()          1 while core 1 runs and answers
 * Between run and wait the caller must not touch what the job reads or writes. The firmware defines these
 * in cpu1.c over hal/fm1_cpu1.h; the host tests in tests/cpu1_host.c (inline, or a POSIX thread with
 * CORES=2, with random delays and a lost job on request), so the same render code is proven on the Mac. */
#pragma once
typedef void (*cpu1_job_t)(uint32_t);
static int cpu1_run(cpu1_job_t fn, uint32_t arg);
static int cpu1_wait(void);
static int cpu1_on(void);
