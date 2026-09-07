//! Frame-data driven melee.
//!
//! An attack is three phases: startup (windup, no hitbox), active (hitbox live)
//! and recovery (committed, vulnerable). Chaining into the next combo step is
//! only allowed inside a cancel window, which is what stops attack-spam from
//! being optimal and gives combat its rhythm.

use raylib::prelude::Rectangle;

/// Direction an entity faces. Stored as an enum rather than a sign so that
/// "which way am I facing" is never confused with "which way am I moving".
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Facing {
    Left,
    Right,
}

impl Facing {
    pub fn sign(self) -> f32 {
        match self {
            Facing::Left => -1.0,
            Facing::Right => 1.0,
        }
    }

    pub fn from_sign(s: f32) -> Facing {
        if s < 0.0 {
            Facing::Left
        } else {
            Facing::Right
        }
    }
}

/// One step of a combo, in seconds.
#[derive(Clone, Copy)]
pub struct Attack {
    pub startup: f32,
    pub active: f32,
    pub recovery: f32,
    /// Reach measured forward from the front edge of the collision box.
    pub reach: f32,
    /// Vertical extent of the hitbox, centred on the torso.
    pub height: f32,
    /// Vertical offset of the hitbox centre from the entity centre.
    pub y_offset: f32,
    pub damage: f32,
    pub knockback: f32,
    pub launch: f32,
    pub stamina: f32,
    /// Self-propulsion applied at the start of the active phase; gives each
    /// swing a small step forward so combos advance across the screen.
    pub lunge: f32,
    /// Screen shake on connect.
    pub shake: f32,
    pub hitstop: f32,
    /// Fraction of (active + recovery) during which the next step can be
    /// buffered. Below 1.0 there is always a punish window on a whiffed combo.
    pub cancel_window: f32,
}

impl Attack {
    pub fn total(&self) -> f32 {
        self.startup + self.active + self.recovery
    }

    /// Hitbox in world space, or `None` outside the active phase.
    pub fn hitbox(&self, elapsed: f32, cx: f32, cy: f32, half_w: f32, facing: Facing) -> Option<Rectangle> {
        if elapsed < self.startup || elapsed >= self.startup + self.active {
            return None;
        }
        let x = match facing {
            Facing::Right => cx + half_w,
            Facing::Left => cx - half_w - self.reach,
        };
        Some(Rectangle::new(
            x,
            cy + self.y_offset - self.height * 0.5,
            self.reach,
            self.height,
        ))
    }

    /// True once the attack has committed far enough to chain into the next.
    pub fn in_cancel_window(&self, elapsed: f32) -> bool {
        let open = self.startup + self.active * 0.55;
        let close = self.startup + (self.active + self.recovery) * self.cancel_window;
        elapsed >= open && elapsed < close
    }
}

/// The player's ground chain. Deliberately escalating: fast pokes that commit
/// little, into a slow finisher that commits a lot and pays out in knockback.
pub const PLAYER_COMBO: [Attack; 3] = [
    // 1: quick horizontal slash.
    Attack {
        startup: 0.070,
        active: 0.075,
        recovery: 0.150,
        reach: 46.0,
        height: 42.0,
        y_offset: -6.0,
        damage: 9.0,
        knockback: 105.0,
        launch: 0.0,
        stamina: 8.0,
        lunge: 105.0,
        shake: 1.6,
        hitstop: 0.035,
        cancel_window: 0.85,
    },
    // 2: rising backhand, slightly longer.
    Attack {
        startup: 0.080,
        active: 0.085,
        recovery: 0.175,
        reach: 52.0,
        height: 48.0,
        y_offset: -10.0,
        damage: 11.0,
        knockback: 135.0,
        launch: -60.0,
        stamina: 10.0,
        lunge: 125.0,
        shake: 2.2,
        hitstop: 0.045,
        cancel_window: 0.80,
    },
    // 3: committed overhead finisher.
    Attack {
        startup: 0.155,
        active: 0.105,
        recovery: 0.330,
        reach: 62.0,
        height: 66.0,
        y_offset: -4.0,
        damage: 20.0,
        knockback: 300.0,
        launch: -190.0,
        stamina: 18.0,
        lunge: 165.0,
        shake: 4.5,
        hitstop: 0.085,
        cancel_window: 0.0,
    },
];

/// Air attack: a single downward-biased slash, no chain.
pub const PLAYER_AIR_ATTACK: Attack = Attack {
    startup: 0.080,
    active: 0.110,
    recovery: 0.190,
    reach: 54.0,
    height: 56.0,
    y_offset: 6.0,
    damage: 13.0,
    knockback: 150.0,
    launch: 40.0,
    stamina: 12.0,
    lunge: 60.0,
    shake: 2.4,
    hitstop: 0.05,
    cancel_window: 0.0,
};

/// Grunt: readable windup, short reach, low commitment.
pub const ENEMY_GRUNT_ATTACK: Attack = Attack {
    startup: 0.340,
    active: 0.090,
    recovery: 0.420,
    reach: 44.0,
    height: 46.0,
    y_offset: -6.0,
    damage: 10.0,
    knockback: 170.0,
    launch: 0.0,
    stamina: 0.0,
    lunge: 90.0,
    shake: 2.0,
    hitstop: 0.04,
    cancel_window: 0.0,
};

/// Brute: long telegraph, big reach, heavily punishable on whiff.
pub const ENEMY_BRUTE_ATTACK: Attack = Attack {
    startup: 0.520,
    active: 0.120,
    recovery: 0.620,
    reach: 62.0,
    height: 64.0,
    y_offset: -4.0,
    damage: 20.0,
    knockback: 300.0,
    launch: -120.0,
    stamina: 0.0,
    lunge: 150.0,
    shake: 4.0,
    hitstop: 0.07,
    cancel_window: 0.0,
};

pub fn overlaps(a: Rectangle, b: Rectangle) -> bool {
    a.x < b.x + b.width && a.x + a.width > b.x && a.y < b.y + b.height && a.y + a.height > b.y
}
