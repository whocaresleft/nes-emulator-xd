#ifndef CARTRIDGE__H
#define CARTRIDGE__H

#include <span>
#include <ranges>
#include <algorithm>
#include "definitions.h"

typedef enum mirroring {
	vertical = 0_u8,
	horizontal = 1_u8,
	four_screen = 2_u8
} mirroring;

class cartridge {

public:
	constexpr static const char* SCREEN_MIRRORING_NAMES[3] = {
		"Vertical", "Horizontal", "Four Screen"
	};

private:
	std::vector<u8> prg_rom;
	std::vector<u8> chr_rom;
	u8 mapper;
	enum mirroring screen_mirroring;


public:
	cartridge(std::vector<u8>& raw) :
		mapper(0),
		screen_mirroring(mirroring::vertical),
		prg_rom(),
		chr_rom()
	{
		constexpr std::array<const u8, 4> nes_tag({ 0x4e_u8, 0x45_u8, 0x53_u8, 0x1a_u8 });
		constexpr usize prg_rom_size = 0x4000_usize;
		constexpr usize chr_rom_size = 0x2000_usize;

		std::span<const u8, 4> tag(raw.begin(), raw.begin() + 4);

		if (!std::ranges::equal(tag, nes_tag)) { return; }
		//std::cout << "TAG CORRECT" << " ";

		u8 mapper = (raw[7] & 0b11110000) | (raw[6] >> 4);
		if (((raw[7] >> 2) & 0b11) != 0) { return; }
		//std::cout << "INES ver CORRECT" << " ";

		bool four_screen = 0 != (raw[6] & 0b1000);
		bool vertical_mirroring = 0 != (raw[6] & 0b1);
		enum mirroring screen_mirroring =
			four_screen ? (mirroring::four_screen)
			: (vertical_mirroring ? mirroring::vertical
				: mirroring::horizontal
				);

		usize prg_size = static_cast<usize>(raw[4]) * prg_rom_size;
		usize chr_size = static_cast<usize>(raw[5]) * chr_rom_size;
		bool skip_trainter = 0 != (raw[6] & 0b100);

		usize prg_start = 16 + (skip_trainter ? 512 : 0);
		usize chr_start = prg_start + prg_size;

		this->prg_rom = std::vector<u8>{ raw.begin() + prg_start, raw.begin() + prg_start + prg_size };
		this->chr_rom = std::vector<u8>{ raw.begin() + chr_start, raw.begin() + chr_start + chr_size };
		this->mapper = mapper;
		this->screen_mirroring = screen_mirroring;
		//std::cout << ": [ prg:" << prg_rom.size() << " - chr:" << chr_rom.size() << " - mapper:" << static_cast<uint32_t>(mapper) << " - mirroring:" << static_cast<int>(screen_mirroring) << " ]" << std::endl;
	}
	cartridge(const cartridge& to_copy) : 
		mapper(to_copy.mapper),
		screen_mirroring(to_copy.screen_mirroring),
		prg_rom(to_copy.prg_rom),
		chr_rom(to_copy.chr_rom)
	{}
	cartridge(const cartridge&& to_move) noexcept = delete;

	std::span<u8> get_prg_rom() { return std::span<u8>(this->prg_rom); }
	std::span<u8> get_chr_rom() { return std::span<u8>(this->chr_rom); }
	u8 get_mapper() { return this->mapper;  }
	enum mirroring get_mirroring() { return this->screen_mirroring; }

	~cartridge() { }
};

#endif