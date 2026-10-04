# Space Shooter

A fast vertical shoot-'em-up for Flipper Zero. Your ship fires automatically: dodge bullets, blast endless waves of aliens and take down a boss every fifth wave.

## Controls

- **Arrows**: move the ship
- **Back**: pause (Back again returns to the menu)
- **Hold Back**: exit
- **OK**: start / resume / retry

## Gameplay

- Three enemy types: zig-zagging **Drifters**, **Shooters** that fire back, and **Divers** that swoop at you.
- Every enemy that reaches the bottom of the screen costs a life, so don't let anyone slip past.
- **Boss** every 5 waves with a health bar, spread shots and an enraged phase.
- Enemies sometimes drop power-ups:
  - **D**: double shot
  - **S**: shield (absorbs one hit)
  - **+**: extra life (max 5)
- Waves are endless and get harder each time.
- Your high score is saved to the SD card.
- Vibration, LED and sound feedback follow your Flipper's notification settings.

## Requirements

No extra hardware needed.

## Building

```
ufbt
ufbt launch   # install and run on a connected Flipper
```
