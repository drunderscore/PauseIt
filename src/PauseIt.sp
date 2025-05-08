#pragma newdecls required

#include <sdktools>
#include <f2stocks>
#include <tf2>

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

ConVar sv_pausable;

// FIXME: This really has the dual-meaning of "is the server paused" AND "did we do the pause".
//        In 100% of cases you'd actually want one or the other.
bool is_paused;

bool allow_pause;
bool allow_unpause;

bool allow_player_unpause;

public void OnPluginStart()
{
    pause_tactical_max = CreateConVar("pause_tactical_max", "-1", "Maximum number of tactical pauses a team gets each match. 0 means none, -1 means unlimited.", FCVAR_NONE);
    pause_technical_max = CreateConVar("pause_technical_max", "-1", "Maximum number of technical pauses a team gets each match. 0 means none, -1 means unlimited.", FCVAR_NONE);
    pause_tactical_length = CreateConVar("pause_tactical_length", "60", "The length of a tactical pause, in seconds.", FCVAR_NONE);
    pause_technical_length = CreateConVar("pause_technical_length", "60", "The length of a technical pause, in seconds.", FCVAR_NONE);

    AddCommandListener(OnPauseCommand, "pause");
    AddCommandListener(OnSetPauseCommand, "setpause");
    AddCommandListener(OnUnpauseCommand, "unpause");

    RegConsoleCmd("pause_tactical", CommandPauseTactical);
    RegConsoleCmd("tactical", CommandPauseTactical);
    RegConsoleCmd("tac", CommandPauseTactical);

    RegConsoleCmd("pause_technical", CommandPauseTechnical);
    RegConsoleCmd("technical", CommandPauseTechnical);
    RegConsoleCmd("tech", CommandPauseTechnical);

    HookEventEx("teamplay_restart_round", OnTeamplayRestartRound, EventHookMode_Post);

    sv_pausable = FindConVar("sv_pausable");

    // From F2's pause plugin
    // FIXME: Replace with a fix to allows players to connect during a pause, probably inside of Rewind.
    RegConsoleCmd("repause", CommandUnpausePause);
    RegConsoleCmd("unpausepause", CommandUnpausePause);
    RegConsoleCmd("pauseunpause", CommandUnpausePause);

    // From F2's pause plugin
    // FIXME: Replace with correct fix to TF2!
    AddCommandListener(CommandSay, "say");
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

void PauseWithoutSideEffects(int client)
{
    allow_pause = true;
    FakeClientCommand(client, "setpause");
    allow_pause = false;
}

void Pause(int client)
{
    is_paused = true;
    PauseWithoutSideEffects(client);
}

void UnpauseWithoutSideEffects(int client)
{
    allow_unpause = true;
    FakeClientCommand(client, "unpause");
    allow_unpause = false;
}

void Unpause(int client)
{
    is_paused = false;
    UnpauseWithoutSideEffects(client);
    allow_player_unpause = false;
}

Action PlayerTryUnpause(int client)
{
    if (!allow_player_unpause)
        return Plugin_Continue;

    allow_player_unpause = false;

    int team = GetClientTeam(client);

    char team_name[128];
    GetTeamName(team, team_name, sizeof(team_name));

    char first_person_message[256];
    char third_person_message[256];

    Format(first_person_message, sizeof(first_person_message), "Your team is unpausing the game.");
    Format(third_person_message, sizeof(third_person_message), "%s team is unpausing the game.", team_name);
    PrintToChatAllPerspective(first_person_message, third_person_message, team);

    float length = 10.0;
    CreateTimer(length, PauseTimerCallback, client);
    CreateTimer(length - 10, PauseTimerInformTickCallback, RoundToFloor(length));
    for (int i = 1; i <= 3; i++)
        CreateTimer(length - i, PauseTimerInformTickCallback, i);

    return Plugin_Handled;
}

Action OnPauseCommand(int client, const char[] command, int argc)
{
    if (!is_paused)
    {
        if (!allow_pause)
        {
            PrintToConsole(client, "You must use pause_technical or pause_tactical in order to pause.");
            PrintToChat(client, "You must use !tech or !tac in order to pause.");
            return Plugin_Handled;
        }
    }
    else
    {
        if (!allow_unpause)
        {
            if (PlayerTryUnpause(client) == Plugin_Handled)
                return Plugin_Handled;

            ReplyToCommand(client, "You are not allowed to unpause the game.");
            return Plugin_Handled;
        }
    }

    return Plugin_Continue;
}

Action OnSetPauseCommand(int client, const char[] command, int argc)
{
    if (allow_pause)
        return Plugin_Continue;

    PrintToConsole(client, "You must use pause_technical or pause_tactical in order to pause.");
    PrintToChat(client, "You must use !tech or !tac in order to pause.");
    return Plugin_Handled;
}

Action OnUnpauseCommand(int client, const char[] command, int argc)
{
    if (allow_unpause)
        return Plugin_Continue;

    if (PlayerTryUnpause(client) == Plugin_Handled)
        return Plugin_Handled;

    ReplyToCommand(client, "You are not allowed to unpause the game.");
    return Plugin_Handled;
}

void PauseTimerCallback(Handle timer, int client)
{
    Unpause(client);
}

void PauseTimerInformTickCallback(Handle timer, int secondsRemaining)
{
    PrintToChatAll("The game will resume in %d second%s", secondsRemaining, secondsRemaining != 1 ? "s" : "");
}

void PrintToChatAllPerspective(const char[] first_person, const char[] third_person, int recipientTeam)
{
    for (int i = 1; i <= MaxClients; i++)
    {
        if (!IsClientConnected(i) || IsFakeClient(i))
            continue;

        int chat_team = GetClientTeam(i);
        if (chat_team == recipientTeam)
            PrintToChat(i, first_person);
        else
            PrintToChat(i, third_person);
    }
}

Action CommandPause(int client, const char[] name, ConVar remaining, float length)
{
    if (is_paused)
        return Plugin_Continue;

    int team = GetClientTeam(client);

    if (team != TFTeam_Red && team != TFTeam_Blue)
        return Plugin_Handled;

    if (remaining.IntValue == 0)
    {
        ReplyToCommand(client, "Your team has no remaining %s pauses.", name);
        return Plugin_Handled;
    }

    char team_name[128];
    GetTeamName(team, team_name, sizeof(team_name));

    char first_person_message[256];
    char third_person_message[256];

    Format(first_person_message, sizeof(first_person_message), "Your team took a %s pause.", name);
    Format(third_person_message, sizeof(third_person_message), "%s team took a %s pause.", team_name, name);
    PrintToChatAllPerspective(first_person_message, third_person_message, team);

    // If might be less than one, which indicates unlimited pauses, in which cause we don't need to do any of this logic.
    if (remaining.IntValue >= 1)
    {
        remaining.IntValue -= 1;

        char plural[2];
        if (remaining.IntValue == 1)
            plural = "";
        else
            plural = "s";

        Format(first_person_message, sizeof(first_person_message), "Your team has %d %s pause%s remaining.", remaining.IntValue, name, plural);
        Format(third_person_message, sizeof(third_person_message), "%s team has %d %s pause%s remaining.", team_name, remaining.IntValue, name, plural);
        PrintToChatAllPerspective(first_person_message, third_person_message, team);
    }

    Pause(client);

    if (length > 0)
    {
        PrintToChatAll("This pause will last %d seconds.", RoundToFloor(length));

        CreateTimer(length, PauseTimerCallback, client);
        CreateTimer(length - 10, PauseTimerInformTickCallback, 10);
        for (int i = 1; i <= 3; i++)
            CreateTimer(length - i, PauseTimerInformTickCallback, i);
    }
    else
    {
        allow_player_unpause = true;
    }

    return Plugin_Handled;
}

Action CommandPauseTactical(int client, int args)
{
    if (!sv_pausable.BoolValue)
        return Plugin_Continue;

    int team = GetClientTeam(client);

    return CommandPause(client, "tactical", pause_remaining_tactical.Get(team), pause_tactical_length.FloatValue);
}

Action CommandPauseTechnical(int client, int args)
{
    if (!sv_pausable.BoolValue)
        return Plugin_Continue;

    int team = GetClientTeam(client);

    return CommandPause(client, "technical", pause_remaining_technical.Get(team), pause_technical_length.FloatValue);
}

void PauseTimerRepauseCallback(Handle timer, int client)
{
    PauseWithoutSideEffects(client);
}

Action CommandUnpausePause(int client, int args)
{
    if (!sv_pausable.BoolValue)
        return Plugin_Continue;

    if (client == 0)
        return Plugin_Continue;

    if (!is_paused)
        return Plugin_Handled;

    int team = GetClientTeam(client);

    char team_name[128];
    GetTeamName(team, team_name, sizeof(team_name));

    char first_person_message[256];
    char third_person_message[256];
    Format(first_person_message, sizeof(first_person_message), "Your team re-paused the game.");
    Format(third_person_message, sizeof(third_person_message), "%s team re-paused the game.", team_name);
    PrintToChatAllPerspective(first_person_message, third_person_message, team);

    UnpauseWithoutSideEffects(client);
    CreateTimer(0.1, PauseTimerRepauseCallback, client, TIMER_FLAG_NO_MAPCHANGE);

    return Plugin_Handled;
}

Action CommandSay(int client, const char[] command, int args) {
    if (client == 0)
        return Plugin_Continue;

    if (!is_paused)
        return Plugin_Continue;

    char buffer[256];
    GetCmdArgString(buffer, sizeof(buffer));
    if (buffer[0] != '\0') {
        if (buffer[strlen(buffer)-1] == '"')
            buffer[strlen(buffer)-1] = '\0';
        if (buffer[0] == '"')
            strcopy(buffer, sizeof(buffer), buffer[1]);

        char dead[16] = "";
        if (TF2_GetClientTeam(client) == TFTeam_Spectator)
            dead = "*SPEC* ";
        else if ((TF2_GetClientTeam(client) == TFTeam_Red || TF2_GetClientTeam(client) == TFTeam_Blue) && !IsPlayerAlive(client))
            dead = "*DEAD* ";

        CPrintToChatAllEx2(client, "%s{teamcolor}%N{default} :  %s", dead, client, buffer);
    }

    return Plugin_Handled;
}
