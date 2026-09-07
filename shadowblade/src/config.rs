//! Central tuning surface.
//!
//! Every sprite dimension, physics constant and palette entry lives here so the
//! look and feel can be calibrated against reference footage without touching
//! game logic. Nothing in this file is derived from another game's data; the
//! values are hand-picked starting points for a 16:9 side-scrolling brawler.

use raylib::prelude::Color;

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

pub const SCREEN_W: i32 = 1280;
pub const SCREEN_H: i32 = 720;
pub const TARGET_FPS: u32 = 60;

/// Logic runs on a fixed step; rendering interpolates nothing but stays smooth
/// because the step matches the refresh target.
pub const TICK: f32 = 1.0 / 60.0;
/// Guard against spiral-of-death after a stall (e.g. window drag).
pub const MAX_TICKS_PER_FRAME: u32 = 5;

// ---------------------------------------------------------------------------
// Character metrics
//
// The collision box is deliberately narrower than the drawn silhouette: the
// visual character overhangs its hitbox, which is what makes a brawler feel
// generous rather than clumsy.
// ---------------------------------------------------------------------------

/// Player collision box, in world pixels.
pub const PLAYER_W: f32 = 30.0;
pub const PLAYER_H: f32 = 72.0;

/// Drawn silhouette height; limbs are scaled from this.
pub const PLAYER_DRAW_H: f32 = 78.0;

pub const ENEMY_W: f32 = 32.0;
pub const ENEMY_H: f32 = 70.0;

/// Ground level in world space (top surface of the main floor).
pub const GROUND_Y: f32 = 560.0;

// ---------------------------------------------------------------------------
// Physics
// ---------------------------------------------------------------------------

pub const GRAVITY: f32 = 2100.0;
pub const MAX_FALL_SPEED: f32 = 1100.0;

pub const PLAYER_RUN_SPEED: f32 = 270.0;
pub const PLAYER_ACCEL: f32 = 2600.0;
pub const PLAYER_FRICTION: f32 = 3000.0;
/// Air control is weaker than ground control.
pub const PLAYER_AIR_ACCEL: f32 = 1200.0;

pub const PLAYER_JUMP_SPEED: f32 = 700.0;
/// Releasing jump early cuts upward velocity by this factor (variable height).
pub const JUMP_CUT: f32 = 0.45;
/// Grace period after walking off a ledge during which jump still works.
pub const COYOTE_TIME: f32 = 0.10;
/// A jump pressed this long before landing still fires on touchdown.
pub const JUMP_BUFFER: f32 = 0.12;

pub const ROLL_SPEED: f32 = 470.0;
pub const ROLL_TIME: f32 = 0.34;
/// Invulnerable window inside the roll, as (start, end) fractions of ROLL_TIME.
pub const ROLL_IFRAMES: (f32, f32) = (0.08, 0.72);
pub const ROLL_COOLDOWN: f32 = 0.30;

// ---------------------------------------------------------------------------
// Vitals
// ---------------------------------------------------------------------------

pub const PLAYER_BASE_HP: f32 = 100.0;
pub const PLAYER_BASE_STAMINA: f32 = 100.0;
pub const STAMINA_REGEN: f32 = 34.0;
/// Regen pauses briefly after spending stamina so spam is self-limiting.
pub const STAMINA_REGEN_DELAY: f32 = 0.45;

pub const ROLL_STAMINA: f32 = 22.0;
pub const BLOCK_CHIP: f32 = 0.18;
/// Stamina drained when a block absorbs a hit, scaled by incoming damage.
pub const BLOCK_STAMINA_PER_DAMAGE: f32 = 0.9;
/// A parry succeeds if the block began within this window before the hit.
pub const PARRY_WINDOW: f32 = 0.16;

pub const HURT_TIME: f32 = 0.26;
pub const INVULN_AFTER_HIT: f32 = 0.45;

// ---------------------------------------------------------------------------
// Progression
// ---------------------------------------------------------------------------

pub const XP_PER_LEVEL_BASE: i32 = 60;
/// Each level needs this much more XP than the last (linear curve).
pub const XP_PER_LEVEL_STEP: i32 = 45;
pub const HP_PER_LEVEL: f32 = 12.0;
pub const STAMINA_PER_LEVEL: f32 = 6.0;
pub const DAMAGE_PER_LEVEL: f32 = 1.6;
pub const SKILL_POINTS_PER_LEVEL: i32 = 1;

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------

/// Player can move this far from centre before the camera starts following.
pub const CAM_DEADZONE_X: f32 = 90.0;
pub const CAM_DEADZONE_Y: f32 = 70.0;
pub const CAM_LERP: f32 = 6.0;
/// Camera sits above the player's feet by this much.
pub const CAM_Y_OFFSET: f32 = -40.0;

pub const SHAKE_DECAY: f32 = 7.0;

// ---------------------------------------------------------------------------
// Palette
//
// A cool, desaturated night palette with a single warm accent for the blade and
// for damage feedback: readable silhouettes first, colour second.
// ---------------------------------------------------------------------------

pub const C_SKY_TOP: Color = Color::new(14, 16, 30, 255);
pub const C_SKY_BOT: Color = Color::new(38, 34, 58, 255);
pub const C_MOON: Color = Color::new(226, 230, 245, 255);

pub const C_FAR: Color = Color::new(26, 28, 46, 255);
pub const C_MID: Color = Color::new(20, 22, 38, 255);
pub const C_NEAR: Color = Color::new(13, 14, 26, 255);

pub const C_GROUND: Color = Color::new(30, 30, 44, 255);
pub const C_GROUND_TOP: Color = Color::new(58, 60, 84, 255);
pub const C_PLATFORM: Color = Color::new(40, 41, 60, 255);

pub const C_PLAYER: Color = Color::new(38, 42, 66, 255);
pub const C_PLAYER_TRIM: Color = Color::new(198, 60, 72, 255);
pub const C_PLAYER_SKIN: Color = Color::new(214, 178, 148, 255);
pub const C_BLADE: Color = Color::new(236, 240, 255, 255);

pub const C_ENEMY: Color = Color::new(62, 46, 48, 255);
pub const C_ENEMY_TRIM: Color = Color::new(150, 122, 70, 255);
pub const C_ENEMY_HEAVY: Color = Color::new(50, 44, 66, 255);
pub const C_ENEMY_HEAVY_TRIM: Color = Color::new(120, 92, 168, 255);

pub const C_ACCENT: Color = Color::new(255, 196, 96, 255);
pub const C_DAMAGE: Color = Color::new(232, 88, 84, 255);
pub const C_HEAL: Color = Color::new(120, 210, 140, 255);
pub const C_STAMINA: Color = Color::new(110, 190, 200, 255);
pub const C_XP: Color = Color::new(150, 130, 220, 255);
pub const C_UI_BG: Color = Color::new(10, 11, 20, 220);
pub const C_UI_LINE: Color = Color::new(90, 96, 128, 255);
pub const C_TEXT: Color = Color::new(226, 230, 245, 255);
pub const C_TEXT_DIM: Color = Color::new(138, 144, 172, 255);
