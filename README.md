(Thumbnail.webp)

# NFSMW HelicopterOptions

An ASI mod for **Need for Speed: Most Wanted (2005)**, PC v1.3, that controls how the police helicopter chases, attacks, flies, spawns and refuels. Every setting in `General.ini` can have its own value at each Heat level from 1 to 10, with separate values for races, etc.

Each section below is named after the game class it changes, which is also the name of the source file that changes it.

## Chasing - `AIActionHeliPursuit`

- How far ahead of your car the helicopter aims, based on your speed.
- How high above you it flies while chasing, while lining up an attack, and when far away.

## Skid attacks - `AIActionHeliPursuit`

- When it can start a ramming attack: the distance window, how lined up it must be, how far above you it may be, and the cooldown between attempts.
- An optional extra pause after every attack.

## Flight model - `SimpleChopper`

- How sharply it turns, and the hardest turn it can make.
- How much acceleration it has.
- Its top speed. The game caps the helicopter at 100 m/s (360 km/h); the mod raises or lowers that cap and scales every speed the helicopter asks for with it, so it actually uses the new limit.
- Its motion stays the same above 60 FPS as it is at 60 FPS.

## Altitude - `HeliSheet`

- Turns the heli sheet, the game's map of the lowest altitude the helicopter may fly at, on or off.
- Ignores the heli sheet only while the helicopter is far away from you, so it flies straight back instead of stalling high up and falling behind.

## Fuel, speed limits and leaving - `AIVehicleHelicopter`, `AIActionHeliExit`

- How long its fuel lasts after it spawns.
- The fastest it may climb or descend, which stops the game from launching it upward and making it bounce.
- How fast it flies away when it leaves.

## Spawning - `AICopManager`

- How far away from you it spawns.

## Credits

- `Eatincrispies` (Mod creator)
- `Fierelier` - (MinGW support)
- `Roskler` - (Testing the mod and reporting issues before release)
