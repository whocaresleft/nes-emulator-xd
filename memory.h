#ifndef MEMORY__H
#define MEMORY__H

#include "definitions.h"

class memory {

public:
	virtual u8 read_u8(const u16 address) const = 0;
	virtual void write_u8(const u16 address, const u8 value) = 0;

	virtual u16 read_u16(const u16 address) const {
		return this->read_u8(address) | (this->read_u8(address + 1) << 8);
	}
	virtual void write_u16(const u16 address, const u16 value) {
		this->write_u8(address, static_cast<u8>(value & 0x00FF));
		this->write_u8(address + 1, static_cast<u8>((value & 0xFF00) >> 8));
		return;
	}
};

#endif