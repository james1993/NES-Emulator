//! Enemy entities and their AI.
//!
//! The AI is a small state machine with a deliberate "think" cadence: enemies
//! re-decide only every so often rather than every tick, which keeps them from
//! jittering at the edge of their attack range and gives the player readable
//! openings.

use crate::combat::{Attack, Facing, ENEMY_BRUTE_ATTACK, ENEMY_GRUNT_ATTACK};
use crate::config as cfg;
use crate::physics::{grounded, move_and_collide};
use crate::world::{EnemyKind, Platform};
use raylib::prelude::*;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum EState {
    Idle,
    Chase,
    Attack,
    Recover,
    Stagger,
    Dead,
}

pub struct Enemy {
    pub id: u32,
    pub kind: EnemyKind,
    pub rect: Rectangle,
    pub vel: Vector2,
    pub facing: Facing,
    pub state: EState,
    pub on_ground: bool,

    pub hp: f32,
    pub max_hp: f32,
    pub state_time: f32,
    pub think: f32,
    pub hitstop: f32,
    pub flash: f32,
    pub death_time: f32,
    pub anim: f32,

    /// Set once per swing so a single active hitbox cannot hit twice.
    pub hit_player: bool,
}

pub struct EnemyStats {
    pub hp: f32,
    pub speed: f32,
    pub xp: i32,
    pub attack: Attack,
    /// Distance at which the enemy commits to a swing.
    pub attack_range: f32,
    /// Distance at which it notices the player at all.
    pub aggro_range: f32,
    /// Delay between decisions.
    pub think_time: f32,
}

pub fn stats_for(kind: EnemyKind) -> EnemyStats {
    match kind {
        EnemyKind::Grunt => EnemyStats {
            hp: 34.0,
            speed: 132.0,
            xp: 22,
            attack: ENEMY_GRUNT_ATTACK,
            attack_range: 62.0,
            aggro_range: 520.0,
            think_time: 0.22,
        },
        EnemyKind::Brute => EnemyStats {
            hp: 82.0,
            speed: 92.0,
            xp: 55,
            attack: ENEMY_BRUTE_ATTACK,
            attack_range: 78.0,
            aggro_range: 640.0,
            think_time: 0.34,
        },
    }
}

impl Enemy {
    pub fn new(id: u32, kind: EnemyKind, x: f32, y: f32) -> Enemy {
        let st = stats_for(kind);
        Enemy {
            id,
            kind,
            rect: Rectangle::new(x, y, cfg::ENEMY_W, cfg::ENEMY_H),
            vel: Vector2::zero(),
            facing: Facing::Left,
            state: EState::Idle,
            on_ground: false,
            hp: st.hp,
            max_hp: st.hp,
            state_time: 0.0,
            think: 0.0,
            hitstop: 0.0,
            flash: 0.0,
            death_time: 0.0,
            anim: 0.0,
            hit_player: false,
        }
    }

    pub fn center(&self) -> Vector2 {
        Vector2::new(self.rect.x + self.rect.width * 0.5, self.rect.y + self.rect.height * 0.5)
    }

    pub fn alive(&self) -> bool {
        self.state != EState::Dead
    }

    /// Corpses linger briefly so the kill reads, then are culled.
    pub fn expired(&self) -> bool {
        self.state == EState::Dead && self.death_time > 1.4
    }

    pub fn attack_hitbox(&self) -> Option<Rectangle> {
        if self.state != EState::Attack {
            return None;
        }
        let atk = stats_for(self.kind).attack;
        let c = self.center();
        atk.hitbox(self.state_time, c.x, c.y, self.rect.width * 0.5, self.facing)
    }

    /// True while the windup is showing — used to draw the telegraph flash.
    pub fn telegraphing(&self) -> bool {
        self.state == EState::Attack && self.state_time < stats_for(self.kind).attack.startup
    }

    fn set_state(&mut self, s: EState) {
        self.state = s;
        self.state_time = 0.0;
    }

    /// Returns XP if this hit was the killing blow.
    pub fn take_hit(&mut self, damage: f32, from_x: f32, knockback: f32, launch: f32) -> Option<i32> {
        if self.state == EState::Dead {
            return None;
        }
        self.hp -= damage;
        self.flash = 0.12;
        let dir = if from_x < self.center().x { 1.0 } else { -1.0 };
        self.vel.x = dir * knockback;
        if launch != 0.0 {
            self.vel.y = launch;
        }
        if self.hp <= 0.0 {
            self.set_state(EState::Dead);
            self.death_time = 0.0;
            Some(stats_for(self.kind).xp)
        } else {
            self.set_state(EState::Stagger);
            None
        }
    }

    pub fn update(&mut self, player_center: Vector2, player_alive: bool, platforms: &[Platform], dt: f32) {
        if self.hitstop > 0.0 {
            self.hitstop -= dt;
            return;
        }

        self.state_time += dt;
        self.anim += dt;
        self.flash = (self.flash - dt).max(0.0);
        self.on_ground = grounded(self.rect, platforms);

        if self.state == EState::Dead {
            self.death_time += dt;
            self.vel.y += cfg::GRAVITY * dt;
            self.vel.x *= 1.0 - (5.0 * dt).min(1.0);
            self.integrate(platforms, dt);
            return;
        }

        let st = stats_for(self.kind);
        let c = self.center();
        let dx = player_center.x - c.x;
        let dy = player_center.y - c.y;
        let dist = dx.abs();

        match self.state {
            EState::Stagger => {
                self.decelerate(dt, 1400.0);
                if self.state_time > 0.28 {
                    self.set_state(EState::Chase);
                }
            }

            EState::Attack => {
                let atk = st.attack;
                let prev = self.state_time - dt;
                if prev < atk.startup && self.state_time >= atk.startup {
                    self.vel.x = self.facing.sign() * atk.lunge;
                    self.hit_player = false;
                }
                if self.state_time >= atk.startup {
                    self.decelerate(dt, 900.0);
                }
                if self.state_time >= atk.total() {
                    self.set_state(EState::Recover);
                }
            }

            EState::Recover => {
                self.decelerate(dt, 1200.0);
                if self.state_time > 0.18 {
                    self.set_state(EState::Chase);
                }
            }

            EState::Idle => {
                self.decelerate(dt, 1200.0);
                if player_alive && dist < st.aggro_range {
                    self.set_state(EState::Chase);
                }
            }

            EState::Chase => {
                self.think -= dt;

                if !player_alive {
                    self.decelerate(dt, 1200.0);
                    self.set_state(EState::Idle);
                } else {
                    self.facing = Facing::from_sign(dx);

                    // Only commit to a swing at roughly the same height,
                    // otherwise the enemy stabs at empty air below a ledge.
                    let aligned = dy.abs() < 56.0;
                    if dist <= st.attack_range && aligned && self.think <= 0.0 {
                        self.think = st.think_time;
                        self.hit_player = false;
                        self.set_state(EState::Attack);
                    } else {
                        // Stop just short of the player so a crowd does not
                        // stack into a single column.
                        let desired = st.attack_range * 0.8;
                        if dist > desired {
                            self.vel.x = self.facing.sign() * st.speed;
                        } else {
                            self.decelerate(dt, 1600.0);
                        }
                    }
                }
            }

            EState::Dead => {}
        }

        self.vel.y = (self.vel.y + cfg::GRAVITY * dt).min(cfg::MAX_FALL_SPEED);
        self.integrate(platforms, dt);
    }

    fn decelerate(&mut self, dt: f32, rate: f32) {
        let d = rate * dt;
        if self.vel.x.abs() <= d {
            self.vel.x = 0.0;
        } else {
            self.vel.x -= self.vel.x.signum() * d;
        }
    }

    fn integrate(&mut self, platforms: &[Platform], dt: f32) {
        let res = move_and_collide(&mut self.rect, self.vel.x * dt, self.vel.y * dt, platforms, false);
        if res.on_ground {
            self.vel.y = 0.0;
            self.on_ground = true;
        }
        if res.hit_wall {
            self.vel.x = 0.0;
        }
        if res.hit_ceiling {
            self.vel.y = 0.0;
        }
    }
}
