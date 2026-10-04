# Game Station

A collection of **12 arcade games** for the Flipper Zero in a single app, with a scrollable menu and a saved high score (or best time) for every game.

## Controls

- **Up / Down** in the menu: pick a game, **OK**: play
- In a game: **Back** pauses (Back again returns to the menu), **hold Back**: exit the app
- Each game shows its own controls on screen when it starts

## The games

| Game | How it plays |
|------|--------------|
| **Space Shooter** | Auto-firing ship, endless waves, a boss every 5th wave, bombs cost a life if they reach the bottom |
| **Snake** | The classic; grow by eating, don't hit the walls or yourself |
| **Breakout** | Bounce the ball, clear the bricks, keep the ball alive across levels |
| **Flappy** | Tap OK to flap through the gaps |
| **Dino Run** | Jump the cacti and duck the birds; it keeps speeding up |
| **Pong** | First to 5 against the CPU, which gets faster each point you win |
| **Tetris** | Full tetromino set, hold Down to soft-drop, OK to hard-drop |
| **2048** | Slide and merge tiles to reach 2048 |
| **Mines** | 12x8 Minesweeper; OK reveals, hold OK to flag |
| **Simon** | Watch the pad sequence (sound + LED) and repeat it |
| **Whack-a-Mole** | Move the cursor and hit the moles before they hide; 30 seconds |
| **Reaction** | Wait for the flash, then hit OK; scored as your average time over 5 rounds |

High scores (and the best reaction time) are saved to the SD card. Sound, vibration and LED feedback follow your Flipper's notification settings.

## Requirements

No extra hardware needed.

## Building

```
ufbt            # build the .fap
ufbt launch     # install and run on a connected Flipper
```

The game logic also builds and runs on a PC for testing; see `test/`.
