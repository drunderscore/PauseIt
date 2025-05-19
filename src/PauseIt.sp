#pragma newdecls required

#include <sdktools>

public Plugin myinfo =
{
	name = "Pause It",
	author = "Dr. Underscore",
	description = "Pause for tournament play",
	version = "1.0.0",
	url = "https://github.com/FiresideCasts"
};

ConVar pause_tactical_max;
ConVar pause_technical_max;
ConVar pause_tactical_length;
ConVar pause_technical_length;
ConVar pause_unpause_require_admin;

ArrayList pause_remaining_tactical;
ArrayList pause_remaining_technical;

// FIXME: Need to fix sending chat message
// FIXME: Fix shitty anti-flood plugin

public void OnPluginStart()
{
    pause_tactical_max = CreateConVar("pause_tactical_max", "-1", "Maximum number of tactical pauses a team gets each match. 0 means none, -1 means unlimited.", FCVAR_NONE);
    pause_technical_max = CreateConVar("pause_technical_max", "-1", "Maximum number of technical pauses a team gets each match. 0 means none, -1 means unlimited.", FCVAR_NONE);
    pause_tactical_length = CreateConVar("pause_tactical_length", "60", "The length of a tactical pause, in seconds.", FCVAR_NONE);
    pause_technical_length = CreateConVar("pause_technical_length", "60", "The length of a technical pause, in seconds.", FCVAR_NONE);

    // AddCommandListener(OnPauseCommand, "pause");
    // AddCommandListener(OnPauseCommand, "setpause");
    // AddCommandListener(OnUnpauseCommand, "unpause");

    RegConsoleCmd("pause_tactical", CommandPauseTactical);
    RegConsoleCmd("tactical", CommandPauseTactical);
    RegConsoleCmd("tac", CommandPauseTactical);

    RegConsoleCmd("pause_technical", CommandPauseTechnical);
    RegConsoleCmd("technical", CommandPauseTechnical);
    RegConsoleCmd("tech", CommandPauseTechnical);

    HookEventEx("teamplay_restart_round", OnTeamplayRestartRound, EventHookMode_Post);
}

public void OnMapStart()
{
    InitializeRemainingPauseCounts();
}

void OnTeamplayRestartRound(Event event, const char[] name, bool dontBroadcast)
{
    if (pause_remaining_tactical != null)
    {
        for (int i = 0; i < pause_remaining_tactical.Length; i++)
        {
            ConVar remaining = pause_remaining_tactical.Get(i);
            remaining.IntValue = pause_tactical_max.IntValue;
        }
    }

    if (pause_remaining_technical != null)
    {
        for (int i = 0; i < pause_remaining_technical.Length; i++)
        {
            ConVar remaining = pause_remaining_technical.Get(i);
            remaining.IntValue = pause_technical_max.IntValue;
        }
    }
}

void InitializeRemainingPauseCounts()
{
    if (pause_remaining_tactical == null)
        pause_remaining_tactical = new ArrayList();
    else
        pause_remaining_tactical.Clear();

    if (pause_remaining_technical == null)
        pause_remaining_technical = new ArrayList();
    else
        pause_remaining_technical.Clear();

    int team_count = GetTeamCount();

    char pause_remaining_tactical_initial[16];
    pause_tactical_max.GetString(pause_remaining_tactical_initial, sizeof(pause_remaining_tactical_initial));

    char pause_remaining_technical_initial[16];
    pause_technical_max.GetString(pause_remaining_technical_initial, sizeof(pause_remaining_technical_initial));

    for (int i = 0; i < team_count; i++)
    {
        char team_name[128];
        GetTeamName(i, team_name, sizeof(team_name));
        int team_name_length = strlen(team_name);
        for (int j = 0; j < team_name_length; j++)
            team_name[j] = CharToLower(team_name[j]);

        char convar_name[64 + sizeof(team_name)];
        Format(convar_name, sizeof(convar_name), "pause_remaining_tactical_%s", team_name);
        pause_remaining_tactical.Push(CreateConVar(convar_name, pause_remaining_tactical_initial, "Remaining tactical pauses.", FCVAR_NONE));

        Format(convar_name, sizeof(convar_name), "pause_remaining_technical_%s", team_name);
        pause_remaining_technical.Push(CreateConVar(convar_name, pause_remaining_technical_initial, "Remaining technical pauses.", FCVAR_NONE));
    }
}

Action OnPauseCommand(int client, const char[] command, int argc)
{
    return Plugin_Handled;
}

Action OnUnpauseCommand(int client, const char[] command, int argc)
{
    return Plugin_Handled;
}

void PauseTimerCallback(Handle timer, any data)
{
    FakeClientCommandEx(data, "unpause");
}

Action CommandPauseTactical(int client, int args)
{
    int team = GetClientTeam(client);
    ConVar remaining_convar = pause_remaining_tactical.Get(team);

    if (remaining_convar.IntValue == 0)
    {
        ReplyToCommand(client, "Your team has no remaining tactical pauses.");
        return Plugin_Handled;
    }

    if (remaining_convar.IntValue >= 1)
        remaining_convar.IntValue -= 1;

    FakeClientCommandEx(client, "setpause");
    CreateTimer(pause_tactical_length.FloatValue, PauseTimerCallback, client);

    return Plugin_Handled;
}

Action CommandPauseTechnical(int client, int args)
{
    int team = GetClientTeam(client);
    ConVar remaining_convar = pause_remaining_technical.Get(team);

    if (remaining_convar.IntValue == 0)
    {
        ReplyToCommand(client, "Your team has no remaining technical pauses.");
        return Plugin_Handled;
    }

    if (remaining_convar.IntValue >= 1)
    {
        remaining_convar.IntValue -= 1;
        char team_name[128];
        GetTeamName(team, team_name, sizeof(team_name));

        char first_person_message[256];
        char third_person_message[256];
        char plural[2];
        if (remaining_convar.IntValue == 1)
            plural = "";
        else
            plural = "s";

        Format(first_person_message, sizeof(first_person_message), "Your team has %d technical pause%s remaining.", remaining_convar.IntValue, plural);
        Format(third_person_message, sizeof(third_person_message), "%s team has %d technical pause%s remaining.", team_name, remaining_convar.IntValue, plural);

        int max_clients = GetMaxClients();
        for (int i = 1; i <= max_clients; i++)
        {
            if (!IsClientConnected(i) || IsFakeClient(i))
                continue;

            int chat_team = GetClientTeam(i);
            if (chat_team == team)
                PrintToChat(i, first_person_message);
            else
                PrintToChat(i, third_person_message);
        }
    }

    FakeClientCommandEx(client, "setpause");
    CreateTimer(pause_technical_length.FloatValue, PauseTimerCallback, client);

    return Plugin_Handled;
}
