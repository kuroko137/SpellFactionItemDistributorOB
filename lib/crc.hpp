#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <string>
#include <vector>

#include "lib/simpleINI.hpp"

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

	struct SectionCRCs {
		uint64_t items = 0;
		uint64_t equipment = 0;
		uint64_t spells = 0;
		uint64_t factions = 0;
		uint64_t packages = 0;
		uint64_t keywords = 0;
	};

	inline uint64_t HashString(const std::string& str)
	{
		uint64_t hash = 0xCBF29CE484222325ULL;
		for (char c : str) {
			hash ^= static_cast<uint8_t>(c);
			hash *= 0x100000001B3ULL;
		}
		return hash;
	}

	inline SectionCRCs ComputeSectionCRCs()
	{
		namespace fs = std::filesystem;

		SectionCRCs crcs{};
		std::vector<fs::path> iniFiles;

		std::string folderPath = R"(Data\OBSE\Plugins\SpellFactionItemDistributor)";
		std::string backupPath = R"(Data\SpellFactionItemDistributor)";

		if (fs::exists(folderPath)) {
			for (const auto& e : fs::directory_iterator(folderPath)) {
				if (e.path().extension() == ".ini")
					iniFiles.push_back(e.path());
			}
		}
		if (iniFiles.empty() && fs::exists(backupPath)) {
			for (const auto& e : fs::directory_iterator(backupPath)) {
				if (e.path().extension() == ".ini")
					iniFiles.push_back(e.path());
			}
		}
		std::ranges::sort(iniFiles);

		auto combine = [](uint64_t hash, uint64_t val) -> uint64_t {
			hash ^= val;
			hash *= 0x100000001B3ULL;
			return hash;
		};

		for (const auto& path : iniFiles) {
			CSimpleIniA ini;
			ini.SetUnicode();
			ini.SetMultiKey();
			ini.SetAllowKeyOnly();
			if (ini.LoadFile(path.string().c_str()) < 0)
				continue;

			CSimpleIniA::TNamesDepend sections;
			ini.GetAllSections(sections);
			sections.sort(CSimpleIniA::Entry::LoadOrder());

			for (const auto& section : sections) {
				std::string sectionName = section.pItem;
				std::string firstToken = sectionName;
				auto pipePos = firstToken.find('|');
				if (pipePos != std::string::npos)
					firstToken = firstToken.substr(0, pipePos);

				std::string content;
				CSimpleIniA::TNamesDepend keys;
				ini.GetAllKeys(sectionName.c_str(), keys);
				keys.sort(CSimpleIniA::Entry::LoadOrder());
				for (const auto& key : keys) {
					content += key.pItem;
					content += '=';
					CSimpleIniA::TNamesDepend values;
					ini.GetAllValues(sectionName.c_str(), key.pItem, values);
					for (const auto& val : values) {
						content += val.pItem;
						content += ';';
					}
				}

				uint64_t sectionHash = HashString(content);

				if (firstToken == "Items")
					crcs.items = combine(crcs.items, sectionHash);
				else if (firstToken == "Equipment" || firstToken == "Equippables")
					crcs.equipment = combine(crcs.equipment, sectionHash);
				else if (firstToken == "Spells")
					crcs.spells = combine(crcs.spells, sectionHash);
				else if (firstToken == "Factions")
					crcs.factions = combine(crcs.factions, sectionHash);
				else if (firstToken == "Packages")
					crcs.packages = combine(crcs.packages, sectionHash);
				else if (firstToken == "Keywords")
					crcs.keywords = combine(crcs.keywords, sectionHash);
			}
		}

		return crcs;
	}
}
