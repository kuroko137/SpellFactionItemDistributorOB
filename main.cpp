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
static constexpr UInt32 kChunkVersion = 1;

void SaveCallback(void*)
{
	auto* mgr = SpellFactionItemDistributor::Manager::GetSingleton();
	auto* intfc = g_serializationInterface;
	if (!intfc) return;

	uint64_t currentCRC = crc::ConfigFolderCRC(
		std::filesystem::path(R"(Data\OBSE\Plugins\SpellFactionItemDistributor)"));
	intfc->OpenRecord(kChunk_ConfigCRC, kChunkVersion);
	intfc->WriteRecordData(&currentCRC, sizeof(currentCRC));

	UInt32 count = 0;
	for (UInt32 refID : mgr->processedForms) {
		if ((refID >> 24) != 0xFF) count++;
	}
	if (count == 0) {
		_MESSAGE("SFID SaveCallback: processedForms empty, nothing written");
		return;
	}

	intfc->OpenRecord(kChunk_ProcessedForms, kChunkVersion);
	intfc->WriteRecordData(&count, sizeof(count));
	for (UInt32 refID : mgr->processedForms) {
		if ((refID >> 24) != 0xFF) {
			intfc->WriteRecordData(&refID, sizeof(refID));
		}
	}
	_MESSAGE("SFID SaveCallback: wrote %d refIDs", count);
}

void LoadCallback(void* reserved)
{
	auto* mgr = SpellFactionItemDistributor::Manager::GetSingleton();
	auto* intfc = g_serializationInterface;
	if (!intfc) return;

	uint64_t savedCRC = 0;

	UInt32 type, version, length;
	while (intfc->GetNextRecordInfo(&type, &version, &length)) {
		if (type == kChunk_ConfigCRC) {
			intfc->ReadRecordData(&savedCRC, sizeof(savedCRC));
			_MESSAGE("SFID LoadCallback: read config CRC %016llX", savedCRC);
		}
		else if (type == kChunk_ProcessedForms) {
			UInt32 count;
			intfc->ReadRecordData(&count, sizeof(count));
			UInt32 loaded = 0;
			for (UInt32 i = 0; i < count; i++) {
				UInt32 refID;
				intfc->ReadRecordData(&refID, sizeof(refID));
				mgr->processedForms.emplace(refID);
				loaded++;
			}
			_MESSAGE("SFID LoadCallback: loaded %d refIDs from co-save", loaded);
		}
	}
	_MESSAGE("SFID LoadCallback: processedForms now has %zu entries",
		mgr->processedForms.size());

	uint64_t currentCRC = crc::ConfigFolderCRC(
		std::filesystem::path(R"(Data\OBSE\Plugins\SpellFactionItemDistributor)"));
	if (currentCRC == 0) {
		currentCRC = crc::ConfigFolderCRC(
			std::filesystem::path(R"(Data\SpellFactionItemDistributor)"));
	}

	if (savedCRC != 0 && currentCRC != savedCRC) {
		_MESSAGE("SFID: Config changed (CRC %016llX -> %016llX), clearing processedForms and resetting init",
			savedCRC, currentCRC);
		mgr->processedForms.clear();
		mgr->ResetInit();
	}
	else {
		_MESSAGE("SFID: Config unchanged (CRC %016llX), keeping processedForms", currentCRC);
	}
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
