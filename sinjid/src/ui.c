/* ===========================================================================
   ui.c -- panels, bars, and the menu / shop / training / title screens.
   The look is ink-on-parchment: dark rounded panels, a thin gold rule, and
   small caps labels, so the interface sits inside the same palette as the art.
   =========================================================================== */
#include "game.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------ text bits */

float ui_text_w(const char *s, float size)
{
    if (G.fontLoaded)
        return MeasureTextEx(size > 30 ? G.fontBig : G.font, s, size, size * 0.06f).x;
    return MeasureTextEx(GetFontDefault(), s, size, size * 0.1f).x;
}

/* Panels never fade, so the guard is the one-frame lock-out set by
   go_panel plus the usual "not mid-transition" test. */
static bool ui_input_ready(const Game *g)
{
    return g->fadeDir <= 0 && g->inputLock == 0;
}

void ui_text(const char *s, float x, float y, float size, Color col)
{
    if (G.fontLoaded)
        DrawTextEx(size > 30 ? G.fontBig : G.font, s, (Vector2){ x, y },
                   size, size * 0.06f, col);
    else DrawTextEx(GetFontDefault(), s, (Vector2){ x, y }, size, size * 0.1f, col);
}

void ui_text_c(const char *s, float cx, float y, float size, Color col)
{
    ui_text(s, cx - ui_text_w(s, size) * 0.5f, y, size, col);
}

static void ui_text_sh(const char *s, float x, float y, float size, Color col)
{
    ui_text(s, x + 1.5f, y + 1.5f, size, (Color){ 8, 6, 10, (unsigned char)(col.a * 0.7f) });
    ui_text(s, x, y, size, col);
}

/* ---------------------------------------------------------------- chrome */

void ui_panel(Rectangle r, const char *title)
{
    DrawRectangleRounded(r, 0.06f, 8, (Color){ 18, 16, 22, 232 });
    DrawRectangleRoundedLines(r, 0.06f, 8, (Color){ 122, 98, 46, 255 });
    DrawLineEx((Vector2){ r.x + 10, r.y + 30 }, (Vector2){ r.x + r.width - 10, r.y + 30 },
               1.0f, (Color){ 122, 98, 46, 120 });
    if (title) ui_text(title, r.x + 14, r.y + 7, 18, C_GOLD);
}

void ui_bar(Rectangle r, float frac, Color fill, Color back, const char *label)
{
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    DrawRectangleRec(r, back);
    Rectangle in = { r.x + 1, r.y + 1, (r.width - 2) * frac, r.height - 2 };
    DrawRectangleRec(in, fill);
    DrawRectangleRec((Rectangle){ in.x, in.y, in.width, in.height * 0.4f },
                     art_shade(fill, 1.25f));
    DrawRectangleLinesEx(r, 1, (Color){ 10, 8, 12, 200 });
    if (label) ui_text(label, r.x + 6, r.y + r.height * 0.5f - 8, 15, C_PARCH);
}

bool ui_button(Rectangle r, const char *label, bool selected, bool enabled)
{
    Color bg = selected ? (Color){ 60, 50, 34, 245 } : (Color){ 26, 23, 30, 220 };
    if (!enabled) bg = (Color){ 22, 20, 24, 180 };
    DrawRectangleRounded(r, 0.25f, 6, bg);
    DrawRectangleRoundedLines(r, 0.25f, 6,
                              selected ? C_GOLD : (Color){ 70, 62, 46, 220 });
    Color tc = enabled ? (selected ? C_PARCH : C_PARCH2) : (Color){ 90, 84, 76, 255 };
    ui_text(label, r.x + 14, r.y + r.height * 0.5f - 10, 19, tc);
    if (selected) {
        float b = sinf((float)GetTime() * 6) * 2;
        ui_text(">", r.x - 12 + b, r.y + r.height * 0.5f - 10, 19, C_GOLD);
    }
    return selected;
}

/* Renders an item's stat block wherever it is asked to. */
void ui_tooltip_item(const ItemDef *it, Rectangle at)
{
    ui_panel(at, NULL);
    ui_text(it->name, at.x + 14, at.y + 6, 20, C_GOLD);
    float y = at.y + 38;
    char b[96];
    struct { const char *tag; int v; } rows[] = {
        { "Life",        it->lifeMax }, { "Mana",       it->manaMax },
        { "Physical Dmg:",    it->phyDmg },
        { "Phys def %",  it->phyDef },  { "Magical Dmg:",  it->magDmg },
        { "Magical Def:",   it->magDef },  { "Shield pts", it->shdPts },
        { "Shd p.def %", it->shdPhyDef }, { "Shd m.def %", it->shdMagDef },
        { "Shd damage",  it->shdDmg },  { "Speed:",      it->speed },
    };
    for (unsigned i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        if (!rows[i].v) continue;
        snprintf(b, sizeof b, "%-11s %+d", rows[i].tag, rows[i].v);
        ui_text(b, at.x + 14, y, 17, rows[i].v > 0 ? C_JADE : C_BLOOD2);
        y += 20;
        if (y > at.y + at.height - 46) break;
    }
    if (it->strNeed > 0) {
        char req[64];
        snprintf(req, sizeof req, "Requires %d Strength", it->strNeed);
        ui_text(req, at.x + 14, at.y + at.height - 52, 17,
                G.p.baseStr >= it->strNeed ? C_JADE : C_BLOOD2);
    }
    if (it->note) ui_text(it->note, at.x + 14, at.y + at.height - 30, 15, C_PARCH2);
}

/* ------------------------------------------------------------ inventory */



/* The interface clip's own space.  Every panel drawn from clip 1458 -- the
   inventory, the skill tree, the merchant, the Elder's save sheet -- shares
   it: the clip sits at (287.85, 223.55) on the 600-wide stage at 0.85 scale,
   and the stage maps onto ours at 1.6. */
#define PSC      0.85f
#define PX(v)    (160.0f + (287.85f + (v) * PSC) * 1.6f)
#define PY(v)    ((223.55f + (v) * PSC) * 1.6f)
#define PW(v)    ((v) * PSC * 1.6f)

static const char *item_type_name(int t)
{
    switch (t) {
    case ITEM_WEAPON:     return "Weapon";
    case ITEM_SHIELD:     return "Shield";
    case ITEM_ARMOUR:     return "Armour";
    case ITEM_HELM:       return "Head Gear";
    case ITEM_RELIC:      return "Relic";
    case ITEM_CONSUMABLE: return "Drink";
    default:              return "";
    }
}

/* ----------------------------------------------------------- inventory */

/* The interface clip's Inventory frame: an eight-cell pack in two rows of
   four, the worn gear arranged as a figure below it, the character's stats in
   two columns on the right, and an explain box above them that reads out
   whatever cell is under the cursor.  Coordinates are the clip's own, from
   tools/groundtruth/panels.json, mapped through PX/PY. */

/* pack cells: slot4..slot11 */
static const float PACK_X[4] = { -258.0f, -183.0f, -108.0f, -33.0f };
static const float PACK_Y[2] = { -121.0f, -52.5f };
/* worn: the original places four -- weapon, armour, shield, helm.  Our fifth
   (a relic) has no cell of its own there, so it sits under the armour. */
static const float WORN_XY[SLOT_COUNT][2] = {
    { -226.2f, 161.1f },   /* SLOT_WEAPON */
    {  -76.2f, 161.1f },   /* SLOT_SHIELD */
    { -151.2f, 161.1f },   /* SLOT_ARMOUR */
    { -151.2f,  84.7f },   /* SLOT_HELM   */
    {  -76.2f,  84.7f },   /* relic, ours -- the cell the original leaves empty */
};
static const char *WORN_LABEL[SLOT_COUNT] =
    { "Weapon", "Shield", "Armour", "Head Gear", "Relic" };

#define INV_CELL   51.0f
#define INV_PACK_N 8
#define INV_N      (INV_PACK_N + SLOT_COUNT)

static Rectangle inv_cell_rect(float cx, float cy)
{
    float w = PW(INV_CELL);
    return (Rectangle){ PX(cx) - w * 0.5f, PY(cy) - w * 0.5f, w, w };
}

static void inv_cell(Rectangle r, const ItemDef *it, bool sel, bool dim)
{
    DrawRectangleRec(r, (Color){ 38, 32, 26, 245 });
    DrawRectangleLinesEx(r, sel ? 3 : 1, sel ? C_GOLD : (Color){ 92, 78, 52, 255 });
    if (it) {
        art_draw_item_icon((int)(it - ITEMS),
                           (Rectangle){ r.x + 4, r.y + 4, r.width - 8, r.height - 8 });
        if (dim) DrawRectangleRec(r, Fade(C_INK, 0.45f));
    }
}

/* what the cursor is over: a pack entry, a worn slot, or nothing */
static const ItemDef *inv_focus(Game *g, int idx, int *packSlot, int *wornSlot)
{
    Player *p = &g->p;
    *packSlot = *wornSlot = -1;
    if (idx < INV_PACK_N) {
        if (idx >= p->invCount) return NULL;
        *packSlot = idx;
        return &ITEMS[p->inv[idx].def];
    }
    int w = idx - INV_PACK_N;
    if (w < 0 || w >= SLOT_COUNT || p->equip[w] < 0) return NULL;
    *wornSlot = w;
    return &ITEMS[p->equip[w]];
}

static void inv_move(Game *g, int dx, int dy)
{
    int i = g->menuIdx;
    if (i < INV_PACK_N) {
        int col = i % 4, row = i / 4;
        if (dx) { col = (col + 4 + dx) % 4; }
        if (dy > 0) { if (row == 1) { g->menuIdx = INV_PACK_N + SLOT_HELM; return; } row = 1; }
        if (dy < 0) { if (row == 0) { g->menuIdx = INV_PACK_N + SLOT_HELM; return; } row = 0; }
        g->menuIdx = row * 4 + col;
        return;
    }
    int w = i - INV_PACK_N;
    if (w == SLOT_HELM) {
        if (dy > 0) g->menuIdx = INV_PACK_N + SLOT_ARMOUR;
        else if (dy < 0) g->menuIdx = 4;                  /* pack, lower row */
        else if (dx > 0) g->menuIdx = INV_PACK_N + SLOT_RELIC;
        else g->menuIdx = INV_PACK_N + SLOT_WEAPON;
        return;
    }
    if (w == SLOT_ARMOUR) {
        if (dy < 0) g->menuIdx = INV_PACK_N + SLOT_HELM;
        else g->menuIdx = INV_PACK_N + (dx > 0 ? SLOT_SHIELD : SLOT_WEAPON);
        return;
    }
    if (w == SLOT_RELIC) {
        if (dy > 0) g->menuIdx = INV_PACK_N + SLOT_SHIELD;
        else if (dy < 0) g->menuIdx = 4;
        else if (dx < 0) g->menuIdx = INV_PACK_N + SLOT_HELM;
        return;
    }
    /* weapon or shield */
    if (dy < 0) g->menuIdx = INV_PACK_N + SLOT_HELM;
    else if (dx) g->menuIdx = INV_PACK_N + SLOT_ARMOUR;
}

void ui_scene_menu(Game *g)
{
    Player *p = &g->p;
    Combatant c;
    player_recalc(p, &c);

    if (g->menuIdx < 0 || g->menuIdx >= INV_N) g->menuIdx = 0;

    if (ui_input_ready(g)) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_I)) {
            go_panel(g, p->zone == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
            return;
        }
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) inv_move(g,  1, 0);
        if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) inv_move(g, -1, 0);
        if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)) inv_move(g, 0,  1);
        if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)) inv_move(g, 0, -1);

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            int ps, ws;
            const ItemDef *it = inv_focus(g, g->menuIdx, &ps, &ws);
            if (it && ps >= 0) {
                if (it->type == ITEM_CONSUMABLE) {
                    c.life = p->curLife > 0 ? p->curLife : c.lifeMax;
                    c.mana = p->curMana;
                    if (use_consumable(g, ps, &c)) {
                        p->curLife = c.life;
                        p->curMana = c.mana;
                    }
                } else if (!player_can_equip(p, p->inv[ps].def)) {
                    ui_toast(g, "%s needs %d Strength; you have %d.",
                             it->name, it->strNeed, p->baseStr);
                } else {
                    player_equip(p, ps);
                    ui_toast(g, "Equipped %s.", it->name);
                }
            } else if (it && ws >= 0) {
                if (player_add_item(p, p->equip[ws]) >= 0) {
                    ui_toast(g, "Removed %s.", it->name);
                    p->equip[ws] = -1;
                } else ui_toast(g, "No room in the pack.");
            }
            player_recalc(p, &c);
        }
    }

    /* ---- the panel ---- */
    Rectangle plate = { PX(-298.1f), PY(-200.2f),
                        PX(322.8f) - PX(-298.1f), PY(232.1f) - PY(-200.2f) };
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 10, 8, 14, 170 });
    DrawRectangleRec(plate, (Color){ 30, 27, 24, 255 });
    DrawRectangleLinesEx(plate, 2, (Color){ 115, 89, 66, 255 });

    ui_text("Inventory", PX(-223.3f), PY(-188.5f), 21, C_GOLD);
    ui_text("Equipment", PX(-223.3f), PY(4.5f), 21, C_GOLD);

    int ps, ws;
    const ItemDef *hov = inv_focus(g, g->menuIdx, &ps, &ws);

    /* the pack, two rows of four */
    for (int i = 0; i < INV_PACK_N; i++) {
        Rectangle r = inv_cell_rect(PACK_X[i % 4], PACK_Y[i / 4]);
        const ItemDef *it = (i < p->invCount) ? &ITEMS[p->inv[i].def] : NULL;
        inv_cell(r, it, g->menuIdx == i, false);
        if (it && p->inv[i].count > 1) {
            char n[8];
            snprintf(n, sizeof n, "%d", p->inv[i].count);
            ui_text(n, r.x + r.width - 16, r.y + r.height - 22, 16, C_PARCH);
        }
    }

    /* the worn figure */
    for (int w = 0; w < SLOT_COUNT; w++) {
        Rectangle r = inv_cell_rect(WORN_XY[w][0], WORN_XY[w][1]);
        const ItemDef *it = (p->equip[w] >= 0) ? &ITEMS[p->equip[w]] : NULL;
        inv_cell(r, it, g->menuIdx == INV_PACK_N + w, false);
        ui_text_c(WORN_LABEL[w], r.x + r.width * 0.5f, r.y + r.height + 2, 15,
                  g->menuIdx == INV_PACK_N + w ? C_GOLD : C_PARCH2);
    }

    /* stats, in the original's two columns */
    {
        const char *LN[4] = { "Physical Dmg:", "Magical Dmg:", "Physical Def:", "Magical Def:" };
        const int   LV[4] = { c.phyDmg + c.strDmg, c.magDmg, c.phyDef, c.magDef };
        const float LY[4] = { 20.0f, 37.5f, 53.8f, 71.3f };
        const char *RN[4] = { "Strength:", "Speed:", "Max Life:", "Max Mana:" };
        const int   RV[4] = { c.str, c.speed, c.lifeMax, c.manaMax };
        const float RY[4] = { 20.0f, 36.7f, 54.6f, 71.3f };
        char b[32];
        for (int i = 0; i < 4; i++) {
            ui_text(LN[i], PX(11.9f),  PY(LY[i]), 16, C_PARCH2);
            snprintf(b, sizeof b, "%d", LV[i]);
            ui_text(b,     PX(133.4f), PY(LY[i]), 17, C_PARCH);
            ui_text(RN[i], PX(182.1f), PY(RY[i]), 16, C_PARCH2);
            snprintf(b, sizeof b, "%d", RV[i]);
            ui_text(b,     PX(264.4f), PY(RY[i]), 17, C_PARCH);
        }
        snprintf(b, sizeof b, "Level: %d", p->level);   /* b is 24 wide */
        ui_text(b, PX(27.8f), PY(-7.8f), 18, (Color){ 0, 204, 255, 255 });
        ui_text(CLASS_NAMES[p->cls], PX(120.0f), PY(-7.8f), 18, C_PARCH);
        snprintf(b, sizeof b, "%d", p->gold);
        ui_text("GOLD:", PX(196.0f), PY(205.4f), 17, C_PARCH2);
        ui_text(b,       PX(262.2f), PY(205.4f), 17, C_GOLD);
    }

    /* the explain box, reading out the cell under the cursor */
    {
        Rectangle eb = { PX(39.7f), PY(-182.8f), PX(300.5f) - PX(39.7f),
                         PY(-28.8f) - PY(-182.8f) };
        DrawRectangleRec(eb, (Color){ 22, 20, 18, 255 });
        DrawRectangleLinesEx(eb, 1.5f, (Color){ 92, 78, 52, 255 });
        if (!hov) {
            ui_text("Move the cursor over any item for its detail.",
                    eb.x + 12, eb.y + 10, 16, C_PARCH2);
        } else {
            ui_text(hov->name, eb.x + 12, eb.y + 8, 19, C_GOLD);
            ui_text(item_type_name(hov->type), eb.x + 12, eb.y + 32, 15, C_PARCH2);
            float y = eb.y + 54;
            char b[64];
            struct { const char *tag; int v; } rows[] = {
                { "Phys dmg",   hov->phyDmg }, { "Magic dmg",  hov->magDmg },
                { "Phys def %", hov->phyDef }, { "Magic def",  hov->magDef },
                { "Shield pts", hov->shdPts }, { "Shd damage", hov->shdDmg },
                { "Speed",      hov->speed  }, { "Life",       hov->lifeMax },
                { "Mana",       hov->manaMax },
            };
            for (unsigned i = 0; i < sizeof rows / sizeof rows[0]; i++) {
                if (!rows[i].v) continue;
                snprintf(b, sizeof b, "%-11s %+d", rows[i].tag, rows[i].v);
                ui_text(b, eb.x + 12, y, 16, rows[i].v > 0 ? C_JADE : C_BLOOD2);
                y += 19;
                if (y > eb.y + eb.height - 34) break;
            }
            if (hov->strNeed > 0) {
                snprintf(b, sizeof b, "Requires %d Strength", hov->strNeed);
                ui_text(b, eb.x + 12, eb.y + eb.height - 26, 16,
                        p->baseStr >= hov->strNeed ? C_JADE : C_BLOOD2);
            }
        }
    }

    ui_text_c(ps >= 0 ? "ENTER equip or use   ESC close"
                      : "ENTER take off   ESC close",
              plate.x + plate.width * 0.5f, plate.y + plate.height - PW(22.0f),
              17, C_GOLD);
}

/* ---------------------------------------------------------------- shop */

/* The merchant screen, laid out as the original's interface clip has it.
   Its positions, in the clip's own space, are: the pack as slot4..slot11 in
   two rows of four; the worn gear as slot0..slot3 in a small figure below it;
   the merchant's stock as ten cells in two rows of five to the right; and the
   description box under those.  The clip sits at (287.85, 223.55) on a 600
   wide stage, and everything below is that, mapped through SX/SY. */
#define SHOP_S   1.6f
#define SHOP_OX  160
#define SX(v)    (SHOP_OX + (float)((287.85f + (v)) * SHOP_S))
#define SY(v)    ((float)((223.55f + (v)) * SHOP_S))
#define CELL_W   (41.2f * SHOP_S)
#define CELL_H   (41.9f * SHOP_S)

static void shop_cell(Rectangle r, const ItemDef *it, bool sel, bool dim)
{
    DrawRectangleRec(r, (Color){ 38, 32, 26, 235 });
    DrawRectangleLinesEx(r, sel ? 3 : 1,
                         sel ? C_GOLD : (Color){ 92, 78, 52, 255 });
    if (!it) return;
    art_draw_item_icon((int)(it - ITEMS), (Rectangle){ r.x + 4, r.y + 4,
                                                       r.width - 8, r.height - 8 });
    if (dim) DrawRectangleRec(r, Fade(C_INK, 0.45f));
}

void ui_scene_shop(Game *g)
{
    Player *p = &g->p;
    const int *stock;
    int n = data_shop_table(g->shopVendor, &stock);
    if (n > 10) n = 10;                       /* the stock grid holds ten */

    /* g->shopMode: 0 = the merchant's stock, 1 = your pack */
    int count = g->shopMode ? p->invCount : n;
    if (count < 0) count = 0;

    if (ui_input_ready(g)) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            go_panel(g, p->zone == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
            return;
        }
        if (IsKeyPressed(KEY_TAB)) { g->shopMode = !g->shopMode; g->shopIdx = 0; }
        int cols = g->shopMode ? 4 : 5;
        if (count > 0) {
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
                g->shopIdx = (g->shopIdx + 1) % count;
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
                g->shopIdx = (g->shopIdx + count - 1) % count;
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
                g->shopIdx = (g->shopIdx + cols) % count;
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
                g->shopIdx = (g->shopIdx + count - cols % count) % count;
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                if (!g->shopMode) {
                    const ItemDef *it = &ITEMS[stock[g->shopIdx]];
                    if (p->gold < it->price) ui_toast(g, "Not enough gold.");
                    else if (player_add_item(p, stock[g->shopIdx]) < 0)
                        ui_toast(g, "Your pack is full.");
                    else { p->gold -= it->price; sound_play(SFX_ITEM);
                           ui_toast(g, "Bought %s.", it->name); }
                } else if (p->invCount > 0) {
                    int def = p->inv[g->shopIdx].def;
                    int price = data_sell_price(def);
                    if (price <= 0) {
                        ui_toast(g, "\"I have no use for that.\"");
                    } else {
                        p->gold += price;
                        p->inv[g->shopIdx].count--;
                        if (p->inv[g->shopIdx].count <= 0) {
                            for (int i = g->shopIdx; i + 1 < p->invCount; i++)
                                p->inv[i] = p->inv[i + 1];
                            p->invCount--;
                            if (g->shopIdx >= p->invCount && g->shopIdx > 0) g->shopIdx--;
                        }
                        sound_play(SFX_COINS);
                        ui_toast(g, "Sold %s for %d gold.", ITEMS[def].name, price);
                    }
                }
            }
        }
    }

    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 8, 7, 11, 225 });
    Rectangle frame = { SX(-310.4f), SY(-234.8f), 642.1f * SHOP_S, 474.1f * SHOP_S };
    DrawRectangleRec(frame, (Color){ 26, 22, 18, 250 });
    DrawRectangleLinesEx(frame, 3, (Color){ 122, 100, 62, 255 });
    DrawRectangleLinesEx((Rectangle){ frame.x + 6, frame.y + 6,
                                      frame.width - 12, frame.height - 12 },
                         1, (Color){ 78, 64, 40, 255 });

    /* the pack: slot4..slot11, two rows of four */
    ui_text("PACK", SX(-273.2f), SY(-186.0f), 17, C_PARCH2);
    for (int i = 0; i < 8; i++) {
        float cx = SX(-273.2f + 59.05f * (i % 4));
        float cy = SY(i < 4 ? -160.8f : -105.3f);
        const ItemDef *it = (i < p->invCount) ? &ITEMS[p->inv[i].def] : NULL;
        shop_cell((Rectangle){ cx, cy, CELL_W, CELL_H }, it,
                  g->shopMode && g->shopIdx == i, false);
        if (it && p->inv[i].count > 1) {
            char c[16]; snprintf(c, sizeof c, "%d", p->inv[i].count);
            ui_text(c, cx + CELL_W - 16, cy + CELL_H - 20, 15, C_PARCH);
        }
    }

    /* the worn gear, in the original's small figure: helm above body,
       weapon to its left, shield to its right */
    ui_text("WORN", SX(-273.2f), SY(-6.0f), 17, C_PARCH2);
    const struct { float x, y; int slot; const char *lab; } WORN[4] = {
        { -189.1f,   0.5f, SLOT_HELM,   "head" },
        { -189.1f,  52.8f, SLOT_ARMOUR, "body" },
        { -248.2f,  52.8f, SLOT_WEAPON, "hand" },
        { -130.0f,  52.8f, SLOT_SHIELD, "off"  },
    };
    for (int i = 0; i < 4; i++) {
        Rectangle r = { SX(WORN[i].x), SY(WORN[i].y), CELL_W, CELL_H };
        int def = p->equip[WORN[i].slot];
        shop_cell(r, def >= 0 ? &ITEMS[def] : NULL, false, true);
        ui_text(WORN[i].lab, r.x + 2, r.y + r.height + 2, 13, C_STEEL2);
    }

    /* the merchant's stock: ten cells, two rows of five */
    ui_text("FOR SALE", SX(22.1f), SY(-164.0f), 17, C_PARCH2);
    for (int i = 0; i < 10; i++) {
        float cx = SX(22.1f + 56.28f * (i % 5));
        float cy = SY(i < 5 ? -138.7f : -55.1f);
        const ItemDef *it = (i < n) ? &ITEMS[stock[i]] : NULL;
        bool poor = it && p->gold < it->price;
        shop_cell((Rectangle){ cx, cy, CELL_W, CELL_H }, it,
                  !g->shopMode && g->shopIdx == i, poor);
        if (it) {
            char c[16]; snprintf(c, sizeof c, "%d", it->price);
            ui_text(c, cx, cy + CELL_H + 2, 13, poor ? C_STEEL2 : C_GOLD2);
        }
    }

    /* the description box */
    Rectangle box = { SX(-310.4f + 12), SY(122.8f), 612.0f * SHOP_S, 78.0f * SHOP_S };
    DrawRectangleRec(box, (Color){ 18, 15, 12, 235 });
    DrawRectangleLinesEx(box, 1, (Color){ 78, 64, 40, 255 });
    const ItemDef *sel = NULL;
    if (g->shopMode) { if (g->shopIdx < p->invCount) sel = &ITEMS[p->inv[g->shopIdx].def]; }
    else             { if (g->shopIdx < n)           sel = &ITEMS[stock[g->shopIdx]]; }
    if (sel) {
        ui_text(sel->name, box.x + 14, box.y + 10, 21, C_GOLD);
        char line[160];
        if (g->shopMode) {
            int sp = data_sell_price(sel - ITEMS);
            snprintf(line, sizeof line, sp > 0 ? "sells for %d gold" : "the merchant will not buy this", sp);
        } else snprintf(line, sizeof line, "%d gold", sel->price);
        ui_text(line, box.x + 14, box.y + 36, 17, C_PARCH2);
        ui_text(sel->note, box.x + 14, box.y + 58, 15, C_STEEL2);
    }

    char purse[64];
    snprintf(purse, sizeof purse, "GOLD  %d", p->gold);
    ui_text(purse, SX(180.0f), SY(122.8f) - 26, 19, C_GOLD);
    /* The original's frame is taller than its own stage, so at this scale it
       bleeds off the bottom; keep the key hint on screen regardless. */
    ui_text(g->shopMode ? "TAB to buy    ENTER sell    ESC leave"
                        : "TAB to sell   ENTER buy     ESC leave",
            frame.x + 16, (float)(SCREEN_H - 26), 16, C_STEEL2);
}

/* ------------------------------------------------------------- training */

/* ------------------------------------------------------------ skill tree */

/* The interface clip's Skills frame lays the nineteen skills out as a graph:
   two columns of nodes -- the physical tree on the left, the magic tree on the
   right -- in six rows gated at levels 0, 1, 5, 10 and 15, each node a disc
   with its rank in a badge.  These are its own node placements. */
static const struct { float x, y; int levelreq; } TREE_NODE[MAX_SKILLS] = {
    {  -126.0f,  -178.8f,  0 }, {     0.3f,  -178.2f,  0 },
    {  -154.7f,  -111.8f,  0 }, {  -104.0f,  -111.8f,  0 },
    {   -20.1f,  -111.8f,  0 }, {    31.6f,  -111.8f,  0 },
    {  -156.3f,   -45.0f,  1 }, {  -103.8f,   -44.9f,  1 },
    {   -19.4f,   -44.9f,  1 }, {    31.7f,   -45.0f,  1 },
    {  -155.0f,    21.9f,  5 }, {   -20.1f,    21.4f,  5 },
    {    33.2f,    21.4f,  5 }, {  -154.2f,    88.1f, 10 },
    {  -102.0f,    88.1f, 10 }, {   -21.0f,    88.1f, 10 },
    {    32.2f,    88.1f, 10 }, {  -128.6f,   154.8f, 15 },
    {   -20.8f,   154.8f, 15 },
};

/* Arrow keys walk the graph: nearest node in the pressed direction. */
static void tree_move(Game *g, int dx, int dy)
{
    float cx = TREE_NODE[g->menuIdx].x, cy = TREE_NODE[g->menuIdx].y;
    int best = -1; float bestD = 1e9f;
    for (int i = 0; i < MAX_SKILLS; i++) {
        if (i == g->menuIdx) continue;
        float ox = TREE_NODE[i].x - cx, oy = TREE_NODE[i].y - cy;
        if (dx > 0 && ox <= 4) continue;
        if (dx < 0 && ox >= -4) continue;
        if (dy > 0 && oy <= 4) continue;
        if (dy < 0 && oy >= -4) continue;
        /* prefer nodes in line with the one we are on */
        float along = dx ? fabsf(ox) : fabsf(oy);
        float across = dx ? fabsf(oy) : fabsf(ox);
        float d = along + across * 3.0f;
        if (d < bestD) { bestD = d; best = i; }
    }
    if (best >= 0) g->menuIdx = best;
}

void ui_scene_train(Game *g)
{
    Player *p = &g->p;

    if (g->menuIdx < 0 || g->menuIdx >= MAX_SKILLS) g->menuIdx = 0;

    if (ui_input_ready(g)) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_T)) {
            go_panel(g, p->zone == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
            return;
        }
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) tree_move(g,  1, 0);
        if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) tree_move(g, -1, 0);
        if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)) tree_move(g, 0,  1);
        if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)) tree_move(g, 0, -1);
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            int id = g->menuIdx;
            if (p->skillPts <= 0) ui_toast(g, "No skill points left.");
            else if (!skill_prereqs_met(p, id))
                ui_toast(g, "%s %s.", SKILLS[id].name, skill_lock_reason(p, id));
            else if (p->skillRank[id] >= SKILLS[id].maxRank)
                ui_toast(g, "%s is already mastered.", SKILLS[id].name);
            else {
                p->skillRank[id]++;
                p->skillPts--;
                sound_play(SFX_SKILL);
            }
        }
    }

    /* ---- the panel ---- */
    Rectangle plate = { PX(-206.2f), PY(-202.6f),
                        PX(322.8f) - PX(-206.2f), PY(232.1f) - PY(-202.6f) };
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 10, 8, 14, 170 });
    DrawRectangleRec(plate, (Color){ 30, 27, 24, 255 });
    DrawRectangleLinesEx(plate, 2, (Color){ 115, 89, 66, 255 });

    Rectangle tp = { PX(-174.8f), PY(-195.4f),
                     PX(50.0f) - PX(-174.8f), PY(173.0f) - PY(-195.4f) };
    DrawRectangleRec(tp, (Color){ 22, 20, 18, 255 });
    DrawRectangleLinesEx(tp, 1.5f, (Color){ 92, 78, 52, 255 });

    /* the level gates the original rules off */
    {
        const struct { const char *tag; float y; } GATE[3] = {
            { "Lvl. 5", 21.6f }, { "Lvl. 10", 88.1f }, { "Lvl. 15", 154.8f }
        };
        for (int i = 0; i < 3; i++) {
            float gy = PY(GATE[i].y - 33.0f);
            DrawLineEx((Vector2){ tp.x + 6, gy }, (Vector2){ tp.x + tp.width - 6, gy },
                       1.0f, (Color){ 92, 78, 52, 140 });
            int need = (i == 0) ? 5 : (i == 1) ? 10 : 15;
            float lw = ui_text_w(GATE[i].tag, 15) + 10;
            Rectangle lb = { PX(-62.0f) - lw * 0.5f, gy - PW(11.0f), lw, PW(20.0f) };
            DrawRectangleRec(lb, (Color){ 22, 20, 18, 255 });
            ui_text_c(GATE[i].tag, lb.x + lw * 0.5f, lb.y + PW(2.0f), 15,
                      p->level >= need ? C_JADE : C_PARCH2);
        }
    }

    /* the edges, drawn from each skill's prerequisites */
    for (int i = 0; i < MAX_SKILLS; i++) {
        for (int k = 0; k < 2; k++) {
            int pr = SKILLS[i].prereq[k];
            if (pr < 0 || pr >= MAX_SKILLS) continue;
            bool have = p->skillRank[pr] > 0;
            DrawLineEx((Vector2){ PX(TREE_NODE[pr].x), PY(TREE_NODE[pr].y) },
                       (Vector2){ PX(TREE_NODE[i].x),  PY(TREE_NODE[i].y) },
                       2.0f, have ? Fade(C_GOLD, 0.45f) : (Color){ 70, 62, 50, 200 });
        }
    }

    /* the nodes */
    for (int i = 0; i < MAX_SKILLS; i++) {
        float cx = PX(TREE_NODE[i].x), cy = PY(TREE_NODE[i].y);
        float r = PW(19.0f);
        int rank = p->skillRank[i];
        bool open = skill_prereqs_met(p, i);
        bool sel = (g->menuIdx == i);
        DrawCircle((int)cx, (int)cy, r,
                   rank > 0 ? (Color){ 60, 52, 38, 255 }
                            : (open ? (Color){ 44, 40, 34, 255 }
                                    : (Color){ 30, 28, 26, 255 }));
        DrawCircleLines((int)cx, (int)cy, r,
                        rank > 0 ? (Color){ 150, 122, 70, 255 } : (Color){ 78, 70, 58, 255 });
        art_skill_glyph(&SKILLS[i], cx, cy, PW(11.0f), rank > 0 || open);
        if (sel) {
            float pulse = 0.6f + 0.4f * sinf((float)GetTime() * 5.0f);
            DrawCircleLines((int)cx, (int)cy, r,        Fade(C_GOLD, pulse));
            DrawCircleLines((int)cx, (int)cy, r - 1.5f, Fade(C_GOLD, pulse));
            DrawCircleLines((int)cx, (int)cy, r + 2.0f, Fade(C_GOLD, pulse * 0.5f));
        }
        /* the rank badge, up and left of the disc as the original places it */
        {
            Rectangle bd = { cx + PW(-16.4f) - PW(12.6f), cy + PW(-13.9f) - PW(12.2f),
                             PW(25.2f), PW(24.4f) };
            DrawRectangleRec(bd, (Color){ 18, 16, 14, 235 });
            DrawRectangleLinesEx(bd, 1.0f, (Color){ 92, 78, 52, 220 });
            char n[8];
            snprintf(n, sizeof n, "%d", rank);
            ui_text_c(n, bd.x + bd.width * 0.5f, bd.y + PW(3.0f), 16,
                      rank > 0 ? C_GOLD : (Color){ 110, 102, 92, 255 });
        }
    }

    /* headings and the readout for the node under the cursor */
    {
        char b[96];
        snprintf(b, sizeof b, "Skill Points: %d", p->skillPts);
        ui_text(b, PX(60.0f), PY(-188.0f), 18, p->skillPts ? C_JADE : C_PARCH2);
        snprintf(b, sizeof b, "Level: %d", p->level);
        ui_text(b, PX(60.0f), PY(-166.0f), 18, (Color){ 0, 204, 255, 255 });

        const SkillDef *sk = &SKILLS[g->menuIdx];
        float bx = PX(60.0f), by = PY(-130.0f);
        ui_text(sk->name, bx, by, 20, C_GOLD);
        snprintf(b, sizeof b, "rank %d of %d", p->skillRank[g->menuIdx], sk->maxRank);
        ui_text(b, bx, by + PW(26.0f), 16, C_PARCH2);
        if (sk->flags & SKF_PASSIVE)      ui_text("Passive", bx, by + PW(46.0f), 16, C_STEEL2);
        else if (sk->manaCost)  { snprintf(b, sizeof b, "Requires %d Mana", sk->manaCost);
                                  ui_text(b, bx, by + PW(46.0f), 16, C_KI); }
        if (TREE_NODE[g->menuIdx].levelreq > 0) {
            snprintf(b, sizeof b, "Needs level %d", TREE_NODE[g->menuIdx].levelreq);
            ui_text(b, bx, by + PW(66.0f), 16,
                    p->level >= TREE_NODE[g->menuIdx].levelreq ? C_JADE : C_BLOOD2);
        }
        /* the description, wrapped */
        {
            const char *d = sk->desc;
            char line[80]; int li = 0, ly = 0;
            for (const char *q = d;; q++) {
                if (*q == 0 || (li > 30 && *q == ' ')) {
                    line[li] = 0;
                    ui_text(line, bx, by + PW(92.0f) + ly * PW(20.0f), 16, C_PARCH2);
                    ly++; li = 0;
                    if (*q == 0) break;
                    continue;
                }
                if (li < 78) line[li++] = *q;
            }
        }
    }

    ui_text_c("ENTER spend a point   ESC leave",
              plate.x + plate.width * 0.5f, plate.y + plate.height - PW(22.0f), 17, C_GOLD);
}

/* --------------------------------------------------------------- dialog */

void ui_scene_dialog(Game *g)
{
    Zone *z = &g->zones[g->p.zone];
    if (g->dialogNpc < 0 || g->dialogNpc >= z->npcCount) {
        go_panel(g, g->p.zone == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
        return;
    }
    Npc *n = &z->npcs[g->dialogNpc];
    if (ui_input_ready(g) && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
                              IsKeyPressed(KEY_ESCAPE))) {
        go_panel(g, g->p.zone == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
        return;
    }
    Rectangle box = { 180, 320, SCREEN_W - 360, 200 };
    ui_panel(box, n->name);
    art_draw_walker(&n->look, (Vector2){ box.x + 80, box.y + 170 }, 1, 0.0f, 0.85f);
    float y = box.y + 52;
    char line[128]; int li = 0;
    for (const char *q = n->line;; q++) {
        if (*q == 0 || *q == '\n') {
            line[li] = 0;
            ui_text(line, box.x + 170, y, 21, C_PARCH);
            y += 30; li = 0;
            if (*q == 0) break;
            continue;
        }
        if (li < 126) line[li++] = *q;
    }
    ui_text("ENTER to close", box.x + box.width - 160, box.y + box.height - 30, 17, C_GOLD);
}

/* ---------------------------------------------------------------- save */

/* The Elder's panel (the interface clip's "Save" frame).  It is a character
   sheet with two action plates down the left: one saves the game, which costs
   nothing, and one rests -- refilling life, mana and energy -- which spends
   one of the rests you carry.  The original keeps those separate; walking up
   to him does neither on its own.  Coordinates below are the clip's own, as
   extracted into tools/groundtruth/save_layout.json, mapped through PX/PY:
   the clip sits at (287.85, 223.55) on the 600-wide stage and is placed at
   0.85 scale, and the stage maps onto ours at 1.6. */

static void sheet_bar(float x0, float x1, float y, float frac, Color hi, Color lo)
{
    Rectangle r = { PX(x0), PY(y) - PW(4.0f), PX(x1) - PX(x0), PW(8.0f) };
    DrawRectangleRec(r, (Color){ 26, 24, 22, 255 });
    if (frac > 0) {
        if (frac > 1) frac = 1;
        DrawRectangleGradientV((int)(r.x + 1), (int)(r.y + 1),
                               (int)((r.width - 2) * frac), (int)(r.height - 2), hi, lo);
    }
    DrawRectangleLinesEx(r, 1, (Color){ 180, 168, 150, 110 });
}

static void sheet_num(const char *label, float lx, float vx, float y,
                      int cur, int max, Color col)
{
    char b[32];
    if (label) ui_text(label, PX(lx), PY(y) - PW(9.0f), 15, C_PARCH2);
    snprintf(b, sizeof b, "%d", cur);
    ui_text(b, PX(vx), PY(y) - PW(9.0f), 16, col);
    if (max >= 0) {
        snprintf(b, sizeof b, "/ %d", max);
        ui_text(b, PX(vx + 44.3f), PY(y) - PW(9.0f), 16, C_PARCH2);
    }
}

static void action_plate(Rectangle r, const char *title, const char *body,
                         const char *btn, bool sel, bool enabled)
{
    DrawRectangleRec(r, (Color){ 38, 33, 28, 255 });
    DrawRectangleLinesEx(r, sel ? 2.5f : 1.5f, sel ? C_GOLD : (Color){ 115, 89, 66, 255 });
    ui_text(title, r.x + 12, r.y + 8, 19, enabled ? C_GOLD : (Color){ 120, 108, 92, 255 });
    /* the blurb, wrapped by hand */
    char line[80]; int li = 0, ly = 0;
    for (const char *q = body;; q++) {
        if (*q == 0 || *q == '\n' || (li > 30 && *q == ' ')) {
            line[li] = 0;
            ui_text(line, r.x + 12, r.y + 34 + ly * 20, 16, C_PARCH2);
            ly++; li = 0;
            if (*q == 0) break;
            continue;
        }
        if (li < 78) line[li++] = *q;
    }
    Rectangle b = { r.x + 12, r.y + r.height - 34, r.width - 24, 26 };
    DrawRectangleRounded(b, 0.3f, 6, sel && enabled ? (Color){ 72, 60, 40, 250 }
                                                    : (Color){ 28, 25, 22, 230 });
    DrawRectangleRoundedLines(b, 0.3f, 6,
                              enabled ? (sel ? C_GOLD : (Color){ 92, 78, 52, 255 })
                                      : (Color){ 60, 54, 48, 255 });
    ui_text_c(btn, b.x + b.width * 0.5f, b.y + 4, 17,
              enabled ? (sel ? C_PARCH : C_PARCH2) : (Color){ 100, 92, 84, 255 });
}

void ui_scene_save(Game *g)
{
    Player *p = &g->p;
    Combatant c;
    player_recalc(p, &c);
    bool canRest = p->rests > 0;

    if (g->savedFlash > 0) g->savedFlash -= GetFrameTime();

    if (ui_input_ready(g)) {
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) ||
            IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) g->saveSel ^= 1;
        if (IsKeyPressed(KEY_ESCAPE)) {
            go_panel(g, p->zone == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
            return;
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            if (g->saveSel == 0) {
                p->saveZone = p->zone; p->saveX = p->tx; p->saveY = p->ty;
                if (save_write(g)) { g->hasSave = true; g->savedFlash = 1.4f; }
                else ui_toast(g, "The shrine is silent. (save failed)");
            } else if (canRest) {
                p->rests--;
                p->curLife = c.lifeMax;
                p->curMana = c.manaMax;
                p->curEnergy = c.engMax;
                sound_play(SFX_HEAL);
                ui_toast(g, "Rested. %d rests left.", p->rests);
            } else {
                ui_toast(g, "You have no rests left.");
            }
        }
    }

    /* ---- the character sheet on the right ---- */
    Rectangle sheet = { PX(36.0f), PY(-180.2f), PX(322.8f) - PX(36.0f),
                        PY(232.1f) - PY(-180.2f) };
    DrawRectangleRec(sheet, (Color){ 30, 27, 24, 255 });
    DrawRectangleLinesEx(sheet, 2, (Color){ 115, 89, 66, 255 });

    /* The original's name field starts at x=127 and is drawn over the portrait
       plate at 156.9.  Ours is opaque, so right-align the name to finish just
       before the plate rather than run under it. */
    ui_text(p->name, PX(154.0f) - ui_text_w(p->name, 19), PY(-151.2f) - PW(9.0f),
            19, C_GOLD);

    /* portrait, class and level */
    {
        Rectangle pr = { PX(156.9f), PY(-208.3f), PX(218.8f) - PX(156.9f),
                         PY(-150.0f) - PY(-208.3f) };
        DrawRectangleRec(pr, (Color){ 22, 20, 18, 255 });
        BeginScissorMode((int)pr.x + 1, (int)pr.y + 1, (int)pr.width - 2, (int)pr.height - 2);
        Combatant t = c;
        t.anim = ANIM_STAND; t.animT = 0;
        art_draw_puppet(&t, (Vector2){ pr.x + pr.width * 0.5f, pr.y + pr.height * 1.22f },
                        1.0f, 0.0f, 1.30f);
        EndScissorMode();
        DrawRectangleLinesEx(pr, 1.5f, (Color){ 115, 89, 66, 255 });
    }
    ui_text(CLASS_NAMES[p->cls], PX(94.5f), PY(-101.8f) - PW(9.0f), 17, C_PARCH);
    {
        char lv[24];
        snprintf(lv, sizeof lv, "Level: %d", p->level);
        ui_text(lv, PX(225.4f), PY(-100.5f) - PW(9.0f), 17, (Color){ 0, 204, 255, 255 });
    }

    /* life / mana / energy, each a bar with its numbers to the right */
    sheet_bar(93.0f, 231.2f, -68.3f, c.lifeMax ? (float)p->curLife / c.lifeMax : 0,
              (Color){ 21, 202, 21, 255 }, (Color){ 15, 124, 14, 255 });
    sheet_num("Life", 46.0f, 237.6f, -73.0f, p->curLife, c.lifeMax, (Color){ 36, 224, 36, 255 });
    sheet_bar(93.0f, 231.2f, -46.6f, c.manaMax ? (float)p->curMana / c.manaMax : 0,
              (Color){ 37, 149, 186, 255 }, (Color){ 26, 77, 113, 255 });
    sheet_num("Mana", 46.0f, 237.6f, -49.3f, p->curMana, c.manaMax, (Color){ 4, 204, 254, 255 });
    sheet_bar(93.0f, 231.2f, -24.2f, c.engMax ? (float)p->curEnergy / c.engMax : 0,
              (Color){ 214, 172, 60, 255 }, (Color){ 128, 100, 30, 255 });
    sheet_num("Eng", 46.0f, 237.6f, -25.2f, p->curEnergy, c.engMax, C_GOLD);

    /* the two stat columns */
    {
        const char *LN[4] = { "Physical Dmg:", "Magical Dmg:", "Physical Def:", "Magical Def:" };
        const int   LV[4] = { c.phyDmg, c.magDmg, c.phyDef, c.magDef };
        const float LY[4] = { 9.8f, 27.4f, 43.5f, 61.1f };
        const char *RN[4] = { "Strength:", "Speed:", "Max Life:", "Max Mana:" };
        const int   RV[4] = { c.str, c.speed, c.lifeMax, c.manaMax };
        char b[32];
        for (int i = 0; i < 4; i++) {
            ui_text(LN[i], PX(44.0f), PY(LY[i]) - PW(2.0f), 15, C_PARCH2);
            snprintf(b, sizeof b, "%d", LV[i]);
            ui_text(b, PX(146.3f), PY(LY[i]) - PW(2.0f), 16, C_PARCH);
            ui_text(RN[i], PX(194.9f), PY(LY[i]) - PW(2.0f), 15, C_PARCH2);
            snprintf(b, sizeof b, "%d", RV[i]);
            ui_text(b, PX(277.3f), PY(LY[i]) - PW(2.0f), 16, C_PARCH);
        }
    }

    /* purse and potions */
    {
        char b[32];
        ui_text("GOLD:", PX(48.4f), PY(97.5f) - PW(9.0f), 16, C_PARCH2);
        snprintf(b, sizeof b, "%d", p->gold);
        ui_text(b, PX(126.8f), PY(97.5f) - PW(9.0f), 17, C_GOLD);
        Rectangle lp = { PX(208.1f), PY(109.9f), PW(18.1f), PW(21.0f) };
        Rectangle mp = { PX(208.5f), PY(149.6f), PW(18.1f), PW(21.0f) };
        DrawCircle((int)(lp.x + lp.width * 0.5f), (int)(lp.y + lp.height * 0.6f),
                   lp.width * 0.42f, (Color){ 190, 48, 48, 255 });
        DrawCircle((int)(mp.x + mp.width * 0.5f), (int)(mp.y + mp.height * 0.6f),
                   mp.width * 0.42f, (Color){ 46, 132, 190, 255 });
        snprintf(b, sizeof b, "x %d", p->lifePots);
        ui_text(b, PX(238.2f), PY(112.2f), 17, C_PARCH);
        snprintf(b, sizeof b, "x %d", p->manaPots);
        ui_text(b, PX(238.6f), PY(151.7f), 17, C_PARCH);
    }

    /* ---- the two action plates on the left ---- */
    {
        Rectangle a = { PX(-295.0f), PY(-155.2f), PX(-9.9f) - PX(-295.0f),
                        PY(-37.7f) - PY(-155.2f) };
        Rectangle r = { PX(-295.0f), PY(6.8f), PX(-9.9f) - PX(-295.0f),
                        PY(124.3f) - PY(6.8f) };
        action_plate(a, "Save Game", "Saves the game.", "Save Game",
                     g->saveSel == 0, true);
        char rl[64];
        snprintf(rl, sizeof rl, "Completely fills up mana, life, and energy.\n"
                                "Rests Remaining: %d", p->rests);
        action_plate(r, "Rest", rl, canRest ? "Rest" : "No rests left",
                     g->saveSel == 1, canRest);
    }

    ui_text_c("ENTER choose   ESC leave", (sheet.x + sheet.width * 0.5f),
             sheet.y + sheet.height - PW(24.0f), 17, C_GOLD);

    /* the original flashes a "saved" clip over the whole panel */
    if (g->savedFlash > 0) {
        float a = g->savedFlash > 1.0f ? 1.0f : g->savedFlash;
        DrawRectangle(0, (int)PY(-40.0f), SCREEN_W, (int)PW(70.0f),
                      Fade((Color){ 12, 10, 14, 255 }, 0.82f * a));
        ui_text_c("Game Saved", (float)SCREEN_W / 2, PY(-30.0f), 40, Fade(C_GOLD, a));
    }
}

/* The Healer opens an interface rather than taking your gold on contact:
   the original's clip sets inventorytype = "Heal" and does
   _root.inventory.gotoAndStop(inventorytype) with _root.pause = true, so
   talking to them brings up a panel you accept or walk away from. */
void ui_scene_heal(Game *g)
{
    Player *p = &g->p;
    Combatant c;
    player_recalc(p, &c);
    int cost = 10 + p->level * 4;
    bool whole = (p->curLife >= c.lifeMax && p->curMana >= c.manaMax);
    bool afford = p->gold >= cost;

    if (ui_input_ready(g)) {
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A) ||
            IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) ||
            IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN))
            g->healSel ^= 1;
        if (IsKeyPressed(KEY_ESCAPE)) {
            go_panel(g, p->zone == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
            return;
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            if (g->healSel == 0 && !whole && afford) {
                p->gold -= cost;
                p->curLife = c.lifeMax;
                p->curMana = c.manaMax;
                sound_play(SFX_HEAL);
                ui_toast(g, "Healed. (-%d gold)", cost);
            } else if (g->healSel == 0 && whole) {
                ui_toast(g, "You are already whole.");
            } else if (g->healSel == 0) {
                ui_toast(g, "You need %d gold.", cost);
            }
            go_panel(g, p->zone == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
            return;
        }
    }

    Rectangle box = { 300, 170, SCREEN_W - 600, 300 };
    ui_panel(box, "Healer");

    char buf[96];
    ui_text("Rest here and be made whole.", box.x + 30, box.y + 56, 20, C_PARCH);

    snprintf(buf, sizeof buf, "Life   %d / %d", p->curLife, c.lifeMax);
    ui_text(buf, box.x + 30, box.y + 100, 19, C_PARCH);
    snprintf(buf, sizeof buf, "Mana   %d / %d", p->curMana, c.manaMax);
    ui_text(buf, box.x + 30, box.y + 126, 19, C_PARCH);

    snprintf(buf, sizeof buf, "Cost   %d gold", cost);
    ui_text(buf, box.x + 30, box.y + 164, 19, afford ? C_GOLD : C_STEEL2);
    snprintf(buf, sizeof buf, "Purse  %d gold", p->gold);
    ui_text(buf, box.x + 30, box.y + 190, 19, C_STEEL2);

    const char *opt0 = whole ? "Already whole" : (afford ? "Accept" : "Cannot pay");
    Color c0 = (whole || !afford) ? C_STEEL2 : C_GOLD;
    ui_text(opt0, box.x + 40, box.y + 236, 22,
            g->healSel == 0 ? c0 : C_PARCH);
    ui_text("Leave", box.x + 240, box.y + 236, 22,
            g->healSel == 1 ? C_GOLD : C_PARCH);
    ui_text(g->healSel == 0 ? ">" : " ", box.x + 22, box.y + 236, 22, C_GOLD);
    ui_text(g->healSel == 1 ? ">" : " ", box.x + 222, box.y + 236, 22, C_GOLD);
    ui_text("ENTER choose   ESC leave", box.x + 30, box.y + box.height - 28, 16, C_STEEL2);
}

/* ---------------------------------------------------------------- title */

void ui_scene_title(Game *g)
{
    art_draw_battle_bg(BG_DARK, g->time);
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 10, 8, 14, 120 });

    /* a lone figure on the ridge */
    Combatant c;
    memset(&c, 0, sizeof c);
    c.look = data_class_look(CLASS_SHADOW);
    c.look.weapon = WEAP_KATANA;
    c.look.weaponTint = C_INK2;
    c.look.helm = HELM_HOOD;
    c.alive = true;
    c.anim = ANIM_STAND;
    art_draw_puppet(&c, (Vector2){ SCREEN_W * 0.76f, 600 }, -1.0f, g->time, 1.7f);

    ui_text_sh("SINJID", 110, 120, 108, C_PARCH);
    ui_text_sh("SHADOW OF THE WARRIOR", 116, 236, 34, C_BLOOD2);
    ui_text("a raylib remake -- all art drawn in code", 118, 282, 18, C_PARCH2);

    const char *opts[3] = { "New Game", "Continue", "About" };
    int n = g->hasSave ? 3 : 2;
    for (int i = 0; i < n; i++) {
        const char *label = g->hasSave ? opts[i] : (i == 0 ? opts[0] : opts[2]);
        ui_button((Rectangle){ 120, 380 + i * 62, 300, 50 }, label, g->titleIdx == i, true);
    }
    ui_text("arrows to choose, ENTER to confirm", 120, 400 + n * 62, 17, C_PARCH2);
}

/* --------------------------------------------------------------- create */

void ui_scene_create(Game *g)
{
    art_draw_battle_bg(BG_VILLAGE, g->time);
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 10, 8, 14, 170 });
    ui_text_sh("CHOOSE YOUR DISCIPLINE", 60, 40, 40, C_GOLD);

    for (int i = 0; i < CLASS_COUNT; i++) {
        Rectangle r = { 60 + i * 300, 110, 280, 60 };
        ui_button(r, CLASS_NAMES[i], g->createIdx == i, true);
    }

    Player tmp;
    memset(&tmp, 0, sizeof tmp);
    tmp.level = 1;
    tmp.cls = (ClassId)g->createIdx;
    data_class_base(&tmp, tmp.cls);
    tmp.look = data_class_look(tmp.cls);
    for (int i = 0; i < SLOT_COUNT; i++) tmp.equip[i] = -1;
    tmp.equip[SLOT_WEAPON] = IT_IRON_KNIFE;
    Combatant c;
    player_recalc(&tmp, &c);
    c.anim = ANIM_STAND;
    art_draw_puppet(&c, (Vector2){ 300, 600 }, 1.0f, g->time, 1.9f);

    Rectangle info = { 540, 200, 680, 300 };
    ui_panel(info, CLASS_NAMES[g->createIdx]);
    float y = info.y + 46;
    char line[128]; int li = 0;
    for (const char *q = CLASS_BLURB[g->createIdx];; q++) {
        if (*q == 0 || *q == '\n') {
            line[li] = 0;
            ui_text(line, info.x + 20, y, 20, C_PARCH);
            y += 28; li = 0;
            if (*q == 0) break;
            continue;
        }
        if (li < 126) line[li++] = *q;
    }
    y += 12;
    char b[128];
    snprintf(b, sizeof b, "Life %d    Mana %d    Speed %d", c.lifeMax, c.manaMax, c.speed);
    ui_text(b, info.x + 20, y, 20, C_GOLD);
    snprintf(b, sizeof b, "Damage %d+%d phys / %d magic     Guard %d",
             c.phyDmg, c.strDmg, c.magDmg, c.shdMax);
    ui_text(b, info.x + 20, y + 28, 20, C_GOLD);

    Rectangle nb = { 540, 530, 680, 90 };
    ui_panel(nb, "NAME");
    char nameLine[64];
    snprintf(nameLine, sizeof nameLine, "%s%s", g->nameBuf,
             (g->createField == 1 && fmodf(g->time, 1.0f) < 0.5f) ? "_" : "");
    ui_text(nameLine, nb.x + 20, nb.y + 42, 26, g->createField == 1 ? C_PARCH : C_PARCH2);

    ui_text(g->createField == 0 ? "left/right to pick a discipline, ENTER to name your warrior"
                                : "type a name, ENTER to begin, ESC to go back",
            60, SCREEN_H - 40, 19, C_PARCH2);
}


/* ---------------------------------------------------------------- portals */

/* The three roads out of the valley.  Each is a ladder of the original's own
   encounters, and each remembers how far you have pushed down it. */
void ui_scene_portal(Game *g)
{
    Player *p = &g->p;
    if (ui_input_ready(g)) {
        if (IsKeyPressed(KEY_ESCAPE)) { go_panel(g, SCENE_VILLAGE); return; }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) g->portalIdx = (g->portalIdx + 1) % 3;
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) g->portalIdx = (g->portalIdx + 2) % 3;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            int idx = g->portalIdx;
            int lvl = p->portalLevel[idx];
            if (lvl >= portal_stage_count(idx)) {
                ui_toast(g, "%s is walked out to its end.", PORTALS[idx].name);
            } else {
                const PortalStage *st = &PORTAL_STAGES[PORTALS[idx].first + lvl];
                int defs[MAX_FOES]; int n = 0;
                defs[n++] = st->enemyA;
                if (st->enemyB >= 0) defs[n++] = st->enemyB;
                g->portalStage = lvl;
                battle_start(g, defs, n, p->level, false, false);
                g->b.isPortal = true;
                g->b.canFlee = true;
            }
        }
    }

    art_draw_battle_bg(BG_DARK, g->time);
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 10, 8, 14, 200 });
    ui_text("THE THREE ROADS", 60, 40, 40, C_GOLD);
    ui_text("Each road runs deeper than the last. You keep your place on every one.",
            60, 88, 19, C_PARCH2);

    for (int i = 0; i < 3; i++) {
        Rectangle r = { 80, 150 + i * 130, SCREEN_W - 160, 110 };
        bool sel = (g->portalIdx == i);
        DrawRectangleRounded(r, 0.08f, 8, sel ? (Color){ 46, 38, 26, 240 }
                                              : (Color){ 20, 18, 24, 220 });
        DrawRectangleRoundedLines(r, 0.08f, 8, sel ? C_GOLD : (Color){ 70, 62, 46, 220 });
        ui_text(PORTALS[i].name, r.x + 24, r.y + 16, 28, sel ? C_PARCH : C_PARCH2);
        int total = portal_stage_count(i), done = p->portalLevel[i];
        char b[96];
        snprintf(b, sizeof b, "stage %d of %d", done < total ? done + 1 : total, total);
        ui_text(b, r.x + 24, r.y + 56, 20, C_GOLD);
        /* progress pips */
        for (int s = 0; s < total; s++) {
            float x = r.x + 300 + s * 22, y = r.y + 66;
            DrawRectangleRounded((Rectangle){ x, y, 16, 16 }, 0.3f, 4,
                                 s < done ? C_JADE : (Color){ 40, 36, 44, 255 });
        }
        if (done >= total)
            ui_text("walked out", r.x + r.width - 150, r.y + 56, 20, C_JADE);
    }
    ui_text("ENTER walk the next stage    ESC leave", 60, SCREEN_H - 44, 19, C_PARCH2);
}

/* --------------------------------------------------------------- training */

/* The original's trainer burns energy for experience: every tick spends
   engrate energy and banks ceil(exprate * 2) experience until you run dry. */
void ui_scene_training(Game *g)
{
    Player *p = &g->p;
    Combatant c;
    player_recalc(p, &c);

    if (ui_input_ready(g)) {
        if (IsKeyPressed(KEY_ESCAPE)) { go_panel(g, SCENE_VILLAGE); return; }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            if (p->curEnergy <= 0) ui_toast(g, "You have nothing left to burn.");
            else {
                int rate = 4 + p->level;
                int spend = p->curEnergy < rate ? p->curEnergy : rate;
                p->curEnergy -= spend;
                int gained = (spend * 2 + 1) / 2 + p->level;
                g->trainGain += gained;
                g->trainT = 0.6f;
                player_gain_exp(g, gained);
            }
        }
    }
    if (g->trainT > 0) g->trainT -= GetFrameTime();

    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 10, 8, 14, 210 });
    ui_text("TRAINING GROUND", 60, 40, 36, C_GOLD);
    ui_text("Work the post until the breath runs out. Energy buys experience.",
            60, 86, 19, C_PARCH2);

    Rectangle bar = { 60, 200, SCREEN_W - 120, 34 };
    ui_bar(bar, c.engMax ? (float)p->curEnergy / c.engMax : 0, C_GOLD,
           (Color){ 24, 22, 28, 220 }, NULL);
    char b[96];
    snprintf(b, sizeof b, "ENERGY  %d / %d", p->curEnergy, c.engMax);
    ui_text(b, bar.x + 8, bar.y + 6, 20, C_INK);

    snprintf(b, sizeof b, "Experience  %d / %d      banked this session: %d",
             p->exp, p->expNext, g->trainGain);
    ui_text(b, 60, 260, 22, C_PARCH);

    /* the training post takes the hits */
    Combatant post;
    memset(&post, 0, sizeof post);
    post.look = data_class_look(CLASS_WARRIOR);
    post.look.cloth = (Color){ 120, 96, 64, 255 };
    post.look.weapon = WEAP_NONE;
    post.alive = true;
    post.anim = g->trainT > 0 ? ANIM_HIT : ANIM_STAND;
    post.animT = g->trainT > 0 ? (0.6f - g->trainT) : g->time;
    art_draw_puppet(&post, (Vector2){ SCREEN_W * 0.72f, 600 }, -1.0f, post.animT, 1.5f);

    Combatant hero;
    player_recalc(p, &hero);
    hero.anim = g->trainT > 0 ? ANIM_ATTACK : ANIM_STAND;
    hero.animT = g->trainT > 0 ? (0.6f - g->trainT) : g->time;
    art_draw_puppet(&hero, (Vector2){ SCREEN_W * 0.42f, 600 }, 1.0f, hero.animT, 1.5f);

    ui_text(p->curEnergy > 0 ? "ENTER strike the post    ESC leave"
                             : "You are spent. Rest before you train again.    ESC leave",
            60, SCREEN_H - 44, 20, p->curEnergy > 0 ? C_GOLD : C_BLOOD2);
}
