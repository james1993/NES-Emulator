/* ===========================================================================
   art.c -- every pixel in this game is drawn here, procedurally.

   The look aims at 2004 Flash vector art: flat shapes, one dark outline pass,
   a single darker side for shading, desaturated earth-and-steel palettes, and
   backgrounds built from layered silhouettes over a gradient sky.

   Characters are segmented puppets -- head, torso, two arms with hands, two
   legs with feet, plus weapon/shield/helm attachments -- posed by keyframed
   joint angles.  That is how the original built its fighters, and it is what
   makes a hand-animated look possible without a single image file.
   =========================================================================== */
#include "game.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#define DEG (PI / 180.0f)

/* ------------------------------------------------------------- primitives */

Color art_shade(Color c, float f)
{
    float r = c.r * f, g = c.g * f, b = c.b * f;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;
    return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, c.a };
}

static Color alpha(Color c, float a)
{
    c.a = (unsigned char)(c.a * a);
    return c;
}

/* A soft contact shadow under the feet.  A single hard ellipse reads as a
   black blob against the floor, so this lays a wide faint pool under a
   smaller, darker core. */
static void ground_shadow(Vector2 at, float w, float h)
{
    DrawEllipse((int)at.x, (int)at.y + 3, w * 1.25f, h * 1.25f, alpha(C_INK, 0.10f));
    DrawEllipse((int)at.x, (int)at.y + 2, w,         h,         alpha(C_INK, 0.16f));
    DrawEllipse((int)at.x, (int)at.y + 1, w * 0.62f, h * 0.62f, alpha(C_INK, 0.20f));
}


/* Winding-agnostic triangle: the shape helpers below build polygons in either
   direction depending on the pose, so just emit both windings. */
static void tri(Vector2 a, Vector2 b, Vector2 c, Color col)
{
    DrawTriangle(a, b, c, col);
    DrawTriangle(a, c, b, col);
}

static void quad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color col)
{
    tri(a, b, c, col);
    tri(a, c, d, col);
}

static Vector2 v2(float x, float y) { return (Vector2){ x, y }; }

static Vector2 vadd(Vector2 a, Vector2 b) { return v2(a.x + b.x, a.y + b.y); }

static Vector2 polar(Vector2 o, float ang, float len)
{
    return v2(o.x + cosf(ang) * len, o.y + sinf(ang) * len);
}

/* A tapered limb: quad body plus round caps, so joints never show a seam. */
static void limb(Vector2 a, Vector2 b, float wa, float wb, Color col)
{
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.0001f) return;
    float nx = -dy / len, ny = dx / len;
    quad(v2(a.x + nx * wa, a.y + ny * wa), v2(b.x + nx * wb, b.y + ny * wb),
         v2(b.x - nx * wb, b.y - ny * wb), v2(a.x - nx * wa, a.y - ny * wa), col);
    DrawCircleV(a, wa, col);
    DrawCircleV(b, wb, col);
}

/* Outlined limb: dark pass first, colour pass inside it. */
static void limb_o(Vector2 a, Vector2 b, float wa, float wb, Color col, float ow)
{
    limb(a, b, wa + ow, wb + ow, C_INK);
    limb(a, b, wa, wb, col);
}

static void blob_o(Vector2 c, float rx, float ry, Color col, float ow)
{
    DrawEllipse((int)c.x, (int)c.y, rx + ow, ry + ow, C_INK);
    DrawEllipse((int)c.x, (int)c.y, rx, ry, col);
}

/* Convex polygon with an ink outline. */
static void poly_o(const Vector2 *pts, int n, Color col, float ow)
{
    Vector2 mid = { 0, 0 };
    for (int i = 0; i < n; i++) { mid.x += pts[i].x; mid.y += pts[i].y; }
    mid.x /= n; mid.y /= n;
    if (ow > 0) {
        for (int i = 0; i < n; i++) {
            Vector2 a = pts[i], b = pts[(i + 1) % n];
            Vector2 ea = v2(a.x + (a.x - mid.x) * 0.0f, a.y + (a.y - mid.y) * 0.0f);
            (void)ea;
            DrawLineEx(a, b, ow * 2.0f, C_INK);
            DrawCircleV(a, ow, C_INK);
        }
    }
    for (int i = 1; i + 1 < n; i++) tri(pts[0], pts[i], pts[i + 1], col);
}

static unsigned hash2(int x, int y)
{
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
static float hashf(int x, int y, int s)
{
    return (hash2(x * 31 + s, y * 17 - s) % 10000) / 10000.0f;
}

/* ------------------------------------------------------------------ poses */

typedef struct {
    float rootX, rootY;      /* whole-body offset                          */
    float lean;              /* torso rotation, radians                    */
    float head;              /* head tilt                                  */
    float shA, elA;          /* front arm: shoulder, elbow                 */
    float shB, elB;          /* back arm                                   */
    float hipA, kneeA;       /* front leg                                  */
    float hipB, kneeB;       /* back leg                                   */
    float crouch;            /* hip drop                                   */
    float weapon;            /* extra weapon rotation                      */
} Pose;

static Pose pose_lerp(Pose a, Pose b, float t)
{
    Pose r;
    float *pa = (float *)&a, *pb = (float *)&b, *pr = (float *)&r;
    int n = sizeof(Pose) / sizeof(float);
    for (int i = 0; i < n; i++) pr[i] = pa[i] + (pb[i] - pa[i]) * t;
    return r;
}

/* Base standing pose: weight on the back foot, blade hand low and ready. */
static Pose pose_stand(float t)
{
    Pose p = { 0 };
    float breathe = sinf(t * 2.0f);
    p.rootY  = breathe * 1.2f;
    p.lean   = -4 * DEG + breathe * 1.5f * DEG;
    p.head   = 2 * DEG - breathe * 1.0f * DEG;
    p.shA    = 50 * DEG;  p.elA = 26 * DEG + breathe * 3 * DEG;
    p.shB    = 108 * DEG; p.elB = -30 * DEG;
    p.hipA   = 78 * DEG;  p.kneeA = 8 * DEG;
    p.hipB   = 100 * DEG; p.kneeB = -6 * DEG;
    p.crouch = 2.0f;
    p.weapon = -46 * DEG;      /* blade held forward and low, not vertical */
    return p;
}

static Pose pose_walk(float ph)
{
    Pose p = { 0 };
    float s = sinf(ph * 2 * PI), c = cosf(ph * 2 * PI);
    p.rootY  = -fabsf(c) * 2.5f;
    p.lean   = -8 * DEG;
    p.head   = 3 * DEG;
    p.shA    = 70 * DEG - s * 32 * DEG;  p.elA = 34 * DEG;
    p.shB    = 110 * DEG + s * 32 * DEG; p.elB = -26 * DEG;
    p.hipA   = 90 * DEG + s * 34 * DEG;  p.kneeA = fmaxf(0, -s) * 40 * DEG;
    p.hipB   = 90 * DEG - s * 34 * DEG;  p.kneeB = fmaxf(0, s) * 40 * DEG;
    p.crouch = 1.5f;
    p.weapon = -40 * DEG;
    return p;
}

/* Attack: wind up behind the head, then drive through with the hips. */
static Pose pose_attack(float ph)
{
    Pose wind = { 0 }, strike = { 0 }, recov = { 0 };
    wind.rootX = -7; wind.lean = 14 * DEG; wind.head = -4 * DEG;
    wind.shA = -52 * DEG; wind.elA = 70 * DEG;
    wind.shB = 150 * DEG; wind.elB = -40 * DEG;
    wind.hipA = 70 * DEG; wind.kneeA = 16 * DEG;
    wind.hipB = 108 * DEG; wind.kneeB = -18 * DEG;
    wind.crouch = 5; wind.weapon = -30 * DEG;

    strike.rootX = 26; strike.lean = -26 * DEG; strike.head = 10 * DEG;
    strike.shA = 96 * DEG; strike.elA = 4 * DEG;
    strike.shB = 60 * DEG; strike.elB = -60 * DEG;
    strike.hipA = 112 * DEG; strike.kneeA = 4 * DEG;
    strike.hipB = 62 * DEG;  strike.kneeB = -34 * DEG;
    strike.crouch = 8; strike.weapon = 34 * DEG;

    recov = pose_stand(0);
    recov.rootX = 8;

    if (ph < 0.35f) {
        float k = ph / 0.35f; k = k * k;
        return pose_lerp(pose_stand(0), wind, k);
    } else if (ph < 0.52f) {
        float k = (ph - 0.35f) / 0.17f;
        k = 1 - (1 - k) * (1 - k) * (1 - k);
        return pose_lerp(wind, strike, k);
    }
    float k = (ph - 0.52f) / 0.48f;
    return pose_lerp(strike, recov, k * k);
}

/* Cast: both hands pulled to the centre, then thrust forward. */
static Pose pose_cast(float ph)
{
    Pose gather = { 0 }, push = { 0 };
    gather.lean = 8 * DEG; gather.head = -6 * DEG;
    gather.shA = 20 * DEG; gather.elA = 96 * DEG;
    gather.shB = 160 * DEG; gather.elB = -96 * DEG;
    gather.hipA = 80 * DEG; gather.hipB = 100 * DEG;
    gather.crouch = 6;

    push.rootX = 10; push.lean = -14 * DEG; push.head = 6 * DEG;
    push.shA = 84 * DEG; push.elA = 6 * DEG;
    push.shB = 96 * DEG; push.elB = -10 * DEG;
    push.hipA = 104 * DEG; push.hipB = 74 * DEG; push.kneeB = -20 * DEG;
    push.crouch = 3;

    if (ph < 0.45f) return pose_lerp(pose_stand(0), gather, ph / 0.45f);
    if (ph < 0.62f) {
        float k = (ph - 0.45f) / 0.17f;
        return pose_lerp(gather, push, 1 - (1 - k) * (1 - k) * (1 - k));
    }
    return pose_lerp(push, pose_stand(0), (ph - 0.62f) / 0.38f);
}

static Pose pose_block(float ph)
{
    Pose b = { 0 };
    b.rootX = -4; b.lean = 12 * DEG; b.head = -8 * DEG;
    b.shA = 40 * DEG;  b.elA = 74 * DEG;
    b.shB = 62 * DEG;  b.elB = 78 * DEG;      /* shield arm up and across    */
    b.hipA = 72 * DEG; b.kneeA = 22 * DEG;
    b.hipB = 112 * DEG; b.kneeB = -20 * DEG;
    b.crouch = 8;
    float k = ph < 0.25f ? ph / 0.25f : 1.0f;
    return pose_lerp(pose_stand(0), b, k);
}

static Pose pose_hit(float ph)
{
    Pose h = { 0 };
    h.rootX = -14; h.lean = 26 * DEG; h.head = -18 * DEG;
    h.shA = 20 * DEG; h.elA = 30 * DEG;
    h.shB = 150 * DEG; h.elB = -20 * DEG;
    h.hipA = 66 * DEG; h.kneeA = 24 * DEG;
    h.hipB = 116 * DEG; h.kneeB = -28 * DEG;
    h.crouch = 7;
    if (ph < 0.25f) return pose_lerp(pose_stand(0), h, ph / 0.25f);
    return pose_lerp(h, pose_stand(0), (ph - 0.25f) / 0.75f);
}

static Pose pose_heal(float ph)
{
    Pose h = { 0 };
    float s = sinf(ph * PI);
    h.rootY = -s * 4;
    h.lean = -6 * DEG; h.head = -10 * DEG - s * 6 * DEG;
    h.shA = -10 * DEG - s * 30 * DEG; h.elA = 60 * DEG;
    h.shB = 190 * DEG + s * 30 * DEG; h.elB = -60 * DEG;
    h.hipA = 84 * DEG; h.hipB = 96 * DEG;
    h.crouch = 2 - s * 2;
    return h;
}

static Pose pose_die(float ph)
{
    Pose d = { 0 };
    float k = ph > 1 ? 1 : ph;
    d.rootX = -10 * k; d.rootY = 26 * k;
    d.lean = 74 * DEG * k; d.head = -30 * DEG * k;
    d.shA = 130 * DEG * k; d.elA = 20 * DEG;
    d.shB = 160 * DEG * k; d.elB = -10 * DEG;
    d.hipA = 90 * DEG + 40 * DEG * k; d.kneeA = 60 * DEG * k;
    d.hipB = 90 * DEG - 30 * DEG * k; d.kneeB = 50 * DEG * k;
    d.crouch = 26 * k;
    return d;
}

static Pose pose_victory(float t)
{
    Pose p = pose_stand(t);
    float s = sinf(t * 3.0f);
    p.shA = -70 * DEG + s * 8 * DEG; p.elA = 20 * DEG;
    p.rootY = -fabsf(sinf(t * 6.0f)) * 3;
    p.head = -6 * DEG;
    return p;
}

static Pose pose_for(AnimState st, float t)
{
    switch (st) {
    case ANIM_WALK:       return pose_walk(fmodf(t * 1.6f, 1.0f));
    case ANIM_ATTACK:     return pose_attack(t > 1 ? 1 : t);
    case ANIM_CAST:       return pose_cast(t > 1 ? 1 : t);
    case ANIM_BLOCK:      return pose_block(t > 1 ? 1 : t);
    case ANIM_BLOCKBREAK: { Pose p = pose_block(1.0f); p.rootX -= 10 * sinf(t * 40); return p; }
    case ANIM_HIT:        return pose_hit(t > 1 ? 1 : t);
    case ANIM_MISS:       return pose_attack(t > 1 ? 1 : t);
    case ANIM_HEAL:       return pose_heal(t > 1 ? 1 : t);
    case ANIM_DIE:        return pose_die(t * 1.4f);
    case ANIM_DEAD:       return pose_die(1.0f);
    case ANIM_VICTORY:    return pose_victory(t);
    default:              return pose_stand(t);
    }
}

/* ------------------------------------------------------------- equipment */

void art_weapon_shape(int shape, Vector2 grip, float ang, float s, Color tint, Color edge)
{
    Vector2 tipv = polar(grip, ang, 0);
    (void)tipv;
    switch (shape) {
    case WEAP_KNIFE: {
        Vector2 tip = polar(grip, ang, 26 * s);
        limb_o(grip, tip, 2.4f * s, 0.8f * s, tint, 1.2f);
        limb_o(grip, polar(grip, ang + PI, 5 * s), 2.0f * s, 2.0f * s, C_INK2, 1.0f);
    } break;
    case WEAP_KATANA: {
        Vector2 mid = polar(grip, ang - 4 * DEG, 24 * s);
        Vector2 tip = polar(mid, ang + 6 * DEG, 26 * s);
        limb_o(grip, mid, 2.6f * s, 2.2f * s, tint, 1.3f);
        limb_o(mid, tip, 2.2f * s, 0.6f * s, tint, 1.3f);
        limb(grip, polar(grip, ang, 6 * s), 3.4f * s, 3.0f * s, edge);       /* tsuba */
        limb_o(grip, polar(grip, ang + PI, 11 * s), 2.2f * s, 2.0f * s, C_INK2, 1.0f);
    } break;
    case WEAP_BROAD: {
        Vector2 tip = polar(grip, ang, 46 * s);
        limb_o(grip, tip, 4.2f * s, 1.4f * s, tint, 1.4f);
        limb(polar(grip, ang + PI / 2, 6 * s), polar(grip, ang - PI / 2, 6 * s),
             1.8f * s, 1.8f * s, edge);
        limb_o(grip, polar(grip, ang + PI, 10 * s), 2.0f * s, 2.4f * s, C_INK2, 1.0f);
    } break;
    case WEAP_AXE: {
        Vector2 tip = polar(grip, ang, 42 * s);
        limb_o(grip, tip, 2.6f * s, 2.2f * s, C_BARK, 1.2f);
        Vector2 h = polar(grip, ang, 34 * s);
        Vector2 pts[4] = {
            polar(h, ang + 90 * DEG, 4 * s), polar(h, ang + 40 * DEG, 17 * s),
            polar(h, ang - 20 * DEG, 15 * s), polar(h, ang - 70 * DEG, 5 * s)
        };
        poly_o(pts, 4, tint, 1.4f);
    } break;
    case WEAP_STAFF: {
        Vector2 a = polar(grip, ang, 34 * s), b = polar(grip, ang + PI, 26 * s);
        limb_o(a, b, 2.2f * s, 2.0f * s, C_BARK, 1.2f);
        DrawCircleV(a, 6.0f * s, C_INK);
        DrawCircleV(a, 4.6f * s, tint);
        DrawCircleV(polar(a, ang - 40 * DEG, 1.4f * s), 2.0f * s, art_shade(tint, 1.6f));
    } break;
    case WEAP_SCYTHE: {
        Vector2 a = polar(grip, ang, 40 * s), b = polar(grip, ang + PI, 22 * s);
        limb_o(a, b, 2.4f * s, 2.0f * s, C_INK2, 1.2f);
        Vector2 pts[5] = { a, polar(a, ang - 60 * DEG, 22 * s), polar(a, ang - 100 * DEG, 30 * s),
                           polar(a, ang - 116 * DEG, 22 * s), polar(a, ang - 80 * DEG, 12 * s) };
        poly_o(pts, 5, tint, 1.3f);
    } break;
    case WEAP_CLAW: {
        for (int i = -1; i <= 1; i++) {
            Vector2 t = polar(grip, ang + i * 16 * DEG, 20 * s);
            limb_o(grip, t, 1.8f * s, 0.5f * s, tint, 1.0f);
        }
    } break;
    case WEAP_SPEAR: {
        Vector2 a = polar(grip, ang, 52 * s), b = polar(grip, ang + PI, 24 * s);
        limb_o(a, b, 2.0f * s, 1.8f * s, C_BARK, 1.1f);
        Vector2 pts[4] = { polar(a, ang, 14 * s), polar(a, ang + 100 * DEG, 5 * s),
                           polar(a, ang + PI, 6 * s), polar(a, ang - 100 * DEG, 5 * s) };
        poly_o(pts, 4, tint, 1.2f);
    } break;
    default: break;
    }
}

void art_shield_shape(int shape, Vector2 grip, float ang, float s, Color tint)
{
    Color dark = art_shade(tint, 0.62f);
    switch (shape) {
    case SHLD_WRIST:
        limb_o(polar(grip, ang + 90 * DEG, 7 * s), polar(grip, ang - 90 * DEG, 7 * s),
               3.4f * s, 3.4f * s, tint, 1.3f);
        break;
    case SHLD_BUCKLER:
        DrawCircleV(grip, 12.5f * s, C_INK);
        DrawCircleV(grip, 11 * s, tint);
        DrawCircleV(grip, 5 * s, dark);
        break;
    case SHLD_KITE: {
        Vector2 pts[5] = { polar(grip, ang + 90 * DEG, 17 * s), polar(grip, ang + 30 * DEG, 13 * s),
                           polar(grip, ang - 60 * DEG, 20 * s), polar(grip, ang - 130 * DEG, 13 * s),
                           polar(grip, ang + 150 * DEG, 13 * s) };
        poly_o(pts, 5, tint, 1.6f);
        DrawLineEx(pts[0], pts[2], 2.4f * s, dark);
    } break;
    case SHLD_TOWER: {
        Vector2 a = polar(grip, ang + 90 * DEG, 22 * s), b = polar(grip, ang - 90 * DEG, 22 * s);
        limb_o(a, b, 9 * s, 9 * s, tint, 1.6f);
        limb(a, b, 3 * s, 3 * s, dark);
    } break;
    case SHLD_BLADE: {
        Vector2 a = polar(grip, ang + 96 * DEG, 20 * s), b = polar(grip, ang - 84 * DEG, 16 * s);
        limb_o(a, b, 4 * s, 1.2f * s, tint, 1.3f);
        limb_o(grip, polar(grip, ang, 9 * s), 3 * s, 2 * s, dark, 1.0f);
    } break;
    case SHLD_SPIKE: {
        DrawCircleV(grip, 12.5f * s, C_INK);
        DrawCircleV(grip, 11 * s, tint);
        for (int i = 0; i < 6; i++) {
            float a = ang + i * 60 * DEG;
            limb_o(polar(grip, a, 8 * s), polar(grip, a, 18 * s), 2.4f * s, 0.4f * s, dark, 0.9f);
        }
    } break;
    default: break;
    }
}

static void draw_helm(int helm, Vector2 head, float r, float face, Color metal, Color cloth,
                      Color trim)
{
    switch (helm) {
    case HELM_BANDANA: {
        limb_o(v2(head.x - r, head.y - r * 0.35f), v2(head.x + r, head.y - r * 0.35f),
               r * 0.34f, r * 0.34f, trim, 1.2f);
        Vector2 tail = v2(head.x - face * r * 1.9f, head.y - r * 0.1f);
        limb(v2(head.x - face * r * 0.8f, head.y - r * 0.3f), tail, r * 0.26f, r * 0.1f, trim);
    } break;
    case HELM_HOOD: {
        Vector2 pts[6] = {
            v2(head.x - r * 1.25f, head.y + r * 0.5f), v2(head.x - r * 1.15f, head.y - r * 0.7f),
            v2(head.x - r * 0.2f, head.y - r * 1.5f), v2(head.x + r * 1.0f, head.y - r * 0.8f),
            v2(head.x + r * 1.2f, head.y + r * 0.45f), v2(head.x, head.y + r * 0.7f)
        };
        for (int i = 0; i < 6; i++) if (face < 0) pts[i].x = 2 * head.x - pts[i].x;
        poly_o(pts, 6, cloth, 1.6f);
        DrawEllipse((int)(head.x + face * r * 0.35f), (int)(head.y + r * 0.05f),
                    r * 0.62f, r * 0.5f, alpha(C_INK, 0.85f));
    } break;
    case HELM_CIRCLET:
        limb_o(v2(head.x - r * 0.95f, head.y - r * 0.45f), v2(head.x + r * 0.95f, head.y - r * 0.45f),
               r * 0.2f, r * 0.2f, metal, 1.1f);
        DrawCircleV(v2(head.x + face * r * 0.3f, head.y - r * 0.5f), r * 0.24f, trim);
        break;
    case HELM_HORNED:
        blob_o(v2(head.x, head.y - r * 0.35f), r * 1.05f, r * 0.85f, metal, 1.4f);
        for (int i = -1; i <= 1; i += 2) {
            Vector2 base = v2(head.x + i * r * 0.8f, head.y - r * 0.7f);
            limb_o(base, v2(base.x + i * r * 0.9f, base.y - r * 1.3f), r * 0.24f, r * 0.05f,
                   trim, 1.1f);
        }
        break;
    case HELM_FULL: {
        blob_o(v2(head.x, head.y - r * 0.15f), r * 1.15f, r * 1.1f, metal, 1.5f);
        DrawRectangleRec((Rectangle){ head.x + face * (-r * 0.2f) - r * 0.5f,
                                      head.y - r * 0.05f, r, r * 0.3f },
                         alpha(C_INK, 0.9f));
        limb_o(v2(head.x, head.y - r * 1.2f), v2(head.x, head.y - r * 1.9f),
               r * 0.16f, r * 0.05f, trim, 1.0f);
    } break;
    default: break;
    }
}

/* ------------------------------------------------------------ the puppet */

/* Draws one fighter.  `facing` is +1 (looking right) or -1 (looking left).
   Units are tuned so a scale of 1.0 gives a fighter about 150px tall.       */
void art_draw_puppet(const Combatant *c, Vector2 at, float facing, float t, float scale)
{
    const Look *lk = &c->look;
    float s = scale * lk->scale;
    Pose p = pose_for(c->anim, t);

    /* Ground shadow. */
    ground_shadow(at, 15 * s, 4.5f * s);

    float shake = c->shake > 0 ? sinf(t * 70.0f) * c->shake * 6.0f : 0.0f;
    Vector2 root = v2(at.x + (p.rootX * facing + shake) * s, at.y + (p.rootY + p.crouch) * s);

    bool wisp = (lk->body == BODY_WISP);
    bool brute = (lk->body == BODY_BRUTE);
    bool undead = (lk->body == BODY_UNDEAD);
    bool beast = (lk->body == BODY_BEAST);

    float torsoLen = brute ? 44 : (undead ? 38 : 40);
    float shoulderW = brute ? 20 : (undead ? 12 : 15);
    float limbW = brute ? 6.5f : (undead ? 3.2f : 5.0f);
    float legW = brute ? 8.0f : (undead ? 3.6f : 6.0f);
    float headR = brute ? 12.5f : (undead ? 10.0f : 11.0f);

    Color cloth = lk->cloth, dark = lk->clothDark, skin = lk->skin;

    /* Colour flash when struck. */
    if (c->flash > 0) {
        float f = c->flash;
        cloth = lerp_col(cloth, (Color){ 255, 210, 200, 255 }, f);
        dark  = lerp_col(dark, (Color){ 220, 160, 150, 255 }, f);
        skin  = lerp_col(skin, (Color){ 255, 220, 210, 255 }, f);
    }

    /* Joint frame. */
    float lean = p.lean * facing;
    Vector2 hip = root;
    Vector2 chest = polar(hip, -PI / 2 + lean, torsoLen * s);
    Vector2 neck = polar(chest, -PI / 2 + lean, 8 * s);
    Vector2 head = polar(neck, -PI / 2 + lean + p.head * facing, headR * s * 0.9f);

    Vector2 shoulderF = vadd(chest, v2(facing * shoulderW * 0.35f * s, 0));
    Vector2 shoulderB = vadd(chest, v2(-facing * shoulderW * 0.45f * s, 1 * s));

    /* Arm chains: angles are measured from +x, mirrored by facing. */
    float aA = (facing > 0 ? p.shA : PI - p.shA);
    float aB = (facing > 0 ? p.shB : PI - p.shB);
    Vector2 elbowA = polar(shoulderF, aA, 19 * s);
    Vector2 handA  = polar(elbowA, aA + p.elA * facing, 18 * s);
    Vector2 elbowB = polar(shoulderB, aB, 19 * s);
    Vector2 handB  = polar(elbowB, aB + p.elB * facing, 18 * s);

    float lA = (facing > 0 ? p.hipA : PI - p.hipA);
    float lB = (facing > 0 ? p.hipB : PI - p.hipB);
    Vector2 kneeA = polar(vadd(hip, v2(facing * 3 * s, 0)), lA, 21 * s);
    Vector2 footA = polar(kneeA, lA + p.kneeA * facing, 22 * s);
    Vector2 kneeB = polar(vadd(hip, v2(-facing * 3 * s, 0)), lB, 21 * s);
    Vector2 footB = polar(kneeB, lB + p.kneeB * facing, 22 * s);

    /* Aura for glowing enemies / mystics. */
    if (lk->glow) {
        float pulse = 0.5f + 0.5f * sinf(t * 3.0f);
        for (int i = 3; i >= 1; i--)
            DrawCircleV(v2(chest.x, chest.y - 4 * s), (26 + i * 9) * s,
                        alpha(lk->trim, 0.05f + 0.03f * pulse));
    }

    /* ---- back leg / back arm ---- */
    if (!wisp) {
        limb_o(vadd(hip, v2(-facing * 3 * s, 0)), kneeB, legW * s, legW * 0.8f * s,
               art_shade(dark, 0.85f), 1.5f);
        limb_o(kneeB, footB, legW * 0.8f * s, legW * 0.6f * s, art_shade(dark, 0.85f), 1.5f);
        limb_o(footB, vadd(footB, v2(facing * 9 * s, 0)), legW * 0.7f * s, legW * 0.5f * s,
               art_shade(C_INK2, 1.2f), 1.2f);
    }
    limb_o(shoulderB, elbowB, limbW * s, limbW * 0.82f * s, art_shade(cloth, 0.72f), 1.4f);
    limb_o(elbowB, handB, limbW * 0.82f * s, limbW * 0.6f * s, art_shade(skin, 0.78f), 1.4f);
    if (lk->shield != SHLD_NONE)
        art_shield_shape(lk->shield, handB, aB + p.elB * facing, s, lk->shieldTint);

    /* ---- torso ---- */
    if (wisp) {
        /* A hovering robe: no legs, a ragged tail that drifts. */
        Vector2 pts[7];
        float sway = sinf(t * 2.2f) * 6 * s;
        pts[0] = vadd(chest, v2(-shoulderW * 0.9f * s, 0));
        pts[1] = vadd(chest, v2(shoulderW * 0.9f * s, 0));
        pts[2] = vadd(hip, v2(shoulderW * 0.8f * s + sway * 0.4f, 10 * s));
        pts[3] = vadd(hip, v2(6 * s + sway, 30 * s));
        pts[4] = vadd(hip, v2(sway * 1.4f, 22 * s));
        pts[5] = vadd(hip, v2(-8 * s + sway, 32 * s));
        pts[6] = vadd(hip, v2(-shoulderW * 0.8f * s + sway * 0.4f, 10 * s));
        poly_o(pts, 7, cloth, 1.8f);
        limb(chest, hip, shoulderW * 0.7f * s, shoulderW * 0.55f * s, dark);
    } else {
        limb_o(hip, chest, shoulderW * 0.62f * s, shoulderW * 0.9f * s, cloth, 1.8f);
        /* shading pass down the back half */
        limb(vadd(hip, v2(-facing * 3 * s, 0)), vadd(chest, v2(-facing * 4 * s, 0)),
             shoulderW * 0.3f * s, shoulderW * 0.42f * s, dark);
        if (undead) {
            for (int i = 0; i < 3; i++) {
                float fy = 0.25f + i * 0.22f;
                Vector2 a = v2(chest.x + (hip.x - chest.x) * fy, chest.y + (hip.y - chest.y) * fy);
                limb(vadd(a, v2(-7 * s, 0)), vadd(a, v2(7 * s, 0)), 1.6f * s, 1.6f * s,
                     art_shade(skin, 0.9f));
            }
        }
        /* sash -- kept inside the torso's width, as above */
        limb(vadd(hip, v2(-shoulderW * 0.30f * s, -4 * s)),
             vadd(hip, v2(shoulderW * 0.30f * s, -2 * s)),
             3.2f * s, 3.2f * s, lk->trim);
        limb(vadd(hip, v2(-facing * 2 * s, 0)), vadd(hip, v2(-facing * 7 * s, 16 * s)),
             2.6f * s, 1.2f * s, lk->trim);
    }

    /* ---- head ---- */
    blob_o(head, headR * s, headR * 1.05f * s, skin, 1.6f);
    if (undead) {
        DrawEllipse((int)(head.x + facing * headR * 0.28f * s), (int)(head.y - headR * 0.1f * s),
                    headR * 0.3f * s, headR * 0.34f * s, C_INK);
        DrawEllipse((int)(head.x + facing * headR * 0.75f * s), (int)(head.y - headR * 0.05f * s),
                    headR * 0.22f * s, headR * 0.3f * s, C_INK);
        for (int i = 0; i < 3; i++)
            DrawRectangle((int)(head.x + facing * (headR * 0.3f + i * 4) * s),
                          (int)(head.y + headR * 0.55f * s), (int)(2.2f * s), (int)(4 * s), C_INK);
    } else if (wisp) {
        DrawEllipse((int)head.x, (int)head.y, headR * 1.1f * s, headR * 1.1f * s,
                    alpha(C_INK, 0.9f));
        for (int i = -1; i <= 1; i += 2)
            DrawCircleV(v2(head.x + facing * headR * 0.45f * s + i * 0.0f,
                           head.y - headR * 0.1f * s + i * headR * 0.3f * s),
                        headR * 0.17f * s, lk->trim);
    } else if (beast) {
        /* snout + mandibles */
        limb_o(head, polar(head, facing > 0 ? 0.1f : PI - 0.1f, headR * 1.5f * s),
               headR * 0.7f * s, headR * 0.3f * s, skin, 1.4f);
        DrawCircleV(v2(head.x + facing * headR * 0.4f * s, head.y - headR * 0.35f * s),
                    headR * 0.26f * s, C_INK);
    } else {
        /* hair mass and two ink eye slits */
        blob_o(v2(head.x - facing * headR * 0.18f * s, head.y - headR * 0.52f * s),
               headR * 1.02f * s, headR * 0.66f * s, lk->hair, 1.2f);
        DrawEllipse((int)(head.x + facing * headR * 0.34f * s), (int)(head.y - headR * 0.02f * s),
                    headR * 0.15f * s, headR * 0.2f * s, C_INK);
        DrawEllipse((int)(head.x + facing * headR * 0.76f * s), (int)(head.y + headR * 0.02f * s),
                    headR * 0.12f * s, headR * 0.18f * s, C_INK);
    }
    draw_helm(lk->helm, head, headR * s, facing, lk->metal, cloth, lk->trim);

    /* ---- front leg ---- */
    if (!wisp) {
        limb_o(vadd(hip, v2(facing * 3 * s, 0)), kneeA, legW * s, legW * 0.8f * s, dark, 1.5f);
        limb_o(kneeA, footA, legW * 0.8f * s, legW * 0.6f * s, dark, 1.5f);
        limb_o(footA, vadd(footA, v2(facing * 10 * s, 0)), legW * 0.75f * s, legW * 0.55f * s,
               C_INK2, 1.2f);
    }

    /* ---- front arm and weapon ---- */
    limb_o(shoulderF, elbowA, limbW * s, limbW * 0.82f * s, cloth, 1.4f);
    limb_o(elbowA, handA, limbW * 0.82f * s, limbW * 0.62f * s, skin, 1.4f);
    DrawCircleV(handA, limbW * 0.75f * s, art_shade(skin, 0.9f));

    if (lk->weapon != WEAP_NONE) {
        float wang = aA + p.elA * facing + p.weapon * facing + (facing > 0 ? -0.35f : 0.35f);
        art_weapon_shape(lk->weapon, handA, wang, s, lk->weaponTint,
                         art_shade(lk->weaponTint, 0.6f));
    }
}

/* ---------------------------------------------------- overworld walker */

/* The overworld uses the same puppet, seen from the side when moving
   left/right and from front/back when moving up/down. */
void art_draw_walker(const Look *lk, Vector2 at, int dir, float walkT, float scale)
{
    Combatant tmp;
    memset(&tmp, 0, sizeof tmp);
    tmp.look = *lk;
    tmp.alive = true;
    tmp.anim = (walkT > 0) ? ANIM_WALK : ANIM_STAND;
    tmp.animT = (walkT > 0) ? walkT : walkT * 0.0f;

    if (dir == 2 || dir == 3) {                        /* left / right      */
        art_draw_puppet(&tmp, at, dir == 3 ? 1.0f : -1.0f,
                        walkT > 0 ? walkT : GetTime() * 0.6f, scale);
        return;
    }

    /* Front/back view: a simpler symmetric stack of the same shapes. */
    float s = scale * lk->scale;
    bool back = (dir == 0);
    float ph = walkT > 0 ? fmodf(walkT * 1.6f, 1.0f) : 0.0f;
    float sw = sinf(ph * 2 * PI);
    float bob = walkT > 0 ? -fabsf(cosf(ph * 2 * PI)) * 2.5f * s : sinf((float)GetTime() * 2) * 1.0f * s;

    ground_shadow(at, 13 * s, 4.0f * s);
    Vector2 hip = v2(at.x, at.y + bob);
    Vector2 chest = v2(hip.x, hip.y - 40 * s);
    Vector2 head = v2(hip.x, hip.y - 58 * s);

    for (int i = -1; i <= 1; i += 2) {                  /* legs             */
        float off = i * 6 * s + (walkT > 0 ? i * sw * 5 * s : 0);
        limb_o(v2(hip.x + i * 5 * s, hip.y), v2(hip.x + off, hip.y + 24 * s),
               6 * s, 4.5f * s, i < 0 ? art_shade(lk->clothDark, 0.85f) : lk->clothDark, 1.5f);
        limb_o(v2(hip.x + off, hip.y + 24 * s), v2(hip.x + off + 1 * s, hip.y + 26 * s),
               4.5f * s, 4 * s, C_INK2, 1.2f);
    }
    for (int i = -1; i <= 1; i += 2) {                  /* arms             */
        float sway = walkT > 0 ? -i * sw * 8 * s : 0;
        limb_o(v2(chest.x + i * 11 * s, chest.y + 2 * s),
               v2(chest.x + i * 14 * s, chest.y + 20 * s + sway * 0.2f), 5 * s, 4 * s,
               lk->cloth, 1.4f);
        limb_o(v2(chest.x + i * 14 * s, chest.y + 20 * s + sway * 0.2f),
               v2(chest.x + i * 15 * s, chest.y + 34 * s + sway), 4 * s, 3.4f * s, lk->skin, 1.4f);
    }
    limb_o(hip, chest, 10 * s, 14 * s, lk->cloth, 1.8f);
    /* The sash sits inside the torso, not around it: the body is 10s to the
       half, so the band has to end where its round caps still land within
       that or it reads as a hoop hung off the waist. */
    limb(v2(hip.x - 5.4f * s, hip.y - 3 * s), v2(hip.x + 5.4f * s, hip.y - 3 * s),
         3.2f * s, 3.2f * s, lk->trim);
    blob_o(head, 11 * s, 11.5f * s, back ? lk->hair : lk->skin, 1.6f);
    if (!back) {
        blob_o(v2(head.x, head.y - 6 * s), 11.2f * s, 7 * s, lk->hair, 1.2f);
        for (int i = -1; i <= 1; i += 2)
            DrawEllipse((int)(head.x + i * 4.2f * s), (int)head.y, 1.7f * s, 2.2f * s, C_INK);
    }
    draw_helm(lk->helm, head, 11 * s, back ? -1.0f : 1.0f, lk->metal, lk->cloth, lk->trim);
    if (lk->weapon != WEAP_NONE && !back) {
        /* sheathed across the back when walking around town */
        limb_o(v2(hip.x - 10 * s, hip.y - 6 * s), v2(hip.x + 12 * s, hip.y - 26 * s),
               2.6f * s, 1.6f * s, art_shade(lk->weaponTint, 0.8f), 1.2f);
    }
}

/* ------------------------------------------------------------ scenery */

static void silhouette_hills(float baseY, float amp, float freq, float phase, Color col, float t)
{
    const int step = 16;
    for (int x = -step; x <= SCREEN_W + step; x += step) {
        float y0 = baseY + sinf((x + phase) * freq) * amp
                          + sinf((x + phase) * freq * 2.3f + 1.7f) * amp * 0.35f;
        float y1 = baseY + sinf((x + step + phase) * freq) * amp
                          + sinf((x + step + phase) * freq * 2.3f + 1.7f) * amp * 0.35f;
        quad(v2((float)x, y0), v2((float)(x + step), y1),
             v2((float)(x + step), SCREEN_H), v2((float)x, SCREEN_H), col);
    }
    (void)t;
}

static void draw_pine(Vector2 at, float h, Color col)
{
    limb(v2(at.x, at.y), v2(at.x, at.y - h * 0.25f), h * 0.035f, h * 0.03f, art_shade(col, 0.8f));
    for (int i = 0; i < 4; i++) {
        float f = i / 4.0f;
        float y = at.y - h * (0.2f + f * 0.72f);
        float w = h * (0.30f - f * 0.055f) * 1.1f;
        tri(v2(at.x - w, y), v2(at.x + w, y), v2(at.x, y - h * 0.30f), col);
    }
}

static void draw_broadleaf(Vector2 at, float h, Color col, Color trunk)
{
    limb(v2(at.x, at.y), v2(at.x - h * 0.06f, at.y - h * 0.55f), h * 0.05f, h * 0.035f, trunk);
    DrawCircleV(v2(at.x - h * 0.1f, at.y - h * 0.68f), h * 0.26f, col);
    DrawCircleV(v2(at.x + h * 0.16f, at.y - h * 0.60f), h * 0.22f, art_shade(col, 0.88f));
    DrawCircleV(v2(at.x - h * 0.02f, at.y - h * 0.86f), h * 0.20f, art_shade(col, 1.08f));
}

static void draw_torii(Vector2 at, float h, Color col)
{
    Color d = art_shade(col, 0.7f);
    limb(v2(at.x - h * 0.36f, at.y), v2(at.x - h * 0.32f, at.y - h), h * 0.06f, h * 0.05f, col);
    limb(v2(at.x + h * 0.36f, at.y), v2(at.x + h * 0.32f, at.y - h), h * 0.06f, h * 0.05f, col);
    limb(v2(at.x - h * 0.5f, at.y - h * 0.98f), v2(at.x + h * 0.5f, at.y - h * 0.98f),
         h * 0.05f, h * 0.05f, d);
    limb(v2(at.x - h * 0.38f, at.y - h * 0.78f), v2(at.x + h * 0.38f, at.y - h * 0.78f),
         h * 0.035f, h * 0.035f, d);
}

/* Battle stages: a gradient sky, parallax silhouettes, then a ground plane. */
void art_draw_battle_bg(int bgStyle, float t)
{
    int zone = bgStyle;
    Color skyTop, skyBot, far, mid, near, ground, groundDk;
    switch (zone) {
    case BG_ARENA2:                                   /* coastal flats      */
        skyTop = (Color){ 62, 84, 122, 255 }; skyBot = (Color){ 196, 150, 116, 255 };
        far = (Color){ 74, 88, 112, 255 }; mid = (Color){ 52, 62, 82, 255 };
        near = (Color){ 34, 40, 56, 255 };
        ground = (Color){ 138, 120, 92, 255 }; groundDk = (Color){ 96, 82, 62, 255 }; break;
    case BG_ARENA3:                                   /* dry rock           */
        skyTop = (Color){ 122, 108, 96, 255 }; skyBot = (Color){ 214, 176, 122, 255 };
        far = (Color){ 176, 142, 100, 255 }; mid = (Color){ 142, 112, 78, 255 };
        near = (Color){ 104, 80, 56, 255 };
        ground = (Color){ 190, 158, 108, 255 }; groundDk = (Color){ 142, 114, 76, 255 }; break;
    case BG_DARK:                                     /* the shadow realm   */
        skyTop = (Color){ 16, 12, 24, 255 }; skyBot = (Color){ 52, 30, 58, 255 };
        far = (Color){ 46, 28, 56, 255 }; mid = (Color){ 32, 20, 40, 255 };
        near = (Color){ 20, 13, 26, 255 };
        ground = (Color){ 46, 32, 52, 255 }; groundDk = (Color){ 30, 20, 36, 255 }; break;
    case BG_VILLAGE:
        skyTop = (Color){ 70, 92, 120, 255 }; skyBot = (Color){ 176, 168, 148, 255 };
        far = (Color){ 96, 108, 104, 255 }; mid = (Color){ 62, 76, 70, 255 };
        near = (Color){ 40, 50, 46, 255 };
        ground = (Color){ 130, 122, 96, 255 }; groundDk = (Color){ 92, 86, 68, 255 }; break;
    case BG_ARENA:
    default:                                          /* open ground        */
        skyTop = (Color){ 66, 88, 112, 255 }; skyBot = (Color){ 168, 178, 150, 255 };
        far = (Color){ 92, 112, 92, 255 }; mid = (Color){ 58, 78, 60, 255 };
        near = (Color){ 36, 52, 40, 255 };
        ground = (Color){ 106, 124, 84, 255 }; groundDk = (Color){ 74, 90, 60, 255 }; break;
    }

    const float HORIZON = 400;
    DrawRectangleGradientV(0, 0, SCREEN_W, (int)HORIZON + 20, skyTop, skyBot);

    /* sun or moon */
    if (zone == BG_DARK) {
        for (int i = 0; i < 26; i++) {
            float x = hashf(i, 3, 11) * SCREEN_W, y = hashf(i, 7, 13) * HORIZON * 0.8f;
            float al = 0.25f + 0.25f * sinf(t * 1.5f + i);
            DrawCircleV(v2(x, y), 1.6f, alpha(C_VIOLET, al));
        }
    } else {
        DrawCircleV(v2(SCREEN_W * 0.74f, HORIZON * 0.36f), 46, alpha(RAYWHITE, 0.10f));
        DrawCircleV(v2(SCREEN_W * 0.74f, HORIZON * 0.36f), 30,
                    alpha((Color){ 250, 226, 178, 255 }, 0.55f));
    }

    /* far layer */
    if (zone == BG_ARENA3 || zone == BG_ARENA2)
        silhouette_hills(HORIZON - 70, 26, 0.006f, t * 2, far, t);
    else
        silhouette_hills(HORIZON - 90, 44, 0.004f, 40, far, t);

    /* mid layer: what stands on the skyline depends on the stage */
    silhouette_hills(HORIZON - 40, 20, 0.009f, 200, mid, t);
    if (zone == BG_ARENA || zone == BG_VILLAGE) {
        for (int i = 0; i < 9; i++) {
            float x = 60 + i * 148 + sinf(i * 2.1f) * 30;
            draw_pine(v2(x, HORIZON - 26), 120 + hashf(i, 1, 5) * 60, near);
        }
        if (zone == BG_VILLAGE) draw_torii(v2(SCREEN_W * 0.18f, HORIZON - 20), 150, near);
    } else if (zone == BG_DARK) {
        for (int i = 0; i < 10; i++) {
            float x = 40 + i * 132;
            float h = 130 + hashf(i, 4, 7) * 90;
            limb(v2(x, HORIZON - 10), v2(x + sinf(i) * 22, HORIZON - 10 - h), 7, 2, near);
            for (int b = 0; b < 3; b++) {
                float by = HORIZON - 10 - h * (0.5f + b * 0.18f);
                limb(v2(x + sinf(i) * 10, by), v2(x + sinf(i) * 10 + (b % 2 ? 26 : -26), by - 20),
                     3, 1, near);
            }
        }
    } else if (zone == BG_ARENA2) {
        for (int i = 0; i < 5; i++) {
            float x = 120 + i * 260;
            limb(v2(x, HORIZON - 20), v2(x + 14, HORIZON - 110), 6, 4, near);
            for (int f = 0; f < 5; f++) {
                float ang = -PI / 2 + (f - 2) * 0.45f;
                limb(v2(x + 14, HORIZON - 110), polar(v2(x + 14, HORIZON - 110), ang, 42), 4, 1, near);
            }
        }
    } else if (zone == BG_ARENA3) {
        for (int i = 0; i < 6; i++) {
            float x = 100 + i * 220;
            float h = 60 + hashf(i, 6, 2) * 40;
            limb(v2(x, HORIZON - 16), v2(x, HORIZON - 16 - h), 9, 8, near);
            limb(v2(x, HORIZON - 16 - h * 0.6f), v2(x - 24, HORIZON - 16 - h * 0.9f), 5, 4, near);
        }
    }

    /* ground plane, with a lighter strip where the fighters stand */
    DrawRectangleGradientV(0, (int)HORIZON - 10, SCREEN_W, SCREEN_H - (int)HORIZON + 10,
                           ground, groundDk);
    DrawRectangle(0, (int)HORIZON - 12, SCREEN_W, 4, alpha(C_INK, 0.25f));
    for (int i = 0; i < 60; i++) {
        float x = hashf(i, 11, 2) * SCREEN_W;
        float y = HORIZON + 20 + hashf(i, 13, 4) * (SCREEN_H - HORIZON - 40);
        float w = 12 + hashf(i, 17, 6) * 40;
        DrawEllipse((int)x, (int)y, w, 2.4f, alpha(groundDk, 0.5f));
    }
    if (zone == BG_ARENA2) {                        /* surf line            */
        for (int i = 0; i < 3; i++) {
            float y = HORIZON - 6 + i * 5 + sinf(t * 1.4f + i) * 2;
            DrawRectangle(0, (int)y, SCREEN_W, 2, alpha(RAYWHITE, 0.25f - i * 0.06f));
        }
    }
    /* vignette */
    DrawRectangleGradientV(0, 0, SCREEN_W, 120, alpha(C_INK, 0.55f), alpha(C_INK, 0.0f));
    DrawRectangleGradientV(0, SCREEN_H - 140, SCREEN_W, 140, alpha(C_INK, 0.0f),
                           alpha(C_INK, 0.6f));
}

/* ------------------------------------------------------------- map tiles */

/* Scenery.  Positions and footprints come from the original's display list
   (src/scenery_table.h); every shape below is drawn here, from nothing. */
/* An item's icon.  The original draws a little picture of the thing; these
   are shapes of the same families -- a blade, a guard, a helm, a robe, a
   flask -- so a slot reads as an object rather than a coloured square. */
void art_draw_item_icon(int def, Rectangle r)
{
    if (def < 0 || def >= ITEM_COUNT) return;
    const ItemDef *it = &ITEMS[def];
    /* Every weapon, shield, armour and helm has a frame of its own in the
       original's part clips, so the icon can wear that item's real colour
       instead of a tint of ours. */
    Color c = it->tint;
    {
        Look tmp = { 0 };
        if (data_dress(&tmp, it->name)) c = tmp.cloth;
    }
    Color d = art_shade(c, 0.6f), l = art_shade(c, 1.25f);
    float cx = r.x + r.width * 0.5f, cy = r.y + r.height * 0.5f;
    float w = r.width, h = r.height;

    switch (it->type) {
    case ITEM_WEAPON: {                       /* blade on a hilt, tip up */
        float bl = h * 0.52f;
        DrawLineEx(v2(cx, cy + h * 0.22f), v2(cx, cy - bl), w * 0.13f, l);
        DrawLineEx(v2(cx, cy + h * 0.22f), v2(cx, cy - bl), w * 0.06f, c);
        DrawLineEx(v2(cx - w * 0.16f, cy + h * 0.20f),
                   v2(cx + w * 0.16f, cy + h * 0.20f), w * 0.09f, d);
        DrawLineEx(v2(cx, cy + h * 0.22f), v2(cx, cy + h * 0.36f), w * 0.10f, d);
    } break;
    case ITEM_SHIELD:                          /* a kite guard */
        DrawTriangle(v2(cx, cy + h * 0.34f), v2(cx - w * 0.26f, cy - h * 0.16f),
                     v2(cx + w * 0.26f, cy - h * 0.16f), c);
        DrawRectangleRec((Rectangle){ cx - w * 0.26f, cy - h * 0.30f, w * 0.52f, h * 0.16f }, l);
        DrawLineEx(v2(cx, cy - h * 0.26f), v2(cx, cy + h * 0.30f), 1.5f, d);
        break;
    case ITEM_HELM:                            /* a domed helm */
        DrawCircleV(v2(cx, cy + h * 0.02f), w * 0.27f, c);
        DrawRectangleRec((Rectangle){ cx - w * 0.29f, cy + h * 0.02f, w * 0.58f, h * 0.12f }, d);
        DrawLineEx(v2(cx, cy - h * 0.26f), v2(cx, cy + h * 0.02f), 2.0f, l);
        break;
    case ITEM_ARMOUR:                          /* a robe / cuirass */
        DrawRectangleRec((Rectangle){ cx - w * 0.24f, cy - h * 0.24f, w * 0.48f, h * 0.50f }, c);
        DrawTriangle(v2(cx - w * 0.24f, cy - h * 0.24f), v2(cx - w * 0.36f, cy - h * 0.04f),
                     v2(cx - w * 0.24f, cy + h * 0.02f), d);
        DrawTriangle(v2(cx + w * 0.24f, cy - h * 0.24f), v2(cx + w * 0.24f, cy + h * 0.02f),
                     v2(cx + w * 0.36f, cy - h * 0.04f), d);
        DrawLineEx(v2(cx, cy - h * 0.20f), v2(cx, cy + h * 0.24f), 1.5f, l);
        break;
    case ITEM_RELIC:                           /* a ring */
        DrawCircleLinesV(v2(cx, cy), w * 0.22f, c);
        DrawCircleLinesV(v2(cx, cy), w * 0.22f - 1, c);
        DrawCircleV(v2(cx, cy - w * 0.22f), w * 0.07f, l);
        break;
    default:                                   /* a stoppered flask */
        DrawRectangleRec((Rectangle){ cx - w * 0.07f, cy - h * 0.30f, w * 0.14f, h * 0.16f }, d);
        DrawCircleV(v2(cx, cy + h * 0.08f), w * 0.22f, c);
        DrawRectangleRec((Rectangle){ cx - w * 0.16f, cy - h * 0.16f, w * 0.32f, h * 0.16f }, c);
        DrawCircleV(v2(cx - w * 0.07f, cy + h * 0.02f), w * 0.06f, l);
        break;
    }
}

void art_draw_scenery(const Prop *pr, const Zone *z, float t)
{
    Rectangle r = { pr->x, pr->y, pr->w, pr->h };
    float cx = r.x + r.width * 0.5f, by = r.y + r.height;
    /* Each object carries the two dominant fills of its own shapes in the
       original, so the palette here is its, not ours. */
    Color wood     = pr->col;
    Color woodDark = pr->colDark;
    Color stone    = pr->col;
    Color leaf     = pr->col;

    switch (pr->kind) {
    case PROP_WATER:
        DrawRectangleRec(r, pr->colDark);
        for (int i = 0; i < 4; i++) {
            float yy = r.y + r.height * (0.2f + 0.2f * i) + sinf(t * 1.4f + i) * 2.0f;
            DrawRectangle((int)(r.x + 6), (int)yy, (int)(r.width - 12), 2,
                          alpha(pr->col, 0.55f));
        }
        DrawRectangleLinesEx(r, 1.5f, alpha(C_INK, 0.4f));
        break;
    case PROP_GENERIC:
        /* Flat fill and a thin edge -- the original's shapes are flat colour
           with a dark outline, and a shaded band would invent depth it has not
           got (most of these are floor inlays and wall panels seen head on). */
        DrawRectangleRec(r, pr->col);
        DrawRectangleLinesEx(r, 1.5f, alpha(C_INK, 0.35f));
        break;
    case PROP_DOORWAY:
        DrawRectangleRec(r, art_shade(stone, 0.75f));
        DrawRectangleRec((Rectangle){ r.x + r.width * 0.16f, r.y + r.height * 0.14f,
                                      r.width * 0.68f, r.height * 0.86f }, (Color){ 18, 14, 20, 255 });
        DrawRectangleRec((Rectangle){ r.x - 3, r.y, r.width + 6, r.height * 0.15f }, wood);
        DrawRectangleLinesEx(r, 2, alpha(C_INK, 0.45f));
        break;
    case PROP_TORCH: {
        float fx = cx, fy = r.y + r.height * 0.34f;
        DrawRectangleRec((Rectangle){ cx - 2.5f, fy, 5, r.height * 0.62f },
                         (Color){ 86, 63, 44, 255 });
        float f = 0.75f + 0.25f * sinf(t * 7.3f + pr->x * 0.05f);
        DrawCircleV(v2(fx, fy), r.width * 0.34f * f, alpha(pr->col, 0.30f));
        DrawCircleV(v2(fx, fy), r.width * 0.19f * f, pr->col);          /* 255,204,0 */
        DrawCircleV(v2(fx, fy - 3), r.width * 0.10f * f, pr->colDark);  /* the highlight */
    } break;
    case PROP_PILLAR:
        /* The original's side pillars are a flat dark column against the wall. */
        DrawRectangleRec(r, art_shade(pr->col, 0.42f));
        DrawRectangleRec((Rectangle){ r.x, r.y, r.width * 0.26f, r.height },
                         art_shade(pr->col, 0.55f));
        DrawRectangleLinesEx(r, 1.5f, alpha(C_INK, 0.4f));
        break;
    case PROP_PLANT: {
        /* A sprite's bounds are the union over all of its frames, so the box
           overstates the silhouette; anchor a slim cluster at its foot. */
        float sw = r.width * 0.30f, sh = r.height * 0.72f;
        for (int i = 0; i < 4; i++) {
            float ox = cx + (i - 1.5f) * sw * 0.34f;
            float ty = by - sh * (0.72f + 0.14f * (i % 3));
            DrawLineEx(v2(cx, by), v2(ox, ty), 2.4f, (Color){ 150, 132, 74, 255 });
            for (int k = 0; k < 3; k++) {
                float t2 = 0.42f + k * 0.22f;
                float lx = cx + (ox - cx) * t2, ly = by + (ty - by) * t2;
                DrawEllipse((int)(lx + (i % 2 ? 5 : -5)), (int)ly, 5.5f, 2.6f,
                            art_shade(leaf, 0.86f + 0.05f * k));
            }
        }
    } break;
    case PROP_BAMBOO: {
        float sw = r.width * 0.42f;
        DrawRectangleRec((Rectangle){ cx - sw * 0.5f, r.y, sw, r.height }, (Color){ 176, 154, 84, 255 });
        for (float sy = r.y + 8; sy < by; sy += 16)
            DrawLineEx(v2(cx - sw * 0.5f, sy), v2(cx + sw * 0.5f, sy), 1.6f, (Color){ 96, 78, 34, 255 });
        DrawEllipse((int)(cx + sw * 1.4f), (int)(r.y + r.height * 0.24f), 6, 2.6f, leaf);
        DrawEllipse((int)(cx - sw * 1.4f), (int)(r.y + r.height * 0.46f), 6, 2.6f, art_shade(leaf, 0.85f));
    } break;
    case PROP_CRATE:
        DrawRectangleRec(r, wood);
        DrawRectangleLinesEx(r, 2, woodDark);
        DrawLineEx(v2(r.x, r.y), v2(r.x + r.width, by), 1.8f, woodDark);
        DrawLineEx(v2(r.x + r.width, r.y), v2(r.x, by), 1.8f, woodDark);
        break;
    case PROP_COUNTER:
        DrawRectangleRec((Rectangle){ r.x, r.y + r.height * 0.22f, r.width, r.height * 0.78f }, woodDark);
        DrawRectangleRec((Rectangle){ r.x - 2, r.y, r.width + 4, r.height * 0.26f }, wood);
        DrawRectangleLinesEx(r, 1.5f, alpha(C_INK, 0.35f));
        break;
    case PROP_SHELF:
        DrawRectangleRec(r, wood);
        DrawRectangleLinesEx(r, 1.5f, woodDark);
        break;
    case PROP_LAMP:
        DrawRectangleRec((Rectangle){ cx - 2, r.y + r.height * 0.35f, 4, r.height * 0.65f }, woodDark);
        DrawCircleV(v2(cx, r.y + r.height * 0.28f), r.width * 0.44f,
                    alpha((Color){ 255, 214, 140, 255 }, 0.28f));
        DrawRectangleRec((Rectangle){ r.x, r.y, r.width, r.height * 0.34f },
                         (Color){ 226, 178, 118, 255 });
        break;
    case PROP_WINDOW:
        DrawRectangleRec(r, (Color){ 46, 52, 66, 255 });
        DrawRectangleLinesEx(r, 4, wood);
        DrawLineEx(v2(cx, r.y), v2(cx, by), 3, wood);
        DrawLineEx(v2(r.x, r.y + r.height * 0.5f), v2(r.x + r.width, r.y + r.height * 0.5f), 3, wood);
        break;
    case PROP_URN:
        DrawEllipse((int)cx, (int)(r.y + r.height * 0.62f), r.width * 0.46f, r.height * 0.38f,
                    (Color){ 108, 126, 150, 255 });
        DrawRectangleRec((Rectangle){ cx - r.width * 0.20f, r.y, r.width * 0.40f, r.height * 0.34f },
                         (Color){ 92, 108, 132, 255 });
        break;
    case PROP_ROCKS:
        for (int i = 0; i < 4; i++) {
            float rx = r.x + r.width * (0.18f + 0.22f * i);
            float ry = by - r.height * (0.16f + 0.10f * (i % 2));
            DrawEllipse((int)rx, (int)ry, r.width * 0.15f, r.height * 0.12f, stone);
        }
        break;
    case PROP_GLOW:
        DrawEllipse((int)cx, (int)(r.y + r.height * 0.5f), r.width * 0.5f, r.height * 0.5f,
                    alpha((Color){ 255, 226, 150, 255 }, 0.16f));
        break;
    case PROP_SHADOW:
        DrawEllipse((int)cx, (int)(r.y + r.height * 0.5f), r.width * 0.5f, r.height * 0.5f,
                    alpha(C_INK, 0.22f));
        break;
    case PROP_EXITSIGN:
        DrawCircleV(v2(cx, r.y + r.height * 0.5f), r.width * 0.62f,
                    alpha((Color){ 255, 214, 120, 255 }, 0.18f));
        break;
    case PROP_EXITARROW: {
        float g = 0.7f + 0.3f * sinf(t * 3.1f);
        DrawTriangle(v2(cx, r.y - 7), v2(cx - 6, r.y + 4), v2(cx + 6, r.y + 4),
                     alpha((Color){ 255, 204, 0, 255 }, g));
    } break;
    default:
        DrawRectangleRec(r, alpha(stone, 0.55f));
        DrawRectangleLinesEx(r, 1.2f, alpha(C_INK, 0.25f));
        break;
    }
}

void art_draw_tile(int tile, int px, int py, int size, const Zone *z, int wx, int wy)
{
    float h = hashf(wx, wy, 1);
    float h2 = hashf(wx, wy, 2);
    Rectangle r = { (float)px, (float)py, (float)size, (float)size };
    Color base = z->ground;

    switch (tile) {
    case T_OCCUPIED:   /* blocked by scenery; the scenery itself is drawn later */
    case T_GRASS:
        base = lerp_col(z->ground, z->groundDark, h * 0.5f);
        DrawRectangleRec(r, base);
        for (int i = 0; i < 3; i++) {
            float gx = px + hashf(wx * 3 + i, wy, 5) * size;
            float gy = py + hashf(wx, wy * 3 + i, 6) * size;
            DrawLineEx(v2(gx, gy), v2(gx + (h2 - 0.5f) * 3, gy - 4 - h * 3),
                       1.4f, art_shade(base, 0.78f));
        }
        break;
    case T_PATH:
        DrawRectangleRec(r, lerp_col(z->propA, z->propB, h * 0.6f));
        for (int i = 0; i < 4; i++) {
            float gx = px + hashf(wx + i, wy, 8) * size;
            float gy = py + hashf(wx, wy + i, 9) * size;
            DrawCircleV(v2(gx, gy), 1.2f + h2, alpha(C_INK, 0.10f));
        }
        break;
    case T_FLOOR:
        DrawRectangleRec(r, lerp_col(z->propA, z->propB, ((wx + wy) & 1) ? 0.25f : 0.0f));
        DrawRectangleLinesEx(r, 1, alpha(C_INK, 0.13f));
        break;
    case T_MAT:
        DrawRectangleRec(r, (Color){ 156, 132, 88, 255 });
        for (int i = 0; i < size; i += 5)
            DrawLine(px + i, py, px + i, py + size, alpha(C_INK, 0.10f));
        DrawRectangleLinesEx(r, 2, (Color){ 96, 76, 48, 255 });
        break;
    case T_SAND:
        DrawRectangleRec(r, lerp_col((Color){ 202, 174, 124, 255 },
                                     (Color){ 176, 148, 104, 255 }, h));
        DrawEllipse(px + size / 2, py + (int)(h2 * size), size * 0.35f, 1.6f,
                    alpha((Color){ 150, 124, 86, 255 }, 0.7f));
        break;
    case T_SNOW:
        DrawRectangleRec(r, lerp_col((Color){ 224, 232, 242, 255 },
                                     (Color){ 196, 208, 224, 255 }, h));
        if (h2 > 0.7f) DrawCircleV(v2(px + h * size, py + h2 * size), 2, alpha(RAYWHITE, 0.8f));
        break;
    case T_WATER: {
        float t = (float)GetTime();
        Color w1 = (Color){ 58, 92, 122, 255 }, w2 = (Color){ 42, 70, 100, 255 };
        DrawRectangleRec(r, lerp_col(w1, w2, 0.5f + 0.5f * sinf(t * 1.2f + wx * 0.6f + wy * 0.4f)));
        DrawLineEx(v2(px + 4, py + size * 0.4f + sinf(t * 2 + wx) * 2),
                   v2(px + size - 4, py + size * 0.4f + cosf(t * 2 + wy) * 2), 1.6f,
                   alpha(RAYWHITE, 0.20f));
    } break;
    case T_LAVA: {
        float t = (float)GetTime();
        Color a = (Color){ 168, 62, 30, 255 }, b = (Color){ 226, 130, 44, 255 };
        DrawRectangleRec(r, lerp_col(a, b, 0.5f + 0.5f * sinf(t * 2 + wx + wy)));
        DrawCircleV(v2(px + size * 0.5f, py + size * 0.5f), 3 + sinf(t * 3 + wx) * 2,
                    alpha((Color){ 255, 214, 140, 255 }, 0.5f));
    } break;
    case T_WALL: {
        DrawRectangleRec(r, (Color){ 92, 84, 76, 255 });
        DrawRectangle(px, py, size, size / 3, (Color){ 108, 100, 90, 255 });
        DrawRectangleLinesEx(r, 2, (Color){ 54, 48, 44, 255 });
        DrawRectangle(px, py + size - 5, size, 5, alpha(C_INK, 0.35f));
    } break;
    case T_DOOR:
        DrawRectangleRec(r, (Color){ 92, 84, 76, 255 });
        DrawRectangleRec((Rectangle){ r.x + 4, r.y + 2, r.width - 8, r.height - 2 },
                         (Color){ 74, 52, 36, 255 });
        DrawCircleV(v2(px + size * 0.72f, py + size * 0.55f), 2.2f, C_GOLD);
        break;
    case T_TREE:
        DrawRectangleRec(r, lerp_col(z->ground, z->groundDark, 0.4f));
        if (z->bgStyle == BG_DARK || z->bgStyle == BG_ARENA3)
            draw_pine(v2(px + size * 0.5f, py + size * 0.95f), size * 1.5f,
                      z->bgStyle == BG_DARK ? (Color){ 52, 46, 52, 255 }
                                            : (Color){ 66, 92, 82, 255 });
        else
            draw_broadleaf(v2(px + size * 0.5f, py + size * 0.95f), size * 1.4f,
                           (Color){ 62, 92, 58, 255 }, C_BARK);
        break;
    case T_ROCK:
        DrawRectangleRec(r, lerp_col(z->ground, z->groundDark, 0.4f));
        {
            Vector2 c = v2(px + size * 0.5f, py + size * 0.62f);
            Vector2 pts[5] = { v2(c.x - size * 0.34f, c.y + size * 0.22f),
                               v2(c.x - size * 0.26f, c.y - size * 0.18f),
                               v2(c.x + size * 0.06f, c.y - size * 0.32f),
                               v2(c.x + size * 0.34f, c.y - size * 0.02f),
                               v2(c.x + size * 0.26f, c.y + size * 0.22f) };
            poly_o(pts, 5, (Color){ 118, 112, 106, 255 }, 1.6f);
            tri(pts[1], pts[2], v2(c.x, c.y + size * 0.1f), (Color){ 146, 140, 132, 255 });
        }
        break;
    case T_PORTAL: {
        float t = (float)GetTime();
        DrawRectangleRec(r, lerp_col(z->ground, z->groundDark, 0.5f));
        for (int i = 3; i >= 0; i--) {
            float rad = size * (0.18f + i * 0.09f) + sinf(t * 2 + i) * 2;
            DrawCircleV(v2(px + size * 0.5f, py + size * 0.5f), rad,
                        alpha(C_VIOLET, 0.10f + 0.08f * (4 - i)));
        }
        DrawCircleV(v2(px + size * 0.5f, py + size * 0.5f), size * 0.16f,
                    alpha(C_KI2, 0.7f + 0.3f * sinf(t * 4)));
    } break;
    case T_VOID:
    default:
        DrawRectangleRec(r, C_INK);
        break;
    }
}

void art_draw_prop(int kind, Vector2 at, float scale, Color a, Color b)
{
    float s = scale;
    switch (kind) {
    case 0:  /* crate */
        DrawRectangleRec((Rectangle){ at.x - 14 * s, at.y - 26 * s, 28 * s, 26 * s },
                         (Color){ 122, 92, 58, 255 });
        DrawRectangleLinesEx((Rectangle){ at.x - 14 * s, at.y - 26 * s, 28 * s, 26 * s }, 2,
                             (Color){ 74, 54, 34, 255 });
        DrawLineEx(v2(at.x - 14 * s, at.y - 26 * s), v2(at.x + 14 * s, at.y),
                   2, (Color){ 74, 54, 34, 255 });
        break;
    case 1:  /* urn */
        blob_o(v2(at.x, at.y - 14 * s), 12 * s, 15 * s, a, 1.6f);
        limb_o(v2(at.x - 6 * s, at.y - 28 * s), v2(at.x + 6 * s, at.y - 28 * s),
               3 * s, 3 * s, b, 1.2f);
        break;
    case 2:  /* lantern on a post */
        limb_o(v2(at.x, at.y), v2(at.x, at.y - 40 * s), 2.4f * s, 2.0f * s, C_BARK, 1.2f);
        DrawRectangleRec((Rectangle){ at.x - 8 * s, at.y - 58 * s, 16 * s, 18 * s },
                         (Color){ 216, 190, 130, 255 });
        DrawRectangleLinesEx((Rectangle){ at.x - 8 * s, at.y - 58 * s, 16 * s, 18 * s }, 2,
                             (Color){ 70, 50, 34, 255 });
        DrawCircleV(v2(at.x, at.y - 49 * s), 16 * s, alpha((Color){ 255, 214, 140, 255 }, 0.13f));
        break;
    case 3:  /* banner */
        limb_o(v2(at.x, at.y), v2(at.x, at.y - 62 * s), 2.2f * s, 1.8f * s, C_BARK, 1.2f);
        {
            float w = sinf((float)GetTime() * 2 + at.x) * 3;
            Vector2 pts[4] = { v2(at.x + 2 * s, at.y - 60 * s), v2(at.x + 20 * s + w, at.y - 56 * s),
                               v2(at.x + 18 * s + w, at.y - 24 * s), v2(at.x + 2 * s, at.y - 28 * s) };
            poly_o(pts, 4, a, 1.4f);
        }
        break;
    case 4:  /* bamboo cluster */
        for (int i = -1; i <= 1; i++) {
            float x = at.x + i * 8 * s;
            limb_o(v2(x, at.y), v2(x + i * 3 * s, at.y - (54 + i * 9) * s), 3 * s, 2.2f * s,
                   (Color){ 108, 138, 76, 255 }, 1.2f);
        }
        break;
    case 5:  /* signpost */
        limb_o(v2(at.x, at.y), v2(at.x, at.y - 44 * s), 2.6f * s, 2.4f * s, C_BARK, 1.2f);
        DrawRectangleRec((Rectangle){ at.x - 16 * s, at.y - 54 * s, 32 * s, 14 * s },
                         (Color){ 158, 128, 84, 255 });
        DrawRectangleLinesEx((Rectangle){ at.x - 16 * s, at.y - 54 * s, 32 * s, 14 * s }, 2,
                             (Color){ 84, 62, 38, 255 });
        break;
    default: break;
    }
}


/* Skills carry no artwork here, so each one is a rune in its own effect
   colour -- enough to tell nineteen of them apart at a glance, in the battle
   bar and on the skill tree alike.  The mark follows the damage type. */
void art_skill_glyph(const SkillDef *sk, float cx, float cy, float r, bool on)
{
    Color c = on ? sk->fx : (Color){ 122, 114, 104, 255 };
    DrawCircle((int)cx, (int)cy, r, alpha(c, on ? 0.32f : 0.16f));
    DrawCircleLines((int)cx, (int)cy, r, alpha(c, on ? 0.95f : 0.55f));
    if (sk->dmgType == DMG_MAGIC) {
        for (int k = 0; k < 4; k++) {
            float ang = (float)k * PI / 2.0f;
            DrawLineEx(v2(cx + cosf(ang) * r * 0.35f, cy + sinf(ang) * r * 0.35f),
                       v2(cx + cosf(ang) * r * 0.85f, cy + sinf(ang) * r * 0.85f),
                       2.0f, alpha(c, on ? 0.95f : 0.4f));
        }
    } else {
        DrawLineEx(v2(cx - r * 0.5f, cy + r * 0.55f),
                   v2(cx + r * 0.6f, cy - r * 0.6f), 2.6f, alpha(c, on ? 0.95f : 0.4f));
    }
}
