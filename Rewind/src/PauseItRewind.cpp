#include "PauseItRewind.h"
#include <JMP/Platform.h>

using namespace std::string_view_literals;

namespace PauseIt::Rewind
{
// FIXME: JMP IS BROKEN MEM SEARCH SIG SCAN THING
// FIXME: JMP IS BROKEN MEM SEARCH SIG SCAN THING
// FIXME: JMP IS BROKEN MEM SEARCH SIG SCAN THING
// FIXME: JMP IS BROKEN MEM SEARCH SIG SCAN THING
// FIXME: JMP IS BROKEN MEM SEARCH SIG SCAN THING
// FIXME: JMP IS BROKEN MEM SEARCH SIG SCAN THING
// FIXME: JMP IS BROKEN MEM SEARCH SIG SCAN THING
// usually works tho! just a little stupid lmao

// Linux x86 ONLY
JMP::Signature Plugin::s_physics_run_think_functions("BE 01 00 00 00 90 83 EC 0C 56 E8"sv);
// Linux x86 ONLY
JMP::Signature Plugin::s_server_game_clients_process_usercmds("55 66 0F EF C0 89 E5 57 56 8D 55 E8 53"sv);

Plugin Plugin::s_the;
PLUGIN_EXPOSE(Plugin, Plugin::s_the);

SH_DECL_HOOK1_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool);

bool Plugin::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
    PLUGIN_SAVEVARS();

    std::span<uint8_t> server_binary_bytes;

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
        JMP::Platform::modify_memory_protection(server_binary_bytes, {.read = true, .write = true, .execute = true});
    }
    catch (const std::exception& ex)
    {
        // Linux x86 ONLY
        snprintf(error, maxlen, "Failed to modify memory protection for server_srv.so: %s", ex.what());
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

    // FIXME: better place
    static constexpr auto physics_run_think_functions_patch_offset = 10;

    // xor eax, eax ; Replace call to UTIL_PlayerByIndex (and pretend it returned 0)
    // NOP to pad out whatever instruction was here previously.
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

    return true;
}

bool Plugin::Unload(char* error, size_t maxlen)
{
    m_server_game_clients_process_usercmds.Remove();
    m_physics_run_think_functions.reset();

    return true;
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
}
