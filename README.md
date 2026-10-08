# NFSMWHelicopterOptions

An ASI mod for **Need for Speed: Most Wanted (2005)**, PC v1.3, that controls how the police helicopter chases, flies, sees, spawns and refuels. Every setting in `General.ini` can have its own value at each Heat level from 1 to 10, with separate values for races.

Every section in `General.ini` is named `[Helicopter:Setting]`. The `In the game` line of each section says which class and value in `speed.exe` it changes, which is also the name of the source file that changes it.

## Chasing

- `LeadBase`, `LeadMax`: how far ahead of your car the helicopter aims.
- `LeadSmoothing`: keeps the helicopter on its line through sharp turns. In the vanilla game the point it aims at jumps the moment your heading changes, so it slides sideways after it and pushes forward again once you straighten out.

## Flight model

- `Turning`: how sharply it turns, and the hardest turn it can make, as one pair of values.
- `MaxChopperAccel`, `MinChopperAccel`: how much acceleration it has.
- Its motion stays the same above 60 FPS as it is at 60 FPS.

## Speed, sight and fuel

- `SpeedCap`: its top speed. The game caps the helicopter at 100 m/s (360 km/h), climbing included; raising the cap lets it keep up with fast cars and stops climbs from slowing it down.
- `LineOfSight`: how far away it can still see your car.
- `FuelTime`: how long its fuel lasts after it spawns.
- `HeliSheet`, `IgnoreHeliSheetDistance`: turns the heli sheet flag, the game's map of the lowest altitude the helicopter may fly at, on or off, or ignores it only while the helicopter is far away from you, so it flies straight back instead of stalling high up and falling behind.

## Leaving

- `FlySpeed`: how fast it flies away when it leaves.

## Spawning

- `SpawnDistance`: how far away from you it spawns.

## Credits

- `Eatincrispies` (Mod creator)
- `Roskler` (Tester and Reporter)
- `Fierelier` - (MinGW support)
