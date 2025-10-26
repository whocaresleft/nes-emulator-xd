#ifndef BUS__H
#define BUS__H

#include "memory.h"
#include "cartridge.h"

class bus : public memory {

private:

	std::array<u8, 0x0800_usize> cpu_vram;
	cartridge* rom;

public:
	bus();
	bus(bus& to_copy) = delete;
	bus(bus&& to_move) noexcept = delete;
	~bus();

	void load(cartridge* rom);

	u8 read_u8(const u16 address) const override;
	void write_u8(const u16 address, const u8 value) override;
};

#endif