#ifndef BUS__H
#define BUS__H

#include "cartridge.h"

class bus {

private:

	static constexpr u16 MIRRORS_PRG[2] = { 0x3FFF, 0x7FFF };

	std::array<u8, 0x0800_usize> cpu_wram;
	std::vector<u8> prg_rom;
	usize mirror_index;
	usize cycles;

public:
	bus() :
		cpu_wram({}),
		prg_rom({}),
		cycles(0),
		mirror_index(0)
	{ }
	bus(bus& to_copy) = delete;
	bus(bus&& to_move) noexcept = delete;
	~bus() {}

	void load(cartridge* rom) { 
		this->prg_rom = rom->prg_rom; 
		this->mirror_index = (this->prg_rom.size() == 0x4000) ? 0_usize : 1_usize;
	}
	void tick(usize cycles) { this->cycles += cycles; }

	u8 read_u8(const u16 address) const {
		if (address >= 0x8000) { return this->prg_rom[(address - 0x8000) & bus::MIRRORS_PRG[this->mirror_index]]; }
		else if (address <= 0x1FFF) { return this->cpu_wram[address & 0x07FF]; }
		else if (address <= 0x3FFF) { return 0; } // [ppu
		else if (address <= 0x4017) { return 0; } // apu and io
		else if (address >= 0x6000 && address <= 0x7FFF) { return 0; } // expansion rom
		return 0;
	}
	void write_u8(const u16 address, const u8 value) {
		if (address <= 0x1FFF) { this->cpu_wram[address & 0x07FF] = value; }
		else if (address <= 0x3FFF) { return; } // ppu
		else if (address <= 0x4017) { return; } // apu and io
		else if (address >= 0x6000 && address <= 0x7FFF) { return; } // expansion rom
	}
	u16 read_u16(const u16 address) const {
		return this->read_u8(address) | (this->read_u8(address + 1) << 8);
	}
	void write_u16(const u16 address, const u16 value) {
		this->write_u8(address, static_cast<u8>(value & 0x00FF));
		this->write_u8(address + 1, static_cast<u8>((value & 0xFF00) >> 8));
	}

	const std::span<u8> get_wram() { return std::span<u8>(cpu_wram); }
};

#endif