#include "Hooks.h"
#include "Manager.h"
#include "obse/GameForms.h"
#include "obse/GameObjects.h"
#include <OBSEKeywords/KeywordAPI.h>

namespace SpellFactionItemDistributor
{
	bool g_hooksInstalled = false;

	static SInt32 GetItemCount(TESObjectREFR* ref, TESForm* form)
	{
		if (!ref || !form) return 0;
		ExtraContainerChanges* xChanges = static_cast<ExtraContainerChanges*>(ref->baseExtraList.GetByType(kExtraData_ContainerChanges));
		if (!xChanges || !xChanges->data) return 0;
		auto* entry = xChanges->GetByType(form);
		return entry ? entry->countDelta : 0;
	}

	static bool HasSpell(TESActorBase* npc, SpellItem* spell) {
		TESSpellList::Entry* entry = &npc->spellList.spellList;
		while (entry) {
			if (entry->type == spell) return true;
			entry = entry->Next();
		}
		entry = &npc->spellList.leveledSpellList;
		while (entry) {
			if (entry->type == spell) return true;
			entry = entry->Next();
		}
		return false;
	}

	static bool HasFaction(TESActorBase* npc, TESFaction* faction) {
		auto* entry = &npc->actorBaseData.factionList;
		while (entry && entry->data) {
			if (entry->data->faction == faction) return true;
			entry = entry->Next();
		}
		return false;
	}

	static bool HasPackage(TESActorBase* npc, TESPackage* package) {
		TESAIForm::PackageEntry* entry = &npc->aiForm.packageList;
		while (entry) {
			if (entry->package == package) return true;
			entry = entry->Next();
		}
		return false;
	}

	static void AddMiscItem(TESObjectREFR* ref, TESForm* form, UInt32 amount) {
		if (ref && form) {
			SInt32 current = GetItemCount(ref, form);
			if (current >= static_cast<SInt32>(amount))
				return;
			ref->AddItem(form, nullptr, amount - current);
		}
	}

	static void RemoveItem(TESObjectREFR* ref, TESForm* form, UInt32 amount) {
		ref->RemoveItem(form, nullptr, amount, 0, 0, nullptr, 0, 0, 0, 0);
	}

	static void AddEquipItem(TESObjectREFR* ref, TESForm* form, UInt32 amount) {
		if (ref && form) {
			// Adding equipment items to creatures corrupts combat state
			// (OBME magic effect processing crashes at sub_5F5D10).
			if (ref->baseForm) {
				UInt32 ft = ref->baseForm->GetFormType();
				if (ft == kFormType_Creature || ft == kFormType_LeveledCreature)
					return;
			}
			SInt32 current = GetItemCount(ref, form);
			if (current >= static_cast<SInt32>(amount))
				return;
			ref->AddItem(form, nullptr, amount - current);
			Manager::GetSingleton()->QueueEquip(ref->refID, form->refID);
		}
	}

	static void AddLevItem(TESObjectREFR* ref, TESForm* form, UInt32 amount) {
		auto* mgr = Manager::GetSingleton();
		uint64_t key = (static_cast<uint64_t>(ref->refID) << 32) | form->refID;

		if (mgr->swappedLeveledItemRefs.contains(key)) {
			_MESSAGE("AddLevItem: SKIP %08X leveled %08X (already distributed)",
				ref->refID, form->refID);
			return;
		}

		TESActorBase* npc = dynamic_cast<TESActorBase*>(ref->baseForm);
		if (!npc) return;

		SInt16 level = npc->actorBaseData.level;
		SInt16 maxLevel = npc->actorBaseData.maxLevel;
		SInt16 minLevel = npc->actorBaseData.minLevel;
		TESLeveledList* lev = dynamic_cast<TESLeveledList*>(form);
		if (!lev) return;

		short itr = amount;
		while (itr > 0) {
			TESForm* newForm = lev->CalcElement(level, true, maxLevel - minLevel);
			if (newForm) {
				ref->AddItem(newForm, nullptr, 1);
			}
			itr--;
		}
		mgr->swappedLeveledItemRefs.emplace(key);
	}

	static void AddSingleSpell(TESObjectREFR* ref, TESForm* form) {
		TESActorBase* npc = OBLIVION_CAST(ref->baseForm, TESForm, TESActorBase);
		SpellItem* spell = OBLIVION_CAST(form, TESForm, SpellItem);
		if (spell && HasSpell(npc, spell))
			return;
		ThisStdCall(0x46F350, &npc->spellList, spell);
		ThisStdCall(0x46ABF0, &npc->spellList, TESSpellList::kModified_BaseSpellList);
	}

	static void AddLevSpell(TESObjectREFR* ref, TESForm* form) {
		TESActorBase* npc = OBLIVION_CAST(ref->baseForm, TESForm, TESActorBase);
		SInt16 level = npc->actorBaseData.level;
		SInt16 maxLevel = npc->actorBaseData.maxLevel;
		SInt16 minLevel = npc->actorBaseData.minLevel;
		TESLeveledList* lev = OBLIVION_CAST(form, TESForm, TESLeveledList);
		TESForm* newForm = lev->CalcElement(level, true, maxLevel - minLevel);
		SpellItem* spell = OBLIVION_CAST(newForm, TESForm, SpellItem);
		if (spell && HasSpell(npc, spell))
			return;
		ThisStdCall(0x46F350, &(npc->spellList), TESSpellList::kModified_BaseSpellList);
		ThisStdCall(0x46ABF0, ref, TESSpellList::kModified_BaseSpellList);
		ThisStdCall(0x46ABF0, ref->baseForm, TESSpellList::kModified_BaseSpellList);
	}

	static void AddToFaction(TESObjectREFR* ref, TESForm* form) {
		TESActorBase* npc = OBLIVION_CAST(ref->baseForm, TESForm, TESActorBase);
		TESFaction* faction = OBLIVION_CAST(form, TESForm, TESFaction);
		if (!npc || !faction || HasFaction(npc, faction))
			return;
		ThisStdCall(0x4675E0, &(npc->actorBaseData), faction, 1);
		ThisStdCall(0x4672F0, &(npc->actorBaseData), TESActorBaseData::kModified_BaseFactions);
	}

	static void AddPackage(TESObjectREFR* ref, TESForm* form) {
		TESActorBase* npc = dynamic_cast<TESActorBase*>(ref->baseForm);
		TESPackage* package = dynamic_cast<TESPackage*>(form);
		if (!npc || !package || HasPackage(npc, package))
			return;
		TESAIForm::PackageEntry* newPackageData = (TESAIForm::PackageEntry*)FormHeap_Allocate(sizeof(TESAIForm::PackageEntry));
		newPackageData->package = package;
		newPackageData->next = nullptr;
		TESAIForm::PackageEntry* last = &npc->aiForm.packageList;
		while (last->next) last = last->next;
		last->next = newPackageData;
		ThisStdCall(0x46ABF0, ref, TESAIForm::kModified_BaseAIData);
	}


	static bool IsItemFormType(UInt32 formType)
	{
		switch (formType)
		{
		case FormType::kFormType_Misc:
		case FormType::kFormType_AlchemyItem:
		case FormType::kFormType_SoulGem:
		case FormType::kFormType_Apparatus:
		case FormType::kFormType_Book:
		case FormType::kFormType_Ingredient:
		case FormType::kFormType_Key:
		case FormType::kFormType_Armor:
		case FormType::kFormType_Weapon:
		case FormType::kFormType_Ammo:
		case FormType::kFormType_Clothing:
		case FormType::kFormType_LeveledItem:
			return true;
		default:
			return false;
		}
	}

	static void AddForms(TESObjectREFR* a_ref, TESForm* formToAdd, UInt32 amount, UInt32 chance, bool trueRandom) {
		if ((a_ref->refID >> 24) == 0xFF && formToAdd) {
			if (a_ref->IsDead(true))
				return;
		}
		auto seededRNG = SeedRNG(static_cast<std::uint32_t>(a_ref->refID));
		if (chance != 100) {
			const auto rng = trueRandom ? SeedRNG().Generate<std::uint32_t>(0, 100) :
				seededRNG.Generate<std::uint32_t>(0, 100);
			if (rng > chance) {
				return;
			}
		}
		if (formToAdd) {
			switch (formToAdd->GetFormType())
			{
			case (FormType::kFormType_Misc):
				AddMiscItem(a_ref, formToAdd, amount);
				break;
			case (FormType::kFormType_AlchemyItem):
				AddMiscItem(a_ref, formToAdd, amount);
				break;
			case (FormType::kFormType_SoulGem):
				AddMiscItem(a_ref, formToAdd, amount);
				break;
			case (FormType::kFormType_Apparatus):
				AddMiscItem(a_ref, formToAdd, amount);
				break;
			case (FormType::kFormType_Book):
				AddMiscItem(a_ref, formToAdd, amount);
				break;
			case (FormType::kFormType_Ingredient):
				AddMiscItem(a_ref, formToAdd, amount);
				break;
			case (FormType::kFormType_Key):
				AddMiscItem(a_ref, formToAdd, amount);
				break;
			case (FormType::kFormType_LeveledItem):
				AddLevItem(a_ref, formToAdd, amount);
				break;
			case (FormType::kFormType_Armor):
				AddEquipItem(a_ref, formToAdd, amount);
				break;
			case (FormType::kFormType_Weapon):
				AddEquipItem(a_ref, formToAdd, amount);
				break;
			case (FormType::kFormType_Ammo):
				AddEquipItem(a_ref, formToAdd, amount);
				break;
			case (FormType::kFormType_Clothing):
				AddEquipItem(a_ref, formToAdd, amount);
				break;
			case (FormType::kFormType_Spell):
				AddSingleSpell(a_ref, formToAdd);
				break;
			case (FormType::kFormType_LeveledSpell):
				AddSingleSpell(a_ref, formToAdd);
				break;
			case (FormType::kFormType_Package):
				AddPackage(a_ref, formToAdd);
				break;
			case (FormType::kFormType_Faction):
				AddToFaction(a_ref, formToAdd);
				break;
			default:
				break;
			}
		}
	}

	static void ProcessResult(SFIDResult sfidResult) {
		const auto& [ref, swapData] = sfidResult;
		if (!ref || swapData.traits.amount == 0) {
			return;
		}

		if (!swapData.keywords.empty() && KeywordAPI::IsReady())
		{
			for (const auto& keyword : swapData.keywords)
			{
				if (!KeywordAPI::HasKeyword(ref->refID, keyword.c_str()))
					KeywordAPI::AddKeyword(ref->refID, keyword.c_str());
				if (!KeywordAPI::HasKeyword(ref->baseForm->refID, keyword.c_str()))
					KeywordAPI::AddKeyword(ref->baseForm->refID, keyword.c_str());
			}
		}
		if (std::holds_alternative<UInt32>(swapData.formToAdd)) {
			const auto formToAdd = std::get<UInt32>(swapData.formToAdd);
			if (formToAdd == 0) {
				return;
			}
			auto form = LookupFormByID(formToAdd);
			if (swapData.traits.remove) {
				RemoveItem(ref, form, swapData.traits.amount);
			}
			AddForms(ref, form, swapData.traits.amount, swapData.traits.chance, swapData.traits.trueRandom);
		}
		else if (std::holds_alternative<std::unordered_set<UInt32>>(swapData.formToAdd)) {
			const auto& set = std::get<std::unordered_set<UInt32>>(swapData.formToAdd);
			for (const auto& form : set) {
				TESForm* newForm = LookupFormByID(form);
				if (newForm) {
					if (swapData.traits.remove) {
						RemoveItem(ref, newForm, swapData.traits.amount);
					}
					AddForms(ref, newForm, swapData.traits.amount, swapData.traits.chance, swapData.traits.trueRandom);
				}
			}
		}
	}

	static inline std::uint32_t originalAddressNPC;
	static inline std::uint32_t originalAddressCREA;

	static void __fastcall GenerateNiNodeHookNPC(TESObjectREFR* a_ref, void* edx)
	{
		Manager* manager = Manager::GetSingleton();
		if (const auto base = a_ref->baseForm) {
			manager->LoadFormsOnce();

			// === 高速パス：全セクション完了済み ===
			if (manager->processedForms.contains(a_ref->refID))
			{
				_MESSAGE("SFID hook: SKIP NPC %08X (all sections done)", a_ref->refID);
				ThisStdCall(originalAddressNPC, a_ref);
				return;
			}

			bool isCloned = IsClonedForm(base->refID);
			uint8_t& flags = manager->processedSections[a_ref->refID];
			bool anyProcessed = false;

			// Keywords（常に処理可能）
			if (!(flags & kKeywords)) {
				std::vector<SFIDResult> keywordResult =
					manager->GetSingleSwapData(a_ref, base, "Keywords");
				for (SFIDResult& kw : keywordResult) {
					ProcessResult(kw);
				}
				flags |= kKeywords;
				anyProcessed = true;
			}

			// Factions（常に処理可能）
			if (!(flags & kFactions)) {
				std::vector<SFIDResult> factionResult =
					manager->GetSingleSwapData(a_ref, base, "Factions");
				for (SFIDResult& fac : factionResult) {
					ProcessResult(fac);
				}
				flags |= kFactions;
				anyProcessed = true;
			}

			// Items & Equipment & Spells & Packages（非クローンのみ）
			if (!isCloned) {
				if (!(flags & kItems)) {
					std::vector<SFIDResult> itemsResult =
						manager->GetSingleSwapData(a_ref, base, "Items");
					for (SFIDResult& item : itemsResult) {
						ProcessResult(item);
					}
					flags |= kItems;
					anyProcessed = true;
				}
				if (!(flags & kEquipment)) {
					std::vector<SFIDResult> equipResult =
						manager->GetSingleSwapData(a_ref, base, "Equipment");
					for (SFIDResult& eq : equipResult) {
						ProcessResult(eq);
					}
					flags |= kEquipment;
					anyProcessed = true;
				}
				if (!(flags & kSpells)) {
					std::vector<SFIDResult> spellsResult =
						manager->GetSingleSwapData(a_ref, base, "Spells");
					for (SFIDResult& sp : spellsResult) {
						ProcessResult(sp);
					}
					flags |= kSpells;
					anyProcessed = true;
				}
				if (!(flags & kPackages)) {
					std::vector<SFIDResult> pkgResult =
						manager->GetSingleSwapData(a_ref, base, "Packages");
					for (SFIDResult& pkg : pkgResult) {
						ProcessResult(pkg);
					}
					flags |= kPackages;
					anyProcessed = true;
				}
			}

			// 全セクション完了したら processedForms にも追加（高速パス用）
			if (flags == kAllSections) {
				manager->processedForms.emplace(a_ref->refID);
				_MESSAGE("SFID hook: NPC %08X all sections completed", a_ref->refID);
			}
			else if (anyProcessed) {
				_MESSAGE("SFID hook: NPC %08X partial process (flags=%02X)",
					a_ref->refID, flags);
			}
			else {
				_MESSAGE("SFID hook: NPC %08X no new sections needed (flags=%02X)",
					a_ref->refID, flags);
			}
		}
		ThisStdCall(originalAddressNPC, a_ref);
	}

	static void __fastcall GenerateNiNodeHookCREA(TESObjectREFR* a_ref, void* edx)
	{
		Manager* manager = Manager::GetSingleton();
		if (const auto base = a_ref->baseForm)
		{
			manager->LoadFormsOnce();

			// === 高速パス：全セクション完了済み ===
			if (manager->processedForms.contains(a_ref->refID))
			{
				_MESSAGE("SFID hook: SKIP CREA %08X (all sections done)", a_ref->refID);
				ThisStdCall(originalAddressCREA, a_ref);
				return;
			}

			bool isCloned = IsClonedForm(base->refID);
			uint8_t& flags = manager->processedSections[a_ref->refID];
			bool anyProcessed = false;

			// Keywords（常に処理可能）
			if (!(flags & kKeywords)) {
				std::vector<SFIDResult> keywordResult =
					manager->GetSingleSwapData(a_ref, base, "Keywords");
				for (SFIDResult& kw : keywordResult) {
					ProcessResult(kw);
				}
				flags |= kKeywords;
				anyProcessed = true;
			}

			// Factions（常に処理可能）
			if (!(flags & kFactions)) {
				std::vector<SFIDResult> factionResult =
					manager->GetSingleSwapData(a_ref, base, "Factions");
				for (SFIDResult& fac : factionResult) {
					ProcessResult(fac);
				}
				flags |= kFactions;
				anyProcessed = true;
			}

			// Items & Equipment & Spells & Packages（非クローンのみ）
			if (!isCloned) {
				if (!(flags & kItems)) {
					std::vector<SFIDResult> itemsResult =
						manager->GetSingleSwapData(a_ref, base, "Items");
					for (SFIDResult& item : itemsResult) {
						ProcessResult(item);
					}
					flags |= kItems;
					anyProcessed = true;
				}
				if (!(flags & kEquipment)) {
					std::vector<SFIDResult> equipResult =
						manager->GetSingleSwapData(a_ref, base, "Equipment");
					for (SFIDResult& eq : equipResult) {
						ProcessResult(eq);
					}
					flags |= kEquipment;
					anyProcessed = true;
				}
				if (!(flags & kSpells)) {
					std::vector<SFIDResult> spellsResult =
						manager->GetSingleSwapData(a_ref, base, "Spells");
					for (SFIDResult& sp : spellsResult) {
						ProcessResult(sp);
					}
					flags |= kSpells;
					anyProcessed = true;
				}
				if (!(flags & kPackages)) {
					std::vector<SFIDResult> pkgResult =
						manager->GetSingleSwapData(a_ref, base, "Packages");
					for (SFIDResult& pkg : pkgResult) {
						ProcessResult(pkg);
					}
					flags |= kPackages;
					anyProcessed = true;
				}
			}

			// 全セクション完了したら processedForms にも追加（高速パス用）
			if (flags == kAllSections) {
				manager->processedForms.emplace(a_ref->refID);
				_MESSAGE("SFID hook: CREA %08X all sections completed", a_ref->refID);
			}
			else if (anyProcessed) {
				_MESSAGE("SFID hook: CREA %08X partial process (flags=%02X)",
					a_ref->refID, flags);
			}
			else {
				_MESSAGE("SFID hook: CREA %08X no new sections needed (flags=%02X)",
					a_ref->refID, flags);
			}
		}
		ThisStdCall(originalAddressCREA, a_ref);
	}

	// Credits to lStewieAl
	[[nodiscard]] __declspec(noinline) UInt32 __stdcall DetourVtable(UInt32 addr, UInt32 dst)
	{
		UInt32 originalFunction = *(UInt32*)addr;
		SafeWrite32(addr, dst);
		return originalFunction;
	}

	static constexpr UInt32 kMainLoopHookAddr = 0x0040F19D;
	static UInt32 originalMainLoopHook = 0;

	static void HandleSFIDMainLoop()
	{
		Manager::GetSingleton()->ProcessAllEquips();
	}

	static __declspec(naked) void MainLoopChainHook()
	{
		__asm
		{
			pushad
			call    HandleSFIDMainLoop
			popad
			jmp     [originalMainLoopHook]
		}
	}

	void Install()
	{
		if (!g_hooksInstalled)
		{
			_MESSAGE("-HOOKS-");
			originalAddressNPC = DetourVtable(0xA6FDE8, reinterpret_cast<UInt32>(GenerateNiNodeHookNPC)); // kVtbl_Character_GenerateNiNode
			//originalAddressNPC = DetourVtable(0xA6E1C0, reinterpret_cast<UInt32>(GenerateNiNodeHookNPC)); // kVtbl_Character_GenerateNiNode
			originalAddressCREA = DetourVtable(0xA71240, reinterpret_cast<UInt32>(GenerateNiNodeHookCREA)); // kVtbl_Creature_GenerateNiNode
			if (*(UInt8*)kMainLoopHookAddr == 0xE9) {
				originalMainLoopHook = 0x0040F1A2 + *(UInt32*)(kMainLoopHookAddr + 1);
				WriteRelJump(kMainLoopHookAddr, (UInt32)&MainLoopChainHook);
				_MESSAGE("Installed main loop hook, chaining to %08X", originalMainLoopHook);
			}
			else {
				_MESSAGE("WARNING: MainLoop hook not found at %08X, deferred Equip disabled", kMainLoopHookAddr);
			}
			_MESSAGE("Installed all hooks");
			g_hooksInstalled = true;
		}

	}
}
