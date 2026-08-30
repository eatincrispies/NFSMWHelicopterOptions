# Configuration Guide

All of HelicopterOptions' settings live in **fifteen plain-text `.ini` files**
in `scripts\HelicopterOptions\Configuration\`. You can open and edit them with
any text editor (such as Notepad). **Settings are read once when the game
starts, so restart the game after making changes.**

Every setting has a short explanation, a recommended range, and its default
value written right above it inside the file itself. This guide is a map of
which file does what, plus a few common setups.

## How the settings files work

- Each file covers one topic and is read independently, so a mistake in one
  file never affects the others.
- Values are checked as they load. Anything out of range is nudged back into a
  safe range, and typos in setting names are pointed out - all noted in the log
  at `scripts\HelicopterOptions\Logs\HelicopterOptions.log`.
- If a file is missing, it is recreated with default values. An existing file
  is never overwritten, so your edits are safe.
- Deleting the whole `Configuration` folder is a clean way to reset everything:
  the files come back with default values the next time you start the game.

## What each file controls

| File | What it controls |
|---|---|
| `GeneralSettings.ini` | Logging, startup summary, frame rate, and the game-version check |
| `Radio.ini` | Restored helicopter police-radio speech |
| `Speed.ini` | Keeping the helicopter's chase speed sensible |
| `Predictions.ini` | Anticipating your movement and reacting when you stop |
| `Recovery.ini` | Handling sudden turns, getting stuck, and circling |
| `Leading.ini` | How far ahead of your car it aims |
| `Altitude.ini` | Flying height and stability protection |
| `Skids.ini` | The attack (swoop / ram) behavior |
| `Aggression.ini` | How pushy it is about attacking |
| `SteeringControl.ini` | Turning sharpness and motion smoothing |
| `Acceleration.ini` | How hard it can accelerate and corner |
| `Visibility.ini` | Whether it can lose sight of you |
| `AIHelicopterBehavior.ini` | Safety net and leave/fuel behavior |
| `Navigation.ini` | Optional height/distance limits for stock flight |
| `Debug.ini` | Extra logging and experimental options |

## Common setups

| Goal | Edit |
|---|---|
| Helicopter follows but never attacks | `Skids.ini` → `[SkidAttack] FollowOnly=1` |
| It never runs out of fuel and leaves | `AIHelicopterBehavior.ini` → `[ExitBehavior] DisableFuelBasedExit=1` |
| It never loses sight of you (harder) | `Visibility.ini` → `Enable=1` and `SeeThroughWalls=1` |
| It attacks more often | `Aggression.ini` → `Enable=1`, raise `AttackFrequency` and `AttackWillingness` |
| Stop constant ramming | `Skids.ini` → `[SkidAttack] ReattackDelaySeconds=8` |
| Keep up better on highways | `Speed.ini` → raise `MinChaseSpeed` |
| Keep it from flying too high in stock flight | `Navigation.ini` → `EnableSafeOverride=1` and set `MaxHeightAboveTarget` |
| Turn off the helicopter radio speech | `Radio.ini` → `[HeliRadioChat] Enable=0` |
| Playing above 60 FPS | Handled automatically; see `GeneralSettings.ini` → `[FrameRate]` |
| Diagnose a problem | `Debug.ini` → `[Telemetry] Enable=1`, then send the log |

## A note on turning several things on

Some options rely on others and will switch those on for you automatically when
needed - for example, turning on movement prediction also enables the aiming
system it steers. You do not have to manage this by hand; the log lists what was
enabled at startup.

The full explanation of every individual setting is inside each `.ini` file.
