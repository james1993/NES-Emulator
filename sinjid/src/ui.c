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
        return MeasureTextEx(G.font, s, size, size * 0.06f).x;
    return MeasureTextEx(GetFontDefault(), s, size, size * 0.1f).x;
}

void ui_text(const char *s, float x, float y, float size, Color col)
{
    if (G.fontLoaded) DrawTextEx(G.font, s, (Vector2){ x, y }, size, size * 0.06f, col);
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
        { "Strength",    it->str },     { "Phys dmg",   it->phyDmg },
        { "Phys def %",  it->phyDef },  { "Magic dmg",  it->magDmg },
        { "Magic def",   it->magDef },  { "Shield pts", it->shdPts },
        { "Shd p.def %", it->shdPhyDef }, { "Shd m.def %", it->shdMagDef },
        { "Shd damage",  it->shdDmg },  { "Speed",      it->speed },
        { "Avoidance",   it->avoid },
    };
    for (unsigned i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        if (!rows[i].v) continue;
        snprintf(b, sizeof b, "%-11s %+d", rows[i].tag, rows[i].v);
        ui_text(b, at.x + 14, y, 17, rows[i].v > 0 ? C_JADE : C_BLOOD2);
        y += 20;
        if (y > at.y + at.height - 46) break;
    }
    if (it->note) ui_text(it->note, at.x + 14, at.y + at.height - 30, 15, C_PARCH2);
}

/* ------------------------------------------------------------ inventory */

static const char *SLOT_NAMES[SLOT_COUNT] = { "Weapon", "Shield", "Armour", "Helm", "Relic" };

static void draw_hero_preview(Game *g, Vector2 at, float scale)
{
    Combatant c;
    player_recalc(&g->p, &c);
    c.anim = ANIM_STAND;
    art_draw_puppet(&c, at, 1.0f, g->time, scale);
}

void ui_scene_menu(Game *g)
{
    Player *p = &g->p;
    Combatant c;
    player_recalc(p, &c);
    if (p->curLife > 0 && p->curLife < c.lifeMax) c.life = p->curLife;
    if (p->curMana < c.manaMax) c.mana = p->curMana;

    /* ---- input ---- */
    if (g->fadeDir <= 0) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_I)) {
            go_scene(g, p->zone == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
            return;
        }
        if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_E)) { g->menuTab = (g->menuTab + 1) % 3; g->menuIdx = 0; }
        if (IsKeyPressed(KEY_Q)) { g->menuTab = (g->menuTab + 2) % 3; g->menuIdx = 0; }

        int n = 0;
        if (g->menuTab == 0) n = SLOT_COUNT;
        else if (g->menuTab == 1) n = p->invCount;
        else n = MAX_SKILLS;
        if (n > 0) {
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) g->menuIdx = (g->menuIdx + 1) % n;
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) g->menuIdx = (g->menuIdx + n - 1) % n;
        }
        if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) {
            if (g->menuTab == 1 && g->menuIdx < p->invCount) {
                const ItemDef *it = &ITEMS[p->inv[g->menuIdx].def];
                if (it->type == ITEM_CONSUMABLE) {
                    c.life = p->curLife > 0 ? p->curLife : c.lifeMax;
                    c.mana = p->curMana;
                    if (use_consumable(g, g->menuIdx, &c)) {
                        p->curLife = c.life;
                        p->curMana = c.mana;
                    }
                } else {
                    player_equip(p, g->menuIdx);
                    ui_toast(g, "Equipped %s.", it->name);
                    if (g->menuIdx >= p->invCount && g->menuIdx > 0) g->menuIdx--;
                }
            } else if (g->menuTab == 0) {
                int slotId = g->menuIdx;
                if (p->equip[slotId] >= 0) {
                    if (player_add_item(p, p->equip[slotId]) >= 0) {
                        ui_toast(g, "Removed %s.", ITEMS[p->equip[slotId]].name);
                        p->equip[slotId] = -1;
                    } else ui_toast(g, "No room in the pack.");
                }
            }
        }
    }

    /* ---- draw ---- */
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 10, 8, 14, 205 });
    const char *tabs[3] = { "CHARACTER", "PACK", "SKILLS" };
    for (int i = 0; i < 3; i++) {
        Rectangle r = { 40 + i * 190, 30, 180, 40 };
        ui_button(r, tabs[i], g->menuTab == i, true);
    }
    ui_text("TAB switch    ENTER use/equip    ESC back", 640, 42, 17, C_PARCH2);

    Rectangle left = { 40, 90, 560, 590 };
    Rectangle right = { 620, 90, 620, 590 };

    if (g->menuTab == 0) {
        ui_panel(left, "EQUIPMENT");
        for (int i = 0; i < SLOT_COUNT; i++) {
            Rectangle r = { left.x + 16, left.y + 46 + i * 52, left.width - 32, 44 };
            char lab[96];
            snprintf(lab, sizeof lab, "%-8s %s", SLOT_NAMES[i],
                     p->equip[i] >= 0 ? ITEMS[p->equip[i]].name : "--");
            ui_button(r, lab, g->menuIdx == i, true);
        }
        float y = left.y + 46 + SLOT_COUNT * 52 + 16;
        char b[96];
        snprintf(b, sizeof b, "%s   Level %d   %s", p->name, p->level, CLASS_NAMES[p->cls]);
        ui_text(b, left.x + 16, y, 22, C_GOLD); y += 34;
        snprintf(b, sizeof b, "Experience %d / %d", p->exp, p->expNext);
        ui_text(b, left.x + 16, y, 18, C_PARCH2); y += 26;
        snprintf(b, sizeof b, "Gold %d", p->gold);
        ui_text(b, left.x + 16, y, 18, C_GOLD); y += 26;
        if (p->statPts || p->skillPts) {
            snprintf(b, sizeof b, "Unspent: %d stat, %d skill  (T at the trainer)",
                     p->statPts, p->skillPts);
            ui_text(b, left.x + 16, y, 18, C_JADE);
        }

        ui_panel(right, "STATISTICS");
        struct { const char *tag; int v; int pct; } st[] = {
            { "Life",             c.lifeMax,   0 },
            { "Mana",             c.manaMax,   0 },
            { "Strength",         c.str,       0 },
            { "Weapon damage",    c.phyDmg,    0 },
            { "Strength damage",  c.strDmg,    0 },
            { "Magic damage",     c.magDmg,    0 },
            { "Physical defence", c.phyDef,    1 },
            { "Magic defence",    c.magDef,    1 },
            { "Shield points",    c.shdMax,    0 },
            { "Shield p.defence", c.shdPhyDef, 1 },
            { "Shield m.defence", c.shdMagDef, 1 },
            { "Shield damage",    c.shdDmg,    0 },
            { "Speed",            c.speed,     0 },
        };
        for (unsigned i = 0; i < sizeof st / sizeof st[0]; i++) {
            char b2[96];
            if (st[i].pct) snprintf(b2, sizeof b2, "%-18s %4d%%", st[i].tag, st[i].v);
            else           snprintf(b2, sizeof b2, "%-18s %5d", st[i].tag, st[i].v);
            ui_text(b2, right.x + 20, right.y + 46 + i * 26, 19, C_PARCH);
        }
        draw_hero_preview(g, (Vector2){ right.x + right.width - 130, right.y + 470 }, 1.15f);
    } else if (g->menuTab == 1) {
        ui_panel(left, "PACK");
        if (p->invCount == 0)
            ui_text("Empty.", left.x + 20, left.y + 50, 20, C_PARCH2);
        int top = g->menuIdx - 9; if (top < 0) top = 0;
        for (int i = top; i < p->invCount && i < top + 11; i++) {
            const ItemDef *it = &ITEMS[p->inv[i].def];
            char lab[96];
            if (p->inv[i].count > 1)
                snprintf(lab, sizeof lab, "%s  x%d", it->name, p->inv[i].count);
            else snprintf(lab, sizeof lab, "%s", it->name);
            ui_button((Rectangle){ left.x + 16, left.y + 44 + (i - top) * 48, left.width - 32, 42 },
                      lab, g->menuIdx == i, true);
        }
        if (p->invCount > 0 && g->menuIdx < p->invCount)
            ui_tooltip_item(&ITEMS[p->inv[g->menuIdx].def],
                            (Rectangle){ right.x, right.y, right.width, 380 });
        draw_hero_preview(g, (Vector2){ right.x + right.width / 2, right.y + 570 }, 1.2f);
    } else {
        ui_panel(left, "SKILLS");
        int top = g->menuIdx - 9; if (top < 0) top = 0;
        for (int i = top; i < MAX_SKILLS && i < top + 11; i++) {
            char lab[96];
            snprintf(lab, sizeof lab, "%-17s %s", SKILLS[i].name,
                     p->skillRank[i] > 0
                        ? TextFormat("rank %d/%d", p->skillRank[i], SKILLS[i].maxRank)
                        : (p->level >= SKILLS[i].reqLevel ? "not learned"
                                                          : TextFormat("locked (Lv%d)",
                                                                       SKILLS[i].reqLevel)));
            ui_button((Rectangle){ left.x + 16, left.y + 44 + (i - top) * 48, left.width - 32, 42 },
                      lab, g->menuIdx == i, p->skillRank[i] > 0);
        }
        const SkillDef *sk = &SKILLS[g->menuIdx];
        ui_panel(right, "DETAIL");
        ui_text(sk->name, right.x + 20, right.y + 42, 26, C_GOLD);
        ui_text(sk->tree == 0 ? "Combat discipline" : "Ki discipline",
                right.x + 20, right.y + 76, 17, C_PARCH2);
        /* wrapped description */
        {
            char line[80]; int li = 0, ly = 0;
            for (const char *q = sk->desc;; q++) {
                if (*q == 0 || (li > 44 && *q == ' ')) {
                    line[li] = 0;
                    ui_text(line, right.x + 20, right.y + 110 + ly * 24, 19, C_PARCH);
                    ly++; li = 0;
                    if (*q == 0) break;
                    continue;
                }
                if (li < 78) line[li++] = *q;
            }
        }
        char b[96];
        float y = right.y + 200;
        snprintf(b, sizeof b, "Requires level %d", sk->reqLevel);
        ui_text(b, right.x + 20, y, 18, C_PARCH2); y += 26;
        if (sk->manaCost) { snprintf(b, sizeof b, "Mana cost %d", sk->manaCost);
                            ui_text(b, right.x + 20, y, 18, C_KI); y += 26; }
        if (sk->engCost) { snprintf(b, sizeof b, "Energy cost %d", sk->engCost);
                           ui_text(b, right.x + 20, y, 18, C_GOLD); y += 26; }
        if (sk->power > 0) {
            float pw = sk->power + sk->powerPerRank * (p->skillRank[g->menuIdx] > 0 ?
                                                       p->skillRank[g->menuIdx] - 1 : 0);
            snprintf(b, sizeof b, "Power %.0f%% of %s damage", pw * 100,
                     sk->dmgType == DMG_MAGIC ? "magic" :
                     (sk->dmgType == DMG_PURE ? "best" : "physical"));
            ui_text(b, right.x + 20, y, 18, C_PARCH); y += 26;
        }
        static const struct { unsigned f; const char *s; } FL[] = {
            { SKF_MULTI,      "Strikes twice" },
            { SKF_IGNORE_SHD, "Ignores shield points" },
            { SKF_SHIELD_DMG, "Triple damage to shields" },
            { SKF_DRAIN,      "Returns life" },
            { SKF_STUN,       "May stun" },
            { SKF_NEVER_MISS, "Never misses" },
            { SKF_HEAL,       "Restores life" },
            { SKF_SHIELD_UP,  "Braces and mends shields" },
            { SKF_BUFF_DEF,   "Raises defence" },
            { SKF_BUFF_ATK,   "Raises damage" },
            { SKF_RESTORE_MP, "Restores mana and energy" },
        };
        for (unsigned i = 0; i < sizeof FL / sizeof FL[0]; i++)
            if (sk->flags & FL[i].f) { ui_text(FL[i].s, right.x + 20, y, 18, C_JADE); y += 24; }
    }
}

/* ---------------------------------------------------------------- shop */

void ui_scene_shop(Game *g)
{
    Player *p = &g->p;
    const int *stock;
    int n = data_shop_table(g->shopVendor, &stock);

    if (g->fadeDir <= 0) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            go_scene(g, p->zone == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
            return;
        }
        if (IsKeyPressed(KEY_TAB)) { g->shopMode = !g->shopMode; g->shopIdx = 0; }
        int count = g->shopMode ? p->invCount : n;
        if (count > 0) {
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) g->shopIdx = (g->shopIdx + 1) % count;
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) g->shopIdx = (g->shopIdx + count - 1) % count;
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                if (!g->shopMode) {
                    const ItemDef *it = &ITEMS[stock[g->shopIdx]];
                    if (p->gold < it->price) ui_toast(g, "Not enough gold.");
                    else if (player_add_item(p, stock[g->shopIdx]) < 0) ui_toast(g, "Your pack is full.");
                    else { p->gold -= it->price; ui_toast(g, "Bought %s.", it->name); }
                } else if (p->invCount > 0) {
                    int def = p->inv[g->shopIdx].def;
                    int price = ITEMS[def].price / 2;
                    p->gold += price;
                    p->inv[g->shopIdx].count--;
                    if (p->inv[g->shopIdx].count <= 0) {
                        for (int i = g->shopIdx; i + 1 < p->invCount; i++) p->inv[i] = p->inv[i + 1];
                        p->invCount--;
                        if (g->shopIdx >= p->invCount && g->shopIdx > 0) g->shopIdx--;
                    }
                    ui_toast(g, "Sold %s for %d gold.", ITEMS[def].name, price);
                }
            }
        }
    }

    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 10, 8, 14, 210 });
    Rectangle left = { 40, 60, 560, 620 }, right = { 620, 60, 620, 400 };
    ui_panel(left, g->shopMode ? "SELLING  (TAB to buy)" : "FOR SALE  (TAB to sell)");
    char gold[64];
    snprintf(gold, sizeof gold, "%d gold", p->gold);
    ui_text(gold, left.x + left.width - 140, left.y + 7, 18, C_GOLD);

    int count = g->shopMode ? p->invCount : n;
    int top = g->shopIdx - 10; if (top < 0) top = 0;
    for (int i = top; i < count && i < top + 12; i++) {
        int def = g->shopMode ? p->inv[i].def : stock[i];
        int price = g->shopMode ? ITEMS[def].price / 2 : ITEMS[def].price;
        char lab[96];
        snprintf(lab, sizeof lab, "%-20s %5dg", ITEMS[def].name, price);
        bool afford = g->shopMode || p->gold >= price;
        ui_button((Rectangle){ left.x + 16, left.y + 42 + (i - top) * 46, left.width - 32, 40 },
                  lab, g->shopIdx == i, afford);
    }
    if (count == 0) ui_text("Nothing here.", left.x + 20, left.y + 50, 20, C_PARCH2);
    else {
        int def = g->shopMode ? p->inv[g->shopIdx].def : stock[g->shopIdx];
        ui_tooltip_item(&ITEMS[def], right);
        /* what it would change if worn */
        const ItemDef *it = &ITEMS[def];
        if (it->type != ITEM_CONSUMABLE) {
            SlotId s = it->type == ITEM_WEAPON ? SLOT_WEAPON :
                       it->type == ITEM_SHIELD ? SLOT_SHIELD :
                       it->type == ITEM_ARMOUR ? SLOT_ARMOUR :
                       it->type == ITEM_HELM   ? SLOT_HELM : SLOT_RELIC;
            Rectangle cmp = { right.x, right.y + 420, right.width, 200 };
            ui_panel(cmp, "COMPARED WITH WORN");
            if (p->equip[s] >= 0) {
                const ItemDef *cur = &ITEMS[p->equip[s]];
                ui_text(cur->name, cmp.x + 14, cmp.y + 40, 19, C_PARCH);
                struct { const char *t; int a, b; } d[] = {
                    { "life", it->lifeMax, cur->lifeMax }, { "p.dmg", it->phyDmg, cur->phyDmg },
                    { "p.def", it->phyDef, cur->phyDef },  { "m.dmg", it->magDmg, cur->magDmg },
                    { "shield", it->shdPts, cur->shdPts }, { "speed", it->speed, cur->speed },
                };
                float x = cmp.x + 14;
                for (unsigned i = 0; i < sizeof d / sizeof d[0]; i++) {
                    int diff = d[i].a - d[i].b;
                    if (!diff) continue;
                    char b2[48];
                    snprintf(b2, sizeof b2, "%s %+d", d[i].t, diff);
                    ui_text(b2, x, cmp.y + 70, 18, diff > 0 ? C_JADE : C_BLOOD2);
                    x += ui_text_w(b2, 18) + 18;
                    if (x > cmp.x + cmp.width - 90) { x = cmp.x + 14; }
                }
            } else ui_text("That slot is empty.", cmp.x + 14, cmp.y + 40, 19, C_PARCH2);
        }
    }
    ui_text("ENTER trade    TAB switch    ESC leave", 40, SCREEN_H - 34, 18, C_PARCH2);
}

/* ------------------------------------------------------------- training */

void ui_scene_train(Game *g)
{
    Player *p = &g->p;
    static const char *STAT_NAMES[7] = { "Life", "Mana", "Strength", "Defence",
                                         "Magic", "Magic defence", "Speed" };
    int *statPtr[7] = { &p->baseLife, &p->baseMana, &p->baseStr, &p->baseDef,
                        &p->baseMag, &p->baseMagDef, &p->baseSpeed };
    const int STEP[7] = { 6, 5, 1, 1, 1, 1, 1 };

    if (g->fadeDir <= 0) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_T)) {
            go_scene(g, p->zone == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
            return;
        }
        if (IsKeyPressed(KEY_TAB)) { g->menuTab = !g->menuTab; g->menuIdx = 0; }
        int n = g->menuTab ? MAX_SKILLS : 7;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) g->menuIdx = (g->menuIdx + 1) % n;
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) g->menuIdx = (g->menuIdx + n - 1) % n;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            if (!g->menuTab) {
                if (p->statPts > 0) {
                    *statPtr[g->menuIdx] += STEP[g->menuIdx];
                    p->statPts--;
                    Combatant c; player_recalc(p, &c);
                    if (p->curLife > 0 && g->menuIdx == 0) p->curLife += STEP[0] * 3;
                    ui_toast(g, "%s raised.", STAT_NAMES[g->menuIdx]);
                } else ui_toast(g, "No stat points left.");
            } else {
                int id = g->menuIdx;
                if (p->skillPts <= 0) ui_toast(g, "No skill points left.");
                else if (p->level < SKILLS[id].reqLevel)
                    ui_toast(g, "%s needs level %d.", SKILLS[id].name, SKILLS[id].reqLevel);
                else if (p->skillRank[id] >= SKILLS[id].maxRank)
                    ui_toast(g, "%s is already mastered.", SKILLS[id].name);
                else {
                    p->skillRank[id]++;
                    p->skillPts--;
                    ui_toast(g, "%s -- rank %d.", SKILLS[id].name, p->skillRank[id]);
                }
            }
        }
    }

    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 10, 8, 14, 210 });
    ui_text("TRAINING", 40, 24, 30, C_GOLD);
    char b[128];
    snprintf(b, sizeof b, "%d stat points     %d skill points", p->statPts, p->skillPts);
    ui_text(b, 40, 62, 20, p->statPts || p->skillPts ? C_JADE : C_PARCH2);
    ui_text("TAB switch    ENTER spend    ESC leave", SCREEN_W - 440, 30, 18, C_PARCH2);

    Rectangle left = { 40, 100, 520, 560 }, right = { 580, 100, 660, 560 };
    ui_panel(left, g->menuTab ? "SKILLS" : "ATTRIBUTES");
    if (!g->menuTab) {
        for (int i = 0; i < 7; i++) {
            char lab[96];
            snprintf(lab, sizeof lab, "%-16s %4d   (+%d)", STAT_NAMES[i], *statPtr[i], STEP[i]);
            ui_button((Rectangle){ left.x + 16, left.y + 46 + i * 56, left.width - 32, 46 },
                      lab, g->menuIdx == i, p->statPts > 0);
        }
        Combatant c;
        player_recalc(p, &c);
        float y = left.y + 46 + 7 * 56 + 16;
        snprintf(b, sizeof b, "Life %d    Mana %d    Speed %d", c.lifeMax, c.manaMax, c.speed);
        ui_text(b, left.x + 16, y, 19, C_PARCH);
        snprintf(b, sizeof b, "Damage %d weapon + %d str / %d magic",
                 c.phyDmg, c.strDmg, c.magDmg);
        ui_text(b, left.x + 16, y + 26, 19, C_PARCH);
    } else {
        int top = g->menuIdx - 8; if (top < 0) top = 0;
        for (int i = top; i < MAX_SKILLS && i < top + 10; i++) {
            char lab[96];
            bool ok = p->level >= SKILLS[i].reqLevel;
            snprintf(lab, sizeof lab, "%-16s %d/%d%s", SKILLS[i].name, p->skillRank[i],
                     SKILLS[i].maxRank, ok ? "" : "  locked");
            ui_button((Rectangle){ left.x + 16, left.y + 44 + (i - top) * 50, left.width - 32, 44 },
                      lab, g->menuIdx == i, ok);
        }
    }

    ui_panel(right, "MASTER RENJIRO");
    if (!g->menuTab) {
        const char *tips[7] = {
            "Life is the only stat that stops a killing blow.",
            "Mana feeds every ki discipline you know.",
            "Strength drives every physical strike you land.",
            "Defence cuts a percentage off every blow that lands.",
            "Magic raises spell damage and everything you mend.",
            "Magic defence blunts spellwork and shadow.",
            "Speed decides who moves first, and how often.",
        };
        ui_text(STAT_NAMES[g->menuIdx], right.x + 20, right.y + 46, 26, C_GOLD);
        ui_text(tips[g->menuIdx], right.x + 20, right.y + 84, 19, C_PARCH);
    } else {
        const SkillDef *sk = &SKILLS[g->menuIdx];
        ui_text(sk->name, right.x + 20, right.y + 46, 26, C_GOLD);
        char line[80]; int li = 0, ly = 0;
        for (const char *q = sk->desc;; q++) {
            if (*q == 0 || (li > 40 && *q == ' ')) {
                line[li] = 0;
                ui_text(line, right.x + 20, right.y + 88 + ly * 24, 19, C_PARCH);
                ly++; li = 0;
                if (*q == 0) break;
                continue;
            }
            if (li < 78) line[li++] = *q;
        }
        snprintf(b, sizeof b, "Requires level %d   rank %d of %d",
                 sk->reqLevel, p->skillRank[g->menuIdx], sk->maxRank);
        ui_text(b, right.x + 20, right.y + 180, 18, C_PARCH2);
    }
    draw_hero_preview(g, (Vector2){ right.x + right.width / 2, right.y + right.height - 40 }, 1.3f);
}

/* --------------------------------------------------------------- dialog */

void ui_scene_dialog(Game *g)
{
    Zone *z = &g->zones[g->p.zone];
    if (g->dialogNpc < 0 || g->dialogNpc >= z->npcCount) {
        go_scene(g, g->p.zone == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
        return;
    }
    Npc *n = &z->npcs[g->dialogNpc];
    if (g->fadeDir <= 0 && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
                            IsKeyPressed(KEY_ESCAPE))) {
        go_scene(g, g->p.zone == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
        return;
    }
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 10, 8, 14, 150 });
    Rectangle box = { 120, SCREEN_H - 250, SCREEN_W - 240, 200 };
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

/* ---------------------------------------------------------------- title */

void ui_scene_title(Game *g)
{
    art_draw_battle_bg(ZONE_SHADOW, g->time);
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
    art_draw_battle_bg(ZONE_VILLAGE, g->time);
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
    switch (tmp.cls) {
    case CLASS_WARRIOR: tmp.equip[SLOT_WEAPON] = 4; tmp.equip[SLOT_SHIELD] = 16; break;
    case CLASS_SHADOW:  tmp.equip[SLOT_WEAPON] = 2; tmp.equip[SLOT_SHIELD] = 15; break;
    case CLASS_MYSTIC:  tmp.equip[SLOT_WEAPON] = 3; tmp.equip[SLOT_SHIELD] = 15; break;
    default:            tmp.equip[SLOT_WEAPON] = 0; tmp.equip[SLOT_SHIELD] = 15; break;
    }
    tmp.equip[SLOT_ARMOUR] = 24;
    tmp.equip[SLOT_HELM] = 32;
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
