//! Calibration overlay for matching the game's proportions to reference art.
//!
//! Supply your own reference image with `--reference <file>` and it is drawn in
//! world space next to the character, with live controls for scale, offset and
//! opacity. Combined with the grid and the hitbox outlines, this turns "the
//! figure looks too stocky" into a number you can paste into `config.rs`.
//!
//! Reference images are read from disk at runtime and never written into the
//! build; `.gitignore` keeps them out of the repository.

use crate::config as cfg;
use crate::enemy::Enemy;
use crate::player::Player;
use raylib::prelude::*;

pub struct Calibration {
    pub enabled: bool,
    pub grid: bool,
    pub outlines: bool,
    pub reference: Option<Texture2D>,
    pub ref_path: String,
    pub ref_scale: f32,
    pub ref_offset: Vector2,
    pub ref_alpha: f32,
}

/// World-space grid spacing, in pixels.
const GRID: f32 = 16.0;
/// Heavier line every N cells, to make counting easy.
const GRID_MAJOR: i32 = 4;

impl Calibration {
    pub fn new(rl: &mut RaylibHandle, thread: &RaylibThread, path: Option<&str>) -> Calibration {
        let mut reference = None;
        let mut ref_path = String::new();
        if let Some(p) = path {
            ref_path = p.to_string();
            match rl.load_texture(thread, p) {
                Ok(t) => reference = Some(t),
                // A missing reference is not fatal: the rest of the overlay is
                // still useful on its own.
                Err(e) => eprintln!("calibrate: could not load reference '{p}': {e}"),
            }
        }
        Calibration {
            enabled: true,
            grid: true,
            outlines: true,
            reference,
            ref_path,
            ref_scale: 1.0,
            ref_offset: Vector2::new(140.0, 0.0),
            ref_alpha: 0.65,
        }
    }

    pub fn handle_input(&mut self, rl: &RaylibHandle) {
        use KeyboardKey::*;
        if rl.is_key_pressed(KEY_F1) {
            self.enabled = !self.enabled;
        }
        if !self.enabled {
            return;
        }
        if rl.is_key_pressed(KEY_G) {
            self.grid = !self.grid;
        }
        if rl.is_key_pressed(KEY_O) {
            self.outlines = !self.outlines;
        }

        // Hold for fine adjustment.
        let fine = rl.is_key_down(KEY_LEFT_CONTROL);
        let nudge = if fine { 1.0 } else { 5.0 };
        let scale_step = if fine { 0.005 } else { 0.02 };

        if rl.is_key_down(KEY_LEFT) {
            self.ref_offset.x -= nudge;
        }
        if rl.is_key_down(KEY_RIGHT) {
            self.ref_offset.x += nudge;
        }
        if rl.is_key_down(KEY_UP) {
            self.ref_offset.y -= nudge;
        }
        if rl.is_key_down(KEY_DOWN) {
            self.ref_offset.y += nudge;
        }
        if rl.is_key_down(KEY_LEFT_BRACKET) {
            self.ref_scale = (self.ref_scale - scale_step).max(0.05);
        }
        if rl.is_key_down(KEY_RIGHT_BRACKET) {
            self.ref_scale += scale_step;
        }
        if rl.is_key_down(KEY_MINUS) {
            self.ref_alpha = (self.ref_alpha - 0.02).max(0.0);
        }
        if rl.is_key_down(KEY_EQUAL) {
            self.ref_alpha = (self.ref_alpha + 0.02).min(1.0);
        }

        // Dump the tuned numbers so they can be pasted straight into config.rs.
        if rl.is_key_pressed(KEY_P) {
            println!("--- calibration ---");
            println!("reference     : {}", self.ref_path);
            println!("ref_scale     : {:.3}", self.ref_scale);
            println!("ref_offset    : ({:.1}, {:.1})", self.ref_offset.x, self.ref_offset.y);
            if let Some(t) = &self.reference {
                println!(
                    "ref drawn size: {:.1} x {:.1} px",
                    t.width as f32 * self.ref_scale,
                    t.height as f32 * self.ref_scale
                );
            }
            println!("PLAYER_W/H    : {:.1} x {:.1}", cfg::PLAYER_W, cfg::PLAYER_H);
            println!("PLAYER_DRAW_H : {:.1}", cfg::PLAYER_DRAW_H);
            println!("ENEMY_W/H     : {:.1} x {:.1}", cfg::ENEMY_W, cfg::ENEMY_H);
        }
    }

    /// World-space layer: grid, reference image and collision outlines.
    pub fn draw_world<D: RaylibDraw>(&self, d: &mut D, player: &Player, enemies: &[Enemy], cam: Vector2) {
        if !self.enabled {
            return;
        }

        if self.grid {
            self.draw_grid(d, cam);
        }

        // The reference sits beside the character, aligned to the same ground
        // line, so heights can be compared directly.
        if let Some(t) = &self.reference {
            let w = t.width as f32 * self.ref_scale;
            let h = t.height as f32 * self.ref_scale;
            let x = player.rect.x + self.ref_offset.x;
            let y = player.rect.y + player.rect.height - h + self.ref_offset.y;
            let a = (255.0 * self.ref_alpha) as u8;
            d.draw_texture_pro(
                t,
                Rectangle::new(0.0, 0.0, t.width as f32, t.height as f32),
                Rectangle::new(x, y, w, h),
                Vector2::zero(),
                0.0,
                Color::new(255, 255, 255, a),
            );
            d.draw_rectangle_lines_ex(Rectangle::new(x, y, w, h), 1.0, Color::new(120, 200, 255, 160));
        }

        if self.outlines {
            // Player collision box.
            d.draw_rectangle_lines_ex(player.rect, 1.0, Color::new(90, 220, 140, 220));
            // Drawn silhouette extent, which intentionally overhangs the box.
            let feet = player.rect.y + player.rect.height;
            d.draw_rectangle_lines_ex(
                Rectangle::new(
                    player.rect.x - 6.0,
                    feet - cfg::PLAYER_DRAW_H,
                    player.rect.width + 12.0,
                    cfg::PLAYER_DRAW_H,
                ),
                1.0,
                Color::new(90, 220, 140, 90),
            );
            // Ground line through the character's feet.
            d.draw_line_ex(
                Vector2::new(cam.x - cfg::SCREEN_W as f32 * 0.5, feet),
                Vector2::new(cam.x + cfg::SCREEN_W as f32 * 0.5, feet),
                1.0,
                Color::new(90, 220, 140, 70),
            );

            // Live attack hitbox: the number that decides whether reach feels right.
            if let Some(hb) = player.attack_hitbox() {
                d.draw_rectangle_rec(hb, Color::new(255, 90, 90, 60));
                d.draw_rectangle_lines_ex(hb, 1.0, Color::new(255, 90, 90, 230));
            }

            for e in enemies {
                if !e.alive() {
                    continue;
                }
                d.draw_rectangle_lines_ex(e.rect, 1.0, Color::new(220, 180, 90, 180));
                if let Some(hb) = e.attack_hitbox() {
                    d.draw_rectangle_rec(hb, Color::new(255, 140, 60, 60));
                    d.draw_rectangle_lines_ex(hb, 1.0, Color::new(255, 140, 60, 230));
                }
            }
        }
    }

    fn draw_grid<D: RaylibDraw>(&self, d: &mut D, cam: Vector2) {
        let hw = cfg::SCREEN_W as f32 * 0.5;
        let hh = cfg::SCREEN_H as f32 * 0.5;
        let x0 = ((cam.x - hw) / GRID).floor() * GRID;
        let x1 = cam.x + hw;
        let y0 = ((cam.y - hh) / GRID).floor() * GRID;
        let y1 = cam.y + hh;

        let minor = Color::new(120, 140, 190, 26);
        let major = Color::new(120, 140, 190, 64);

        let mut x = x0;
        while x < x1 {
            let is_major = ((x / GRID).round() as i32) % GRID_MAJOR == 0;
            d.draw_line_ex(
                Vector2::new(x, y0),
                Vector2::new(x, y1),
                1.0,
                if is_major { major } else { minor },
            );
            x += GRID;
        }
        let mut y = y0;
        while y < y1 {
            let is_major = ((y / GRID).round() as i32) % GRID_MAJOR == 0;
            d.draw_line_ex(
                Vector2::new(x0, y),
                Vector2::new(x1, y),
                1.0,
                if is_major { major } else { minor },
            );
            y += GRID;
        }
    }

    /// Screen-space layer: the numeric readout.
    pub fn draw_overlay<D: RaylibDraw>(&self, d: &mut D) {
        if !self.enabled {
            return;
        }
        let x = cfg::SCREEN_W - 330;
        let y = 90;
        d.draw_rectangle(x - 12, y - 12, 320, 190, Color::new(10, 11, 20, 225));
        d.draw_rectangle_lines_ex(
            Rectangle::new((x - 12) as f32, (y - 12) as f32, 320.0, 190.0),
            1.0,
            cfg::C_UI_LINE,
        );
        d.draw_text("CALIBRATION  (F1)", x, y, 16, cfg::C_ACCENT);

        let lines = [
            format!("grid {}   outlines {}", on(self.grid), on(self.outlines)),
            format!("collision  {:.0} x {:.0}", cfg::PLAYER_W, cfg::PLAYER_H),
            format!("silhouette {:.0} tall", cfg::PLAYER_DRAW_H),
            format!("grid cell  {:.0} px", GRID),
            match &self.reference {
                Some(t) => format!(
                    "ref {:.0}x{:.0} @ {:.2}",
                    t.width as f32 * self.ref_scale,
                    t.height as f32 * self.ref_scale,
                    self.ref_scale
                ),
                None => "ref  (none loaded)".to_string(),
            },
            format!("offset {:.0},{:.0}  alpha {:.2}", self.ref_offset.x, self.ref_offset.y, self.ref_alpha),
        ];
        for (i, l) in lines.iter().enumerate() {
            d.draw_text(l, x, y + 26 + i as i32 * 17, 13, cfg::C_TEXT);
        }
        d.draw_text(
            "G grid  O box  arrows move  [ ] scale",
            x,
            y + 140,
            11,
            cfg::C_TEXT_DIM,
        );
        d.draw_text("- = alpha   CTRL fine   P print", x, y + 156, 11, cfg::C_TEXT_DIM);
    }
}

fn on(b: bool) -> &'static str {
    if b {
        "on"
    } else {
        "off"
    }
}
