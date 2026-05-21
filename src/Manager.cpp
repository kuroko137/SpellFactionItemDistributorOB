#include "Manager.h"
#include "EditorIDMapper/EditorIDMapperAPI.h"
#include "OBSEKeywords/KeywordAPI.h"
#include "fstream"
#include "lib/boost/trim.hpp"

extern OBSEScriptInterface* g_script;

namespace SpellFactionItemDistributor
{
	FormCode GetFormCodeFromString(std::string formString) {
		if (formString == "Forms") return form;
		if (formString == "Spells") return spell;
		if (formString == "Factions") return faction;
		if (formString == "Equipment" || formString == "Equippables") return equippable;
		if (formString == "Packages") return package;
		if (formString == "Items") return item;
		if (formString == "Keywords") return keyword;
	}

	FormMap<SwapDataVec>& Manager::get_form_vec(const std::string& a_str)
	{
		switch (GetFormCodeFromString(a_str))
		{
		case (item): {
			return allItems;
			break;
		}
		case (equippable): {
			return allEquipment;
			break;
		}
		case (spell): {
			return allSpells;
			break;
		}
		case (faction): {
			return allFactions;
			break;
		}
		case (package): {
			return allPackages;
			break;
		}
		case (keyword):
		{
			return allKeywords;
			break;
		}
		default:
			return allItems;
			break;
		}
	}

	ConditionalFormMap& Manager::get_form_map(const std::string& a_str)
	{
		switch (GetFormCodeFromString(a_str))
		{
		case (item): {
			return allItemsConditional;
			break;
		}
		case (equippable): {
			return allEquipmentConditional;
			break;
		}
		case (spell): {
			return allSpellsConditional;
			break;
		}
		case (faction): {
			return allFactionsConditional;
			break;
		}
		case (package): {
			return allPackagesConditional;
			break;
		}
		case (keyword):
		{
			return allKeywordsConditional;
			break;
		}
		default:
			break;
		}
	}

	ConditionalFormMap& Manager::get_form_map_all(const std::string& a_str)
	{
		switch (GetFormCodeFromString(a_str))
		{
		case (item): {
			return applyToAllItems;
			break;
		}
		case (equippable): {
			return applyToAllEquipment;
			break;
		}
		case (spell): {
			return applyToAllSpells;
			break;
		}
		case (faction): {
			return applyToAllFactions;
			break;
		}
		case (package): {
			return applyToAllPackages;
			break;
		}
		case (keyword):
		{
			return applyToAllKeywords;
			break;
		}
		default:
			break;
		}
	}

	void Manager::get_forms(
		const std::string& a_path,
		const std::string& a_str,
		const std::vector<CompiledCondition>& conditions,
		std::string formType)
	{
		auto& formMap = get_form_map(formType);

		DistributeRecordData::GetForms(a_path, a_str,
			[&, conditions](UInt32 a_baseID, const DistributeRecordData& swapData) {
				auto& entries = formMap[a_baseID];

				ConditionalEntry entry;

				entry.conditions = conditions;

				entry.swapData.push_back(swapData);

				entries.push_back(std::move(entry));
			});
	}


	void Manager::get_forms_all(
		const std::string& a_path,
		const std::string& a_str,
		const std::vector<CompiledCondition>& conditions,
		std::string formType)
	{
		auto& formMap = get_form_map_all(formType);

		DistributeRecordData::GetForms(a_path, a_str,
			[&](UInt32 a_baseID, const DistributeRecordData& swapData) {
				auto& entries = formMap[a_baseID];

				ConditionalEntry entry;
				entry.conditions = conditions;
				entry.swapData.push_back(swapData);

				entries.push_back(std::move(entry));
			});
	}

	static bool HasKeywordCell(TESObjectCELL* cell,
		const CompiledCondition& cond)
	{
		if (!cell)
			return false;

		bool match = false;

		if (cond.formID != 0)
		{
			match = (cell->refID == cond.formID);
		}
		else
		{
			const char* id = EditorIDMapper::ReverseLookup(cell->refID);
			if (!id) return false;
			std::string editorID = id;
			std::transform(editorID.begin(), editorID.end(),
				editorID.begin(), ::tolower);

			match = (editorID.find(cond.text) != std::string::npos);
		}

		return cond.isExclusion ? !match : match;
	}


	static bool HasKeywordWorldspace(TESObjectCELL* cell,
		const CompiledCondition& cond)
	{
		if (!cell || !cell->worldSpace)
			return false;

		bool match = false;

		if (cond.formID != 0)
		{
			match = (cell->worldSpace->refID == cond.formID);
		}
		else
		{
			const char* worldspaceEditorID = EditorIDMapper::ReverseLookup(cell->worldSpace->refID);
			if (!worldspaceEditorID) return false;
			std::string editorID = worldspaceEditorID;
			std::transform(editorID.begin(), editorID.end(),
				editorID.begin(), ::tolower);

			match = (editorID.find(cond.text) != std::string::npos);
		}

		return cond.isExclusion ? !match : match;
	}


	static bool HasKeywordRegion(TESObjectCELL* cell,
		const CompiledCondition& cond)
	{
		if (!cell)
			return false;

		auto* regionList =
			static_cast<ExtraRegionList*>(
				cell->extraData.GetByType(kExtraData_RegionList));

		if (!regionList || !regionList->regionList)
			return false;

		bool match = false;

		TESRegionList::Entry* regionPtr =
			&regionList->regionList->regionList;

		while (regionPtr)
		{
			TESRegion* region = regionPtr->region;
			if (!region)
				break;

			if (cond.formID != 0)
			{
				if (region->refID == cond.formID)
				{
					match = true;
					break;
				}
			}
			else
			{
				const char* id = EditorIDMapper::ReverseLookup(region->refID);
				if (!id) return false;
				std::string editorID = id;
				std::transform(editorID.begin(), editorID.end(),
					editorID.begin(), ::tolower);

				if (editorID.find(cond.text) != std::string::npos)
				{
					match = true;
					break;
				}
			}

			regionPtr = regionPtr->next;
		}

		return cond.isExclusion ? !match : match;
	}


	static bool HasKeywordEditorID(TESObjectREFR* ref,
		const CompiledCondition& cond)
	{
		if (!ref || !ref->baseForm)
			return false;

		bool match = false;

		if (cond.formID != 0)
		{
			match = (ref->baseForm->refID == cond.formID);
		}
		else
		{
			const char* refEditorID = EditorIDMapper::ReverseLookup(ref->baseForm->refID);
			if (!refEditorID) return false;
			std::string editorID = refEditorID;
			std::transform(editorID.begin(), editorID.end(),
				editorID.begin(), ::tolower);

			match = (editorID.find(cond.text) != std::string::npos);
		}

		return cond.isExclusion ? !match : match;
	}


	static bool HasKeywordName(TESObjectREFR* ref,
		const CompiledCondition& cond)
	{
		if (!ref || !ref->baseForm || !ref->baseForm->GetFullName())
			return false;

		std::string name = ref->baseForm->GetFullName()->name.m_data;
		std::transform(name.begin(), name.end(), name.begin(), ::tolower);

		bool match = (name.find(cond.text) != std::string::npos);

		return cond.isExclusion ? !match : match;
	}


	static bool HasKeywordRace(TESObjectREFR* ref,
		const CompiledCondition& cond)
	{
		if (!ref)
			return false;

		auto* actor = dynamic_cast<TESActorBase*>(ref->baseForm);
		if (!actor)
			return false;

		auto* npc = dynamic_cast<TESNPC*>(actor);
		if (!npc || !npc->race.race)
			return false;

		bool match = false;

		if (cond.formID != 0)
		{
			match = (npc->race.race->refID == cond.formID);
		}
		else
		{
			const char* id = EditorIDMapper::ReverseLookup(npc->race.race->refID);
			if (!id) return false;
			std::string editorID = id;
			std::transform(editorID.begin(), editorID.end(),
				editorID.begin(), ::tolower);

			match = (editorID.find(cond.text) != std::string::npos);
		}

		return cond.isExclusion ? !match : match;
	}


	static bool HasKeywordFaction(TESObjectREFR* ref,
		const CompiledCondition& cond)
	{
		if (!ref)
			return false;

		auto* actor = dynamic_cast<TESActorBase*>(ref->baseForm);
		if (!actor)
			return false;

		bool match = false;

		auto* entry = &actor->actorBaseData.factionList;

		while (entry && entry->data)
		{
			TESFaction* faction = entry->data->faction;

			if (cond.formID != 0)
			{
				if (faction->refID == cond.formID)
				{
					match = true;
					break;
				}
			}
			else
			{
				const char* id = EditorIDMapper::ReverseLookup(faction->refID);
				if (!id) return false;
				std::string editorID = id;
				std::transform(editorID.begin(), editorID.end(),
					editorID.begin(), ::tolower);

				if (editorID.find(cond.text) != std::string::npos)
				{
					match = true;
					break;
				}
			}

			entry = entry->Next();
		}

		return cond.isExclusion ? !match : match;
	}


	static bool HasKeywordClass(
		TESObjectREFR* ref,
		const CompiledCondition& cond)
	{
		if (!ref || !ref->baseForm)
			return false;

		auto* actor = dynamic_cast<TESActorBase*>(ref->baseForm);
		if (!actor)
			return false;

		auto* npc = dynamic_cast<TESNPC*>(actor);
		if (!npc || !npc->npcClass)
			return false;

		const UInt32 classFormID = npc->npcClass->refID;

		bool matched = false;

		if (cond.formID != 0)
		{
			matched = (cond.formID == classFormID);
		}
		else if (!cond.text.empty())
		{
			const char* id = EditorIDMapper::ReverseLookup(npc->npcClass->refID);
			if (!id) return false;
			std::string classEditorID = id;

			std::string keyLower = cond.text;
			std::string editorLower = classEditorID;

			std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);
			std::transform(editorLower.begin(), editorLower.end(), editorLower.begin(), ::tolower);

			matched = (editorLower.find(keyLower) != std::string::npos);
		}

		return cond.isExclusion ? !matched : matched;
	}


	static bool HasKeywordItem(TESObjectREFR* ref,
		const CompiledCondition& cond)
	{
		if (!ref)
			return false;

		TESContainer* container = ref->GetContainer();
		if (!container)
			return false;

		bool match = false;

		TESContainer::Entry* entry = &container->list;

		while (entry && entry->data)
		{
			TESForm* form = entry->data->type;

			if (!form)
			{
				entry = entry->Next();
				continue;
			}

			if (cond.formID != 0)
			{
				if (form->refID == cond.formID)
				{
					match = true;
					break;
				}
			}
			else
			{
				const char* id = EditorIDMapper::ReverseLookup(form->refID);
				if (!id) return false;
				std::string editorID = id;
				std::transform(editorID.begin(), editorID.end(),
					editorID.begin(), ::tolower);

				if (editorID.find(cond.text) != std::string::npos)
				{
					match = true;
					break;
				}
			}

			entry = entry->Next();
		}

		return cond.isExclusion ? !match : match;
	}


	static bool HasKeywordMod(TESObjectREFR* ref,
		const CompiledCondition& cond)
	{
		if (!ref || !ref->baseForm)
			return false;

		UInt8 modIndex = ref->baseForm->GetModIndex();
		std::string modName =
			(*g_dataHandler)->GetNthModName(modIndex);

		std::transform(modName.begin(), modName.end(),
			modName.begin(), ::tolower);

		bool match = false;

		if (cond.formID != 0)
		{
			match = false;
		}
		else
		{
			match = (modName.find(cond.text) != std::string::npos);
		}

		return cond.isExclusion ? !match : match;
	}

	static bool HasKeywordKeyword(TESObjectREFR* ref,
		const CompiledCondition& cond)
	{
		if (!ref || !ref->baseForm)
			return false;

		bool match = false;

		const char* keyword = cond.text.c_str();

		if (!keyword) return false;

		bool hasKeywordBase = KeywordAPI::HasKeyword(ref->baseForm->refID, keyword);
		bool hasKeywordRef = KeywordAPI::HasKeyword(ref->refID, keyword);

		match = (hasKeywordBase || hasKeywordRef);

		return cond.isExclusion ? !match : match;
	}


	static bool HasKeywordSex(TESObjectREFR* ref,
		const CompiledCondition& cond)
	{
		if (!ref || !ref->baseForm)
			return false;

		auto* actor = dynamic_cast<TESActorBase*>(ref->baseForm);
		if (!actor)
			return false;

		auto* npc = dynamic_cast<TESNPC*>(actor);
		if (!npc)
			return false;

		bool isFemale = npc->actorBaseData.IsFemale();

		bool match = false;
		if (cond.text == "female")
			match = isFemale;
		else if (cond.text == "male")
			match = !isFemale;

		return cond.isExclusion ? !match : match;
	}

	static bool HasKeywordActorType(TESObjectREFR* ref,
		const CompiledCondition& cond)
	{
		if (!ref || !ref->baseForm)
			return false;

		auto* actor = dynamic_cast<TESActorBase*>(ref->baseForm);
		if (!actor)
			return false;

		bool isNPC = (dynamic_cast<TESNPC*>(actor) != nullptr);
		bool isCreature = (dynamic_cast<TESCreature*>(actor) != nullptr);

		bool match = false;
		if (cond.text == "npc")
			match = isNPC;
		else if (cond.text == "creature")
			match = isCreature;

		return cond.isExclusion ? !match : match;
	}

	bool IsValid(const CompiledCondition& cond,
		TESObjectREFR* ref)
	{
		switch (cond.type)
		{
		case ConditionType::All:
			return true;

		case ConditionType::ActorType:
			return HasKeywordActorType(ref, cond);

		case ConditionType::Cell:
			return HasKeywordCell(ref->parentCell, cond);

		case ConditionType::Worldspace:
			return HasKeywordWorldspace(ref->parentCell, cond);

		case ConditionType::Region:
			return HasKeywordRegion(ref->parentCell, cond);

		case ConditionType::EditorID:
			return HasKeywordEditorID(ref, cond);

		case ConditionType::Race:
			return HasKeywordRace(ref, cond);

		case ConditionType::Faction:
			return HasKeywordFaction(ref, cond);

		case ConditionType::Item:
			return HasKeywordItem(ref, cond);

		case ConditionType::Name:
			return HasKeywordName(ref, cond);

		case ConditionType::Mod:
			return HasKeywordMod(ref, cond);

		case ConditionType::Class:
			return HasKeywordClass(ref, cond);

		case ConditionType::Keyword:
			return HasKeywordKeyword(ref, cond);

		case ConditionType::Sex:
			return HasKeywordSex(ref, cond);

		default:
			return false;
		}
	}


	bool IsValidAll(
		const std::vector<CompiledCondition>& conditions,
		TESObjectREFR* ref)
	{
		if (!ref)
			return false;

		for (const auto& cond : conditions)
		{
			if (!IsValid(cond, ref))
				return false;
		}

		return true;
	}


	CompiledCondition CompileCondition(const std::string& rawCondition)
	{
		CompiledCondition compiled{};

		if (rawCondition.empty())
			return compiled;

		std::string condition = rawCondition;
		boost::trim(condition);

		if (!condition.empty() && condition.front() == '-')
		{
			compiled.isExclusion = true;
			condition.erase(condition.begin());
			boost::trim(condition);
		}

		if (string::iequals(condition, "ALL"))
		{
			compiled.type = ConditionType::All;
			return compiled;
		}

		const auto colonPos = condition.find(':');

		std::string typeStr;
		std::string valueStr;

		if (colonPos != std::string::npos)
		{
			typeStr = condition.substr(0, colonPos);
			valueStr = condition.substr(colonPos + 1);

			boost::trim(typeStr);
			boost::trim(valueStr);
		}
		else
		{
			typeStr = "editorid";
			valueStr = condition;
		}
		std::transform(typeStr.begin(), typeStr.end(), typeStr.begin(), ::tolower);
		std::transform(valueStr.begin(), valueStr.end(), valueStr.begin(), ::tolower);

		if (typeStr == "cell")     compiled.type = ConditionType::Cell;
		else if (typeStr == "race")     compiled.type = ConditionType::Race;
		else if (typeStr == "class")    compiled.type = ConditionType::Class;
		else if (typeStr == "faction")  compiled.type = ConditionType::Faction;
		else if (typeStr == "item")     compiled.type = ConditionType::Item;
		else if (typeStr == "name")     compiled.type = ConditionType::Name;
		else if (typeStr == "mod")      compiled.type = ConditionType::Mod;
		else if (typeStr == "editorid")      compiled.type = ConditionType::EditorID;
		else if (typeStr == "keyword")      compiled.type = ConditionType::Keyword;
		else if (typeStr == "sex")         compiled.type = ConditionType::Sex;
		else if (typeStr == "actortype")   compiled.type = ConditionType::ActorType;
		else                            compiled.type = ConditionType::EditorID;

		if (UInt32 id = DistributeRecordData::GetFormID(valueStr.c_str()); id != 0)
		{
			compiled.formID = id;
			compiled.text.clear();
		}
		else
		{
			compiled.formID = 0;
			compiled.text = std::move(valueStr);
		}

		return compiled;
	}

	void Manager::LoadFormsOnce()
	{
		bool expected = false;
		if (formsLoaded.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
			LoadForms();
		}
	}

	void Manager::LoadForms()
	{
		allItemsConditional.clear();
		applyToAllItems.clear();
		allEquipmentConditional.clear();
		applyToAllEquipment.clear();
		allSpellsConditional.clear();
		applyToAllSpells.clear();
		allFactionsConditional.clear();
		applyToAllFactions.clear();
		allPackagesConditional.clear();
		applyToAllPackages.clear();
		allKeywordsConditional.clear();
		applyToAllKeywords.clear();
		_MESSAGE("-INI-");

		std::string sfidFolderPath = R"(Data\OBSE\Plugins\SpellFactionItemDistributor)";
		std::string sfidFolderBackupPath = R"(Data\SpellFactionItemDistributor)";

		const std::filesystem::path sfidFolder{ sfidFolderPath };
		const std::filesystem::path sfidFolderBackup{ sfidFolderBackupPath };
		if (!exists(sfidFolder) && !exists(sfidFolderBackup)) {
			_WARNING("SFID folder not found...");
			return;
		}

		std::vector<std::string> configs = dist::get_configs(sfidFolderPath);

		if (configs.empty() && std::filesystem::exists(sfidFolderBackupPath)) {
			_WARNING("No .ini files were found in Data\\OBSE\\Plugins\\SpellFactionItemDistributor folder, falling back to Data\\SpellFactionItemDistributor...");
			configs = dist::get_configs(sfidFolderBackupPath);
			if (configs.empty())
			{
				_WARNING("No .ini files were found in Data\\SpellFactionItemDistributor folder, aborting...");
				return;
			}
		}

		_MESSAGE("%u matching inis found", configs.size());

		for (auto& path : configs) {
			_MESSAGE("\tINI : %s", path.c_str());

			CSimpleIniA ini;
			ini.SetUnicode();
			ini.SetMultiKey();
			ini.SetAllowKeyOnly();

			if (const auto rc = ini.LoadFile(path.c_str()); rc < 0) {
				_ERROR("\tcouldn't read INI");
				continue;
			}

			CSimpleIniA::TNamesDepend sections;
			ini.GetAllSections(sections);
			sections.sort(CSimpleIniA::Entry::LoadOrder());

			constexpr auto push_filter =
				[](const std::string& rawCondition,
					std::vector<CompiledCondition>& outFilters) {
						if (rawCondition.empty())
							return;

						outFilters.emplace_back(CompileCondition(rawCondition));
				};

			for (auto& [section, comment, keyOrder] : sections) {
				std::vector<std::vector<CompiledCondition>> orGroups;
				std::vector<std::string> splitSection = string::split(section, "|");
				if (string::icontains(section, "|")) {
					boost::trim(splitSection[1]);
					std::vector<std::string> orParts = string::split(splitSection[1], ",");
					_MESSAGE("\t\treading [%s] : %u OR groups", splitSection[0].c_str(), orParts.size());

					for (auto& orPart : orParts) {
						std::vector<std::string> andParts = string::split(orPart, "&");
						std::vector<CompiledCondition> andConditions;
						andConditions.reserve(andParts.size());
						for (auto& andPart : andParts) {
							push_filter(andPart, andConditions);
						}
						orGroups.push_back(std::move(andConditions));
					}
				}
				else
				{
					orGroups.emplace_back();
				}

				for (const auto& andConditions : orGroups) {
					CSimpleIniA::TNamesDepend values;
					ini.GetAllKeys(section, values);
					values.sort(CSimpleIniA::Entry::LoadOrder());
					if (splitSection[0] == "Items") {
						if (!values.empty()) {
							_MESSAGE("\t\t\t%u items found", values.size());
							for (const auto& key : values) {
								get_forms(path, key.pItem, andConditions, splitSection[0]);
							}
						}
					}
					else if (splitSection[0] == "Equipment" || splitSection[0] == "Equippables") {
						if (!values.empty()) {
							_MESSAGE("\t\t\t%u equippables found", values.size());
							for (const auto& key : values) {
								get_forms(path, key.pItem, andConditions, splitSection[0]);
							}
						}
					}
					else if (splitSection[0] == "Spells") {
						if (!values.empty()) {
							_MESSAGE("\t\t\t%u spells found", values.size());
							for (const auto& key : values) {
								get_forms(path, key.pItem, andConditions, splitSection[0]);
							}
						}
					}
					else if (splitSection[0] == "Factions") {
						if (!values.empty()) {
							_MESSAGE("\t\t\t%u factions found", values.size());
							for (const auto& key : values) {
								get_forms(path, key.pItem, andConditions, splitSection[0]);
							}
						}
					}
					else if (splitSection[0] == "Packages") {
						if (!values.empty()) {
							_MESSAGE("\t\t\t%u packages found", values.size());
							for (const auto& key : values) {
								get_forms(path, key.pItem, andConditions, splitSection[0]);
							}
						}
					}
					else if (splitSection[0] == "Keywords")
					{
						if (!values.empty())
						{
							_MESSAGE("\t\t\t%u keywords found", values.size());
							for (const auto& key : values)
							{
								get_forms(path, key.pItem, andConditions, splitSection[0]);
							}
						}
					}
				}
			}
		}

		_MESSAGE("-RESULT-");

		//_MESSAGE("%u Items processed", allItems.size());
		_MESSAGE("%u Items processed", allItemsConditional.size());
		for (const auto& [baseID, entries] : allItemsConditional)
		{
			_MESSAGE("BaseID %08X has %u entries",
				baseID,
				entries.size());
		}
		//_MESSAGE("%u conditional Items processed for ALL\n", applyToAllItems.size());

		//_MESSAGE("%u Equippables processed", allEquipment.size());
		_MESSAGE("%u Equippables processed", allEquipmentConditional.size());
		for (const auto& [baseID, entries] : allEquipmentConditional)
		{
			_MESSAGE("BaseID %08X has %u entries",
				baseID,
				entries.size());
		}
		//_MESSAGE("%u conditional Equippables processed for ALL\n", applyToAllEquipment.size());

		//_MESSAGE("%u Spells processed", allSpells.size());
		_MESSAGE("%u Spells processed", allSpellsConditional.size());
		for (const auto& [baseID, entries] : allSpellsConditional)
		{
			_MESSAGE("BaseID %08X has %u entries",
				baseID,
				entries.size());
		}
		//_MESSAGE("%u conditional Spells processed for ALL\n", applyToAllSpells.size());

		//_MESSAGE("%u Factions processed", allFactions.size());
		_MESSAGE("%u Factions processed", allFactionsConditional.size());
		for (const auto& [baseID, entries] : allFactionsConditional)
		{
			_MESSAGE("BaseID %08X has %u entries",
				baseID,
				entries.size());
		}
		//_MESSAGE("%u conditional Factions processed for ALL\n", applyToAllFactions.size());

		//_MESSAGE("%u Packages processed", allPackages.size());
		_MESSAGE("%u Packages processed", allPackagesConditional.size());
		for (const auto& [baseID, entries] : allPackagesConditional)
		{
			_MESSAGE("BaseID %08X has %u entries",
				baseID,
				entries.size());
		}

		_MESSAGE("%u Keywords processed", allKeywordsConditional.size());
		for (const auto& [baseID, entries] : allKeywordsConditional)
		{
			_MESSAGE("BaseID %08X has %u entries",
				baseID,
				entries.size());
		}
		//_MESSAGE("%u conditional Packages processed for ALL\n", applyToAllPackages.size());

		_MESSAGE("-END-");
	}
	void Manager::PrintConflicts() const
	{
		if (hasConflicts) {
			Console_Print(std::format("[SFID] Conflicts found, check SpellFactionItemDistributor.log in {} for more info\n", GetOblivionDirectory()).c_str());
		}
	}


	SFIDResult Manager::GetConditionalBase(
		TESObjectREFR* a_ref,
		TESForm* a_base,
		const ConditionalFormMap& conditionalForms,
		std::string formType)
	{
		DistributeRecordData empty{};

		if (!a_ref || !a_base)
			return { nullptr, empty };

		const auto itRef = conditionalForms.find(a_ref->refID);
		const auto itBase = conditionalForms.find(a_base->refID);
		const auto itAll = conditionalForms.find(0xFFFFFFFF);

		auto processBucket =
			[&](const ConditionalEntryVec& entries) -> SFIDResult {
			for (const auto& entry : entries)
			{
				if (!IsValidAll(entry.conditions, a_ref))
					continue;

				for (auto it = entry.swapData.rbegin();
					it != entry.swapData.rend(); ++it)
				{
					return { a_ref, *it };
				}
			}

			return { nullptr, empty };
			};

		if (itRef != conditionalForms.end())
			return processBucket(itRef->second);

		if (itBase != conditionalForms.end())
			return processBucket(itBase->second);

		if (itAll != conditionalForms.end())
			return processBucket(itAll->second);

		return { nullptr, empty };
	}



	std::vector<SFIDResult> Manager::GetBaseAll(
		TESObjectREFR* a_ref,
		TESForm* a_base,
		const ConditionalFormMap& conditionalForms,
		const std::string& formType)
	{
		std::vector<SFIDResult> results;

		if (!a_ref || !a_base)
			return results;

		const bool getAll =
			!string::iequals(formType, "Equipment") &&
			!string::iequals(formType, "Items");

		const auto itRef = conditionalForms.find(a_ref->refID);
		const auto itBase = conditionalForms.find(a_base->refID);
		const auto itAll = conditionalForms.find(0xFFFFFFFF);

		static thread_local std::mt19937 rng{ std::random_device{}() };

		auto processBucket =
			[&](const ConditionalEntryVec& entries) {
			for (const auto& entry : entries)
			{

				if (!IsValidAll(entry.conditions, a_ref))
				{
					continue;
				}

				if (entry.swapData.empty())
					continue;

				if (getAll)
				{
					for (auto it = entry.swapData.rbegin();
						it != entry.swapData.rend(); ++it)
					{
						results.emplace_back(a_ref, *it);
					}
				}
				else
				{
					std::uniform_int_distribution<size_t> dist(
						0, entry.swapData.size() - 1);

					size_t index = dist(rng);
					results.emplace_back(a_ref, entry.swapData[index]);
				}
			}
			};

		if (itRef != conditionalForms.end())
			processBucket(itRef->second);
		else if (itBase != conditionalForms.end())
			processBucket(itBase->second);
		else if (itAll != conditionalForms.end())
			processBucket(itAll->second);

		return results;
	}


	std::vector<SFIDResult> Manager::GetSingleSwapData(TESObjectREFR* a_ref, TESForm* a_base, std::string formType)
	{
		auto& allFormsConditional = get_form_map(formType);

		std::vector<SFIDResult> sfidResult;
		sfidResult = GetBaseAll(a_ref, a_base, allFormsConditional, formType);
		return sfidResult;
	}

	std::vector<std::vector<SFIDResult>> Manager::GetAllSwapData(TESObjectREFR* a_ref, TESForm* a_base, bool a_skipItemsAndEquipment) {
		std::vector<std::vector<SFIDResult>> resultVec;
		resultVec.reserve(5);
		if (allFactionsConditional.size() > 0) {
			resultVec.push_back(GetSingleSwapData(a_ref, a_base, "Factions"));
		}
		if (!a_skipItemsAndEquipment && allItemsConditional.size() > 0) {
			resultVec.push_back(GetSingleSwapData(a_ref, a_base, "Items"));
		}
		if (!a_skipItemsAndEquipment && allEquipmentConditional.size() > 0) {
			resultVec.push_back(GetSingleSwapData(a_ref, a_base, "Equipment"));
		}
		if (allSpellsConditional.size() > 0) {
			resultVec.push_back(GetSingleSwapData(a_ref, a_base, "Spells"));
		}
		if (allPackagesConditional.size() > 0) {
			resultVec.push_back(GetSingleSwapData(a_ref, a_base, "Packages"));
		}
		if (allKeywordsConditional.size() > 0)
		{
			resultVec.push_back(GetSingleSwapData(a_ref, a_base, "Keywords"));
		}

		return resultVec;
	}

	void Manager::QueueEquip(UInt32 refID, UInt32 formID)
	{
		pendingEquips.emplace(refID, formID);
	}

	void Manager::ProcessEquips(TESObjectREFR* a_ref)
	{
		if (!a_ref) return;
		auto [begin, end] = pendingEquips.equal_range(a_ref->refID);
		if (begin == end) return;

		std::vector<UInt32> toEquip;
		for (auto it = begin; it != end; ++it)
			toEquip.push_back(it->second);
		pendingEquips.erase(begin, end);

		for (UInt32 formID : toEquip) {
			if (auto* form = LookupFormByID(formID)) {
				a_ref->Equip(form, 1, nullptr, 0);
			}
		}
	}

	void Manager::ProcessAllEquips()
	{
		if (pendingEquips.empty()) return;

		auto snapshot = std::move(pendingEquips);
		pendingEquips = {};

		for (auto& [refID, formID] : snapshot) {
			TESForm* refForm = LookupFormByID(refID);
			TESObjectREFR* ref = refForm
				? OBLIVION_CAST(refForm, TESForm, TESObjectREFR)
				: nullptr;
			if (!ref || !ref->GetNiNode()) {
				pendingEquips.emplace(refID, formID);
				continue;
			}
			// Engine bug: EquipItem on a creature duplicates its 3D nodes,
			// causing random combat crashes. Items are already in inventory.
			if (ref->baseForm) {
				UInt32 ft = ref->baseForm->GetFormType();
				if (ft == kFormType_Creature || ft == kFormType_LeveledCreature)
					continue;
			}
			auto* form = LookupFormByID(formID);
			if (form) {
				ref->Equip(form, 1, nullptr, 0);
			}
		}
	}
}