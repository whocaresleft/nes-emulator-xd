#ifndef PPU_REGISTERS__H
#define PPU_REGISTERS__H

#include "definitions.h"

typedef struct ppu_ctrl {

	constexpr static u8 nmi_enable				= 0b10000000_u8;
	constexpr static u8 master_slave			= 0b01000000_u8;
	constexpr static u8 sprite_height		    = 0b00100000_u8;
	constexpr static u8 background_tile_select  = 0b00010000_u8;
	constexpr static u8 sprite_tile_select		= 0b00001000_u8;
	constexpr static u8 increment_mode			= 0b00000100_u8;	
	constexpr static u8 nametable_select		= 0b00000011_u8;
	
private:
	constexpr static u16 nametable_offsets[4] = { 0x2000, 0x2400, 0x2800, 0x2C00 };
	constexpr static u8 vram_address_increments[2] = { 1, 32 };
	constexpr static u16 pattern_addresses[2] = { 0x0000, 0x1000 };
	constexpr static u8 sprite_sizes[2] = { 8, 16 };
	constexpr static u8 master_slave_options[2] = { 0, 1 };

	u8 bits;

public:
	ppu_ctrl() : 
		bits(0b00000000_u8)
	{}

	u16 nametable_address() const { return nametable_offsets[this->bits & nametable_select]; }
	u8 vram_address_increment() const { return vram_address_increments[(this->bits & increment_mode) >> 2]; }
	u16 sprite_pattern_address() const { return pattern_addresses[(this->bits & sprite_tile_select) >> 3]; }
	u16 background_pattern_address() const { return pattern_addresses[(this->bits & background_tile_select) >> 4]; }
	u8 sprite_size() const { return sprite_sizes[(this->bits & sprite_height) >> 5]; }
	u8 master_slave_select() const { return master_slave_options[(this->bits & master_slave) >> 6]; }
	bool generate_nmi() const { return this->bits & nmi_enable; }

	void update(const u8 bits) { this->bits = bits; }
	u8 snapshot() const { return this->bits; }
} ppu_ctrl;

typedef struct ppu_scroll {

} ppu_scroll;


typedef struct ppu_mask { 
private:

	constexpr static u8 grayscale						= 0b00000001_u8;
	constexpr static u8 show_background_left			= 0b00000010_u8;
	constexpr static u8 show_sprites_left				= 0b00000100_u8;
	constexpr static u8 enable_background_rendering		= 0b00001000_u8;
	constexpr static u8 enable_sprite_rendering			= 0b00010000_u8;
	constexpr static u8 enhance_blue_green_red			= 0b11100000_u8;
	
	u8 bits;

public:
	ppu_mask():
		bits(0b00000000_u8)
	{ }

	bool is_grayscale() const { return this->bits & grayscale; }
	bool show_background_in_leftmost_8px() const { return this->bits & show_background_left; }
	bool show_sprites_in_leftmost_8px() const { return this->bits & show_sprites_left; }
	bool is_background_rendering_enabled() const { return this->bits & enable_background_rendering; }
	bool is_sprite_rendering_enabled() const { return this->bits & enable_sprite_rendering; }
	u8 color_enhancement_r_g_b() const { return (this->bits & enhance_blue_green_red) >> 5; }

	void update(const u8 bits) { this->bits = bits;}
	u8 snapshot() const { return this->bits; }
} ppu_mask;



typedef struct ppu_status {

private:
	constexpr static u8 vblank			= 0b10000000_u8;
	constexpr static u8 sprite_zero_hit = 0b01000000_u8;
	constexpr static u8 sprite_overflow = 0b00100000_u8;

	u8 bits;

public:
	ppu_status () : 
		bits(0b00000000_u8)
	{}

	void set_vblank(bool value) { 
		if (value) this->bits |= vblank;
		else this->bits &= ~vblank;
	}
	void set_sprite_zero_hit(bool value) {
		if (value) this->bits |= sprite_zero_hit;
		else this->bits &= ~sprite_zero_hit;
	}
	void set_sprite_overflow(bool value) {
		if (value) this->bits |= sprite_overflow;
		else this->bits &= ~sprite_overflow;
	}

	bool is_in_vblank() const { return this->bits & vblank; }
	u8 snapshot() const { return this->bits; }

} ppu_status;

#endif