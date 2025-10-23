//#include "SDL.h"
#include <iostream>
#include <filesystem>
#include "cpu.h"
#include <vector>
#include <fstream>

std::vector<u8> read_file(const std::string& path);

int main(int argc, char* argv[]) {

	std::vector<u8> game = read_file("nestest.txt");

	cartridge* rom = new cartridge(game);
	cpu my_cpu = cpu();
	my_cpu.load(rom);
	my_cpu.reset();

	std::ofstream log("nestest.log");
	if (!log.is_open()) {
		std::cerr << "OOPS";
		return 1;
	}
	my_cpu.run_with_callback_first([&log](cpu& my_ref) { log << my_ref.trace() << std::endl; });

	delete rom;

	std::cout << "Success {" << int(my_cpu.read_u8(0x6000)) << "}" << std::endl;
	return 0;
}

#include <fstream>
#include <cstdint>
#include <string>
#include <iterator>
std::vector<u8> read_file(const std::string& path) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) throw std::runtime_error("Cannot open file");

	std::streamsize size = std::filesystem::file_size(path);
	file.seekg(0, std::ios::beg);

	std::vector<u8> buffer(size);
	if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
		throw std::runtime_error("Error reading file");

	return buffer;
}