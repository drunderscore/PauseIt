#pragma once

#include "BytePatch.h"
#include <ISmmPlugin.h>
#include <JMP/Signature.h>
#include <convar.h>
#include <memory>
#include <subhook.h>

namespace PauseIt::Rewind
{
class Plugin final : public ISmmPlugin
{
public:
    bool Load(PluginId, ISmmAPI*, char* error, size_t maxlen, bool late) override;
    bool Unload(char* error, size_t maxlen) override;

    const char* GetAuthor() override { return "resin"; }
    const char* GetName() override { return "Pause It: Rewind"; }
    const char* GetDescription() override { return "Fix bugs during server processing and simulation whilst paused."; }
    const char* GetURL() override { return "https://jame.xyz"; }
    const char* GetLicense() override { return "Hopefully BSD 2-Clause"; }
    const char* GetVersion() override { return "1.0.0"; }
    // FIXME: Doesn't this make the build non-reproducible?
    const char* GetDate() override { return __DATE__; }
    const char* GetLogTag() override { return GetName(); }

    static Plugin& the() { return s_the; }
    static Plugin s_the;

private:
    // FIXME: Linux servers have symbols available, use the names instead of signature scanning.
    static JMP::Signature s_physics_run_think_functions;
    static JMP::Signature s_server_game_clients_process_usercmds;
    static float on_server_game_clients_process_usercmds(void* self, edict_t* player, bf_read* buf, int numcmds,
                                                         int totalcmds, int dropped_packets, bool ignore, bool paused);

    std::unique_ptr<Flask::BytePatch> m_physics_run_think_functions;
    subhook::Hook m_server_game_clients_process_usercmds;
};

PLUGIN_GLOBALVARS();
}
