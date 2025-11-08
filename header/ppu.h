#ifndef PPU__H
#define PPU__H

#include "definitions.h"
#include "ppu_registers.h"
#include "cartridge.h"
#include <iostream>
#include <functional>
#include "palette.h"
class bus;

class ppu {

private:

	std::function<void(ppu&)> tick_callback;
	void request_nmi();

	static constexpr u16 MIRRORED_ADDRESSES[3][4] = {
		{ 0x0000, 0x0400, 0x0000, 0x0400 }, //{ 0x0000, 0x0400, 0x0400, 0x0800 }, // Vertical
		{ 0x0000, 0x0000, 0x0400, 0x0400 }, //{ 0x0000, 0x0000, 0x0800, 0x0800 }, // Horizontal
		{ 0x0000, 0x0000, 0x0000, 0x0000 }//{ 0x0000, 0x0000, 0x0000, 0x0000 }  // Four Screen
	};

	usize scanlines;
	usize cycles;
	ppu_ctrl control;
	ppu_mask mask;
	ppu_status status;

	std::atomic<bool> frame_ready;

	u16 vram_address, vram_address_temp;
	u8 fine_x;
	bool address_latch;

	u8 oam_address;
	u8 data_buffer;

	bus* cpu_bus;

	std::span<u32> pixel_buffer_last;
	std::span<u32> pixel_buffer_current;

	enum mirroring mirroring_type;
	std::span<u8> chr_rom;
	std::array<u8, 32> palette_table;
	std::array<u8, 256> oam_memory;
	std::array<u8, 2048> vram;

	u8 latch_attribute;
	u8 latch_nametable;
	u8 latch_pattern_lo;
	u8 latch_pattern_hi;
	u16 bg_shift_pattern_lo, bg_shift_pattern_hi;
	u16 bg_shift_attrib_lo, bg_shift_attrib_hi;


	u16 mirror_address(const u16 address) const {
		u16 vram_index = address - 0x2000;
		u16 name_table = vram_index >> 10;
		u16 offset = vram_index & 0x03FF;
		u16 base = MIRRORED_ADDRESSES[this->mirroring_type][name_table];
		return base + offset;
	}

	u8 read_ppu_bus(const u16 address) const {
		if (address <= 0x1FFF) { return this->chr_rom.empty() ? 0 : this->chr_rom[address & (this->chr_rom.size() - 1)]; }
		if (address <= 0x3EFF) { return this->vram[this->mirror_address(address & 0x2FFF)]; }
		if (address <= 0x3FFF) {
			u16 palette_address = address & 0x001F;
			if ((palette_address & 0x0003) == 0) {
				palette_address &= 0x000F;
			}
			return this->palette_table[palette_address]; 
		}
		return 0;
	}
	void write_ppu_bus(const u16 address, const u8 data) {
		if (address <= 0x1FFF) { 
			if (this->chr_rom.empty()) 
				return; 
			//std::cout << "SCRITTURA CHR-RAM: Addr=" << std::hex << address
			//	<< " Data=" << (int)data << std::dec << std::endl;
			this->chr_rom[address & (this->chr_rom.size() - 1)] = data; 
		}
		else if (address <= 0x3EFF) { this->vram[this->mirror_address(address & 0x2FFF)] = data; }
		else if (address <= 0x3FFF) {
			u16 palette_address = address & 0x001F;
			if ((palette_address & 0x0003) == 0) {
				palette_address &= 0x000F;
			}
			this->palette_table[palette_address] = data;
		}
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
			//std::cout << std::hex << "I'm about to write at address " << address << " the following data: " << static_cast<int>(data) << std::dec << std::endl;
			this->write_ppu_bus(address, data);
		}
		else {
			u16 palette_address = address & 0x001F;
			if ((palette_address & 0x0003) == 0) {
				palette_address &= 0x000F;
			}
			//std::cout << "Sto per scrivere 0x" << std::hex << static_cast<int>(data) << " nella palette table all'indirizzo 0x" << static_cast<int>(palette_address) << std::dec << std::endl;
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
			u16 palette_address = address & 0x001F;
			if ((palette_address & 0x0003) == 0) {
				palette_address &= 0x000F;
			}
			data_to_return = this->palette_table[palette_address];
			this->data_buffer = this->read_ppu_bus(address - 0x1000);
		}
		return data_to_return;
	}
	void update_scanline() {
		this->cycles -= 341;
		
		if (this->scanlines <= 239) { // draw
			auto offset = 256 * this->scanlines;
			this->draw_line(this->pixel_buffer_current.subspan(offset, 256));
		}
		else if (this->scanlines == 241) { // Generate NMI
			this->status.set_vblank(true);
			
			this->swap_buffers();
			this->frame_ready = true;

			if (this->control.generate_nmi()) {
				this->request_nmi();
			}
		}

		else if (this->scanlines == 261) {
			this->status.update(0);
			
			if (this->mask.is_rendering_enabled())
				this->vram_address = this->vram_address_temp;
		}

		this->scanlines++;
		if (this->scanlines > 261) {
			this->scanlines = 0;
		}
	}

	void fill_with_backdrop_color(std::span<u32> target_row) {
		u32 backdrop_color = this->get_color_from_palette(0);
		for (auto& x : target_row)
			x = backdrop_color;
	}

	void draw_line(std::span<u32> target_row) {

		if (!this->mask.is_rendering_enabled()) {
			this->fill_with_backdrop_color(target_row);
			//std::cout << "Fillato con sfondo" << std::endl;
			return;
		}
		
		for (int z = 0; z < 2; ++z) {
			this->latch_nametable = this->read_ppu_bus(0x2000 | (this->vram_address & 0x0FFF));
			this->latch_attribute = this->read_ppu_bus(this->calculate_attr_address(this->vram_address));
			this->latch_pattern_lo = this->read_ppu_bus(this->calculate_patt_address(this->vram_address, this->latch_nametable, 0));
			this->latch_pattern_hi = this->read_ppu_bus(this->calculate_patt_address(this->vram_address, this->latch_nametable, 8));
			this->load_shift_registers();
			this->increment_coarse_x(this->vram_address);
		}


		for (usize cycle = 0; cycle < 256; ++cycle) {
		
			u8 final_palette_index = 0;
			if (this->mask.is_background_rendering_enabled()) {
				u16 bit_selector = 0x8000 >> this->fine_x;

				u8 bit0 = (this->bg_shift_pattern_lo & bit_selector) > 0;
				u8 bit1 = (this->bg_shift_pattern_hi & bit_selector) > 0;
				u8 pattern_bits = (bit1 << 1) | bit0;

				u8 bit_attr0 = (this->bg_shift_attrib_lo & bit_selector) > 0;
				u8 bit_attr1 = (this->bg_shift_attrib_hi & bit_selector) > 0;
				u8 attribute_bits = (bit_attr1 << 1) | bit_attr0;

				final_palette_index = (attribute_bits << 2) | pattern_bits;
			}
			auto color = this->get_color_from_palette(final_palette_index);
			//std::cout << std::hex << "Sto per scrivere " << color << " nel pixel_buffer, alla posizione: " << (this->scanlines * 256 + cycle) << std::dec << std::endl;
			//std::cout << "PPU Draw: Idx=" << static_cast<int>(final_palette_index)
			//	<< " -> Color=" << std::hex << color << ", this: ";
				
			target_row[cycle] = color;
			//std:: cout << target_row[cycle] << std::dec << std::endl;

			this->bg_shift_pattern_lo <<= 1;
			this->bg_shift_pattern_hi <<= 1;
			this->bg_shift_attrib_lo <<= 1;
			this->bg_shift_attrib_hi <<= 1;

			switch (cycle & 7) {
			case 1:
				this->latch_nametable = this->read_ppu_bus(0x2000 | (this->vram_address & 0x0FFF));
				break;
			case 3:
				this->latch_attribute = this->read_ppu_bus(this->calculate_attr_address(this->vram_address));
				break;
			case 5:
				this->latch_pattern_lo = this->read_ppu_bus(this->calculate_patt_address(this->vram_address, this->latch_nametable, 0));
				break;
			case 7:
				this->latch_pattern_hi = this->read_ppu_bus(this->calculate_patt_address(this->vram_address, this->latch_nametable, 8));
				break;
			case 0:
				this->load_shift_registers();
				this->increment_coarse_x(this->vram_address);
				break;
			}
		}
		this->increment_y(this->vram_address);
		this->recreate_x(this->vram_address, this->vram_address_temp);
	}

	void load_shift_registers() {
		this->bg_shift_pattern_lo = (this->bg_shift_pattern_lo & 0xFF00) | this->latch_pattern_lo;
		this->bg_shift_pattern_hi = (this->bg_shift_pattern_hi & 0xFF00) | this->latch_pattern_hi;
		this->load_attribute_shift_registers(this->latch_attribute);
	}

	void recreate_x(u16& v, u16 t) {
		v = (v & 0x7BE0) | (t & 0x041F);
	}

	void increment_y(u16& v) {
		if ((v & 0x7000) != 0x7000) {
			v += 0x1000;
		}
		else {
			v &= ~0x7000;
			u16 coarse_y = (v & 0x03E0) >> 5;
			if (coarse_y == 29) {
				coarse_y = 0;
				v ^= 0x0800;
			}
			else if (coarse_y == 31) {
				coarse_y = 0;
			}
			else {
				coarse_y++;
			}
			v = (v & ~0x03E0) | (coarse_y << 5);
		}
	}

	void load_attribute_shift_registers(u8& latch_attr) {
		u8 quadrant_shift = ((this->vram_address >> 4) & 0x04) | (this->vram_address & 0x02);
		u8 palette_bits = (this->latch_attribute >> quadrant_shift) & 0x03;
		if (palette_bits & 0x01) {
			this->bg_shift_attrib_lo = (this->bg_shift_attrib_lo & 0xFF00) | 0x00FF;
		}
		else {
			this->bg_shift_attrib_lo = (this->bg_shift_attrib_lo & 0xFF00) | 0x0000;
		}

		if (palette_bits & 0x02) {
			this->bg_shift_attrib_hi = (this->bg_shift_attrib_hi & 0xFF00) | 0x00FF;
		}
		else {
			this->bg_shift_attrib_hi = (this->bg_shift_attrib_hi & 0xFF00) | 0x0000;
		}
	}
	void increment_coarse_x(u16& v) {
		if ((v & 0x001F) == 31) {
			v &= ~0x001F;
			v ^= 0x0400;
		}
		else {
			v++;
		}
	}

	u16 calculate_patt_address(u16& v, u8 latch_nametable, u8 off) {
		u16 pattern_base = this->control.background_pattern_address();
		u16 tile_offset = this->latch_nametable * 16;
		u16 fine_y = (v >> 12) & 0x07;
		return pattern_base + tile_offset + fine_y + off;
	}

	u16 calculate_attr_address(u16 v) {
		u16 nametable_base = (v & 0x0C00);
		u16 attr_y_offset = (v >> 4) & 0x38;
		u16 attr_x_offset = (v >> 2) & 0x07;

		return 0x23C0 | nametable_base | attr_y_offset | attr_x_offset;
	}

	u32 get_color_from_palette(usize index) {
		if ((index & 3) == 0)
			index = 0;

		u8 nes_color_index = this->palette_table[index & 0x1F];
		return NES_HARDWARE_PALETTE[nes_color_index & 0x3F];
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
		scanlines(261),
		palette_table({}),
		tick_callback(nullptr),
		mirroring_type(mirroring::vertical),
		pixel_buffer_last({}),
		pixel_buffer_current({}),
		cpu_bus(nullptr),
		frame_ready(false),

		latch_attribute(0),
		latch_nametable(0),
		latch_pattern_lo(0),
		latch_pattern_hi(0),
		bg_shift_pattern_lo(0),
		bg_shift_pattern_hi(0),
		bg_shift_attrib_lo(0),
		bg_shift_attrib_hi(0)
	{
	}

	void set_pixel_buffers(std::vector<u32> buffer1, std::vector<u32> buffer2) {
		this->pixel_buffer_current = std::span<u32>(buffer1);
		this->pixel_buffer_last = std::span<u32>(buffer2);
	}

	template<typename C>
	void set_tick_callback(C&& callback) {
		this->tick_callback = callback;
	}

	void swap_buffers() {
		std::swap(this->pixel_buffer_current, this->pixel_buffer_last);
	}

	void connect(bus* cpu_bus) { this->cpu_bus = cpu_bus; }

	void load(cartridge* rom) { this->chr_rom = rom->get_chr_rom(); this->mirroring_type = rom->get_mirroring(); }
	void tick(usize cycles) {
		this->cycles += cycles;
		while (this->cycles >= 341) {
			this->update_scanline();
		}
		if (this->tick_callback) this->tick_callback(*this);
	}

	void write(u8 address, u8 data) {
		switch (address & 7) {
		case 0: this->write_control(data); break;
		case 1: this->write_mask(data); break;
		case 3: this->write_oam_address(data); break;
		case 4: this->write_oam_data(data); break;
		case 5: this->write_scroll(data); break;
		case 6: this->write_address(data); break;
		case 7: this->write_data(data); break;
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

	bool is_frame_ready() { return this->frame_ready.load(); }
	void reset_frame_ready() { this->frame_ready.store(false); }
	std::span<u32> get_last_screen() { 
		return this->pixel_buffer_last;
	}
};

#endif