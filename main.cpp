#include "src/Hooks.h"
#include "src/Manager.h"
#include "EditorIDMapper/EditorIDMapperAPI.h"
#include "OBSEKeywords/KeywordAPI.h"
#include "lib/crc.hpp"

IDebugLog		gLog("SpellFactionItemDistributor.log");
PluginHandle	g_pluginHandle = kPluginHandle_Invalid;

OBSEMessagingInterface* g_messagingInterface{};
OBSEInterface* g_OBSEInterface{};
OBSECommandTableInterface* g_cmdTableInterface{};

// OBLIVION = Is not being compiled as a CS plugin.
#if OBLIVION
OBSEScriptInterface* g_script{};
OBSEStringVarInterface* g_stringInterface{};
OBSEArrayVarInterface* g_arrayInterface{};
OBSESerializationInterface* g_serializationInterface{};
OBSEConsoleInterface* g_consoleInterface{};
OBSEEventManagerInterface* g_eventInterface{};
#endif

void MessageHandler(OBSEMessagingInterface::Message* msg)
{
	if (msg->type == OBSEMessagingInterface::kMessage_PreLoadGame)
	{
		SpellFactionItemDistributor::Install();
	}
}

void UnifiedMessageHandler(OBSEMessagingInterface::Message* msg)
{
	EditorIDMapper::MessageHandler(msg);
	KeywordAPI::MessageHandler(msg);
}

// co-save serialization
static constexpr UInt32 kChunk_ProcessedForms = 'PRFM';
static constexpr UInt32 kChunk_ConfigCRC = 'CRCF';
static constexpr UInt32 kChunk_SectionCRC = 'SECC';
static constexpr UInt32 kChunk_ProcessFlags = 'PFLG';
static constexpr UInt32 kChunk_LevRefs = 'LEVR';
static constexpr UInt32 kChunkVersion = 1;

using namespace SpellFactionItemDistributor;

void SaveCallback(void*)
{
	auto* mgr = SpellFactionItemDistributor::Manager::GetSingleton();
	mgr->ClearPendingEquips();
	auto* intfc = g_serializationInterface;
	if (!intfc) return;

	// (1) セクション別CRCを保存
	crc::SectionCRCs crcs = crc::ComputeSectionCRCs();
	intfc->OpenRecord(kChunk_SectionCRC, kChunkVersion);
	intfc->WriteRecordData(&crcs, sizeof(crcs));

	// (2) processedSections を保存 (refID + flags のペア配列)
	{
		UInt32 count = static_cast<UInt32>(mgr->processedSections.size());
		intfc->OpenRecord(kChunk_ProcessFlags, kChunkVersion);
		intfc->WriteRecordData(&count, sizeof(count));
		for (const auto& [refID, flags] : mgr->processedSections) {
			intfc->WriteRecordData(&refID, sizeof(refID));
			intfc->WriteRecordData(&flags, sizeof(flags));
		}
		_MESSAGE("SFID SaveCallback: wrote %u process flags", count);
	}

	// (3) swappedLeveledItemRefs を保存
	{
		UInt32 count = static_cast<UInt32>(mgr->swappedLeveledItemRefs.size());
		intfc->OpenRecord(kChunk_LevRefs, kChunkVersion);
		intfc->WriteRecordData(&count, sizeof(count));
		for (uint64_t key : mgr->swappedLeveledItemRefs) {
			intfc->WriteRecordData(&key, sizeof(key));
		}
		_MESSAGE("SFID SaveCallback: wrote %u leveled ref keys", count);
	}

	// (4) 従来の processedForms も保存（高速パス用、後方互換用）
	{
		UInt32 count = 0;
		for (UInt32 refID : mgr->processedForms) {
			if ((refID >> 24) != 0xFF) count++;
		}
		intfc->OpenRecord(kChunk_ProcessedForms, kChunkVersion);
		intfc->WriteRecordData(&count, sizeof(count));
		for (UInt32 refID : mgr->processedForms) {
			if ((refID >> 24) != 0xFF) {
				intfc->WriteRecordData(&refID, sizeof(refID));
			}
		}
		_MESSAGE("SFID SaveCallback: wrote %u processed refIDs", count);
	}
}

void LoadCallback(void* reserved)
{
	auto* mgr = SpellFactionItemDistributor::Manager::GetSingleton();
	mgr->ClearPendingEquips();
	auto* intfc = g_serializationInterface;
	if (!intfc) return;

	crc::SectionCRCs savedCRCs{};

	UInt32 type, version, length;
	while (intfc->GetNextRecordInfo(&type, &version, &length)) {
		switch (type) {
		case kChunk_ConfigCRC: {
			// 旧形式：読み捨て（新バージョンでは使わない）
			uint64_t oldCRC;
			intfc->ReadRecordData(&oldCRC, sizeof(oldCRC));
			break;
		}
		case kChunk_ProcessedForms: {
			// 旧形式：processedForms を復元（高速パス用）
			UInt32 count;
			intfc->ReadRecordData(&count, sizeof(count));
			for (UInt32 i = 0; i < count; i++) {
				UInt32 refID;
				intfc->ReadRecordData(&refID, sizeof(refID));
				mgr->processedForms.emplace(refID);
			}
			_MESSAGE("SFID LoadCallback: loaded %u processed refIDs", count);
			break;
		}
		case kChunk_SectionCRC: {
			intfc->ReadRecordData(&savedCRCs, sizeof(savedCRCs));
			_MESSAGE("SFID LoadCallback: loaded section CRCs");
			break;
		}
		case kChunk_ProcessFlags: {
			UInt32 count;
			intfc->ReadRecordData(&count, sizeof(count));
			for (UInt32 i = 0; i < count; i++) {
				UInt32 refID;
				uint8_t flags;
				intfc->ReadRecordData(&refID, sizeof(refID));
				intfc->ReadRecordData(&flags, sizeof(flags));
				mgr->processedSections.emplace(refID, flags);
			}
			_MESSAGE("SFID LoadCallback: loaded %u process flag entries", count);
			break;
		}
		case kChunk_LevRefs: {
			UInt32 count;
			intfc->ReadRecordData(&count, sizeof(count));
			for (UInt32 i = 0; i < count; i++) {
				uint64_t key;
				intfc->ReadRecordData(&key, sizeof(key));
				mgr->swappedLeveledItemRefs.emplace(key);
			}
			_MESSAGE("SFID LoadCallback: loaded %u leveled ref keys", count);
			break;
		}
		}
	}

	// セクション別CRC比較
	crc::SectionCRCs currentCRCs = crc::ComputeSectionCRCs();

	auto compare_and_clear = [](uint64_t saved, uint64_t current, uint8_t sectBit,
		auto& processedSections, auto& processedForms, auto& swappedRefs,
		bool isItemsSection) {
		if (saved != 0 && saved != current) {
			// CRC不一致 → 該当セクションのビットのみ全NPCからクリア
			for (auto& [refID, flags] : processedSections) {
				flags &= ~sectBit;
				// 全ビットが立たなくなったら processedForms からも削除
				if (flags != kAllSections) {
					processedForms.erase(refID);
				}
			}
			_MESSAGE("SFID: Section CRC changed (bit %02X), flags cleared", sectBit);
			if (isItemsSection) {
				swappedRefs.clear();
				_MESSAGE("SFID: Items changed, swappedLeveledItemRefs cleared");
			}
		}
		else if (saved != 0) {
			_MESSAGE("SFID: Section CRC unchanged (bit %02X), flags kept", sectBit);
		}
		else {
			// saved == 0 は co-save にデータがない場合（初回 or 旧バージョン）
			_MESSAGE("SFID: Section CRC not found in save (bit %02X)", sectBit);
		}
	};

	compare_and_clear(savedCRCs.items,     currentCRCs.items,     kItems,
		mgr->processedSections, mgr->processedForms, mgr->swappedLeveledItemRefs, true);
	compare_and_clear(savedCRCs.equipment, currentCRCs.equipment, kEquipment,
		mgr->processedSections, mgr->processedForms, mgr->swappedLeveledItemRefs, false);
	compare_and_clear(savedCRCs.spells,    currentCRCs.spells,    kSpells,
		mgr->processedSections, mgr->processedForms, mgr->swappedLeveledItemRefs, false);
	compare_and_clear(savedCRCs.factions,  currentCRCs.factions,  kFactions,
		mgr->processedSections, mgr->processedForms, mgr->swappedLeveledItemRefs, false);
	compare_and_clear(savedCRCs.packages,  currentCRCs.packages,  kPackages,
		mgr->processedSections, mgr->processedForms, mgr->swappedLeveledItemRefs, false);
	compare_and_clear(savedCRCs.keywords,  currentCRCs.keywords,  kKeywords,
		mgr->processedSections, mgr->processedForms, mgr->swappedLeveledItemRefs, false);
}



bool OBSEPlugin_Query(const OBSEInterface* OBSE, PluginInfo* info)
{
	// fill out the info structure
	info->infoVersion = PluginInfo::kInfoVersion;
	info->name = "SpellFactionItemDistributor";
	info->version = 1;

	// version checks
	if (OBSE->obseVersion < OBSE_VERSION_INTEGER)
	{
		_ERROR("OBSE version too old (got %08X expected at least %08X)", OBSE->obseVersion, OBSE_VERSION_PADDEDSTRING);
		return false;
	}

	if (!OBSE->isEditor)
	{
		if (OBSE->oblivionVersion < OBLIVION_VERSION_1_2_416)
		{
			_ERROR("incorrect runtime version (got %08X need at least %08X)", OBSE->oblivionVersion, OBLIVION_VERSION_1_2_416);
			return false;
		}
	}
	else
	{
		if (OBSE->editorVersion < CS_VERSION_1_2)
		{
			_ERROR("incorrect editor version (got %08X need at least %08X)", OBSE->editorVersion, CS_VERSION_1_2);
			return false;
		}
	}

	return true;
}

bool OBSEPlugin_Load(OBSEInterface* OBSE)
{
	g_pluginHandle = OBSE->GetPluginHandle();

	g_OBSEInterface = OBSE;

	g_messagingInterface = static_cast<OBSEMessagingInterface*>(OBSE->QueryInterface(kInterface_Messaging));
	g_messagingInterface->RegisterListener(g_pluginHandle, "OBSE", MessageHandler);

	OBSE->SetOpcodeBase(0x2770);

	if (!OBSE->isEditor)
	{
#if OBLIVION
		g_script = static_cast<OBSEScriptInterface*>(OBSE->QueryInterface(kInterface_Script));
		g_stringInterface = static_cast<OBSEStringVarInterface*>(OBSE->QueryInterface(kInterface_StringVar));
		g_arrayInterface = static_cast<OBSEArrayVarInterface*>(OBSE->QueryInterface(kInterface_ArrayVar));
		g_eventInterface = static_cast<OBSEEventManagerInterface*>(OBSE->QueryInterface(kInterface_EventManager));
		g_serializationInterface = static_cast<OBSESerializationInterface*>(OBSE->QueryInterface(kInterface_Serialization));
		g_consoleInterface = static_cast<OBSEConsoleInterface*>(OBSE->QueryInterface(kInterface_Console));

		if (g_serializationInterface) {
			g_serializationInterface->SetSaveCallback(g_pluginHandle, SaveCallback);
			g_serializationInterface->SetPreloadCallback(g_pluginHandle, LoadCallback);
		}
#endif
	}

	g_messagingInterface->RegisterListener(g_pluginHandle, nullptr, UnifiedMessageHandler);

	KeywordAPI::Init(g_messagingInterface, g_pluginHandle);
	EditorIDMapper::Init(g_messagingInterface, g_pluginHandle);

	return true;
}
