//! Player entity: input handling, state machine and progression.

use crate::combat::{Attack, Facing, PLAYER_AIR_ATTACK, PLAYER_COMBO};
use crate::config as cfg;
use crate::physics::{grounded, move_and_collide};
use crate::world::Platform;
use raylib::prelude::*;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum PState {
    Idle,
    Run,
    Air,
    Attack,
    AirAttack,
    Block,
    Roll,
    Hurt,
    Dead,
}

/// Unlockable passives. Kept small and readable: each one changes a number the
/// player can feel, rather than adding a subsystem.
#[derive(Default, Clone, Copy)]
pub struct Skills {
    pub keen_edge: i32,   // +damage
    pub swift: i32,       // +move speed, cheaper rolls
    pub ironhide: i32,    // damage reduction
    pub siphon: i32,      // heal on finisher hit
}

impl Skills {
    pub fn damage_mult(&self) -> f32 {
        1.0 + self.keen_edge as f32 * 0.12
    }
    pub fn speed_mult(&self) -> f32 {
        1.0 + self.swift as f32 * 0.07
    }
    pub fn roll_cost_mult(&self) -> f32 {
        (1.0 - self.swift as f32 * 0.10).max(0.4)
    }
    pub fn damage_taken_mult(&self) -> f32 {
        (1.0 - self.ironhide as f32 * 0.09).max(0.35)
    }
    pub fn siphon_heal(&self) -> f32 {
        self.siphon as f32 * 3.5
    }
}

/// Buffered inputs, sampled once per frame and consumed by the fixed-step
/// update so a fast tap is never dropped between ticks.
#[derive(Default, Clone, Copy)]
pub struct InputState {
    pub move_x: f32,
    pub jump_pressed: bool,
    pub jump_held: bool,
    pub attack_pressed: bool,
    pub block_held: bool,
    pub roll_pressed: bool,
    pub down_held: bool,
}

pub struct Player {
    pub rect: Rectangle,
    pub vel: Vector2,
    pub facing: Facing,
    pub state: PState,
    pub on_ground: bool,

    pub hp: f32,
    pub max_hp: f32,
    pub stamina: f32,
    pub max_stamina: f32,

    pub level: i32,
    pub xp: i32,
    pub xp_needed: i32,
    pub skill_points: i32,
    pub skills: Skills,

    // Timers
    pub state_time: f32,
    pub coyote: f32,
    pub jump_buffer: f32,
    pub invuln: f32,
    pub stam_delay: f32,
    pub roll_cd: f32,
    pub block_time: f32,
    pub hitstop: f32,

    // Combo
    pub combo_index: usize,
    pub combo_buffered: bool,
    /// Enemies already struck by the current active hitbox, so one swing cannot
    /// hit the same target twice.
    pub hit_ids: Vec<u32>,

    pub anim: f32,
    pub parried: bool,
}

impl Player {
    pub fn new(spawn: Vector2) -> Player {
        Player {
            rect: Rectangle::new(spawn.x, spawn.y, cfg::PLAYER_W, cfg::PLAYER_H),
            vel: Vector2::zero(),
            facing: Facing::Right,
            state: PState::Idle,
            on_ground: false,
            hp: cfg::PLAYER_BASE_HP,
            max_hp: cfg::PLAYER_BASE_HP,
            stamina: cfg::PLAYER_BASE_STAMINA,
            max_stamina: cfg::PLAYER_BASE_STAMINA,
            level: 1,
            xp: 0,
            xp_needed: cfg::XP_PER_LEVEL_BASE,
            skill_points: 0,
            skills: Skills::default(),
            state_time: 0.0,
            coyote: 0.0,
            jump_buffer: 0.0,
            invuln: 0.0,
            stam_delay: 0.0,
            roll_cd: 0.0,
            block_time: 0.0,
            hitstop: 0.0,
            combo_index: 0,
            combo_buffered: false,
            hit_ids: Vec::new(),
            anim: 0.0,
            parried: false,
        }
    }

    pub fn center(&self) -> Vector2 {
        Vector2::new(self.rect.x + self.rect.width * 0.5, self.rect.y + self.rect.height * 0.5)
    }

    pub fn current_attack(&self) -> Option<&'static Attack> {
        match self.state {
            PState::Attack => PLAYER_COMBO.get(self.combo_index),
            PState::AirAttack => Some(&PLAYER_AIR_ATTACK),
            _ => None,
        }
    }

    /// Hitbox of the swing in flight, if any.
    pub fn attack_hitbox(&self) -> Option<Rectangle> {
        let atk = self.current_attack()?;
        let c = self.center();
        atk.hitbox(self.state_time, c.x, c.y, self.rect.width * 0.5, self.facing)
    }

    pub fn is_invulnerable(&self) -> bool {
        if self.invuln > 0.0 {
            return true;
        }
        if self.state == PState::Roll {
            let f = self.state_time / cfg::ROLL_TIME;
            return f >= cfg::ROLL_IFRAMES.0 && f <= cfg::ROLL_IFRAMES.1;
        }
        false
    }

    pub fn is_blocking(&self) -> bool {
        self.state == PState::Block
    }

    fn set_state(&mut self, s: PState) {
        self.state = s;
        self.state_time = 0.0;
        self.hit_ids.clear();
    }

    fn spend_stamina(&mut self, amount: f32) -> bool {
        if self.stamina < amount {
            return false;
        }
        self.stamina -= amount;
        self.stam_delay = cfg::STAMINA_REGEN_DELAY;
        true
    }

    pub fn add_xp(&mut self, amount: i32) -> bool {
        self.xp += amount;
        let mut leveled = false;
        while self.xp >= self.xp_needed {
            self.xp -= self.xp_needed;
            self.level += 1;
            self.xp_needed = cfg::XP_PER_LEVEL_BASE + cfg::XP_PER_LEVEL_STEP * (self.level - 1);
            self.max_hp += cfg::HP_PER_LEVEL;
            self.max_stamina += cfg::STAMINA_PER_LEVEL;
            // Levelling restores the player: it is a reward, not a bookkeeping
            // step, and it keeps the pace up between waves.
            self.hp = self.max_hp;
            self.stamina = self.max_stamina;
            self.skill_points += cfg::SKILL_POINTS_PER_LEVEL;
            leveled = true;
        }
        leveled
    }

    pub fn attack_damage(&self, atk: &Attack) -> f32 {
        (atk.damage + cfg::DAMAGE_PER_LEVEL * (self.level - 1) as f32) * self.skills.damage_mult()
    }

    /// Returns true if the hit was taken (false if dodged, or already dead).
    pub fn take_hit(&mut self, damage: f32, from_x: f32) -> HitOutcome {
        if self.state == PState::Dead {
            return HitOutcome::Ignored;
        }
        if self.is_invulnerable() {
            return HitOutcome::Ignored;
        }

        if self.is_blocking() {
            // A block that began within the parry window negates the hit
            // entirely and costs nothing.
            if self.block_time <= cfg::PARRY_WINDOW {
                self.parried = true;
                self.invuln = 0.18;
                return HitOutcome::Parried;
            }
            let stam_cost = damage * cfg::BLOCK_STAMINA_PER_DAMAGE;
            if self.stamina >= stam_cost {
                self.stamina -= stam_cost;
                self.stam_delay = cfg::STAMINA_REGEN_DELAY;
                let chip = damage * cfg::BLOCK_CHIP * self.skills.damage_taken_mult();
                self.hp -= chip;
                self.vel.x = if from_x < self.rect.x { 90.0 } else { -90.0 };
                if self.hp <= 0.0 {
                    self.die();
                    return HitOutcome::Killed;
                }
                return HitOutcome::Blocked;
            }
            // Guard broken: the hit lands in full and staggers harder.
            self.stamina = 0.0;
            self.stam_delay = cfg::STAMINA_REGEN_DELAY * 2.0;
        }

        let taken = damage * self.skills.damage_taken_mult();
        self.hp -= taken;
        self.vel.x = if from_x < self.rect.x { 200.0 } else { -200.0 };
        self.vel.y = -140.0;
        self.invuln = cfg::INVULN_AFTER_HIT;

        if self.hp <= 0.0 {
            self.die();
            HitOutcome::Killed
        } else {
            self.set_state(PState::Hurt);
            HitOutcome::Hit
        }
    }

    fn die(&mut self) {
        self.hp = 0.0;
        self.set_state(PState::Dead);
        self.vel.x = 0.0;
    }

    pub fn update(&mut self, input: InputState, platforms: &[Platform], dt: f32) {
        // Hitstop freezes the actor for a few frames on a connect, which is what
        // gives a hit its weight. Timers that gate input still run.
        if self.hitstop > 0.0 {
            self.hitstop -= dt;
            return;
        }

        self.state_time += dt;
        self.anim += dt;
        self.invuln = (self.invuln - dt).max(0.0);
        self.roll_cd = (self.roll_cd - dt).max(0.0);
        self.stam_delay = (self.stam_delay - dt).max(0.0);

        if self.stam_delay <= 0.0 && self.state != PState::Dead {
            let regen = if self.is_blocking() { cfg::STAMINA_REGEN * 0.3 } else { cfg::STAMINA_REGEN };
            self.stamina = (self.stamina + regen * dt).min(self.max_stamina);
        }

        if self.state == PState::Dead {
            self.apply_gravity(dt);
            self.integrate(platforms, false, dt);
            return;
        }

        // Input buffering.
        if input.jump_pressed {
            self.jump_buffer = cfg::JUMP_BUFFER;
        } else {
            self.jump_buffer = (self.jump_buffer - dt).max(0.0);
        }

        let was_ground = self.on_ground;
        self.on_ground = grounded(self.rect, platforms);
        if self.on_ground {
            self.coyote = cfg::COYOTE_TIME;
        } else {
            self.coyote = (self.coyote - dt).max(0.0);
        }
        if self.on_ground && !was_ground && self.state == PState::Air {
            self.set_state(PState::Idle);
        }

        match self.state {
            PState::Hurt => {
                if self.state_time >= cfg::HURT_TIME {
                    self.set_state(if self.on_ground { PState::Idle } else { PState::Air });
                }
                self.decelerate(dt, 2200.0);
            }

            PState::Roll => {
                self.vel.x = self.facing.sign() * cfg::ROLL_SPEED * self.skills.speed_mult();
                if self.state_time >= cfg::ROLL_TIME {
                    self.roll_cd = cfg::ROLL_COOLDOWN;
                    self.set_state(if self.on_ground { PState::Idle } else { PState::Air });
                }
            }

            PState::Attack | PState::AirAttack => {
                let atk = *self.current_attack().expect("attack state without attack data");

                // Buffer the next combo step for the whole swing; consume it
                // when the cancel window opens.
                if input.attack_pressed {
                    self.combo_buffered = true;
                }

                // Lunge fires once, at the moment the hitbox goes live.
                let prev = self.state_time - dt;
                if prev < atk.startup && self.state_time >= atk.startup {
                    self.vel.x = self.facing.sign() * atk.lunge;
                }

                if self.state == PState::Attack {
                    self.decelerate(dt, 1500.0);
                } else {
                    self.apply_gravity(dt);
                }

                let chainable = self.state == PState::Attack && self.combo_index + 1 < PLAYER_COMBO.len();
                if chainable && self.combo_buffered && atk.in_cancel_window(self.state_time) {
                    let next = PLAYER_COMBO[self.combo_index + 1];
                    if self.spend_stamina(next.stamina) {
                        self.combo_index += 1;
                        self.combo_buffered = false;
                        self.set_state(PState::Attack);
                    }
                } else if self.state_time >= atk.total() {
                    self.combo_index = 0;
                    self.combo_buffered = false;
                    self.set_state(if self.on_ground { PState::Idle } else { PState::Air });
                }
            }

            PState::Block => {
                self.block_time += dt;
                self.decelerate(dt, 3000.0);
                if !input.block_held || !self.on_ground || self.stamina <= 0.0 {
                    self.set_state(PState::Idle);
                }
                if input.move_x.abs() > 0.1 {
                    self.facing = Facing::from_sign(input.move_x);
                }
                self.try_start_actions(input, true);
            }

            PState::Idle | PState::Run | PState::Air => {
                self.free_movement(input, dt);
                self.try_start_actions(input, false);
            }

            PState::Dead => {}
        }

        self.apply_gravity_if_airborne(dt);
        let drop = input.down_held && input.jump_pressed;
        self.integrate(platforms, drop, dt);
    }

    /// Actions available out of a neutral (or blocking) state.
    fn try_start_actions(&mut self, input: InputState, from_block: bool) {
        // Roll cancels a block, and is the primary defensive option.
        if input.roll_pressed && self.roll_cd <= 0.0 && self.on_ground {
            let cost = cfg::ROLL_STAMINA * self.skills.roll_cost_mult();
            if self.stamina >= cost {
                self.spend_stamina(cost);
                if input.move_x.abs() > 0.1 {
                    self.facing = Facing::from_sign(input.move_x);
                }
                self.set_state(PState::Roll);
                return;
            }
        }

        if input.attack_pressed {
            if self.on_ground {
                let atk = PLAYER_COMBO[0];
                if self.stamina >= atk.stamina {
                    self.spend_stamina(atk.stamina);
                    self.combo_index = 0;
                    self.combo_buffered = false;
                    self.set_state(PState::Attack);
                    return;
                }
            } else if self.stamina >= PLAYER_AIR_ATTACK.stamina {
                self.spend_stamina(PLAYER_AIR_ATTACK.stamina);
                self.set_state(PState::AirAttack);
                return;
            }
        }

        if from_block {
            return;
        }

        if input.block_held && self.on_ground && self.stamina > 0.0 {
            self.block_time = 0.0;
            self.set_state(PState::Block);
            return;
        }

        if self.jump_buffer > 0.0 && self.coyote > 0.0 {
            self.jump_buffer = 0.0;
            self.coyote = 0.0;
            self.vel.y = -cfg::PLAYER_JUMP_SPEED;
            self.on_ground = false;
            self.set_state(PState::Air);
        }
    }

    fn free_movement(&mut self, input: InputState, dt: f32) {
        let max_speed = cfg::PLAYER_RUN_SPEED * self.skills.speed_mult();
        let accel = if self.on_ground { cfg::PLAYER_ACCEL } else { cfg::PLAYER_AIR_ACCEL };

        if input.move_x.abs() > 0.1 {
            self.facing = Facing::from_sign(input.move_x);
            self.vel.x += input.move_x * accel * dt;
            self.vel.x = self.vel.x.clamp(-max_speed, max_speed);
        } else if self.on_ground {
            self.decelerate(dt, cfg::PLAYER_FRICTION);
        }

        // Variable jump height: releasing the button cuts the rise short.
        if !input.jump_held && self.vel.y < 0.0 {
            self.vel.y *= 1.0 - (1.0 - cfg::JUMP_CUT) * (dt * 60.0).min(1.0);
        }

        self.state = if !self.on_ground {
            PState::Air
        } else if input.move_x.abs() > 0.1 {
            PState::Run
        } else {
            PState::Idle
        };
    }

    fn decelerate(&mut self, dt: f32, rate: f32) {
        let d = rate * dt;
        if self.vel.x.abs() <= d {
            self.vel.x = 0.0;
        } else {
            self.vel.x -= self.vel.x.signum() * d;
        }
    }

    fn apply_gravity(&mut self, dt: f32) {
        self.vel.y = (self.vel.y + cfg::GRAVITY * dt).min(cfg::MAX_FALL_SPEED);
    }

    fn apply_gravity_if_airborne(&mut self, dt: f32) {
        // Rolling and grounded attacks stay pinned to the floor; everything else
        // is subject to gravity.
        if matches!(self.state, PState::Attack | PState::Block) && self.on_ground {
            self.vel.y = self.vel.y.max(0.0);
        }
        if self.state != PState::AirAttack {
            self.apply_gravity(dt);
        }
    }

    fn integrate(&mut self, platforms: &[Platform], drop_through: bool, dt: f32) {
        let res = move_and_collide(&mut self.rect, self.vel.x * dt, self.vel.y * dt, platforms, drop_through);
        if res.on_ground {
            self.vel.y = 0.0;
            self.on_ground = true;
        }
        if res.hit_ceiling {
            self.vel.y = 0.0;
        }
        if res.hit_wall {
            self.vel.x = 0.0;
        }
    }
}

#[derive(PartialEq, Eq, Clone, Copy, Debug)]
pub enum HitOutcome {
    Ignored,
    Blocked,
    Parried,
    Hit,
    Killed,
}
