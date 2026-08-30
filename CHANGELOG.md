# Changelog

## V2.2.1

- **The radio now uses the helicopter's real line of sight.** The pilot's
  "lost visual" and "spotted" calls previously guessed from distance. They now
  follow the helicopter's own sight state - the same one that sends it into
  search mode - so the calls line up with what the helicopter is actually
  doing. The distance check remains as an automatic fallback.
- The detailed status log now shows what the helicopter is doing
  (chase / search / attack).
- Documentation corrections around what the game does and does not expose.

## V2.2

**Works properly above 60 FPS.** Need for Speed: Most Wanted was built to run
at 60 frames per second and its helicopter was tuned for that. If you had
removed the 60 FPS cap - capping at your monitor's refresh rate instead, or
uncapping entirely - the helicopter's flying got worse the faster the game ran:
it stopped swaying and sweeping as it followed you, and above 200 FPS it
stiffened up completely and just hung in the air pointing at you.

The helicopter now flies the way it does at 60 FPS at any frame rate - 75, 120,
144, 165, 240 or higher - while still moving as smoothly as your frame rate
allows. Three things were behind it:

- **The game stops working out how the helicopter is moving above 200 FPS.**
  It measures the helicopter's speed by comparing where it is now with where it
  was last frame, and only does that when at least 5 thousandths of a second
  have passed. That is exactly 200 FPS. Run faster and it never measures at
  all, so the helicopter's sideways and forward movement fades to nothing. That
  internal limit is now lowered.
- **The mod's own timing was too coarse.** It used a clock that only advances
  about every 16 ms, so above roughly 64 FPS most updates measured no time
  passing and were skipped as faults - which also kept the movement estimate
  frozen, disabling prediction, stopped-player handling and sudden-turn
  recovery. Timing now uses a high-resolution clock.
- **Per-frame amounts ran too fast.** Anything moving "a fraction of the way
  each frame" closed that gap several times faster at several times the frame
  rate, and the game's motion smoothing lost the lag that produces the swaying
  flight. These now follow the real frame time, held steady so that normal
  frame-time jitter does not make the movement alternate between swaying and
  stiff.

New in `GeneralSettings.ini` under `[FrameRate]`: `Enable` (on by default),
`HelicopterUpdateRate` (60 by default - the rate the helicopter flies as if it
were running at), and `LogFrameRate`. Nothing changes if you play capped at
60 FPS.

## V2.0 (release candidate: V2.0-rc2)

Second release candidate. New since V2.0-rc1:

- **Restored helicopter radio speech** (`Radio.ini`). The game ships a full
  set of helicopter radio lines that are almost never - or never - heard in
  normal play. The helicopter now talks on the police radio at the right
  moments: the dispatcher announces it joining the pursuit, and the pilot
  reports moving into position, spotting you, losing sight of you, your
  position when you stop and when you take off again, a bullhorn arrest call
  when you stay stopped under it, low fuel, and leaving. Optional extras
  (off by default) announce attack runs, obstructions, and full-aggression
  swarming. Everything plays through the game's own radio system - normal
  radio effect, correct channel, queued behind other chatter - and built-in
  cooldowns keep lines occasional.
- **`IgnoreHeliSheet` restored** (`Navigation.ini`): ignore the game's
  terrain-height guidance at all times, letting the helicopter fly and
  descend anywhere. Blunter than the safe override; see the notes in the
  file before enabling.

## V2.0-rc1

First release candidate. Version 2.0 is a complete rebuild of HelicopterOptions. The police helicopter
now flies and chases much more competently, almost everything is configurable,
and the whole mod is safer and better behaved. Compared with the original
release (V1.0.1):

### Added

- **Chase-speed control** that keeps the helicopter from crawling on straights,
  with an optional minimum and maximum chase speed.
- **Movement prediction** so the helicopter aims where you are going, easing off
  when you brake or turn hard.
- **Stopped-player handling** that holds a close position when you stop instead
  of drifting off into the distance.
- **Sudden-turn recovery** that re-centers the helicopter when you make a hard
  U-turn or handbrake turn.
- **Stability protection** that gently levels the helicopter if it ever rolls,
  pitches, or spins too far, so it recovers instead of tumbling.
- **Configurable attacks**: control attack frequency, angle, timing, and spacing;
  a follow-only mode that never attacks; and an option to stop repeated ramming.
- **Optional aggression** that can build up over a long pursuit.
- **A safety net** that hands control back to the game if anything ever goes
  wrong, so the helicopter always keeps working.
- **Extras**: never lose sight of you, never run out of fuel, adjustable flying
  height, turning, and acceleration.

### Improved

- The helicopter keeps up and corners with you far more naturally than before.
- Motion is smoother and more stable, with built-in protection against the old
  uncontrollable-spinning behavior.
- Logging is clearer and written in plain language, and it never floods the file.

### Fixed

- Resolved the long-standing spinning/instability problems from the original
  release.
- Corrected many internal values that did not match the game's real behavior.
- The mod now reliably recognizes the supported game version and starts up
  cleanly.

### Configuration

- Settings are now split into **fifteen topic-based `.ini` files** under
  `scripts\HelicopterOptions\Configuration\`, each with plain-English
  explanations, recommended ranges, and default values.
- Missing settings files are recreated automatically; existing files are never
  overwritten.
- Sensible defaults improve the helicopter noticeably out of the box, while
  experimental and difficulty-increasing options stay switched off.

### Compatibility

- Only runs on the supported English PC v1.3 game version; other versions are
  left completely untouched.
- Works alongside other `.asi` mods: if another mod already changed the same
  part of the game, the affected feature is skipped and everything else keeps
  working.
- Changes are applied only in memory and fully undone when the game closes.

### Removed

- The old single settings file (replaced by the topic files above).
- The preset system.

## V1.0.1

Original release. Preserved untouched in `V1.0.1_ORIGINAL_BACKUP/`.
