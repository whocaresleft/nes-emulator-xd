#ifndef INSTRUCTION__H
#define INSTRUCTION__H

class cpu;

#include "definitions.h"
#include <utility>

typedef enum addressing_mode_type {
	absolute = 0,
	absolute_x,
	absolute_y,
	accumulator,
	immediate,
	implied,
	indirect,
	indirect_x,
	indirect_y,
	relative,
	zero_page,
	zero_page_x,
	zero_page_y,
} addressing_mode_type;

static const char* ADDRESSING_MODE_NAMES[] = {
	"Absolute", "Absolute X", "Absolute Y",
	"Accumulator", "Immediate", "Implied",
	"Indirect", "Indirect X", "Indirect Y",
	"Relative", "Zero Page", "Zero Page X", "Zero Page Y"
};

typedef struct addressing_mode {
	const u8 length;
	std::pair<u16, bool>(cpu::*get_address)(const u16 address) const;
} addressing_mode;

typedef struct instruction {
	const u8 opcode;
	const char mnemonic[4];
	const addressing_mode_type mode;
	const u8 cycles;
	void (cpu::* handler)(const addressing_mode mode);
} instruction;

#endif