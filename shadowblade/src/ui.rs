//! HUD, menus and overlays. Drawn in screen space, after the camera pass.

use crate::config as cfg;
use crate::player::Player;
use raylib::prelude::*;
use crate::util::measure_text;

pub const SKILL_NAMES: [&str; 4] = ["Keen Edge", "Swift", "Ironhide", "Siphon"];
pub const SKILL_DESCS: [&str; 4] = [
    "+12% attack damage per rank",
    "+7% speed, -10% roll cost per rank",
    "-9% damage taken per rank",
    "Heal on finisher hits",
];

fn bar<D: RaylibDraw>(d: &mut D, x: f32, y: f32, w: f32, h: f32, frac: f32, fill: Color, label: &str) {
    d.draw_rectangle_rec(Rectangle::new(x - 2.0, y - 2.0, w + 4.0, h + 4.0), cfg::C_UI_BG);
    d.draw_rectangle_rec(Rectangle::new(x, y, w, h), Color::new(28, 30, 44, 255));
    d.draw_rectangle_rec(Rectangle::new(x, y, w * frac.clamp(0.0, 1.0), h), fill);
    d.draw_rectangle_lines_ex(Rectangle::new(x - 2.0, y - 2.0, w + 4.0, h + 4.0), 1.0, cfg::C_UI_LINE);
    d.draw_text(label, x as i32 + 6, y as i32 + (h as i32 - 10) / 2, 10, cfg::C_TEXT);
}

pub fn draw_hud<D: RaylibDraw>(
    d: &mut D,
    p: &Player,
    wave: usize,
    waves: usize,
    remaining: usize,
    stage: &str,
    time: f32,
) {
    bar(d, 24.0, 24.0, 300.0, 20.0, p.hp / p.max_hp, cfg::C_DAMAGE, "HP");
    d.draw_text(
        &format!("{} / {}", p.hp.max(0.0).round() as i32, p.max_hp.round() as i32),
        250,
        29,
        10,
        cfg::C_TEXT,
    );

    bar(d, 24.0, 52.0, 220.0, 12.0, p.stamina / p.max_stamina, cfg::C_STAMINA, "STA");
    bar(d, 24.0, 72.0, 220.0, 8.0, p.xp as f32 / p.xp_needed as f32, cfg::C_XP, "");

    d.draw_text(&format!("LV {}", p.level), 24, 88, 18, cfg::C_ACCENT);
    d.draw_text(&format!("XP {}/{}", p.xp, p.xp_needed), 92, 92, 12, cfg::C_TEXT_DIM);

    if p.skill_points > 0 {
        let pulse = 160 + ((time * 4.0).sin() * 90.0) as i32;
        d.draw_text(
            &format!("{} SKILL POINT(S)  -  press TAB", p.skill_points),
            24,
            112,
            14,
            Color::new(255, 196, 96, pulse.clamp(0, 255) as u8),
        );
    }

    // Stage / wave status, right aligned.
    let title = format!("{}", stage);
    let tw = measure_text(&title, 18);
    d.draw_text(&title, cfg::SCREEN_W - tw - 24, 24, 18, cfg::C_TEXT);
    let sub = format!("Wave {}/{}   -   {} left", wave.min(waves), waves, remaining);
    let sw = measure_text(&sub, 14);
    d.draw_text(&sub, cfg::SCREEN_W - sw - 24, 48, 14, cfg::C_TEXT_DIM);
}

pub fn draw_controls<D: RaylibDraw>(d: &mut D) {
    let lines = [
        "A / D  move      W or SPACE  jump      S+jump  drop through",
        "J  attack (chain x3)      K  block / parry      L or SHIFT  roll",
        "TAB  skills      R  restart      ESC  quit",
    ];
    let y0 = cfg::SCREEN_H - 76;
    d.draw_rectangle(0, y0 - 10, cfg::SCREEN_W, 86, Color::new(10, 11, 20, 150));
    for (i, l) in lines.iter().enumerate() {
        d.draw_text(l, 24, y0 + i as i32 * 20, 14, cfg::C_TEXT_DIM);
    }
}

pub fn draw_skill_menu<D: RaylibDraw>(d: &mut D, p: &Player, cursor: usize) {
    d.draw_rectangle(0, 0, cfg::SCREEN_W, cfg::SCREEN_H, Color::new(6, 7, 14, 205));

    let w = 560;
    let h = 330;
    let x = (cfg::SCREEN_W - w) / 2;
    let y = (cfg::SCREEN_H - h) / 2;
    d.draw_rectangle(x, y, w, h, cfg::C_UI_BG);
    d.draw_rectangle_lines_ex(
        Rectangle::new(x as f32, y as f32, w as f32, h as f32),
        2.0,
        cfg::C_UI_LINE,
    );

    d.draw_text("SKILLS", x + 24, y + 20, 26, cfg::C_ACCENT);
    d.draw_text(
        &format!("Points available: {}", p.skill_points),
        x + 24,
        y + 54,
        14,
        cfg::C_TEXT_DIM,
    );

    let ranks = [
        p.skills.keen_edge,
        p.skills.swift,
        p.skills.ironhide,
        p.skills.siphon,
    ];

    for i in 0..4 {
        let ry = y + 90 + i as i32 * 52;
        let selected = i == cursor;
        if selected {
            d.draw_rectangle(x + 16, ry - 8, w - 32, 44, Color::new(40, 44, 70, 255));
        }
        let name_color = if selected { cfg::C_ACCENT } else { cfg::C_TEXT };
        d.draw_text(SKILL_NAMES[i], x + 28, ry, 18, name_color);
        d.draw_text(SKILL_DESCS[i], x + 28, ry + 20, 12, cfg::C_TEXT_DIM);

        // Rank pips.
        for r in 0..5 {
            let px = x + w - 40 - (4 - r) * 18;
            let filled = r < ranks[i];
            let c = if filled { cfg::C_XP } else { Color::new(46, 48, 68, 255) };
            d.draw_rectangle(px, ry + 4, 12, 12, c);
        }
    }

    d.draw_text(
        "W/S select    ENTER spend point    TAB close",
        x + 24,
        y + h - 32,
        13,
        cfg::C_TEXT_DIM,
    );
}

pub fn draw_center_banner<D: RaylibDraw>(d: &mut D, title: &str, subtitle: &str, color: Color) {
    d.draw_rectangle(0, 0, cfg::SCREEN_W, cfg::SCREEN_H, Color::new(6, 7, 14, 190));
    let tw = measure_text(title, 60);
    d.draw_text(title, (cfg::SCREEN_W - tw) / 2, cfg::SCREEN_H / 2 - 70, 60, color);
    let sw = measure_text(subtitle, 20);
    d.draw_text(
        subtitle,
        (cfg::SCREEN_W - sw) / 2,
        cfg::SCREEN_H / 2 + 10,
        20,
        cfg::C_TEXT_DIM,
    );
}

/// Brief wave-start callout.
pub fn draw_wave_toast<D: RaylibDraw>(d: &mut D, text: &str, life: f32) {
    let k = (life / 1.6).clamp(0.0, 1.0);
    let a = (255.0 * k.min(1.0)) as u8;
    let tw = measure_text(text, 34);
    d.draw_text(
        text,
        (cfg::SCREEN_W - tw) / 2,
        150,
        34,
        Color::new(cfg::C_ACCENT.r, cfg::C_ACCENT.g, cfg::C_ACCENT.b, a),
    );
}
