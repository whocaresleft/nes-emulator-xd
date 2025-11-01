#ifndef INTERRUPT__H
#define INTERRUPT__H

#include "definitions.h"

typedef struct interrupt {
	const usize cpu_cycles;
	const u8 b_g_mask;
	const u16 vector_address;

	constexpr interrupt(const usize cycles, const u8 b_g, const u16 vector) :
		cpu_cycles(cycles),
		b_g_mask(b_g),
		vector_address(vector)
	{
	}
} interrupt;

namespace interrupts {
	constexpr static interrupt nmi_interrupt{ 7_usize, 0b00000000_u8, 0xFFFA_u16 };
	constexpr static interrupt irq_interrupt{ 7_usize, 0b00000000_u8, 0xFFFE_u16 };
	constexpr static interrupt brk_interrupt{ 0_usize, 0b00010000_u8, 0xFFFE_u16 };
};

#endif