# Shadowblade

A side-scrolling ninja action-RPG proof of concept, built in Rust on
[raylib](https://www.raylib.com/). Original code and original art: every visual
is drawn procedurally from primitives at runtime, and the project ships no image
or audio assets at all.

## Build and run

Needs a Rust toolchain and the X11/GL development headers that raylib's vendored
C source links against:

```sh
sudo apt-get install -y libxrandr-dev libxinerama-dev libxcursor-dev \
                        libxi-dev libgl1-mesa-dev libxkbcommon-dev
cargo run --release
```

`raylib-sys` compiles the bundled raylib C source with CMake on first build, so
expect the initial `cargo build` to take a couple of minutes.

## Controls

| Key | Action |
| --- | --- |
| `A` / `D` | Move |
| `W` or `Space` | Jump (hold for height) |
| `S` + jump | Drop through a one-way platform |
| `J` | Attack — chains up to three hits |
| `K` | Block; tapping it just before a hit parries |
| `L` or `Shift` | Roll (invulnerable through the middle) |
| `Tab` | Skill menu |
| `R` | Restart |
| `Esc` | Quit |

## Screenshot capture

```sh
cargo run --release -- --capture ./shots
```

Runs a scripted session on a fixed clock and writes PNGs at preset frames, so
rendering can be checked without a human at the keyboard. It works headlessly
under `xvfb-run -a -s "-screen 0 1280x720x24"`, which is how the art in this
repo was verified.

## Calibrating the look

`src/config.rs` is the tuning surface. Sprite dimensions, physics constants,
progression curves and the full palette live there and nowhere else, so the game
can be re-proportioned against reference footage without touching game logic.
The values in it now are hand-picked starting points for a 16:9 brawler, not
measurements taken from anything.

Combat feel is tuned separately in `src/combat.rs`, where each attack is frame
data — startup, active, recovery, reach, damage, knockback, cancel window.

## Architecture

| Module | Responsibility |
| --- | --- |
| `main.rs` | Window, fixed-step loop, hit resolution, mode/menu handling |
| `config.rs` | All tunable constants and the palette |
| `combat.rs` | Attack frame data, hitbox generation, facing |
| `player.rs` | Player state machine, input, combos, progression |
| `enemy.rs` | Enemy state machine and AI |
| `physics.rs` | Swept AABB movement, one-way platforms |
| `world.rs` | Stage geometry, wave scheduling, follow camera |
| `render.rs` | Procedural art: parallax, terrain, articulated figures |
| `ui.rs` | HUD, skill menu, overlays |

### How combat works

An attack is three phases: **startup** (windup, no hitbox), **active** (hitbox
live) and **recovery** (committed and vulnerable). Chaining to the next combo
step is only allowed inside a cancel window that closes before recovery ends, so
a whiffed combo always leaves a punish window.

The same frame data drives both the hitbox and the drawn pose, which means a
swing's animation cannot drift out of sync with what it actually hits.

Supporting feel:

- **Hitstop** on every connect, scaled by the attack's weight.
- **Trauma-based screen shake** that saturates, so a flurry stays readable.
- **Coyote time** and **jump buffering** so platforming inputs are forgiving.
- **Stamina** gates attacking, rolling and blocking; a broken guard takes the
  hit in full.
- **Parry** on a block that begins within a narrow window, which staggers the
  attacker for a free punish.
- **Telegraphs** — enemies flare during windup, so every swing is reactable.

## Status

Proof of concept. One stage, three waves, two enemy types, four passive skills.
No audio, no save system, no stage transitions.
