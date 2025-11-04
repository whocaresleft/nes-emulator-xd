#ifndef PPU__H
#define PPU__H

#include "definitions.h"
#include "ppu_registers.h"
#include "cartridge.h"
#include <iostream>
class bus;

class ppu {

private:

	void request_nmi();

	static constexpr u16 MIRRORED_ADDRESSES[3][4] = {
		{ 0x0000, 0x0000, 0x0800, 0x0800 }, // Vertical
		{ 0x0000, 0x0400, 0x0400, 0x0800 }, // Horizontal
		{ 0x0000, 0x0000, 0x0000, 0x0000 }  // Four Screen
	};

	usize scanlines;
	usize cycles;
	ppu_ctrl control;
	ppu_mask mask;
	ppu_status status;

	u16 vram_address, vram_address_temp;
	u8 fine_x;
	bool address_latch;

	u8 oam_address;
	u8 data_buffer;

	bus* cpu_bus;

	enum mirroring mirroring_type;
	std::span<u8> chr_rom;
	std::array<u8, 32> palette_table;
	std::array<u8, 256> oam_memory;
	std::array<u8, 2048> vram;

	u16 mirror_address(const u16 address) const {
		u16 mirrored = address & 0b10111111111111_u16;
		u16 vram_index = mirrored - 0x2000_u16;
		u16 name_table = vram_index >> 10_u16;
		return vram_index - MIRRORED_ADDRESSES[this->mirroring_type][name_table];
	}

	u8 read_ppu_bus(const u16 address) const {
		if (address <= 0x1FFF) { return this->chr_rom[address]; }
		if (address <= 0x2FFF) { return this->vram[this->mirror_address(address)]; }
		return 0;
	}
	void write_ppu_bus(const u16 address, const u8 data) {
		if (0x2000 <= address && address <= 0x2FFF) { this->vram[this->mirror_address(address)] = data; }
	}

	void write_control(const u8 data) {
		bool was_nmi_enabled = this->control.generate_nmi();
		this->control.update(data);
		bool is_nmi_enabled = this->control.generate_nmi();
		if (this->status.is_in_vblank() && !was_nmi_enabled && is_nmi_enabled) {
			this->request_nmi();
		}
	}
	void write_mask(const u8 data) { this->mask.update(data); }
	void write_oam_address(const u8 data) { this->oam_address = data; }
	void write_oam_data(const u8 data) { this->oam_memory[this->oam_address++] = data; }
	void write_scroll(const u8 data) { 
		if (!this->address_latch) { 
			this->fine_x = data & 0b00000111_u8;
			this->vram_address_temp = (this->vram_address_temp & ~0x001F) | (static_cast<u16>(data) >> 3);
		}
		else {
			this->vram_address_temp = (this->vram_address_temp & ~0x7000) | (static_cast<u16>(data & 0b111_u8) << 12);
			this->vram_address_temp = (this->vram_address_temp & ~0x03E0_u16) | ((static_cast<u16>(data) & 0b11111000_u8) << 2);
		}
		this->address_latch = !this->address_latch;
	}
	void write_address(const u8 data) {
		if (!this->address_latch) {
			this->vram_address_temp = (this->vram_address_temp & 0x80FF) | (static_cast<u16>(data & 0x3F) << 8);
			this->address_latch = true;
		}
		else {
			this->vram_address_temp = (this->vram_address_temp & 0xFF00) | static_cast<u16>(data);
			this->vram_address = this->vram_address_temp;
			this->address_latch = false;
		}
	}
	void write_data(const u8 data) {
		u16 address = this->vram_address & 0x3FFF;

		if (address <= 0x3EFF) {
			this->write_ppu_bus(address, data);
		}
		else {
			u16 palette_address = address & 0x000F;
			if ((palette_address & 0x0003) == 0) {
				palette_address = 0x0000;
			}
			this->palette_table[palette_address] = data;
		}

		this->vram_address += this->control.vram_address_increment();
	}

	u8 read_status() {
		u8 data = this->status.snapshot();
		this->status.set_vblank(false);
		this->address_latch = false;
		return data;
	}
	u8 read_oam_data() const {
		return this->oam_memory[this->oam_address];
	}
	u8 read_data() {
		u16 address = this->vram_address = 0x3FFF;
		this->vram_address += this->control.vram_address_increment();

		u8 data_to_return{ 0 }, data_from_vram{ 0 };

		if (address <= 0x3EFF) {
			data_from_vram = this->read_ppu_bus(address);
			data_to_return = this->data_buffer;
			this->data_buffer = data_from_vram;
		}
		else {
			u16 palette_address = address & 0x000F;
			if ((palette_address & 0x0003) == 0) {
				palette_address = 0x0000;
			}
			data_to_return = this->palette_table[palette_address];
			this->data_buffer = this->read_ppu_bus(address - 0x1000);
		}
		return data_to_return;
	}
	void update_scanline() {
		this->cycles -= 341;

		this->scanlines++;

		if (this->scanlines <= (239 + 1)) { // draw
			this->control.update(0b10000000);
			// this->draw_line(this->scanlines - 1) // index is current -1 because f shift
		}
		else if (this->scanlines == (241 + 1)) { // Generate NMI
			this->status.set_vblank(true);
			if (this->control.generate_nmi()) {
				this->request_nmi();
			}
		}
		else if (this->scanlines == (261 + 1)) { // NMI over if still on
			this->status.update(0);
			if (this->mask.is_rendering_enabled())
				this->vram_address = this->vram_address_temp;
			this->scanlines = 0;
		}
	}

public:
	ppu() : 
		vram_address(0),
		vram_address_temp(0),
		fine_x(0),
		address_latch(false),
		cycles(0),
		control(),
		mask(),
		status(),
		oam_address(0),
		data_buffer(0),
		chr_rom({}),
		oam_memory({}),
		vram({}),
		scanlines(0),
		palette_table({}),
		mirroring_type(mirroring::vertical),
		cpu_bus(nullptr)
	{}

	void connect(bus* cpu_bus) { this->cpu_bus = cpu_bus; }

	void load(cartridge* rom) { this->chr_rom = rom->get_chr_rom(); this->mirroring_type = rom->get_mirroring(); }
	void tick(usize cycles) {
		this->cycles += cycles;
		while (this->cycles >= 341) {
			this->update_scanline();
		}
	}

	void write(u8 address, u8 data) {
		switch (address & 7) {
		case 0: this->write_control(data); break;
		case 1: this->write_mask(data); break;
		case 3: this->write_oam_address(data); break;
		case 4: this->write_oam_data(data); break;
		case 5: this->write_scroll(data); break;
		case 6: this->write_address(data); break;
		case 7: this->write_control(data); break;
		default: break;
		}
	}

	u8 read(u8 address, u8 last_bus_value) {
		switch (address & 7) {
		case 2: return this->read_status();
		case 4: return this->read_oam_data();
		case 7: return this->read_data();
		default: return last_bus_value;
		}
	}

	std::span<u8> get_oam_memory() { return std::span<u8>(this->oam_memory); }
	std::span<u8> get_color_palette() { return std::span<u8>(this->palette_table); }
	std::span<u8> get_vram() { return std::span<u8>(this->vram); }

	usize get_cycles() { return this->cycles; }
	usize get_scanlines() { return this->scanlines; }
	ppu_ctrl get_control() { return this->control; }
	ppu_mask get_mask() { return this->mask; }
	ppu_status get_status() { return this->status; }

	u16 get_vram_address() { return this->vram_address; }
	u16 get_vram_address_temp() { return this->vram_address_temp; }
	u8 get_fine_x() { return this->fine_x; }
	bool get_address_latch() { return this->address_latch; }

	u8 get_oam_address() { return this->oam_address; }
	u8 get_data_buffer() { return this->data_buffer; }
};

#endif