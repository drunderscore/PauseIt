#include "PauseItRewind.h"
#include <JMP/Platform.h>
#include <dlfcn.h>
#include <iserver.h>
#include <stdio.h>

SMEXT_LINK(&PauseIt::Rewind::Plugin::the());

namespace PauseIt::Rewind
{
Plugin Plugin::s_the;
const sp_nativeinfo_t Plugin::s_natives[]{
    {"IsPaused", is_paused},
    {"SetPaused", set_paused},
    {nullptr, nullptr},
};

bool Plugin::SDK_OnLoad(char* error, size_t maxlength, bool late)
{
    auto server_library = dlopen("tf/bin/server_srv.so", RTLD_NOW);
    if (!server_library)
    {
        snprintf(error, maxlength, "Failed to dlopen tf/bin/server_srv.so");
        return false;
    }

    auto physics_run_think_functions = memutils->ResolveSymbol(server_library, "_Z25Physics_RunThinkFunctionsb");
    auto server_game_clients_process_usercmds =
        memutils->ResolveSymbol(server_library, "_ZN18CServerGameClients15ProcessUsercmdsEP7edict_tP7bf_readiiibb");
    dlclose(server_library);

    if (!physics_run_think_functions)
    {
        snprintf(error, maxlength, "Failed to find Physics_RunThinkFunctions");
        return false;
    }

    if (!server_game_clients_process_usercmds)
    {
        snprintf(error, maxlength, "Failed to find CServerGameClients::ProcessUsercmds");
        return false;
    }

    std::span<uint8_t> server_binary_bytes;

    try
    {
        server_binary_bytes = JMP::Platform::get_bytes_for_library_name("tf/bin/server_srv.so");
    }
    catch (const std::exception& ex)
    {
        snprintf(error, maxlength, "Failed to get bytes for server_srv.so: %s", ex.what());
        return false;
    }

    try
    {
        JMP::Platform::modify_memory_protection(server_binary_bytes, {.read = true, .write = true, .execute = true});
    }
    catch (const std::exception& ex)
    {
        snprintf(error, maxlength, "Failed to modify memory protection for server_srv.so: %s", ex.what());
        return false;
    }

    static constexpr auto physics_run_think_functions_patch_offset = 0xD4;

    // xor eax, eax ; Replace call to UTIL_PlayerByIndex (and pretend it returned 0)
    // NOP to pad out whatever instruction was here previously.
    m_physics_run_think_functions = std::make_unique<Flask::BytePatch>(
        static_cast<uint8_t*>(physics_run_think_functions) + physics_run_think_functions_patch_offset,
        std::vector<uint8_t>{0x31, 0xC0, 0x90, 0x90, 0x90});

    if (!m_server_game_clients_process_usercmds.Install(
            server_game_clients_process_usercmds, reinterpret_cast<void*>(on_server_game_clients_process_usercmds),
            subhook::HookFlags::HookNoFlags))
    {
        snprintf(error, maxlength, "Failed to hook CServerGameClients::ProcessUsercmds");
        return false;
    }

    sharesys->AddNatives(myself, s_natives);

    return true;
}

void Plugin::SDK_OnUnload()
{
    m_server_game_clients_process_usercmds.Remove();
    m_physics_run_think_functions.reset();
}

float Plugin::on_server_game_clients_process_usercmds(void* self, edict_t* player, bf_read* buf, int numcmds,
                                                      int totalcmds, int dropped_packets, bool ignore, bool paused)
{
    auto& subhook = Plugin::the().m_server_game_clients_process_usercmds;

    subhook::ScopedHookRemove server_game_clients_process_usercmds_subhook_scope_remove(&subhook);

    // Treat "pause" as "ignore".
    return reinterpret_cast<decltype(on_server_game_clients_process_usercmds)*>(subhook.GetSrc())(
        self, player, buf, numcmds, totalcmds, dropped_packets, ignore | paused, paused);
}

cell_t Plugin::is_paused(SourcePawn::IPluginContext*, const cell_t*) { return engine->GetIServer()->IsPaused(); }

cell_t Plugin::set_paused(SourcePawn::IPluginContext*, const cell_t* params)
{
    engine->GetIServer()->SetPaused(params[1]);
    return 0;
}
}
