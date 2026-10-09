# NFSMWHelicopterOptions

An ASI mod for **Need for Speed: Most Wanted (2005)**, PC v1.3, that controls how the police helicopter chases, flies, sees, spawns and refuels. Every setting in `General.ini` can have its own value at each Heat level from 1 to 10, with separate values for races.

## Chasing

- `Leading`: how far ahead of your car the helicopter aims, as one pair of values.

## Attacking

- `CrushAttack`: replaces the skid attack, which mostly clips the side of your car, with one that comes in over your car and drops onto it.

## Flight model

- `Turning`: how sharply it turns, and the hardest turn it can make, as one pair of values.
- `Acceleration`: the most and the least acceleration it has, as one pair of values.
- Its motion stays the same above 60 FPS as it is at 60 FPS.

## Speed, sight and fuel

- `SpeedCap`: its top speed. The game caps the helicopter at 100 m/s (360 km/h), climbing included; raising the cap lets it keep up with fast cars and stops climbs from slowing it down.
- `LineOfSight`: how far away it can still see your car.
- `FuelTime`: how long its fuel lasts after it spawns.
- `HeliSheet`, `IgnoreHeliSheetDistance`: turns the heli sheet, the game's map of the lowest altitude the helicopter may fly at, on or off, or ignores it only while the helicopter is far away from you, so it flies straight back instead of stalling high up and falling behind.

## Leaving

- `FlySpeed`: how fast it flies away when it leaves.

## Spawning

- `SpawnDistance`: how far away from you it spawns.

## Installation

1. Copy `NFSMWHelicopterOptions.asi` and the `HelicopterOptions` folder into the `scripts` folder in your game directory, and launch the game.

The settings are in `scripts\HelicopterOptions\Configuration\General.ini`. The `Presets` folder has a ready-made aggressive one.

## Credits

- `Eatincrispies` (Mod creator)
- `Fierelier` - (MinGW support)
