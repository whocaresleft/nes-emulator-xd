#ifndef UTILITY__H
#define UTILITY__H

#include <string>
#include <fstream>
#include <iterator>
#include <filesystem>
#include <cstdint>
std::vector<uint8_t> read_file(const std::string& path) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) throw std::runtime_error("Cannot open file");

	std::streamsize size = std::filesystem::file_size(path);
	file.seekg(0, std::ios::beg);

	std::vector<uint8_t> buffer(size);
	if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
		throw std::runtime_error("Error reading file");

	return buffer;
}

#endif