//! Shadowblade - a side-scrolling ninja action-RPG proof of concept.
//!
//! Built on raylib with no external art: every visual is drawn procedurally at
//! runtime (see `render.rs`). Tuning constants live in `config.rs`.

mod combat;
mod config;
mod enemy;
mod physics;
mod player;
mod render;
mod ui;
mod util;
mod world;

use combat::overlaps;
use config as cfg;
use enemy::{stats_for, EState, Enemy};
use player::{HitOutcome, InputState, PState, Player};
use raylib::prelude::*;
use render::{FloatText, Spark};
use world::{stage_one, GameCamera, Stage};

#[derive(PartialEq, Eq, Clone, Copy)]
enum Mode {
    Playing,
    Skills,
    Dead,
    Victory,
}

/// Small deterministic RNG. Effects do not need statistical quality, and a
/// self-contained generator keeps the crate dependency-free beyond raylib.
struct Rng(u32);

impl Rng {
    fn next_u32(&mut self) -> u32 {
        // xorshift32
        let mut x = self.0;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        self.0 = x;
        x
    }
    fn range(&mut self, lo: f32, hi: f32) -> f32 {
        lo + (self.next_u32() % 10_000) as f32 / 10_000.0 * (hi - lo)
    }
}

struct Game {
    stage: Stage,
    player: Player,
    enemies: Vec<Enemy>,
    cam: GameCamera,
    sparks: Vec<Spark>,
    floats: Vec<FloatText>,
    rng: Rng,

    mode: Mode,
    next_id: u32,
    wave: usize,
    wave_delay: f32,
    toast: String,
    toast_life: f32,
    skill_cursor: usize,
    time: f32,
}

impl Game {
    fn new() -> Game {
        let stage = stage_one();
        let player = Player::new(stage.player_spawn);
        let cam = GameCamera::new(player.center());
        let mut g = Game {
            stage,
            player,
            enemies: Vec::new(),
            cam,
            sparks: Vec::new(),
            floats: Vec::new(),
            rng: Rng(0x1234_5678),
            mode: Mode::Playing,
            next_id: 1,
            wave: 0,
            wave_delay: 1.0,
            toast: String::new(),
            toast_life: 0.0,
            skill_cursor: 0,
            time: 0.0,
        };
        g.start_wave(0);
        g
    }

    fn start_wave(&mut self, index: usize) {
        if index >= self.stage.waves.len() {
            return;
        }
        for s in &self.stage.waves[index].spawns {
            self.enemies.push(Enemy::new(self.next_id, s.kind, s.x, s.y));
            self.next_id += 1;
        }
        self.wave = index;
        self.toast = format!("WAVE {}", index + 1);
        self.toast_life = 1.6;
    }

    fn living_enemies(&self) -> usize {
        self.enemies.iter().filter(|e| e.alive()).count()
    }

    fn burst(&mut self, at: Vector2, count: i32, color: Color, speed: f32) {
        for _ in 0..count {
            let a = self.rng.range(0.0, std::f32::consts::TAU);
            let sp = self.rng.range(speed * 0.4, speed);
            self.sparks.push(Spark {
                pos: at,
                vel: Vector2::new(a.cos() * sp, a.sin() * sp - 40.0),
                life: self.rng.range(0.25, 0.5),
                max_life: 0.5,
                color,
                size: self.rng.range(3.0, 7.0),
            });
        }
    }

    fn float(&mut self, at: Vector2, text: String, color: Color) {
        self.floats.push(FloatText { pos: at, life: 0.9, text, color });
    }

    fn update(&mut self, input: InputState, dt: f32) {
        self.time += dt;
        self.toast_life = (self.toast_life - dt).max(0.0);

        self.player.update(input, &self.stage.platforms, dt);

        let pc = self.player.center();
        let alive = self.player.state != PState::Dead;
        for e in &mut self.enemies {
            e.update(pc, alive, &self.stage.platforms, dt);
        }

        self.resolve_player_hits();
        self.resolve_enemy_hits();

        self.enemies.retain(|e| !e.expired());

        // Wave pacing: a short breather between waves.
        if self.mode == Mode::Playing && self.living_enemies() == 0 {
            self.wave_delay -= dt;
            if self.wave_delay <= 0.0 {
                if self.wave + 1 < self.stage.waves.len() {
                    self.start_wave(self.wave + 1);
                    self.wave_delay = 1.6;
                } else {
                    self.mode = Mode::Victory;
                }
            }
        }

        if self.player.state == PState::Dead && self.player.state_time > 1.2 {
            self.mode = Mode::Dead;
        }

        self.update_effects(dt);

        let focus = self.player.center();
        self.cam.update(focus, self.stage.width, dt);
    }

    /// Player swing against every enemy it has not already struck this swing.
    fn resolve_player_hits(&mut self) {
        let Some(hb) = self.player.attack_hitbox() else { return };
        let Some(atk) = self.player.current_attack().copied() else { return };
        let damage = self.player.attack_damage(&atk);
        let is_finisher = self.player.state == PState::Attack && self.player.combo_index == 2;

        let mut events: Vec<(Vector2, Option<i32>)> = Vec::new();

        for e in &mut self.enemies {
            if !e.alive() || self.player.hit_ids.contains(&e.id) {
                continue;
            }
            if !overlaps(hb, e.rect) {
                continue;
            }
            self.player.hit_ids.push(e.id);
            let xp = e.take_hit(damage, self.player.center().x, atk.knockback, atk.launch);
            e.hitstop = atk.hitstop;
            events.push((e.center(), xp));
        }

        if events.is_empty() {
            return;
        }

        self.player.hitstop = atk.hitstop;
        self.cam.add_shake(atk.shake);

        let mut heal = 0.0;
        for (pos, xp) in events {
            self.burst(pos, 10, cfg::C_ACCENT, 300.0);
            self.float(
                Vector2::new(pos.x, pos.y - 40.0),
                format!("{}", damage.round() as i32),
                cfg::C_ACCENT,
            );
            if is_finisher {
                heal += self.player.skills.siphon_heal();
            }
            if let Some(xp) = xp {
                self.burst(pos, 16, cfg::C_XP, 380.0);
                if self.player.add_xp(xp) {
                    let c = self.player.center();
                    self.float(Vector2::new(c.x, c.y - 70.0), "LEVEL UP".into(), cfg::C_ACCENT);
                    self.burst(c, 26, cfg::C_ACCENT, 420.0);
                    self.cam.add_shake(3.0);
                }
            }
        }

        if heal > 0.0 {
            self.player.hp = (self.player.hp + heal).min(self.player.max_hp);
            let c = self.player.center();
            self.float(Vector2::new(c.x, c.y - 55.0), format!("+{}", heal.round() as i32), cfg::C_HEAL);
        }
    }

    /// Enemy swings against the player.
    fn resolve_enemy_hits(&mut self) {
        let mut outcomes: Vec<(HitOutcome, Vector2)> = Vec::new();

        for i in 0..self.enemies.len() {
            if !self.enemies[i].alive() || self.enemies[i].hit_player {
                continue;
            }
            let Some(hb) = self.enemies[i].attack_hitbox() else { continue };
            if !overlaps(hb, self.player.rect) {
                continue;
            }
            self.enemies[i].hit_player = true;

            let atk = stats_for(self.enemies[i].kind).attack;
            let from_x = self.enemies[i].center().x;
            let outcome = self.player.take_hit(atk.damage, from_x);
            let contact = Vector2::new(
                (hb.x + hb.width * 0.5 + self.player.center().x) * 0.5,
                self.player.center().y,
            );

            if outcome == HitOutcome::Parried {
                // A parry staggers the attacker: the reward is a free punish.
                self.enemies[i].take_hit(0.0, self.player.center().x, 240.0, 0.0);
                self.enemies[i].state = EState::Stagger;
                self.enemies[i].state_time = -0.25;
            }
            outcomes.push((outcome, contact));
        }

        for (outcome, at) in outcomes {
            match outcome {
                HitOutcome::Parried => {
                    self.burst(at, 22, cfg::C_BLADE, 420.0);
                    self.float(Vector2::new(at.x, at.y - 60.0), "PARRY".into(), cfg::C_BLADE);
                    self.cam.add_shake(3.5);
                    self.player.hitstop = 0.10;
                }
                HitOutcome::Blocked => {
                    self.burst(at, 10, cfg::C_STAMINA, 260.0);
                    self.float(Vector2::new(at.x, at.y - 55.0), "BLOCK".into(), cfg::C_STAMINA);
                    self.cam.add_shake(1.6);
                }
                HitOutcome::Hit | HitOutcome::Killed => {
                    self.burst(at, 14, cfg::C_DAMAGE, 330.0);
                    self.cam.add_shake(4.0);
                    self.player.hitstop = 0.06;
                }
                HitOutcome::Ignored => {}
            }
        }

        // Consume the parry flag set inside the player.
        self.player.parried = false;
    }

    fn update_effects(&mut self, dt: f32) {
        for s in &mut self.sparks {
            s.life -= dt;
            s.vel.y += 900.0 * dt;
            s.pos.x += s.vel.x * dt;
            s.pos.y += s.vel.y * dt;
        }
        self.sparks.retain(|s| s.life > 0.0);

        for t in &mut self.floats {
            t.life -= dt;
            t.pos.y -= 42.0 * dt;
        }
        self.floats.retain(|t| t.life > 0.0);
    }
}


/// Scripted input for `--capture`, so screenshots show the game in motion
/// rather than a character standing still. Frame numbers are at 60fps.
fn scripted_input(frame: u32) -> InputState {
    let mut i = InputState::default();
    match frame {
        // Run right across the floor.
        0..=70 => i.move_x = 1.0,
        // Swing the three-hit chain: one press per attack, spaced to land
        // inside each cancel window.
        71..=73 => i.attack_pressed = frame == 71,
        74..=86 => i.attack_pressed = frame == 80,
        87..=110 => i.attack_pressed = frame == 92,
        // Approach, then hold block.
        111..=150 => i.move_x = 1.0,
        151..=185 => i.block_held = true,
        // Roll through.
        186..=188 => {
            i.move_x = 1.0;
            i.roll_pressed = frame == 186;
        }
        189..=220 => i.move_x = 1.0,
        // Jump and hit an air attack.
        221..=240 => {
            i.move_x = 1.0;
            i.jump_pressed = frame == 221;
            i.jump_held = true;
            i.attack_pressed = frame == 234;
        }
        _ => i.move_x = 1.0,
    }
    i
}

fn sample_input(rl: &RaylibHandle) -> InputState {
    use KeyboardKey::*;
    let left = rl.is_key_down(KEY_A) || rl.is_key_down(KEY_LEFT);
    let right = rl.is_key_down(KEY_D) || rl.is_key_down(KEY_RIGHT);
    InputState {
        move_x: (right as i32 - left as i32) as f32,
        jump_pressed: rl.is_key_pressed(KEY_SPACE) || rl.is_key_pressed(KEY_W) || rl.is_key_pressed(KEY_UP),
        jump_held: rl.is_key_down(KEY_SPACE) || rl.is_key_down(KEY_W) || rl.is_key_down(KEY_UP),
        attack_pressed: rl.is_key_pressed(KEY_J),
        block_held: rl.is_key_down(KEY_K),
        roll_pressed: rl.is_key_pressed(KEY_L) || rl.is_key_pressed(KEY_LEFT_SHIFT),
        down_held: rl.is_key_down(KEY_S) || rl.is_key_down(KEY_DOWN),
    }
}

fn main() {
    // `--capture <dir>` runs a scripted session and writes PNGs at fixed
    // frames, which makes the rendering verifiable without a human at the
    // keyboard. raylib caches the working directory inside InitWindow and
    // resolves screenshot paths against it, so the chdir must happen first.
    let args: Vec<String> = std::env::args().collect();
    let capture_dir = args
        .iter()
        .position(|a| a == "--capture")
        .and_then(|i| args.get(i + 1))
        .cloned();
    if let Some(dir) = &capture_dir {
        std::fs::create_dir_all(dir).expect("could not create capture directory");
        std::env::set_current_dir(dir).expect("could not enter capture directory");
    }

    let (mut rl, thread) = raylib::init()
        .size(cfg::SCREEN_W, cfg::SCREEN_H)
        .title("Shadowblade")
        .vsync()
        .build();
    rl.set_target_fps(cfg::TARGET_FPS);
    rl.set_exit_key(None); // ESC is handled explicitly so menus can use it.

    const SHOT_FRAMES: [u32; 6] = [40, 82, 95, 165, 190, 238];
    let mut frame: u32 = 0;

    let mut game = Game::new();
    let mut accumulator = 0.0f32;

    while !rl.window_should_close() {
        // --- Input ---
        if rl.is_key_pressed(KeyboardKey::KEY_ESCAPE) {
            if game.mode == Mode::Skills {
                game.mode = Mode::Playing;
            } else {
                break;
            }
        }
        if rl.is_key_pressed(KeyboardKey::KEY_R) {
            game = Game::new();
            accumulator = 0.0;
        }
        if rl.is_key_pressed(KeyboardKey::KEY_TAB) {
            game.mode = match game.mode {
                Mode::Playing => Mode::Skills,
                Mode::Skills => Mode::Playing,
                other => other,
            };
        }

        if game.mode == Mode::Skills {
            if rl.is_key_pressed(KeyboardKey::KEY_W) || rl.is_key_pressed(KeyboardKey::KEY_UP) {
                game.skill_cursor = (game.skill_cursor + 3) % 4;
            }
            if rl.is_key_pressed(KeyboardKey::KEY_S) || rl.is_key_pressed(KeyboardKey::KEY_DOWN) {
                game.skill_cursor = (game.skill_cursor + 1) % 4;
            }
            if rl.is_key_pressed(KeyboardKey::KEY_ENTER) && game.player.skill_points > 0 {
                let s = &mut game.player.skills;
                let rank = match game.skill_cursor {
                    0 => &mut s.keen_edge,
                    1 => &mut s.swift,
                    2 => &mut s.ironhide,
                    _ => &mut s.siphon,
                };
                if *rank < 5 {
                    *rank += 1;
                    game.player.skill_points -= 1;
                }
            }
        }

        // --- Fixed-step simulation ---
        if game.mode == Mode::Playing || game.mode == Mode::Dead {
            let input = if capture_dir.is_some() {
                scripted_input(frame)
            } else {
                sample_input(&rl)
            };
            // In capture mode the clock is fixed so runs are reproducible.
            accumulator += if capture_dir.is_some() {
                cfg::TICK
            } else {
                rl.get_frame_time().min(0.25)
            };
            let mut steps = 0;
            while accumulator >= cfg::TICK && steps < cfg::MAX_TICKS_PER_FRAME {
                game.update(input, cfg::TICK);
                accumulator -= cfg::TICK;
                steps += 1;
            }
            if steps == cfg::MAX_TICKS_PER_FRAME {
                accumulator = 0.0; // Drop the backlog rather than spiral.
            }
        }

        // --- Render ---
        let cam = game.cam.to_raylib();
        let mut d = rl.begin_drawing(&thread);
        d.clear_background(cfg::C_SKY_TOP);
        render::draw_background(&mut d, cam.target.x, cam.target.y);

        {
            let mut w = d.begin_mode2D(cam);
            render::draw_stage(&mut w, &game.stage);
            for e in &game.enemies {
                render::draw_enemy(&mut w, e);
            }
            render::draw_player(&mut w, &game.player);
            render::draw_sparks(&mut w, &game.sparks);
            render::draw_float_text(&mut w, &game.floats);
        }

        ui::draw_hud(
            &mut d,
            &game.player,
            game.wave + 1,
            game.stage.waves.len(),
            game.living_enemies(),
            game.stage.name,
            game.time,
        );
        ui::draw_controls(&mut d);

        if game.toast_life > 0.0 {
            let toast = game.toast.clone();
            ui::draw_wave_toast(&mut d, &toast, game.toast_life);
        }

        match game.mode {
            Mode::Skills => ui::draw_skill_menu(&mut d, &game.player, game.skill_cursor),
            Mode::Dead => ui::draw_center_banner(&mut d, "DEFEATED", "press R to try again", cfg::C_DAMAGE),
            Mode::Victory => {
                ui::draw_center_banner(&mut d, "STAGE CLEAR", "press R to play again", cfg::C_ACCENT)
            }
            Mode::Playing => {}
        }

        d.draw_fps(cfg::SCREEN_W - 90, cfg::SCREEN_H - 24);
        drop(d);

        if let Some(dir) = &capture_dir {
            let _ = dir;
            if SHOT_FRAMES.contains(&frame) {
                rl.take_screenshot(&thread, &format!("frame_{:04}.png", frame));
            }
            if frame > *SHOT_FRAMES.last().unwrap() + 2 {
                break;
            }
        }
        frame += 1;
    }
}
