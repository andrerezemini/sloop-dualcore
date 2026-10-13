/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
/* -Oz on selected cold entry points, preserving -Os on the audio DSP. */
#ifndef FELUCCA_COLD_FN
#if FELUCCA_COLD
#define FELUCCA_COLD_FN __attribute__((minsize))
#else
#define FELUCCA_COLD_FN
#endif
#endif
