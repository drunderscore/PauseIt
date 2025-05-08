#include "PauseItRewind.h"
#include "hl2sdk/game/server/enginecallback.h"
#include "ihandleentity.h"
#include <JMP/Platform.h>
#include <cstring>
#include <eiface.h>
#include <iclient.h>
#include <igameevents.h>
#include <iplayerinfo.h>
#include <iserver.h>
#include <server_class.h>
#include <span>
#include <tier1/mempool.h>
#include <toolframework/ienginetool.h>

class CHLTVEntityData;
class CReplayEntityData;
class CEventInfo;
typedef intptr_t PackedEntityHandle_t;

class CFrameSnapshotEntry
{
public:
    ServerClass* m_pClass;
    int m_nSerialNumber;
    // Keeps track of the fullpack info for this frame for all entities in any pvs:
    PackedEntityHandle_t m_pPackedData;
};

class CFrameSnapshot
{
    DECLARE_FIXEDSIZE_ALLOCATOR(CFrameSnapshot);

public:
    CFrameSnapshot();
    ~CFrameSnapshot();

    // Reference-counting.
    void AddReference();
    void ReleaseReference();

    CFrameSnapshot* NextSnapshot() const;

public:
    CInterlockedInt m_ListIndex; // Index info CFrameSnapshotManager::m_FrameSnapshots.

    // Associated frame.
    int m_nTickCount; // = sv.tickcount

    // State information
    CFrameSnapshotEntry* m_pEntities;
    int m_nNumEntities; // = sv.num_edicts

    // This list holds the entities that are in use and that also aren't entities for inactive clients.
    unsigned short* m_pValidEntities;
    int m_nValidEntities;

    // Additional HLTV info
    CHLTVEntityData* m_pHLTVEntityData;     // is NULL if not in HLTV mode or array of m_pValidEntities entries
    CReplayEntityData* m_pReplayEntityData; // is NULL if not in replay mode or array of m_pValidEntities entries

    CEventInfo** m_pTempEntities; // temp entities
    int m_nTempEntities;

    CUtlVector<int> m_iExplicitDeleteSlots;

private:
    // Snapshots auto-delete themselves when their refcount goes to zero.
    CInterlockedInt m_nReferences;
};

class CBaseClient : public IGameEventListener2, public IClient, public IClientMessageHandler
{
};

// We need 3 things:
// - Fix usercmd processing during pause
//   Hook CServerGameClients::ProcessUsercmds to treat paused as ignored.
// - Fix physics thinking/server simulation during pause
//   @ 0x00789154, replacing call to UTIL_PlayerByIndex, make it's supposed retval be  good enough. could also shunt
//   maxClients or whatever.
//   xor eax, eax
//   nop
//   nop
//   nop

// - Fix disconnecting players & "repausing"
// FIXME: looks like my current thought path will still dispatch player_disconnect event?
// FIXME: ^ an actual problem: things refer to the UserId which refers to clients, but it goes -1 other cause we get rid
// of it the client but not the player. GetPlayerUserId could probably return something stable regardless probably?
// reserve a client for that player.
//
// OKAY so we have to:
// - Patch CGameClient::SpawnPlayer to not invoke CServerGameEnts::FreeContainingEntity and InitializeEntityDLLFields
// (don't free entity and initialize edict, destroying existing)
// - Patch ClientPutInServer to only invoke if edict is not FL_EDICT_FULL (don't CTFPlayer::CreatePlayer if this edict
// already has an entity)
// FIXME: need to detect when to do this logic or not
// - Patch FinishClientPutInServer to not invoke CBaseEntity::Spawn and CBasePlayer::InitialSpawn (by NO GOOD MEANS HOW
// TF DO WE DETECT THIS??)
//
// - Patch CFrameSnapshotManager::TakeTickSnapshot to not dispose clients who are not active (fixes packing entities)

using namespace std::string_view_literals;

namespace PauseIt::Rewind
{
class BaseAccessor : public IConCommandBaseAccessor
{
public:
    bool RegisterConCommandBase(ConCommandBase* pCommandBase) override
    {
        /* Always call META_REGCVAR instead of going through the engine. */
        return META_REGCVAR(pCommandBase);
    }
} s_base_accessor;

// FIXME: JMP IS BROKEN MEM SEARCH SIG SCAN THING
// FIXME: JMP IS BROKEN MEM SEARCH SIG SCAN THING
// FIXME: JMP IS BROKEN MEM SEARCH SIG SCAN THING
// FIXME: JMP IS BROKEN MEM SEARCH SIG SCAN THING
// FIXME: JMP IS BROKEN MEM SEARCH SIG SCAN THING
// FIXME: JMP IS BROKEN MEM SEARCH SIG SCAN THING
// FIXME: JMP IS BROKEN MEM SEARCH SIG SCAN THING

// Linux x86 ONLY
JMP::Signature Plugin::s_physics_run_think_functions("BE 01 00 00 00 90 83 EC 0C 56 E8"sv);
// Linux x86 ONLY
JMP::Signature Plugin::s_server_game_clients_process_usercmds("55 66 0F EF C0 89 E5 57 56 8D 55 E8 53"sv);
// Linux x86 ONLY
JMP::Signature Plugin::s_frame_snapshot_manager_take_tick_snapshot("55 89 E5 57 56 53 81 EC 20 10 00 00"sv);
// Linux x86 ONLY
// FIXME: Really shitty signature
// FIXME: wtf why doesnt this work???
// JMP::Signature Plugin::s_game_client_spawn_player("55 89 E5 56 53 83 EC 20 80 3D"sv);
// THIS ONE ISNT ACTUALLY AT THE HEAD
JMP::Signature Plugin::s_game_client_spawn_player("50 FF 52 10 59 FF B3"sv);

// JMP::Signature Plugin::s_finish_client_put_in_server("55 89 E5 57 56 53 81 EC 94 00 00 00 A1 70 13 7F 01"sv);
JMP::Signature Plugin::s_finish_client_put_in_server("55 89 E5 57 56 53 81 EC 94 00 00 00 A1 70"sv);
JMP::Signature Plugin::s_client_put_in_server("55 89 E5 53 83 EC 0C 8B 5D 0C FF 75 08"sv);
JMP::Signature Plugin::s_game_client_get_send_frame("55 66 0F EF C0 89 E5 56 53 83 EC 10 A1"sv);

Plugin Plugin::s_the;
PLUGIN_EXPOSE(Plugin, Plugin::s_the);

SH_DECL_HOOK1_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool);

bool Plugin::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
    PLUGIN_SAVEVARS();

    std::span<uint8_t> server_binary_bytes;
    std::span<uint8_t> engine_binary_bytes;

    try
    {
        // Linux x86 ONLY
        server_binary_bytes = JMP::Platform::get_bytes_for_library_name("tf/bin/server_srv.so");
    }
    catch (const std::exception& ex)
    {
        snprintf(error, maxlen, "Failed to get bytes for server_srv.so: %s", ex.what());
        return false;
    }

    try
    {
        // Linux x86 ONLY
        engine_binary_bytes = JMP::Platform::get_bytes_for_library_name("bin/engine_srv.so");
        Msg("Engine: %d (0x%x) bytes @ 0x%x\n", engine_binary_bytes.size(), engine_binary_bytes.size(),
            engine_binary_bytes.data());
    }
    catch (const std::exception& ex)
    {
        snprintf(error, maxlen, "Failed to get bytes for engine_srv.so: %s", ex.what());
        return false;
    }

    try
    {
        JMP::Platform::modify_memory_protection(server_binary_bytes, {.read = true, .write = true, .execute = true});
    }
    catch (const std::exception& ex)
    {
        // Linux x86 ONLY
        snprintf(error, maxlen, "Failed to modify memory protection for server_srv.so: %s", ex.what());
        return false;
    }

    try
    {
        JMP::Platform::modify_memory_protection(engine_binary_bytes, {.read = true, .write = true, .execute = true});
    }
    catch (const std::exception& ex)
    {
        // Linux x86 ONLY
        snprintf(error, maxlen, "Failed to modify memory protection for engine_srv.so: %s", ex.what());
        return false;
    }

    auto physics_run_think_functions = s_physics_run_think_functions.find_in(server_binary_bytes);
    if (!physics_run_think_functions)
    {
        snprintf(error, maxlen, "Failed to find Physics_RunThinkFunctions");
        return false;
    }

    auto server_game_clients_process_usercmds = s_server_game_clients_process_usercmds.find_in(server_binary_bytes);
    if (!server_game_clients_process_usercmds)
    {
        snprintf(error, maxlen, "Failed to find CServerGameClients::ProcessUsercmds");
        return false;
    }

    // FIXME: debug only!
    auto frame_snapshot_manager_take_tick_snapshot =
        s_frame_snapshot_manager_take_tick_snapshot.find_in(engine_binary_bytes);
    if (!frame_snapshot_manager_take_tick_snapshot)
    {
        snprintf(error, maxlen, "Failed to find CFrameSnapshotManager::TakeTickSnapshot");
        return false;
    }

    // static const char needle[] = "\x55\x89\xE5\x56\x53\x83\xEC\x20";
    // auto fuck = memmem(engine_binary_bytes.data(), engine_binary_bytes.size(), needle, sizeof(needle) - 1);
    // Msg("Fuck: 0x%x\n", fuck);
    auto game_client_spawn_player = s_game_client_spawn_player.find_in(engine_binary_bytes);
    if (!game_client_spawn_player)
    {
        snprintf(error, maxlen, "Failed to find CGameClient::SpawnPlayer");
        return false;
    }

    auto finish_client_put_in_server = s_finish_client_put_in_server.find_in(server_binary_bytes);
    if (!finish_client_put_in_server)
    {
        snprintf(error, maxlen, "Failed to find FinishClientPutInServer");
        return false;
    }

    auto client_put_in_server = s_client_put_in_server.find_in(server_binary_bytes);
    if (!client_put_in_server)
    {
        snprintf(error, maxlen, "Failed to find ClientPutInServer");
        return false;
    }

    m_game_client_get_send_frame =
        reinterpret_cast<GameClientGetSendFrame>(s_game_client_get_send_frame.find_in(engine_binary_bytes));
    if (!m_game_client_get_send_frame)
    {
        snprintf(error, maxlen, "Failed to find CGameClient::GetSendFrame");
        return false;
    }

    // FIXME: better place
    static constexpr auto physics_run_think_functions_patch_offset = 10;

    m_physics_run_think_functions = std::make_unique<Flask::BytePatch>(
        static_cast<uint8_t*>(physics_run_think_functions) + physics_run_think_functions_patch_offset,
        std::vector<uint8_t>{0x31, 0xC0, 0x90, 0x90, 0x90});

    if (!m_server_game_clients_process_usercmds.Install(
            server_game_clients_process_usercmds, reinterpret_cast<void*>(on_server_game_clients_process_usercmds),
            subhook::HookFlags::HookNoFlags))
    {
        snprintf(error, maxlen, "Failed to hook CServerGameClients::ProcessUsercmds");
        return false;
    }

    if (!m_frame_snapshot_manager_take_tick_snapshot.Install(
            frame_snapshot_manager_take_tick_snapshot,
            reinterpret_cast<void*>(on_frame_snapshot_manager_take_tick_snapshot), subhook::HookFlags::HookNoFlags))
    {
        snprintf(error, maxlen, "Failed to hook CFrameSnapshotManager::TakeTickSnapshot");
        return false;
    }

    // FIXME: better place
    static constexpr auto frame_snapshot_manager_take_tick_snapshot_patch_offset = 137;

    // FIXME: maybe jmp instead
    // Remove the IClient::IsActive check. long-winded, because it does non-virtual thunk and virtual inlining bullshit!
    m_frame_snapshot_manager_take_tick_snapshot_patch = std::make_unique<Flask::BytePatch>(
        static_cast<uint8_t*>(frame_snapshot_manager_take_tick_snapshot) +
            frame_snapshot_manager_take_tick_snapshot_patch_offset,
        std::vector<uint8_t>{
            0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
            0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        });

    // FIXME: better place
    // for the other one in the fixme
    // static constexpr auto game_client_spawn_player_patch_offset = 24;
    static constexpr auto game_client_spawn_player_patch_offset = -16;

    // Jump over calls to CServerGameEnts::FreeContainingEntity and InitializeEntityDLLFields (don't stomp us).
    m_game_client_spawn_player = std::make_unique<Flask::BytePatch>(static_cast<uint8_t*>(game_client_spawn_player) +
                                                                        game_client_spawn_player_patch_offset,
                                                                    std::vector<uint8_t>{0xEB, 0x21});

    if (!m_finish_client_put_in_server.Install(finish_client_put_in_server,
                                               reinterpret_cast<void*>(on_finish_client_put_in_server),
                                               subhook::HookFlags::HookNoFlags))
    {
        snprintf(error, maxlen, "Failed to hook FinishClientPutInServer");
        return false;
    }

    if (!m_client_put_in_server.Install(client_put_in_server, reinterpret_cast<void*>(on_client_put_in_server),
                                        subhook::HookFlags::HookNoFlags))
    {
        snprintf(error, maxlen, "Failed to hook ClientPutInServer");
        return false;
    }

    // FIXME: don't need some of these
    // FIXME: shouldn't happen in this order
    GET_V_IFACE_CURRENT(GetEngineFactory, m_cvar, ICvar, CVAR_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetEngineFactory, m_engine_server, IVEngineServer, INTERFACEVERSION_VENGINESERVER);
    // FIXME: only for debug btw
    GET_V_IFACE_CURRENT(GetServerFactory, m_server_game_ents, IServerGameEnts, INTERFACEVERSION_SERVERGAMEENTS);
    // FIXME: only for debug btw
    GET_V_IFACE_CURRENT(GetServerFactory, m_player_info_manager, IPlayerInfoManager,
                        INTERFACEVERSION_PLAYERINFOMANAGER);
    GET_V_IFACE_CURRENT(GetServerFactory, m_server_game_dll, IServerGameDLL, INTERFACEVERSION_SERVERGAMEDLL);

    // GET_V_IFACE_CURRENT(GetEngineFactory, m_engine_tool, IEngineTool, VENGINETOOL_INTERFACE_VERSION);

    SH_ADD_HOOK(IServerGameDLL, GameFrame, m_server_game_dll, SH_MEMBER(this, &Plugin::on_game_frame), true);

    // FIXME: Don't need?
#if SOURCE_ENGINE >= SE_ORANGEBOX
    g_pCVar = m_cvar;
    ConVar_Register(0, &s_base_accessor);
#else
    ConCommandBaseMgr::OneTimeInit(&s_BaseAccessor);
#endif

    // FIXME: need?
    g_SMAPI->AddListener(this, this);

    // Entry 46
    m_base_server_remove_client_from_game_original_entry =
        reinterpret_cast<decltype(on_base_server_remove_client_from_game)**>(
            &(*reinterpret_cast<void***>(m_engine_server->GetIServer()))[46]);

    m_base_server_remove_client_from_game_original = *m_base_server_remove_client_from_game_original_entry;
    *m_base_server_remove_client_from_game_original_entry = on_base_server_remove_client_from_game;

    return true;
}

bool Plugin::Unload(char* error, size_t maxlen)
{
    // FIXME: better destruction order
    m_physics_run_think_functions.reset();
    m_server_game_clients_process_usercmds.Remove();
    m_frame_snapshot_manager_take_tick_snapshot.Remove();
    m_finish_client_put_in_server.Remove();
    m_client_put_in_server.Remove();

    if (m_base_server_remove_client_from_game_original_entry && m_base_server_remove_client_from_game_original)
    {
        *m_base_server_remove_client_from_game_original_entry = m_base_server_remove_client_from_game_original;
        m_base_server_remove_client_from_game_original_entry = nullptr;
        m_base_server_remove_client_from_game_original = nullptr;
    }

    SH_REMOVE_HOOK(IServerGameDLL, GameFrame, m_server_game_dll, SH_MEMBER(this, &Plugin::on_game_frame), true);

    return true;
}

void Plugin::on_client_put_in_server(edict_t* edict, const char* player_name)
{
    // Only do this if we don't have a full server entity yet, otherwise use the existing one.
    if (!(edict->m_fStateFlags & FL_EDICT_FULL))
    {
        auto& subhook = Plugin::the().m_client_put_in_server;
        subhook::ScopedHookRemove client_put_in_server_subhook_scope_remove(&subhook);
        reinterpret_cast<decltype(on_client_put_in_server)*>(subhook.GetSrc())(edict, player_name);
    }
}

void Plugin::fuck()
{
    for (auto i = 1; i <= 4; i++)
    {
        auto edict = Plugin::the().m_engine_server->PEntityOfEntIndex(i);
        Msg("Edict %d: free = %d\n", i, edict->IsFree());
        Msg("Classname = %s\n", edict->GetClassName());
        auto networkable = edict->GetNetworkable();
        Msg("...networkable = 0x%x\n", networkable);
        if (networkable)
        {
            auto networkable_server_class = networkable->GetServerClass();
            Msg("...networkable server class = 0x%x\n", networkable_server_class);

            if (networkable_server_class)
                Msg("...networkable server class name = %s\n", networkable_server_class->GetName());
        }

        auto player_info = Plugin::the().m_player_info_manager->GetPlayerInfo(edict);
        Msg("...player info = 0x%x\n", player_info);
        if (player_info)
        {
            Msg("...player info connected = %d\n", player_info->IsConnected());
            Msg("...player info name = %s\n", player_info->GetName());
        }

        auto entity = Plugin::the().m_server_game_ents->EdictToBaseEntity(edict);
        Msg("...base entity = 0x%x\n", entity);
    }
}

CFrameSnapshot* Plugin::on_frame_snapshot_manager_take_tick_snapshot(void* self, int tickcount)
{
    auto& subhook = Plugin::the().m_frame_snapshot_manager_take_tick_snapshot;

    subhook::ScopedHookRemove frame_snapshot_manager_take_tick_snapshot_subhook_scope_remove(&subhook);

    auto value =
        reinterpret_cast<decltype(on_frame_snapshot_manager_take_tick_snapshot)*>(subhook.GetSrc())(self, tickcount);
    // Msg("CFrameSnapshotManager::TakeTickSnapshot(0x%x, %d) = 0x%x\n", self, tickcount, value);

    // for (auto i = 0; i < value->m_nNumEntities; i++)
    // {
    //     auto entry = value->m_pEntities[i];
    //     if (!entry.m_pClass)
    //         Warning("Entity %d serial %d missing class\n", i, entry.m_nSerialNumber);
    // }

    return value;
}

// Do nothing.
// TODO: WE DONT WANT TO DO THIS !!!!
// TODO: WE DONT WANT TO DO THIS !!!!
// TODO: WE DONT WANT TO DO THIS !!!!
// TODO: WE DONT WANT TO DO THIS !!!!
// TODO: WE DONT WANT TO DO THIS !!!!
// Do nothing.
static ConVar pause_fuck_you("pause_fuck_you", "0");

// typedef void (*BaseServerSendClientMessagesFn)(void* self, bool send_snapshots);
typedef void (*BaseClientSendSnaphot)(void* self, CClientFrame* frame);

void Plugin::on_finish_client_put_in_server(CTFPlayer* player)
{
    Warning("Finish!\n");
    if (pause_fuck_you.GetBool())
    {
        // FIXME: THIS IS NOT WHAT WE SHOULD DO WE WANT TO DO SV_ForceUpdate BRO!!! which will force the next time we do
        // this to force it instead. smarter
        // (*reinterpret_cast<BaseServerSendClientMessagesFn**>(Plugin::the().m_engine_server->GetIServer()))[47](
        //     Plugin::the().m_engine_server->GetIServer(), true);
        // Warning("Doing a shitty SendClientMessages\n");

        Msg("We have player 0x%x, which we think is %d\n", player,
            reinterpret_cast<IHandleEntity*>(player)->GetRefEHandle().GetEntryIndex());

        auto instance = Plugin::the().m_engine_server->GetIServer()->GetClient(
            reinterpret_cast<IHandleEntity*>(player)->GetRefEHandle().GetEntryIndex() - 1);

        Msg("that gets us to client 0x%x\n", instance);

        // disabled cause it doesnt actually work LMAO
        if (instance && !instance->IsFakeClient() && !instance->IsHLTV() && !instance->IsReplay() && false)
        {
            // btw: this stuff doesnt actually work lololol
            auto frame = Plugin::the().m_game_client_get_send_frame(instance);
            Msg("to get frame 0x%x\n", frame);

            // entry 52 is CBaseClient::SendSnapshot i think?
            (*reinterpret_cast<BaseClientSendSnaphot**>(instance))[52](instance, frame);
        }
    }
    else
    {
        auto& subhook = Plugin::the().m_finish_client_put_in_server;

        subhook::ScopedHookRemove finish_client_put_in_server_subhook_scope_remove(&subhook);

        reinterpret_cast<decltype(on_finish_client_put_in_server)*>(subhook.GetSrc())(player);
    }

    // if (Plugin::the().m_engine_tool->IsGamePaused())
    {
        // Warning("SV_ForceSend because FinishClientPutInServer whilst paused\n");
        // Plugin::the().m_engine_tool->ForceSend();
    }
}

float Plugin::on_server_game_clients_process_usercmds(void* self, edict_t* player, bf_read* buf, int numcmds,
                                                      int totalcmds, int dropped_packets, bool ignore, bool paused)
{
    auto& subhook = Plugin::the().m_server_game_clients_process_usercmds;

    subhook::ScopedHookRemove server_game_clients_process_usercmds_subhook_scope_remove(&subhook);

    return reinterpret_cast<decltype(on_server_game_clients_process_usercmds)*>(subhook.GetSrc())(
        self, player, buf, numcmds, totalcmds, dropped_packets, ignore | paused, paused);
}

void Plugin::on_base_server_remove_client_from_game(void* self, CBaseClient* client)
{
    // Warning("hi there i renove client from game sometimes\n");
    if (!client->IsFakeClient() && !client->IsHLTV() && !client->IsReplay())
        Warning("Ignoring ask to remove client %s from game!\n", client->GetClientName());
    else
        Plugin::the().m_base_server_remove_client_from_game_original(self, client);

    // Plugin::the().m_base_server_remove_client_from_game_original(self, client);
    // auto& subhook = Plugin::the().m_server_game_clients_process_usercmds;

    // subhook::ScopedHookRemove server_game_clients_process_usercmds_subhook_scope_remove(&subhook);

    // return reinterpret_cast<decltype(on_server_game_clients_process_usercmds)*>(subhook.GetSrc())(
    // self, player, buf, numcmds, totalcmds, dropped_packets, ignore | paused, paused);
}

void Plugin::pause_rewind_check(const CCommand& args) { Plugin::fuck(); }

void Plugin::on_game_frame(bool) {}

ConCommand Plugin::s_pause_rewind_check("pause_rewind_check", Plugin::pause_rewind_check);
}
