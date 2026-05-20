#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <vector>

namespace crc
{
	inline uint64_t FileHash(const std::filesystem::path& path)
	{
		std::ifstream ifs(path, std::ios::binary);
		if (!ifs) return 0;
		uint64_t hash = 0xCBF29CE484222325ULL;
		char buf[4096];
		while (ifs.read(buf, sizeof(buf)) || ifs.gcount()) {
			for (std::streamsize i = 0; i < ifs.gcount(); ++i) {
				hash ^= static_cast<uint8_t>(buf[i]);
				hash *= 0x100000001B3ULL;
			}
		}
		return hash;
	}

	inline uint64_t ConfigFolderCRC(const std::filesystem::path& folder)
	{
		namespace fs = std::filesystem;
		if (!fs::exists(folder)) return 0;

		std::vector<fs::path> files;
		for (const auto& e : fs::directory_iterator(folder)) {
			if (e.path().extension() == ".ini")
				files.push_back(e.path());
		}
		std::ranges::sort(files);

		uint64_t combined = 0;
		for (const auto& f : files) {
			combined ^= FileHash(f);
			combined *= 0x100000001B3ULL;
		}
		return combined;
	}
}
