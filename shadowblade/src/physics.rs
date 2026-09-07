//! Swept AABB movement against the stage.
//!
//! Axes are resolved separately (X then Y), which is the standard trick for
//! keeping a body from catching on the seams between adjacent platforms.

use crate::world::Platform;
use raylib::prelude::Rectangle;

pub struct MoveResult {
    pub on_ground: bool,
    pub hit_wall: bool,
    pub hit_ceiling: bool,
}

/// Moves `rect` by (dx, dy), resolving collisions. `drop_through` disables
/// one-way platforms for this step so the player can fall off a ledge.
pub fn move_and_collide(
    rect: &mut Rectangle,
    dx: f32,
    dy: f32,
    platforms: &[Platform],
    drop_through: bool,
) -> MoveResult {
    let mut res = MoveResult { on_ground: false, hit_wall: false, hit_ceiling: false };

    // --- X axis ---
    rect.x += dx;
    for p in platforms {
        // One-way platforms never block horizontal movement.
        if p.one_way {
            continue;
        }
        if intersects(*rect, p.rect) {
            if dx > 0.0 {
                rect.x = p.rect.x - rect.width;
            } else if dx < 0.0 {
                rect.x = p.rect.x + p.rect.width;
            }
            res.hit_wall = true;
        }
    }

    // --- Y axis ---
    let prev_bottom = rect.y + rect.height;
    rect.y += dy;
    for p in platforms {
        if p.one_way {
            // Only solid when falling and when the body was fully above the
            // surface at the start of the step.
            if drop_through || dy <= 0.0 || prev_bottom > p.rect.y + 1.0 {
                continue;
            }
        }
        if intersects(*rect, p.rect) {
            if dy > 0.0 {
                rect.y = p.rect.y - rect.height;
                res.on_ground = true;
            } else if dy < 0.0 {
                if p.one_way {
                    continue;
                }
                rect.y = p.rect.y + p.rect.height;
                res.hit_ceiling = true;
            }
        }
    }

    res
}

/// Strict overlap test: touching edges do not count, which avoids a body being
/// reported as colliding with the floor it is resting on.
fn intersects(a: Rectangle, b: Rectangle) -> bool {
    a.x < b.x + b.width && a.x + a.width > b.x && a.y < b.y + b.height && a.y + a.height > b.y
}

/// True when a body standing on `rect` would be supported by some platform.
pub fn grounded(rect: Rectangle, platforms: &[Platform]) -> bool {
    let probe = Rectangle::new(rect.x, rect.y + rect.height, rect.width, 2.0);
    platforms.iter().any(|p| {
        if p.one_way {
            // Supported only if resting essentially on the surface.
            let top = p.rect.y;
            let bottom = rect.y + rect.height;
            (bottom - top).abs() < 3.0 && probe.x < p.rect.x + p.rect.width && probe.x + probe.width > p.rect.x
        } else {
            intersects(probe, p.rect)
        }
    })
}
