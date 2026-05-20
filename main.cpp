#include "src/Hooks.h"
#include "src/Manager.h"
#include "EditorIDMapper/EditorIDMapperAPI.h"
#include "OBSEKeywords/KeywordAPI.h"

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

	if (!OBSE->isEditor)
	{
#if OBLIVION
		g_script = static_cast<OBSEScriptInterface*>(OBSE->QueryInterface(kInterface_Script));
		g_stringInterface = static_cast<OBSEStringVarInterface*>(OBSE->QueryInterface(kInterface_StringVar));
		g_arrayInterface = static_cast<OBSEArrayVarInterface*>(OBSE->QueryInterface(kInterface_ArrayVar));
		g_eventInterface = static_cast<OBSEEventManagerInterface*>(OBSE->QueryInterface(kInterface_EventManager));
		g_serializationInterface = static_cast<OBSESerializationInterface*>(OBSE->QueryInterface(kInterface_Serialization));
		g_consoleInterface = static_cast<OBSEConsoleInterface*>(OBSE->QueryInterface(kInterface_Console));
#endif
	}

	g_messagingInterface->RegisterListener(g_pluginHandle, nullptr, UnifiedMessageHandler);

	KeywordAPI::Init(g_messagingInterface, g_pluginHandle);
	EditorIDMapper::Init(g_messagingInterface, g_pluginHandle);

	return true;
}
