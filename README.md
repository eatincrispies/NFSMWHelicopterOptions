# NFSMWHelicopterOptions

An ASI mod for **Need for Speed: Most Wanted (2005)**, PC v1.3, that controls how the police helicopter chases, flies, sees, spawns and refuels. The settings are split over two files in `scripts\HelicopterOptions\Configuration`: `General.ini` holds the everyday ones (spawn distance, top speed, sight, fuel, attack length, search speed and leaving speed), and `Advanced.ini` holds the finer AI and flight-model tuning (leading, closing speed, search radius, turning, acceleration, braking and the heli sheet). Every setting can have its own value at each Heat level from 1 to 10, with separate values for races.

Every section is named `[Helicopter:Setting]`. The `In the game` line of each section names the function or value in `speed.exe` it changes. The names come from the [Need for Speed: Most Wanted decompilation](https://github.com/dbalatoni13/nfsmw), and the source in `HelicopterOptions/src/Helicopter` uses the same class, member and function names, with one file per game class.

## Chasing

- `Leading`: how far ahead of your car the helicopter aims, as one pair of values.
- `ClosingSpeed`: how fast it closes in on the point it chases when it is near, mid-range and far away.

## Attacking

- `AttackDuration`: how long one skid-hit attack lasts.

## Searching

During cooldown the helicopter searches around where it last saw you. It favours the roads ahead of the way you were driving and widens out from there. It skips the side where ground units are already searching. Every few points, and at nearby hiding spots, tunnels and overpasses, it stops to hover and look around. It starts low and slow near where it lost you, then flies higher and faster as the search widens.

- `SearchSpeed`: how fast it flies while it searches for you during cooldown, from the first circle to the widest, as one pair of values.
- `SearchRadius`: how far out it circles while it searches, from the first circle to the largest, as one pair of values.

## Flight model

- `Turning`: how sharply it turns, and the hardest turn it can make, as one pair of values.
- `Acceleration`: the most and the least acceleration it has, as one pair of values.
- `Brake`: how far out it starts braking and how much it slows down when it does, as one pair of values.
- Its motion stays the same above 60 FPS as it is at 60 FPS.

## Speed, sight and fuel

- `SpeedCap`: its top speed. The game caps the helicopter at 100 m/s (360 km/h), climbing included; raising the cap lets it keep up with fast cars and stops climbs from slowing it down.
- `LineOfSight`: how far away it can still see your car.
- `HidingSpots`: whether the map's hiding spots can hide you from it.
- `AllAroundSight`, `FrontSightOnly`: how close you must be for it to see you whichever way it faces, and whether further out it only sees you in front of it. Buildings and tunnels still block its view.
- `FuelTime`: how long its fuel lasts after it spawns.
- `HeliSheet`, `IgnoreHeliSheetDistance`: turns the heli sheet, the game's map of the lowest altitude the helicopter may fly at, on or off, or ignores it only while the helicopter is far away from you, so it flies straight back instead of stalling high up and falling behind.

## Taken out

- When the helicopter is destroyed, the pilot says his "going down" line. This is skipped when NFSMWRevisedAirSupport is installed, which does it itself.
- When a pursuit breaker takes the helicopter out, it bursts in the gas station explosion (`fxex_gasstation`) and then burns with fire and black smoke (`fxcar_cop_death1`) until the wreck is removed, the same way cop cars play their death effect.
- For the big flames, install `Modscripts\HelicopterFire.nfsms` with NFS-VltEd (File > Import > Modscript). It adds the `michael_bay` emitter group, based on the NFSMW Cop Explosion Mod (v3.0), without changing the cops. Without it the wreck burns with the game's smaller `fxfire_sm1` fire and `fxex_large1` flare-ups. `HelicopterFire_uninstall.nfsms` removes it. If you already have the Cop Explosion Mod installed, `michael_bay` is already there and you don't need this modscript.

## Leaving

- `FlySpeed`: how fast it flies away when it leaves.
- When its fuel runs out it turns away from you, climbs to 30 m above you and flies off in a straight line. The game's own exit flies to fixed points it can overshoot, which can leave it circling until it despawns.

## Spawning

- `SpawnDistance`: how far away from you it spawns.
- `Respawning`: the shortest wait after a helicopter is taken out or leaves before the next one can join.
- `DespawnDistance`: how far away it must be before the game removes it from the pursuit and the minimap.

## Installation

1. Copy `NFSMWHelicopterOptions.asi` and the `HelicopterOptions` folder into the `scripts` folder in your game directory, and launch the game.

The settings are in `scripts\HelicopterOptions\Configuration\General.ini` and `Advanced.ini` next to it.

## Credits

- `Eatincrispies` (Mod creator)
- `Fierelier` - (MinGW support)
