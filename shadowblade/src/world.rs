//! Stage geometry, spawn scheduling and the follow camera.

use crate::config as cfg;
use raylib::prelude::*;

#[derive(Clone, Copy)]
pub struct Platform {
    pub rect: Rectangle,
    /// One-way platforms are only solid when falling onto them from above.
    pub one_way: bool,
}

impl Platform {
    pub fn solid(x: f32, y: f32, w: f32, h: f32) -> Platform {
        Platform { rect: Rectangle::new(x, y, w, h), one_way: false }
    }

    pub fn ledge(x: f32, y: f32, w: f32) -> Platform {
        Platform { rect: Rectangle::new(x, y, w, 14.0), one_way: true }
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum EnemyKind {
    Grunt,
    Brute,
}

#[derive(Clone, Copy)]
pub struct Spawn {
    pub kind: EnemyKind,
    pub x: f32,
    pub y: f32,
}

/// A wave holds back its spawns until the previous wave is cleared, which keeps
/// the arena from turning into an unreadable crowd.
pub struct Wave {
    pub spawns: Vec<Spawn>,
}

pub struct Stage {
    pub name: &'static str,
    pub width: f32,
    pub platforms: Vec<Platform>,
    pub waves: Vec<Wave>,
    pub player_spawn: Vector2,
}

pub fn stage_one() -> Stage {
    let g = cfg::GROUND_Y;
    Stage {
        name: "The Broken Approach",
        width: 2600.0,
        platforms: vec![
            // Main floor, split by a pit the player must clear.
            Platform::solid(0.0, g, 1150.0, 260.0),
            Platform::solid(1420.0, g, 1180.0, 260.0),
            // Ledges over the pit and around the arena.
            Platform::ledge(1180.0, g - 118.0, 200.0),
            Platform::ledge(560.0, g - 132.0, 190.0),
            Platform::ledge(1720.0, g - 126.0, 210.0),
            Platform::ledge(2160.0, g - 210.0, 180.0),
            // Bounding walls.
            Platform::solid(-60.0, g - 700.0, 60.0, 960.0),
            Platform::solid(2600.0, g - 700.0, 60.0, 960.0),
        ],
        waves: vec![
            Wave {
                spawns: vec![
                    Spawn { kind: EnemyKind::Grunt, x: 640.0, y: g - 120.0 },
                    Spawn { kind: EnemyKind::Grunt, x: 900.0, y: g - 120.0 },
                ],
            },
            Wave {
                spawns: vec![
                    Spawn { kind: EnemyKind::Grunt, x: 1600.0, y: g - 120.0 },
                    Spawn { kind: EnemyKind::Grunt, x: 1900.0, y: g - 120.0 },
                    Spawn { kind: EnemyKind::Brute, x: 2100.0, y: g - 120.0 },
                ],
            },
            Wave {
                spawns: vec![
                    Spawn { kind: EnemyKind::Brute, x: 1750.0, y: g - 120.0 },
                    Spawn { kind: EnemyKind::Brute, x: 2250.0, y: g - 120.0 },
                    Spawn { kind: EnemyKind::Grunt, x: 2000.0, y: g - 300.0 },
                ],
            },
        ],
        player_spawn: Vector2::new(140.0, g - 120.0),
    }
}

/// Follow camera with a deadzone, exponential smoothing and trauma-based shake.
pub struct GameCamera {
    pub target: Vector2,
    pub shake: f32,
    seed: f32,
}

impl GameCamera {
    pub fn new(at: Vector2) -> GameCamera {
        GameCamera { target: at, shake: 0.0, seed: 0.0 }
    }

    pub fn add_shake(&mut self, amount: f32) {
        // Trauma accumulates but saturates, so a flurry of hits does not turn
        // the screen into an unreadable blur.
        self.shake = (self.shake + amount).min(9.0);
    }

    pub fn update(&mut self, focus: Vector2, stage_w: f32, dt: f32) {
        let dx = focus.x - self.target.x;
        if dx.abs() > cfg::CAM_DEADZONE_X {
            let excess = dx.abs() - cfg::CAM_DEADZONE_X;
            self.target.x += dx.signum() * excess * (cfg::CAM_LERP * dt).min(1.0);
        }
        let dy = (focus.y + cfg::CAM_Y_OFFSET) - self.target.y;
        if dy.abs() > cfg::CAM_DEADZONE_Y {
            let excess = dy.abs() - cfg::CAM_DEADZONE_Y;
            self.target.y += dy.signum() * excess * (cfg::CAM_LERP * dt).min(1.0);
        }

        // Keep the view inside the stage horizontally.
        let half_w = cfg::SCREEN_W as f32 * 0.5;
        self.target.x = self.target.x.clamp(half_w, (stage_w - half_w).max(half_w));

        self.shake = (self.shake - cfg::SHAKE_DECAY * dt).max(0.0);
        self.seed += dt * 34.0;
    }

    pub fn to_raylib(&self) -> Camera2D {
        // Deterministic pseudo-noise: cheap, and stable across frames.
        let s = self.shake * self.shake * 0.9;
        let ox = (self.seed * 1.7).sin() * s;
        let oy = (self.seed * 2.3).cos() * s;
        Camera2D {
            target: Vector2::new(self.target.x + ox, self.target.y + oy),
            offset: Vector2::new(cfg::SCREEN_W as f32 * 0.5, cfg::SCREEN_H as f32 * 0.5),
            rotation: 0.0,
            zoom: 1.0,
        }
    }
}
