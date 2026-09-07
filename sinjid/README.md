# Sinjid: Shadow of the Warrior — a raylib/C remake

An unofficial fan remake of the 2004 browser RPG *Sinjid: Shadow of the Warrior*
(Infrarift), written from scratch in C99 with [raylib](https://www.raylib.com/).

**There are no asset files in this project.** Every character, weapon, shield,
tile, background, particle and UI element is drawn procedurally at runtime by
`src/art.c`. Nothing was extracted from the original game; the item names,
skills, enemies, maps and writing are all original to this remake.

![the village hub](docs/village.png)

![turn-based combat](docs/battle.png)

## Building

Requires a C99 compiler and raylib 4.x/5.x.

```sh
make                      # uses pkg-config to find raylib
make RAYLIB=/path/to/raylib   # or point at a raylib source tree
./sinjid
```

On Debian/Ubuntu, raylib's own build needs the usual GL/X11 headers:

```sh
sudo apt install libgl1-mesa-dev libx11-dev libxrandr-dev \
                 libxinerama-dev libxcursor-dev libxi-dev
```

## Controls

| key | action |
| --- | --- |
| arrows / WASD | walk, move the cursor |
| ENTER / SPACE | talk, confirm |
| ESC | back, open the menu |
| I | character, pack and skills |
| T | training (spend stat and skill points) |
| TAB / Q / E | switch tabs |

Talk to the shrine in the village to rest and save; the save file is written to
`sinjid_save.dat` in the working directory.

## What was taken from the original

The original is a Flash file, so its design is legible from the compiled
ActionScript symbol table. The systems this remake reproduces:

* **Speed-ordered turn combat**, up to two fighters a side, with initiative
  rerolled every round, so a fast character sometimes acts twice in a row.
* **A separate shield layer.** Every fighter has shield points with their own
  physical and magic defence. Blows are spent breaking shields down before they
  reach life, and only the overflow bleeds through (at half strength). Weapons
  carry a *shield damage* stat, and skills can triple damage against shields
  (`Shield Breaker`) or bypass them entirely (`Gut Thrust`, `Frost Nail`).
* **Split physical/magic damage** with matching defences, plus pure damage that
  respects neither.
* **Two resources**: mana for ki disciplines, energy for combat skills. Energy
  regenerates every round, so the physical tree is sustainable and the magic
  tree is burst.
* **Nine animation states** driving segmented puppets — stand, walk, attack,
  cast, block, block-break, hit, heal and die.
* **A tile overworld** of single-screen zones joined by road gates, with random
  encounters rolled per step against a per-zone enemy pool.
* **A village hub**: blacksmith, general goods, healer, trainer, save shrine and
  a wave-based arena.

## Layout

| file | contents |
| --- | --- |
| `src/game.h` | all types, the palette, and the module interfaces |
| `src/art.c` | the whole renderer: puppets, poses, weapons, tiles, backgrounds |
| `src/battle.c` | turn order, damage resolution, enemy AI, battle HUD |
| `src/world.c` | zone maps, walking, NPCs, encounters, travel |
| `src/ui.c` | panels, bars, and the menu/shop/training/title screens |
| `src/data.c` | items, skills, enemies, classes |
| `src/main.c` | window, game loop, scene dispatch, save files, scripted input |

### How the characters are drawn

`art.c` has no sprites. A fighter is a skeleton — hip, chest, neck, head, two
arms with elbows and hands, two legs with knees and feet — and an animation is a
set of keyframed joint angles blended together (`pose_attack` winds up behind the
head, then drives through with the hips). Limbs are drawn as tapered capsules
with an ink outline pass underneath, which is what gives the flat 2004 vector
look. Equipment is a shape family (`WEAP_KATANA`, `SHLD_KITE`, `HELM_HORNED`…)
drawn at the hand or head with the item's own tint, so a new weapon changes the
silhouette without any new art.

## Scripted input

For screenshots and CI the game can play itself:

```sh
./sinjid --script "ret,wait40,ret,ret,wait90,shot:village,down,down,quit"
```

Tokens are key names (`up`, `down`, `left`, `right`, `ret`, `esc`, `tab`, single
letters), `waitN` frames, `shot:name` to write `name.png`, and `quit`. It runs
fine under Xvfb with software GL.

## Licence and attribution

This is a fan project, not affiliated with or endorsed by the creators of the
original game. The code and all generated art here are original work.
