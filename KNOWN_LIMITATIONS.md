# Known Limitations

HelicopterOptions does a lot, but a few frequently requested things are not
included. In every case that is because the game itself does not make them
possible to do reliably, and adding them anyway would cause glitches or
crashes. This page explains what is missing and why, in plain terms.

Importantly: every setting that appears in the configuration files actually
does something. Nothing here is a hidden or fake switch.

## Helicopter can sometimes fly high in stock flight

In some situations the base game commands the helicopter to fly quite high
above you, which can make it feel distant. `Navigation.ini` includes an
optional height limit (`EnableSafeOverride` with `MaxHeightAboveTarget`) that
brings it back down in many of these cases while still keeping it from diving
into buildings. It improves the common cases but cannot completely override the
game's own flight height in every situation, so you may still occasionally see
the helicopter climb higher than you would like.

## Multiple helicopters at once

The game is built around a single police helicopter and only keeps track of one
at a time. Forcing extra helicopters to appear leaves the game confused about
them, which causes glitches when they disappear. There are experimental options
in `Debug.ini` for people who want to tinker, but reliable multi-helicopter
support is not something this mod can safely provide.

## Helicopters colliding with each other

Even if two helicopters are present, the game has no physical collision between
them, so they pass through one another. This mod does not change that.

## Individual helicopter minimap icons

The game does not expose a reliable way to give extra helicopters their own
minimap icons, so this is not included.

## Avoiding buildings and terrain

The helicopter cannot actively steer around specific buildings or obstacles:
the game gives it no way to query them, so it relies on the game's own
built-in minimum-height handling (which the height limit above tunes) to stay
out of the ground. The game does contain a basic road-following ability the
helicopter uses when flying away; making chase flight use it is being looked
at for a future version.

## Tunnels

The game does not mark where tunnels are in a way the mod can read, so there is
no special tunnel behavior (such as holding back or waiting at the exit).

## Damage-aware behavior

Behavior that reacts to the helicopter taking damage is not included yet. The
helicopter's damage state does exist inside the game and reading it safely is
being looked at for a future version.

## Different attack styles (from the left, right, rear, fake passes)

Which side the helicopter attacks from is decided by the game, and there is no
safe way to force a specific approach without risking instability, so directed
attack variants are not included.

## On-screen debug overlay

Diagnostic information is written to the log file rather than drawn on screen.
An on-screen overlay would risk conflicting with other overlay mods, so the log
is used instead.

## Stability recovery

The built-in stability protection helps the helicopter recover from rolling,
pitching, or spinning by easing its speed, holding a little extra height, and
steadying its motion. It does this without forcibly snapping the helicopter
upright, because doing that safely is not possible in this game. In the vast
majority of cases the gentler recovery is enough.

## Helicopter radio speech

The restored radio speech plays every helicopter line that the game's speech
system can actually reach: the events exist in the game's speech data and the
mod triggers them through the game's own radio scheduler. However, the speech
archives also contain groups of recorded lines that the game's own event data
never points to at all. Those recordings cannot be played through the radio
system - there is no event connected to them - so the mod does not use them.
Playing them any other way would bypass the radio effect, channels, and
queueing, and is out of scope.

Also note that what each line says is fixed by the game's recordings; the mod
chooses *when* a line plays, not its wording. Lines are matched to moments by
their official event names - for example the "lost visual" call plays when the
helicopter itself loses sight of you and starts searching, using the same
line-of-sight state that drives its flying.

## Very high frame rates

The helicopter is designed around a 60 FPS game and the mod keeps it behaving
that way at any frame rate (see `[FrameRate]` in `GeneralSettings.ini`). The
parts of the helicopter that the mod controls are handled; a few things buried
deeper in the game still run once per frame rather than on a clock, so at
extreme frame rates you may notice small differences from how it flies at
60 FPS. If something looks wrong, set `LogFrameRate=1` and include the log.

## Minor cosmetic notes

- With "never run out of fuel" enabled, the internal fuel value keeps counting
  down past empty. This is harmless and only visible in detailed logging.
- Adjusting how often the helicopter attacks can also slightly shift the height
  it chooses during a chase, because the game ties those two together.
