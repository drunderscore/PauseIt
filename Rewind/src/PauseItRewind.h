#pragma once

#include "BytePatch.h"
#include <smsdk_ext.h>
#include <subhook.h>

class bf_read;

namespace PauseIt::Rewind
{
class Plugin final : public SDKExtension
{
public:
    static Plugin& the() { return s_the; }

    bool SDK_OnLoad(char* error, size_t maxlength, bool late) override;
    void SDK_OnUnload() override;

private:
    static Plugin s_the;
    static const sp_nativeinfo_t s_natives[];

    static float on_server_game_clients_process_usercmds(void* self, edict_t* player, bf_read* buf, int numcmds,
                                                         int totalcmds, int dropped_packets, bool ignore, bool paused);

    static cell_t is_paused(SourcePawn::IPluginContext*, const cell_t* params);
    static cell_t set_paused(SourcePawn::IPluginContext*, const cell_t* params);

    std::unique_ptr<Flask::BytePatch> m_physics_run_think_functions;
    subhook::Hook m_server_game_clients_process_usercmds;
};
}
