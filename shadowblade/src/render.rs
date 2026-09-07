//! Procedural rendering. Every visual here is drawn from primitives at runtime;
//! there are no image assets in this project.
//!
//! Characters are articulated figures: a torso, head and four limbs posed by
//! angle, driven by the entity's state machine and a phase value. That keeps
//! the animation authored in code where the frame data already lives, so a
//! swing's visual and its hitbox can never drift out of sync.

use crate::config as cfg;
use crate::enemy::{stats_for, EState, Enemy};
use crate::player::{PState, Player};
use crate::world::{EnemyKind, Platform, Stage};
use raylib::prelude::*;
use crate::util::measure_text;

/// Draws a limb as a rectangle rotated about the end nearest the body.
fn limb<D: RaylibDraw>(d: &mut D, pivot: Vector2, len: f32, thick: f32, angle_deg: f32, color: Color) {
    d.draw_rectangle_pro(
        Rectangle::new(pivot.x, pivot.y, len, thick),
        Vector2::new(0.0, thick * 0.5),
        angle_deg,
        color,
    );
}

fn lerp(a: f32, b: f32, t: f32) -> f32 {
    a + (b - a) * t.clamp(0.0, 1.0)
}

// ---------------------------------------------------------------------------
// Background
// ---------------------------------------------------------------------------

/// Parallax layers. Drawn in screen space with the camera's X folded in at a
/// fraction, which is cheaper and steadier than moving real geometry.
pub fn draw_background<D: RaylibDraw>(d: &mut D, cam_x: f32, cam_y: f32) {
    let w = cfg::SCREEN_W;
    let h = cfg::SCREEN_H;

    // Vertical sky gradient.
    d.draw_rectangle_gradient_v(0, 0, w, h, cfg::C_SKY_TOP, cfg::C_SKY_BOT);

    // Moon, nearly fixed (very distant).
    let moon_x = 980.0 - cam_x * 0.02;
    let moon_y = 130.0 - cam_y * 0.02;
    d.draw_circle_v(Vector2::new(moon_x, moon_y), 62.0, Color::new(226, 230, 245, 40));
    d.draw_circle_v(Vector2::new(moon_x, moon_y), 46.0, cfg::C_MOON);
    // Bite out of the moon to suggest craters without texturing it.
    d.draw_circle_v(Vector2::new(moon_x - 14.0, moon_y - 10.0), 8.0, Color::new(206, 212, 232, 255));
    d.draw_circle_v(Vector2::new(moon_x + 12.0, moon_y + 14.0), 5.0, Color::new(206, 212, 232, 255));

    // Far ridge line.
    draw_ridge(d, cam_x * 0.08, cam_y * 0.05, 430.0, 150.0, 320.0, cfg::C_FAR);
    // Mid ridge with pagoda silhouettes.
    draw_ridge(d, cam_x * 0.18, cam_y * 0.10, 500.0, 110.0, 240.0, cfg::C_MID);
    draw_pagodas(d, cam_x * 0.18, cam_y * 0.10);
    // Near treeline.
    draw_ridge(d, cam_x * 0.34, cam_y * 0.18, 585.0, 70.0, 170.0, cfg::C_NEAR);
}

/// A ridge built from overlapping triangles at a fixed period.
fn draw_ridge<D: RaylibDraw>(d: &mut D, off_x: f32, off_y: f32, base_y: f32, height: f32, period: f32, color: Color) {
    let base = base_y - off_y;
    let start = -((off_x / period).floor() * period) - period * 2.0;
    let mut x = start;
    let mut i = 0;
    while x < cfg::SCREEN_W as f32 + period {
        // Deterministic height variation from the peak index.
        let n = ((i as f32 * 12.9898).sin() * 43758.547).fract().abs();
        let peak = height * (0.55 + n * 0.75);
        let px = x - off_x % period;
        // Counter-clockwise winding.
        d.draw_triangle(
            Vector2::new(px - period * 0.75, base),
            Vector2::new(px + period * 0.75, base),
            Vector2::new(px, base - peak),
            color,
        );
        x += period;
        i += 1;
    }
    d.draw_rectangle(0, base as i32, cfg::SCREEN_W, cfg::SCREEN_H - base as i32, color);
}

/// A trapezoid, as two triangles. Vertices follow the same winding as the
/// ridge triangles above, which raylib renders correctly.
fn trapezoid<D: RaylibDraw>(d: &mut D, x: f32, y: f32, half_bottom: f32, half_top: f32, h: f32, color: Color) {
    d.draw_triangle(
        Vector2::new(x - half_bottom, y),
        Vector2::new(x + half_bottom, y),
        Vector2::new(x + half_top, y - h),
        color,
    );
    d.draw_triangle(
        Vector2::new(x - half_bottom, y),
        Vector2::new(x + half_top, y - h),
        Vector2::new(x - half_top, y - h),
        color,
    );
}

/// Tiered silhouettes on the mid ridge, to read as a temple district.
///
/// Each tier is a wide, shallow roof over a narrower body. Keeping the roofs
/// much wider than the bodies is what reads as a pagoda rather than a spire.
fn draw_pagodas<D: RaylibDraw>(d: &mut D, off_x: f32, off_y: f32) {
    let color = Color::new(16, 17, 30, 255);
    const TIER_H: f32 = 34.0;

    for k in 0..4 {
        let wx = 260.0 + k as f32 * 420.0;
        let x = wx - off_x;
        if x < -160.0 || x > cfg::SCREEN_W as f32 + 160.0 {
            continue;
        }
        let base = 500.0 - off_y - (k % 2) as f32 * 26.0;
        let tiers = 3 + (k % 2);

        // Central trunk, so the tiers read as one connected building.
        let total_h = tiers as f32 * TIER_H;
        d.draw_rectangle_rec(Rectangle::new(x - 13.0, base - total_h, 26.0, total_h + 8.0), color);

        for t in 0..tiers {
            let tf = t as f32;
            let y = base - tf * TIER_H;
            // Roofs shrink going up.
            let eave = 60.0 - tf * 11.0;
            let body_half = eave * 0.42;

            // Body sits under this tier's roof.
            d.draw_rectangle_rec(Rectangle::new(x - body_half, y - TIER_H + 12.0, body_half * 2.0, TIER_H - 12.0), color);
            // Shallow flared roof.
            trapezoid(d, x, y - TIER_H + 14.0, eave, eave * 0.30, 13.0, color);
            // Eave tips turned up, the detail that sells the roofline.
            d.draw_triangle(
                Vector2::new(x - eave, y - TIER_H + 14.0),
                Vector2::new(x - eave + 11.0, y - TIER_H + 14.0),
                Vector2::new(x - eave + 3.0, y - TIER_H + 5.0),
                color,
            );
            d.draw_triangle(
                Vector2::new(x + eave - 11.0, y - TIER_H + 14.0),
                Vector2::new(x + eave, y - TIER_H + 14.0),
                Vector2::new(x + eave - 3.0, y - TIER_H + 5.0),
                color,
            );
        }

        // Finial on top.
        let top = base - total_h;
        d.draw_rectangle_rec(Rectangle::new(x - 2.0, top - 14.0, 4.0, 14.0), color);
        d.draw_circle_v(Vector2::new(x, top - 16.0), 3.5, color);
    }
}

// ---------------------------------------------------------------------------
// Stage geometry
// ---------------------------------------------------------------------------

pub fn draw_stage<D: RaylibDraw>(d: &mut D, stage: &Stage) {
    for p in &stage.platforms {
        draw_platform(d, p);
    }
}

fn draw_platform<D: RaylibDraw>(d: &mut D, p: &Platform) {
    if p.one_way {
        d.draw_rectangle_rec(p.rect, cfg::C_PLATFORM);
        // Bright lip so the standable surface is unambiguous.
        d.draw_rectangle_rec(Rectangle::new(p.rect.x, p.rect.y, p.rect.width, 3.0), cfg::C_GROUND_TOP);
        // Support struts.
        let n = (p.rect.width / 46.0).max(1.0) as i32;
        for i in 0..n {
            let x = p.rect.x + 12.0 + i as f32 * 46.0;
            d.draw_rectangle_rec(Rectangle::new(x, p.rect.y + p.rect.height, 5.0, 12.0), cfg::C_NEAR);
        }
    } else {
        d.draw_rectangle_rec(p.rect, cfg::C_GROUND);
        d.draw_rectangle_rec(Rectangle::new(p.rect.x, p.rect.y, p.rect.width, 4.0), cfg::C_GROUND_TOP);
        // Sparse stonework, keyed to position so it does not crawl.
        let cols = (p.rect.width / 64.0) as i32;
        let rows = ((p.rect.height / 48.0) as i32).min(4);
        for cx in 0..cols {
            for cy in 0..rows {
                let x = p.rect.x + cx as f32 * 64.0 + ((cy % 2) as f32 * 32.0);
                let y = p.rect.y + 10.0 + cy as f32 * 48.0;
                if x + 54.0 > p.rect.x + p.rect.width {
                    continue;
                }
                d.draw_rectangle_lines_ex(
                    Rectangle::new(x + 6.0, y, 54.0, 38.0),
                    1.0,
                    Color::new(44, 45, 64, 110),
                );
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Player
// ---------------------------------------------------------------------------

pub fn draw_player<D: RaylibDraw>(d: &mut D, p: &Player) {
    let s = p.facing.sign();
    let cx = p.rect.x + p.rect.width * 0.5;
    let feet = p.rect.y + p.rect.height;

    // Blink out on i-frames so the state is legible.
    if p.invuln > 0.0 && ((p.invuln * 30.0) as i32) % 2 == 0 {
        return;
    }

    draw_shadow(d, cx, feet, p.rect.width);

    let body = cfg::C_PLAYER;
    let trim = cfg::C_PLAYER_TRIM;

    // Pose parameters, resolved per state.
    let mut lean = 0.0f32;
    let crouch;
    let arm_front;
    let mut arm_back = 200.0f32;
    let mut leg_split = 0.0f32;
    let mut blade_angle = 200.0f32;
    let mut blade_len = 52.0f32;
    let mut show_trail = false;

    match p.state {
        PState::Idle => {
            let b = (p.anim * 2.4).sin();
            crouch = b * 1.5;
            arm_front = -40.0 + b * 4.0;
        }
        PState::Run => {
            let c = (p.anim * 13.0).sin();
            leg_split = c * 34.0;
            arm_front = -55.0 + c * 22.0;
            arm_back = 205.0 - c * 22.0;
            lean = 9.0;
            crouch = (p.anim * 26.0).sin().abs() * 2.5;
        }
        PState::Air => {
            leg_split = 20.0;
            lean = 5.0;
            arm_front = -70.0;
            arm_back = 225.0;
            crouch = -2.0;
        }
        PState::Attack | PState::AirAttack => {
            let atk = p.current_attack().copied().unwrap_or(crate::combat::PLAYER_COMBO[0]);
            let t = p.state_time;
            show_trail = t >= atk.startup && t < atk.startup + atk.active;

            // Windup pulls the blade back; the active phase whips it through;
            // recovery settles. Each combo step gets its own arc.
            let (wind_a, strike_a) = match (p.state, p.combo_index) {
                (PState::AirAttack, _) => (-150.0, 70.0),
                (_, 0) => (-135.0, 35.0),
                (_, 1) => (60.0, -95.0),
                _ => (-175.0, 65.0),
            };

            if t < atk.startup {
                let k = t / atk.startup.max(0.0001);
                blade_angle = lerp(200.0, wind_a, k * k);
                lean = lerp(0.0, -12.0, k);
                arm_front = lerp(-40.0, wind_a, k);
            } else if t < atk.startup + atk.active {
                let k = (t - atk.startup) / atk.active.max(0.0001);
                // Ease-out: fastest at the very start of the active window.
                let e = 1.0 - (1.0 - k) * (1.0 - k);
                blade_angle = lerp(wind_a, strike_a, e);
                lean = lerp(-12.0, 20.0, e);
                arm_front = blade_angle;
                blade_len = 58.0;
            } else {
                let k = (t - atk.startup - atk.active) / atk.recovery.max(0.0001);
                blade_angle = lerp(strike_a, 200.0, k);
                lean = lerp(20.0, 0.0, k);
                arm_front = lerp(strike_a, -40.0, k);
            }
            crouch = 3.0;
            leg_split = 26.0;
        }
        PState::Block => {
            arm_front = -95.0;
            arm_back = 190.0;
            lean = -6.0;
            crouch = 5.0;
            blade_angle = -95.0;
            leg_split = 18.0;
        }
        PState::Roll => {
            // Tuck into a ball and spin: the whole figure rotates.
            let k = p.state_time / cfg::ROLL_TIME;
            let ang = s * k * 360.0;
            let r = 22.0;
            let ccx = cx;
            let ccy = feet - r - 2.0;
            d.draw_circle_v(Vector2::new(ccx, ccy), r, body);
            d.draw_circle_v(Vector2::new(ccx, ccy), r * 0.62, Color::new(30, 33, 54, 255));
            // Scarf streak marks the spin.
            let a = ang.to_radians();
            d.draw_line_ex(
                Vector2::new(ccx + a.cos() * r * 0.5, ccy + a.sin() * r * 0.5),
                Vector2::new(ccx + a.cos() * (r + 16.0), ccy + a.sin() * (r + 16.0)),
                4.0,
                trim,
            );
            return;
        }
        PState::Hurt => {
            lean = -18.0;
            arm_front = -100.0;
            arm_back = 250.0;
            leg_split = 24.0;
            crouch = 6.0;
        }
        PState::Dead => {
            // Collapse flat over the death timer.
            let k = (p.state_time / 0.6).clamp(0.0, 1.0);
            let cy = lerp(feet - cfg::PLAYER_DRAW_H * 0.5, feet - 12.0, k);
            d.draw_rectangle_pro(
                Rectangle::new(cx, cy, cfg::PLAYER_DRAW_H * 0.8, 22.0),
                Vector2::new(cfg::PLAYER_DRAW_H * 0.4, 11.0),
                lerp(-90.0 * s, 0.0, k),
                body,
            );
            return;
        }
    }

    draw_figure(
        d, cx, feet, s, lean, crouch, leg_split, arm_front, arm_back, blade_angle, blade_len,
        body, trim, cfg::C_PLAYER_SKIN, true, show_trail, cfg::C_BLADE,
    );
}

// ---------------------------------------------------------------------------
// Enemies
// ---------------------------------------------------------------------------

pub fn draw_enemy<D: RaylibDraw>(d: &mut D, e: &Enemy) {
    let s = e.facing.sign();
    let cx = e.rect.x + e.rect.width * 0.5;
    let feet = e.rect.y + e.rect.height;

    let (mut body, trim) = match e.kind {
        EnemyKind::Grunt => (cfg::C_ENEMY, cfg::C_ENEMY_TRIM),
        EnemyKind::Brute => (cfg::C_ENEMY_HEAVY, cfg::C_ENEMY_HEAVY_TRIM),
    };
    // White flash on impact.
    if e.flash > 0.0 {
        body = Color::new(255, 255, 255, 255);
    }

    if e.state == EState::Dead {
        let k = (e.death_time / 0.5).clamp(0.0, 1.0);
        let alpha = (1.0 - (e.death_time - 0.9).max(0.0) / 0.5).clamp(0.0, 1.0);
        let c = Color::new(body.r, body.g, body.b, (255.0 * alpha) as u8);
        d.draw_rectangle_pro(
            Rectangle::new(cx, lerp(feet - cfg::ENEMY_H * 0.5, feet - 11.0, k), cfg::ENEMY_H * 0.8, 20.0),
            Vector2::new(cfg::ENEMY_H * 0.4, 10.0),
            lerp(-90.0 * s, 0.0, k),
            c,
        );
        return;
    }

    draw_shadow(d, cx, feet, e.rect.width);

    let mut lean = 0.0f32;
    let mut crouch = 0.0f32;
    let mut arm_front = -25.0f32;
    let mut arm_back = 205.0f32;
    let mut leg_split = 0.0f32;
    let mut blade_angle = 205.0f32;
    let mut show_trail = false;

    match e.state {
        EState::Idle => {
            crouch = (e.anim * 2.0).sin() * 1.4;
        }
        EState::Chase => {
            let c = (e.anim * 10.0).sin();
            leg_split = c * 28.0;
            arm_front = -45.0 + c * 16.0;
            arm_back = 210.0 - c * 16.0;
            lean = 7.0;
        }
        EState::Attack => {
            let atk = stats_for(e.kind).attack;
            let t = e.state_time;
            show_trail = t >= atk.startup && t < atk.startup + atk.active;
            if t < atk.startup {
                let k = t / atk.startup.max(0.0001);
                blade_angle = lerp(205.0, -150.0, k);
                lean = lerp(0.0, -16.0, k);
                arm_front = blade_angle;
            } else if t < atk.startup + atk.active {
                let k = (t - atk.startup) / atk.active.max(0.0001);
                let e2 = 1.0 - (1.0 - k) * (1.0 - k);
                blade_angle = lerp(-150.0, 50.0, e2);
                lean = lerp(-16.0, 22.0, e2);
                arm_front = blade_angle;
            } else {
                let k = (t - atk.startup - atk.active) / atk.recovery.max(0.0001);
                blade_angle = lerp(50.0, 205.0, k);
                lean = lerp(22.0, 0.0, k);
                arm_front = lerp(50.0, -25.0, k);
            }
            leg_split = 24.0;
            crouch = 3.0;
        }
        EState::Recover => {
            lean = 12.0;
            leg_split = 18.0;
            crouch = 5.0;
            arm_front = -15.0;
        }
        EState::Stagger => {
            lean = -20.0;
            arm_front = -105.0;
            arm_back = 250.0;
            crouch = 7.0;
        }
        EState::Dead => {}
    }

    let scale = if e.kind == EnemyKind::Brute { 1.16 } else { 1.0 };
    draw_figure(
        d, cx, feet, s, lean, crouch, leg_split, arm_front, arm_back, blade_angle,
        48.0 * scale, body, trim, Color::new(180, 150, 128, 255), false, show_trail,
        Color::new(210, 214, 232, 255),
    );

    // Telegraph: a warning flare during the windup so the swing is reactable.
    if e.telegraphing() {
        let c = e.center();
        let k = (e.state_time / stats_for(e.kind).attack.startup).clamp(0.0, 1.0);
        let a = (60.0 + 195.0 * k) as u8;
        d.draw_circle_v(Vector2::new(c.x, c.y - 52.0), 5.0 + k * 4.0, Color::new(255, 120, 96, a));
    }

    // Health pip above the head once damaged.
    if e.hp < e.max_hp {
        let w = 40.0 * scale;
        let x = cx - w * 0.5;
        let y = feet - cfg::ENEMY_H * scale - 16.0;
        d.draw_rectangle_rec(Rectangle::new(x, y, w, 4.0), Color::new(0, 0, 0, 170));
        d.draw_rectangle_rec(
            Rectangle::new(x, y, w * (e.hp / e.max_hp).clamp(0.0, 1.0), 4.0),
            cfg::C_DAMAGE,
        );
    }
}

// ---------------------------------------------------------------------------
// Shared figure drawing
// ---------------------------------------------------------------------------

fn draw_shadow<D: RaylibDraw>(d: &mut D, cx: f32, feet: f32, w: f32) {
    d.draw_ellipse(cx as i32, feet as i32, w * 0.55, 6.0, Color::new(0, 0, 0, 90));
}

/// Draws an articulated figure. Angles are in degrees, measured so that 0 points
/// along the facing direction; the caller passes them in facing-local space and
/// this function mirrors them.
#[allow(clippy::too_many_arguments)]
fn draw_figure<D: RaylibDraw>(
    d: &mut D,
    cx: f32,
    feet: f32,
    s: f32,
    lean: f32,
    crouch: f32,
    leg_split: f32,
    arm_front: f32,
    arm_back: f32,
    blade_angle: f32,
    blade_len: f32,
    body: Color,
    trim: Color,
    skin: Color,
    scarf: bool,
    trail: bool,
    blade_color: Color,
) {
    // Mirror every angle for a left-facing figure.
    let m = |a: f32| if s < 0.0 { 180.0 - a } else { a };

    let hip = Vector2::new(cx + lean * 0.25 * s, feet - 34.0 + crouch);
    let chest = Vector2::new(hip.x + lean * 0.45 * s, hip.y - 24.0);
    let head = Vector2::new(chest.x + lean * 0.3 * s, chest.y - 15.0);

    // --- Back limbs first (behind the torso) ---
    limb(d, hip, 32.0, 9.0, m(90.0 - leg_split * 0.5), Color::new(body.r / 2 + 10, body.g / 2 + 10, body.b / 2 + 14, 255));
    limb(d, chest, 26.0, 7.5, m(arm_back), Color::new(body.r / 2 + 10, body.g / 2 + 10, body.b / 2 + 14, 255));

    // --- Front leg ---
    limb(d, hip, 32.0, 9.5, m(90.0 + leg_split * 0.5), body);

    // --- Torso ---
    d.draw_rectangle_pro(
        Rectangle::new(hip.x, hip.y, 20.0, 30.0),
        Vector2::new(10.0, 28.0),
        lean * 0.6 * s,
        body,
    );
    // Sash across the chest.
    d.draw_rectangle_pro(
        Rectangle::new(chest.x, chest.y + 6.0, 21.0, 5.0),
        Vector2::new(10.5, 2.5),
        lean * 0.6 * s + 18.0 * s,
        trim,
    );

    // --- Head ---
    d.draw_circle_v(head, 9.5, body);
    // Face strip, offset toward the facing direction.
    d.draw_rectangle_pro(
        Rectangle::new(head.x + 3.0 * s, head.y - 1.0, 9.0, 5.0),
        Vector2::new(4.5, 2.5),
        lean * 0.4 * s,
        skin,
    );

    if scarf {
        // Trailing scarf: two tapering segments swept opposite the lean.
        let base = Vector2::new(head.x - 6.0 * s, head.y + 3.0);
        let a1 = m(168.0 - lean * 1.6);
        limb(d, base, 20.0, 5.5, a1, trim);
        let r1 = a1.to_radians();
        let tip = Vector2::new(base.x + r1.cos() * 20.0, base.y + r1.sin() * 20.0);
        limb(d, tip, 15.0, 4.0, m(150.0 - lean * 2.4), trim);
    }

    // --- Front arm and blade ---
    limb(d, chest, 24.0, 8.0, m(arm_front), body);
    let ar = m(arm_front).to_radians();
    let hand = Vector2::new(chest.x + ar.cos() * 24.0, chest.y + ar.sin() * 24.0);

    let br = m(blade_angle).to_radians();
    let tip = Vector2::new(hand.x + br.cos() * blade_len, hand.y + br.sin() * blade_len);
    // Guard.
    d.draw_circle_v(hand, 4.0, trim);
    // Blade: a bright core with a darker spine for readability at speed.
    d.draw_line_ex(hand, tip, 4.5, blade_color);
    d.draw_line_ex(hand, tip, 1.5, Color::new(255, 255, 255, 220));

    if trail {
        // Arc smear behind the blade sells the speed of the active frames.
        for i in 1..=4 {
            let back = m(blade_angle - 26.0 * i as f32 * s.signum()).to_radians();
            let t2 = Vector2::new(hand.x + back.cos() * blade_len, hand.y + back.sin() * blade_len);
            let a = (110 - i * 24).max(0) as u8;
            d.draw_line_ex(hand, t2, 3.0, Color::new(blade_color.r, blade_color.g, blade_color.b, a));
        }
    }
}

// ---------------------------------------------------------------------------
// Effects
// ---------------------------------------------------------------------------

pub struct Spark {
    pub pos: Vector2,
    pub vel: Vector2,
    pub life: f32,
    pub max_life: f32,
    pub color: Color,
    pub size: f32,
}

pub struct FloatText {
    pub pos: Vector2,
    pub life: f32,
    pub text: String,
    pub color: Color,
}

pub fn draw_sparks<D: RaylibDraw>(d: &mut D, sparks: &[Spark]) {
    for s in sparks {
        let k = (s.life / s.max_life).clamp(0.0, 1.0);
        let c = Color::new(s.color.r, s.color.g, s.color.b, (255.0 * k) as u8);
        d.draw_rectangle_rec(
            Rectangle::new(s.pos.x - s.size * 0.5, s.pos.y - s.size * 0.5, s.size * k, s.size * k),
            c,
        );
    }
}

pub fn draw_float_text<D: RaylibDraw>(d: &mut D, texts: &[FloatText]) {
    for t in texts {
        let k = (t.life / 0.9).clamp(0.0, 1.0);
        let c = Color::new(t.color.r, t.color.g, t.color.b, (255.0 * k) as u8);
        let w = measure_text(&t.text, 20);
        d.draw_text(&t.text, t.pos.x as i32 - w / 2, t.pos.y as i32, 20, c);
    }
}
