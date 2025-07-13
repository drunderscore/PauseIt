# Pause It
The gameplay-integrity focused pause suite that'll time you out

## Plugin
Fine-grained control on pause variables for a match.

### Usage
| ConVar                   | Description  |
|--------------------------|--------------|
| `pause_tactical_max`     | Maximum number of tactical pauses a team gets each match. 0 means none, -1 means unlimited. |
| `pause_technical_max`    | Maximum number of technical pauses a team gets each match. 0 means none, -1 means unlimited. |
| `pause_tactical_length`  | The length of a tactical pause, in seconds. |
| `pause_technical_length` | The length of a technical pause, in seconds. |

| Command (console or !chat) | Aliases |
|----------------------------|---------|
| `pause_tactical`           | `tactical`, `tac` |
| `pause_technical`          | `technical`, `tech`, `tec` |

_During_ the match, you may use, for example, `pause_remaining_technical_red` to adjust the number of pauses that team has in the current moment.

## Rewind
Low-level fixes for pauses and other gameplay-integrity goodies.

### What it Does
- Prevents pauses from charging the Medi Gun, draining Spy cloak, etc.
- Prevents pauses from performing player simulation, causing movement during the pause.
  - Rocket jumping during a pause will _keep_ you in the air, and once unpaused, you can very easily continue your jump.
- **TODO: Allows users to disconnect without disrupting their player.** (this exists already! but isn't ready for use just yet.)
  - You will be kept in-place, as-is -- with your Med Gun charge, engineer buildings, spy timing, stickies, etc.
  - Simply reconnect! Or, someone else can in your place LMAO
