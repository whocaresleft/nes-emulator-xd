#include "../header/bus.h"

bus::bus() :
	cpu_vram({}),
	rom(nullptr)
{}

bus::~bus() {}

void bus::load(cartridge* rom) {
	this->rom = rom;
}

u8 bus::read_u8(const u16 address) const {
	if (address <= 0x1FFF) { return this->cpu_vram[address & 0x07FF]; }
	if (0x2000 <= address && address <= 0x3FFF) { return 0; } // PPU
	if (0x8000 <= address && address <= 0xFFFF) { 
		u16 addr = address - 0x8000;
		const std::vector<u8>& prg_ref = this->rom->prg_ref();
		if ((prg_ref.size() == 0x4000) && addr >= 0x4000) { addr &= 0x3FFF; }
		return prg_ref[static_cast<usize>(addr)];
	}
	return 0;
}
void bus::write_u8(const u16 address, const u8 value) {
	if (address <= 0x1FFF) { this->cpu_vram[address & 0x07FF] = value; }
	if (0x2000 <= address && address <= 0x3FFF) { return; } // PPU
	if (0x8000 <= address && address <= 0xFFFF) { return; }
}