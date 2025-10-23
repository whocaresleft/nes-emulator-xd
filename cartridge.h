#ifndef CARTRIDGE__H
#define CARTRIDGE__H

#include "definitions.h"

class cartridge {

private:
	std::vector<u8> prg_rom;
	std::vector<u8> chr_rom;
	u8 mapper;
	enum mirroring {
		vertical = 0_u8,
		horizontal = 1_u8,
		four_screen = 2_u8
	} screen_mirroring;

public:

	cartridge(std::vector<u8>& raw);
	cartridge(const cartridge& to_copy);
	cartridge(const cartridge&& to_move) noexcept;

	const std::vector<u8>& prg_ref() const;

	~cartridge();
};

#endif