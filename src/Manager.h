#pragma once

#include "DistributeData.h"
#include "Defs.h"

namespace SpellFactionItemDistributor
{
	enum FormCode {
		form,
		spell,
		faction,
		package,
		equippable,
		item,
		keyword
	};

	enum SectionType : uint8_t {
		kItems      = 1 << 0,
		kEquipment  = 1 << 1,
		kSpells     = 1 << 2,
		kFactions   = 1 << 3,
		kPackages   = 1 << 4,
		kKeywords   = 1 << 5,
	};
	static constexpr uint8_t kAllSections = 0x3F;

	enum class ConditionType
	{
		EditorID,
		Race,
		Class,
		Faction,
		Item,
		Name,
		Mod,
		Cell,
		Worldspace,
		Region,
		Keyword,
		ActorType,
		Sex,
		All
	};

	struct CompiledCondition
	{
		ConditionType type;
		UInt32 formID;
		std::string text;
		bool isExclusion;
	};

	struct ConditionalEntry
	{
		std::vector<CompiledCondition> conditions;
		std::vector<DistributeRecordData> swapData;
	};

	bool operator==(const CompiledCondition& lhs,
		const CompiledCondition& rhs)
	{
		return lhs.type == rhs.type &&
			lhs.formID == rhs.formID &&
			lhs.text == rhs.text &&
			lhs.isExclusion == rhs.isExclusion;
	}

	using ConditionalEntryVec = std::vector<ConditionalEntry>;
	using ConditionalFormMap = std::unordered_map<UInt32, ConditionalEntryVec>;

	struct ConditionalInput
	{
		ConditionalInput(TESObjectREFR* a_ref, TESForm* a_form) :
			ref(a_ref),
			base(a_form),
			currentCell(a_ref->parentCell),
			faction(nullptr),
			race(nullptr)
		{
		}

		[[nodiscard]] bool IsValid(const CompiledCondition& cond, TESObjectREFR* ref);

		[[nodiscard]] bool IsValidAll(const std::vector<CompiledCondition>& conditions, TESObjectREFR* ref);

		// members
		TESObjectREFR* ref;
		TESForm* base;
		TESObjectCELL* currentCell;
		TESFaction* faction;
		TESRace* race;
		std::string editorID;
		std::string name;
	};
	
	static CompiledCondition CompileCondition(const std::string&);

	class Manager
	{
	public:

		static Manager* GetSingleton()
		{
			static Manager singleton;
			return std::addressof(singleton);
		}

		void LoadFormsOnce();

		void            PrintConflicts() const;
		std::vector<SFIDResult>      GetSingleSwapData(TESObjectREFR* a_ref, TESForm* a_base, std::string formType);
		std::vector<std::vector<SFIDResult>> GetAllSwapData(TESObjectREFR* a_ref, TESForm* a_base, bool a_skipItemsAndEquipment = false);
		SFIDResult GetConditionalBase(
			TESObjectREFR* a_ref,
			TESForm* a_base,
			const ConditionalFormMap& conditionalForms,
			std::string formType);

		std::vector<SFIDResult> GetBaseAll(
			TESObjectREFR* a_ref,
			TESForm* a_base,
			const ConditionalFormMap& conditionalForms,
			const std::string& formType);

		std::unordered_set<UInt32> processedForms;
		std::unordered_map<UInt32, uint8_t> processedSections;
		std::unordered_set<uint64_t> swappedLeveledItemRefs{};
		uint64_t savedConfigCRC{ 0 };

		void QueueEquip(UInt32 refID, UInt32 formID);
		void ProcessEquips(TESObjectREFR* a_ref);
		void ProcessAllEquips();
		void ClearPendingEquips() { pendingEquips.clear(); }

	private:
		Manager() = default;
		~Manager() = default;

		Manager(const Manager&) = delete;
		Manager(Manager&&) = delete;
		Manager& operator=(const Manager&) = delete;
		Manager& operator=(Manager&&) = delete;

		void LoadForms();

		FormMap<SwapDataVec>& get_form_vec(const std::string& a_str);
		ConditionalFormMap& get_form_map(const std::string& a_str);
		ConditionalFormMap& get_form_map_all(const std::string& a_str);
		void get_forms(
			const std::string& a_path,
			const std::string& a_str,
			const std::vector<CompiledCondition>& conditions,
			std::string formType);

		void get_forms_all(
			const std::string& a_path,
			const std::string& a_str,
			const std::vector<CompiledCondition>& conditions,
			std::string formType);

		void get_forms_keywords(
			const std::string& a_path,
			const std::string& a_str,
			const std::vector<CompiledCondition>& conditions,
			const std::vector<std::string>& keywords);

		FormMap<SwapDataVec> allItems{};
		ConditionalFormMap allItemsConditional{};
		ConditionalFormMap applyToAllItems{};

		FormMap<SwapDataVec> allEquipment{};
		ConditionalFormMap allEquipmentConditional{};
		ConditionalFormMap applyToAllEquipment{};

		FormMap<SwapDataVec> allSpells{};
		ConditionalFormMap allSpellsConditional{};
		ConditionalFormMap applyToAllSpells{};

		FormMap<SwapDataVec> allFactions{};
		ConditionalFormMap allFactionsConditional{};
		ConditionalFormMap applyToAllFactions{};

		FormMap<SwapDataVec> allPackages{};
		ConditionalFormMap allPackagesConditional{};
		ConditionalFormMap applyToAllPackages{};

		FormMap<SwapDataVec> allKeywords{};
		ConditionalFormMap allKeywordsConditional{};
		ConditionalFormMap applyToAllKeywords{};


		std::unordered_multimap<UInt32, UInt32> pendingEquips;

		bool hasConflicts{ false };
		std::atomic<bool> formsLoaded{ false };

	public:
		void ResetInit() { formsLoaded.store(false, std::memory_order_release); }

	private:
	};
}