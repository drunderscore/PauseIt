#pragma once

#include "BytePatch.h"
#include <ISmmPlugin.h>
#include <JMP/Signature.h>
#include <convar.h>
#include <memory>
#include <subhook.h>

class CBaseClient;
class CClientFrame;
class CCommand;
class CFrameSnapshot;
class CTFPlayer;
class ICVar;
class IPlayerInfoManager;
class IServerGameDLL;
class IServerGameEnts;
class IVEngineServer;
struct IEngineTool;
struct bf_read;
struct edict_t;

namespace PauseIt::Rewind
{
class Plugin final : public ISmmPlugin, public IMetamodListener
{
public:
    bool Load(PluginId, ISmmAPI*, char* error, size_t maxlen, bool late) override;
    bool Unload(char* error, size_t maxlen) override;

    const char* GetAuthor() override { return "James Puleo (Dr. Underscore)"; }
    const char* GetName() override { return "PauseIt: Rewind"; }
    const char* GetDescription() override
    {
        return "Fixes bugs during server processing and simulation whilst paused.";
    }
    // TODO: I made this! where are we putting it tho. i want to own it i think. i do.
    const char* GetURL() override { return "https://firesidecasts.tv"; }
    const char* GetLicense() override { return "Hopefully BSD 2-Clause"; }
    const char* GetVersion() override { return "1.0.0"; }
    // FIXME: Doesn't this make the build non-reproducible?
    const char* GetDate() override { return __DATE__; }
    const char* GetLogTag() override { return GetName(); }

    static Plugin& the() { return s_the; }
    static Plugin s_the;

    static void fuck();

    void on_game_frame(bool simulating);

private:
    // FIXME: Aren't these available as symbols even if we don't hook to the top of the function, still good lol.
    static JMP::Signature s_physics_run_think_functions;
    static JMP::Signature s_server_game_clients_process_usercmds;
    static JMP::Signature s_frame_snapshot_manager_take_tick_snapshot;
    static JMP::Signature s_game_client_spawn_player;
    static JMP::Signature s_finish_client_put_in_server;
    static JMP::Signature s_client_put_in_server;
    static JMP::Signature s_game_client_get_send_frame;

    IVEngineServer* m_engine_server{};
    // FIXME: only for debug btw
    IServerGameEnts* m_server_game_ents{};
    // FIXME: only for debug btw
    IPlayerInfoManager* m_player_info_manager{};
    // FIXME: only for debug btw
    ICvar* m_cvar{};
    // FIXME: only for debug btw
    IServerGameDLL* m_server_game_dll{};
    IEngineTool* m_engine_tool{};
    std::unique_ptr<Flask::BytePatch> m_physics_run_think_functions;
    subhook::Hook m_server_game_clients_process_usercmds;
    subhook::Hook m_frame_snapshot_manager_take_tick_snapshot;
    subhook::Hook m_finish_client_put_in_server;
    subhook::Hook m_client_put_in_server;
    std::unique_ptr<Flask::BytePatch> m_frame_snapshot_manager_take_tick_snapshot_patch;
    std::unique_ptr<Flask::BytePatch> m_game_client_spawn_player;

    typedef CClientFrame* (*GameClientGetSendFrame)(void* self);

    GameClientGetSendFrame m_game_client_get_send_frame{};

    // FIXME: POSIX-only calling convention among all the other issues
    static float on_server_game_clients_process_usercmds(void* self, edict_t* player, bf_read* buf, int numcmds,
                                                         int totalcmds, int dropped_packets, bool ignore, bool paused);
    static void on_base_server_remove_client_from_game(void* self, CBaseClient*);
    static CFrameSnapshot* on_frame_snapshot_manager_take_tick_snapshot(void* self, int tickcount);

    static void on_finish_client_put_in_server(CTFPlayer*);
    static void on_client_put_in_server(edict_t*, const char* player_name);

    decltype(on_base_server_remove_client_from_game)** m_base_server_remove_client_from_game_original_entry{};
    decltype(on_base_server_remove_client_from_game)* m_base_server_remove_client_from_game_original{};

    static void pause_rewind_check(const CCommand&);
    static ConCommand s_pause_rewind_check;
};

PLUGIN_GLOBALVARS();
}
