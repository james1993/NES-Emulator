/* ===========================================================================
   main.c -- window, game loop, scene dispatch, character maths, save files.
   =========================================================================== */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

Game G;

/* ------------------------------------------------------------------ util */
int rnd(int lo, int hi)
{
    if (hi <= lo) return lo;
    return lo + GetRandomValue(0, hi - lo);
}

float frnd(float lo, float hi)
{
    return lo + (hi - lo) * (GetRandomValue(0, 10000) / 10000.0f);
}

float approach(float v, float target, float rate)
{
    if (v < target) { v += rate; if (v > target) v = target; }
    else            { v -= rate; if (v < target) v = target; }
    return v;
}

Color lerp_col(Color a, Color b, float t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return (Color){ (unsigned char)(a.r + (b.r - a.r) * t),
                    (unsigned char)(a.g + (b.g - a.g) * t),
                    (unsigned char)(a.b + (b.b - a.b) * t),
                    (unsigned char)(a.a + (b.a - a.a) * t) };
}

void ui_toast(Game *g, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g->toast, sizeof g->toast, fmt, ap);
    va_end(ap);
    g->toastT = 2.6f;
}


/* ------------------------------------------------------- scripted input --
   `./sinjid --script "ret,wait20,right,right,shot:create,..."` replays a key
   sequence and writes screenshots, so the game can be driven head-lessly in
   CI or for capturing stills.  Tokens:
       <key>       one press, held HOLD_FRAMES frames
       waitN       idle for N frames
       shot:name   write name.png
       quit        close the window
   Recognised keys: up down left right ret esc space tab i t q e a-z digits. */
#undef IsKeyPressed
#undef IsKeyDown
#undef GetCharPressed

#define SCRIPT_MAX 256
#define HOLD_FRAMES 8
#define GAP_FRAMES  10

typedef struct { int key; int wait; char shot[64]; bool quit; } ScriptStep;
static ScriptStep SCRIPT[SCRIPT_MAX];
static int  scriptLen = 0, scriptAt = 0, scriptFrame = 0;
static bool scriptOn = false;

static int key_from_name(const char *s)
{
    if (!strcmp(s, "up")) return KEY_UP;
    if (!strcmp(s, "down")) return KEY_DOWN;
    if (!strcmp(s, "left")) return KEY_LEFT;
    if (!strcmp(s, "right")) return KEY_RIGHT;
    if (!strcmp(s, "ret") || !strcmp(s, "enter")) return KEY_ENTER;
    if (!strcmp(s, "esc")) return KEY_ESCAPE;
    if (!strcmp(s, "space")) return KEY_SPACE;
    if (!strcmp(s, "tab")) return KEY_TAB;
    if (s[0] >= 'a' && s[0] <= 'z' && s[1] == 0) return KEY_A + (s[0] - 'a');
    if (s[0] >= '0' && s[0] <= '9' && s[1] == 0) return KEY_ZERO + (s[0] - '0');
    return 0;
}

static void script_parse(const char *spec)
{
    char buf[4096];
    snprintf(buf, sizeof buf, "%s", spec);
    char *tok = strtok(buf, ",");
    while (tok && scriptLen < SCRIPT_MAX) {
        ScriptStep *st = &SCRIPT[scriptLen];
        memset(st, 0, sizeof *st);
        while (*tok == ' ') tok++;
        if (!strncmp(tok, "wait", 4)) st->wait = atoi(tok + 4);
        else if (!strncmp(tok, "shot:", 5)) snprintf(st->shot, sizeof st->shot, "%s", tok + 5);
        else if (!strcmp(tok, "quit")) st->quit = true;
        else st->key = key_from_name(tok);
        scriptLen++;
        tok = strtok(NULL, ",");
    }
    scriptOn = scriptLen > 0;
}

/* Advances one step per frame budget; returns true while the script runs. */
static void script_tick(void)
{
    if (!scriptOn || scriptAt >= scriptLen) return;
    ScriptStep *st = &SCRIPT[scriptAt];
    scriptFrame++;
    int budget = st->wait ? st->wait : (HOLD_FRAMES + GAP_FRAMES);
    if (st->shot[0] && scriptFrame == 1) {
        char path[96];
        snprintf(path, sizeof path, "%s.png", st->shot);
        TakeScreenshot(path);
        budget = 2;
    }
    if (st->quit) budget = 1;
    if (scriptFrame >= budget) { scriptAt++; scriptFrame = 0; }
}

static bool script_done(void) { return scriptOn && scriptAt >= scriptLen; }

bool sj_key_pressed(int key)
{
    if (!scriptOn) return IsKeyPressed(key);
    if (scriptAt >= scriptLen) return false;
    ScriptStep *st = &SCRIPT[scriptAt];
    return st->key == key && scriptFrame == 1;
}

bool sj_key_down(int key)
{
    if (!scriptOn) return IsKeyDown(key);
    if (scriptAt >= scriptLen) return false;
    ScriptStep *st = &SCRIPT[scriptAt];
    return st->key == key && scriptFrame <= HOLD_FRAMES;
}

int sj_char_pressed(void)
{
    if (!scriptOn) return GetCharPressed();
    return 0;
}

#define IsKeyPressed(k)  sj_key_pressed(k)
#define IsKeyDown(k)     sj_key_down(k)
#define GetCharPressed() sj_char_pressed()

/* --------------------------------------------------------- player maths */

/* The original's curve: the next level always costs 50 x your current level. */
int player_exp_for_level(int lvl)
{
    return 50 * (lvl < 1 ? 1 : lvl);
}

/* Rolls the player's allocated stats, gear and class into a battle-ready
   Combatant.  Everything the battle code reads lives on that struct. */
void player_recalc(Player *p, Combatant *out)
{
    memset(out, 0, sizeof *out);
    snprintf(out->name, sizeof out->name, "%s", p->name);
    out->isHero = true;
    out->alive  = true;
    out->level  = p->level;

    const ClassGrowth *g = &CLASS_GROWTH[p->cls];
    int lv = p->level - 1;                      /* growth applies past level 1 */
    int life = p->baseLife + g->tough * 15 * lv;
    int mana = p->baseMana + g->magDmg * 2 * lv;
    int str  = p->baseStr;
    int pdmg = p->basePhyDmg + g->phyDmg * lv, pdef = p->basePhyDef;
    int mdmg = p->baseMagDmg + g->magDmg * lv, mdef = p->baseMagDef;
    int shd = p->baseShdPts, shdPDef = 0, shdMDef = 0, shdDmg = 0;
    int spd = p->baseSpeed + g->speed * lv, avd = 0;

    Look lk = p->look;
    lk.weapon = WEAP_NONE; lk.shield = SHLD_NONE; lk.helm = HELM_NONE;

    for (int s = 0; s < SLOT_COUNT; s++) {
        int id = p->equip[s];
        if (id < 0 || id >= ITEM_COUNT) continue;
        const ItemDef *it = &ITEMS[id];
        life += it->lifeMax; mana += it->manaMax;
        pdmg += it->phyDmg;  pdef += it->phyDef;
        mdmg += it->magDmg;  mdef += it->magDef;
        shd  += it->shdPts;  shdPDef += it->shdPhyDef; shdMDef += it->shdMagDef;
        shdDmg += it->shdDmg; spd += it->speed;
        if (it->type == ITEM_WEAPON) { lk.weapon = it->shape; lk.weaponTint = it->tint; }
        if (it->type == ITEM_SHIELD) { lk.shield = it->shape; lk.shieldTint = it->tint; }
        if (it->type == ITEM_HELM)   { lk.helm   = it->shape; }
        if (it->type == ITEM_ARMOUR) { lk.cloth  = it->tint;
                                       lk.clothDark = art_shade(it->tint, 0.62f); }
    }


    out->lifeMax = life;   out->life = life;
    out->manaMax = mana;   out->mana = mana;
    out->engMax  = p->baseEng;  out->eng = p->baseEng * 2 / 5;
    out->engRate = 18 + p->level + (p->cls == CLASS_BALANCED ? 6 : 0);
    out->str = str;
    /* Weapon damage and strength damage are separate components; each defence
       below is a percentage reduction, so they are capped. */
    out->phyDmg = pdmg;
    out->strDmg = str * 2;
    out->magDmg = mdmg;
    out->phyDef = pdef > DEF_CAP ? DEF_CAP : pdef;
    out->magDef = mdef > DEF_CAP ? DEF_CAP : mdef;
    out->shdMax = shd; out->shd = shd;
    out->shdPhyDef = shdPDef > DEF_CAP ? DEF_CAP : shdPDef;
    out->shdMagDef = shdMDef > DEF_CAP ? DEF_CAP : shdMDef;
    out->shdDmg = shdDmg;
    /* Speed decides both turn order and how often blows miss you, so an
       item's avoidance folds into it. */
    out->speed = spd + avd / 2;
    if (out->speed < 1) out->speed = 1;
    out->atkSpd = out->speed;
    out->look = lk;
    out->atkBuff = out->defBuff = 1.0f;
}

int player_add_item(Player *p, int def)
{
    if (def < 0 || def >= ITEM_COUNT) return -1;
    if (ITEMS[def].type == ITEM_CONSUMABLE) {
        for (int i = 0; i < p->invCount; i++)
            if (p->inv[i].def == def && p->inv[i].count < 99) { p->inv[i].count++; return i; }
    }
    if (p->invCount >= MAX_INVENTORY) return -1;
    p->inv[p->invCount].def = def;
    p->inv[p->invCount].count = 1;
    return p->invCount++;
}

static void inv_remove(Player *p, int idx, int n)
{
    if (idx < 0 || idx >= p->invCount) return;
    p->inv[idx].count -= n;
    if (p->inv[idx].count <= 0) {
        for (int i = idx; i + 1 < p->invCount; i++) p->inv[i] = p->inv[i + 1];
        p->invCount--;
    }
}

/* True when the wearer is strong enough for the item. */
bool player_can_equip(const Player *p, int def)
{
    if (def < 0 || def >= ITEM_COUNT) return false;
    return p->baseStr >= ITEMS[def].strNeed;
}

void player_equip(Player *p, int invIndex)
{
    if (invIndex < 0 || invIndex >= p->invCount) return;
    int def = p->inv[invIndex].def;
    const ItemDef *it = &ITEMS[def];
    if (!player_can_equip(p, def)) return;
    SlotId slot;
    switch (it->type) {
    case ITEM_WEAPON: slot = SLOT_WEAPON; break;
    case ITEM_SHIELD: slot = SLOT_SHIELD; break;
    case ITEM_ARMOUR: slot = SLOT_ARMOUR; break;
    case ITEM_HELM:   slot = SLOT_HELM;   break;
    case ITEM_RELIC:  slot = SLOT_RELIC;  break;
    default: return;
    }
    int old = p->equip[slot];
    p->equip[slot] = def;
    inv_remove(p, invIndex, 1);
    if (old >= 0) player_add_item(p, old);
}

/* Level-up rewards, exactly as the original grants them: one stat point and
   one skill point, +5 life, +5 mana and +3 energy -- and on every fifth level
   a bonus of +2 Strength, +2 Speed and a second skill point. */
/* A skill opens only once its prerequisites are learned and the level is
   reached -- the original's tree, recovered from its skill buttons. */
bool skill_prereqs_met(const Player *p, int id)
{
    if (id < 0 || id >= MAX_SKILLS) return false;
    for (int i = 0; i < 2; i++) {
        int r = SKILLS[id].prereq[i];
        if (r >= 0 && r < MAX_SKILLS && p->skillRank[r] <= 0) return false;
    }
    return p->level >= SKILLS[id].reqLevel;
}

const char *skill_lock_reason(const Player *p, int id)
{
    static char buf[96];
    if (p->level < SKILLS[id].reqLevel) {
        snprintf(buf, sizeof buf, "needs level %d", SKILLS[id].reqLevel);
        return buf;
    }
    for (int i = 0; i < 2; i++) {
        int r = SKILLS[id].prereq[i];
        if (r >= 0 && r < MAX_SKILLS && p->skillRank[r] <= 0) {
            snprintf(buf, sizeof buf, "needs %s", SKILLS[r].name);
            return buf;
        }
    }
    return NULL;
}

void player_gain_exp(Game *g, int exp)
{
    Player *p = &g->p;
    p->exp += exp;
    while (p->exp >= p->expNext) {
        p->exp -= p->expNext;
        p->statPts++;
        p->skillPts++;
        p->baseLife += 5;
        p->baseMana += 5;
        p->baseEng  += 3;
        p->level++;
        bool bonus = (p->level % 5) == 0;
        if (bonus) {
            p->baseStr   += 2;
            p->baseSpeed += 2;
            p->skillPts++;
        }
        p->expNext = player_exp_for_level(p->level);
        if (p->exp >= p->expNext) p->exp = p->expNext - 1;   /* carry-over cap */
        if (bonus) ui_toast(g, "Level %d! Bonus: +2 Strength, +2 Speed, +1 skill.", p->level);
        else       ui_toast(g, "Level %d! +1 stat point, +1 skill point.", p->level);
    }
}

/* ------------------------------------------------------------ save files */
#define SAVE_MAGIC 0x534A4432u   /* 'SJD2' */
#define SAVE_PATH  "sinjid_save.dat"

bool save_exists(void) { return FileExists(SAVE_PATH); }

bool save_write(const Game *g)
{
    FILE *f = fopen(SAVE_PATH, "wb");
    if (!f) return false;
    unsigned magic = SAVE_MAGIC;
    fwrite(&magic, sizeof magic, 1, f);
    fwrite(&g->p, sizeof g->p, 1, f);
    fclose(f);
    return true;
}

bool save_read(Game *g)
{
    FILE *f = fopen(SAVE_PATH, "rb");
    if (!f) return false;
    unsigned magic = 0;
    if (fread(&magic, sizeof magic, 1, f) != 1 || magic != SAVE_MAGIC) { fclose(f); return false; }
    if (fread(&g->p, sizeof g->p, 1, f) != 1) { fclose(f); return false; }
    fclose(f);
    return true;
}

/* ------------------------------------------------------------ new player */
static void new_player(Game *g, ClassId cls, const char *name)
{
    Player *p = &g->p;
    memset(p, 0, sizeof *p);
    snprintf(p->name, sizeof p->name, "%s", (name && name[0]) ? name : "Sinjid");
    p->cls = cls;
    p->level = 1;
    p->exp = 0;
    p->expNext = player_exp_for_level(1);
    p->gold = 75;
    p->statPts = 3;      /* something to spend on the first visit to the trainer */
    p->skillPts = 2;
    data_class_base(p, cls);
    p->look = data_class_look(cls);
    for (int i = 0; i < SLOT_COUNT; i++) p->equip[i] = -1;
    p->skillRank[0] = 1;                     /* Strike  */
    p->skillRank[1] = 1;                     /* Guard   */
    if (cls == CLASS_SPELLCASTER || cls == CLASS_BALANCED) p->skillRank[10] = 1;

    /* Item indices below are positions in the original's own item table. */
    /* The original starts you with an Iron Knife and nothing else worn. */
    p->equip[SLOT_WEAPON] = IT_IRON_KNIFE;
    player_add_item(p, IT_RICE_BALL); player_add_item(p, IT_RICE_BALL);
    player_add_item(p, IT_RICE_BALL); player_add_item(p, IT_GREEN_TEA);
    p->zone = ZONE_VILLAGE;
    p->tx = 10; p->ty = 9; p->dir = 1;
    p->saveZone = ZONE_VILLAGE; p->saveX = 10; p->saveY = 9;
    p->arenaWave = 0;
}

/* --------------------------------------------------------------- scenes */
void go_scene(Game *g, Scene s)
{
    g->fadeDir = 1;
    g->fadeTarget = s;
}

static void update_fade(Game *g, float dt)
{
    if (g->fadeDir > 0) {
        g->fade += dt * 3.4f;
        if (g->fade >= 1.0f) {
            g->fade = 1.0f;
            g->prevScene = g->scene;
            g->scene = g->fadeTarget;
            g->fadeDir = -1;
        }
    } else if (g->fadeDir < 0) {
        g->fade -= dt * 3.0f;
        if (g->fade <= 0.0f) { g->fade = 0.0f; g->fadeDir = 0; }
    }
}

/* Consumables can be used from the pause menu and from battle. */
bool use_consumable(Game *g, int invIdx, Combatant *on)
{
    Player *p = &g->p;
    if (invIdx < 0 || invIdx >= p->invCount) return false;
    const ItemDef *it = &ITEMS[p->inv[invIdx].def];
    if (it->type != ITEM_CONSUMABLE) return false;
    bool used = false;
    if (it->lifeMax > 0 && on->life < on->lifeMax) {
        int h = it->lifeMax;
        if (on->life + h > on->lifeMax) h = on->lifeMax - on->life;
        on->life += h; used = true;
        ui_toast(g, "%s: +%d life.", it->name, h);
    }
    if (it->manaMax > 0 && on->mana < on->manaMax) {
        int m = it->manaMax;
        if (on->mana + m > on->manaMax) m = on->manaMax - on->mana;
        on->mana += m; used = true;
        ui_toast(g, "%s: +%d mana.", it->name, m);
    }
    if (it->shdPts > 0 && on->shd < on->shdMax) {
        int s = it->shdPts;
        if (on->shd + s > on->shdMax) s = on->shdMax - on->shd;
        on->shd += s; used = true;
        ui_toast(g, "%s: +%d shield.", it->name, s);
    }
    if (used) inv_remove(p, invIdx, 1);
    else ui_toast(g, "No effect right now.");
    return used;
}

/* --------------------------------------------------------------- startup */
static void load_fonts(Game *g)
{
    const char *paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
        "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
    };
    for (unsigned i = 0; i < sizeof paths / sizeof paths[0]; i++) {
        if (!FileExists(paths[i])) continue;
        g->font = LoadFontEx(paths[i], 32, NULL, 0);
        if (g->font.texture.id != 0) {
            SetTextureFilter(g->font.texture, TEXTURE_FILTER_BILINEAR);
            g->fontLoaded = true;
            return;
        }
    }
    g->font = GetFontDefault();
    g->fontLoaded = false;
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--script") && i + 1 < argc) script_parse(argv[++i]);
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_W, SCREEN_H, "Sinjid: Shadow of the Warrior -- raylib remake");
    SetTargetFPS(60);
    SetExitKey(0);
    load_fonts(&G);
    data_init_zones(G.zones);
    G.scene = SCENE_TITLE;
    G.hasSave = save_exists();
    G.fade = 1.0f; G.fadeDir = -1;
    new_player(&G, CLASS_WARRIOR, "Sinjid");   /* replaced on New Game       */
    snprintf(G.nameBuf, sizeof G.nameBuf, "Sinjid");
    G.nameLen = 6;

    RenderTexture2D target = LoadRenderTexture(SCREEN_W, SCREEN_H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    while (!WindowShouldClose()) {
        if (script_done()) break;
        script_tick();
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;
        G.time += dt;
        if (G.toastT > 0) G.toastT -= dt;
        update_fade(&G, dt);

        bool blocked = (G.fadeDir > 0);

        /* ------------------------------------------------------- update */
        switch (G.scene) {
        case SCENE_TITLE:
            if (!blocked) {
                int n = G.hasSave ? 3 : 2;
                if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) G.titleIdx = (G.titleIdx + 1) % n;
                if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) G.titleIdx = (G.titleIdx + n - 1) % n;
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                    int pick = G.titleIdx;
                    if (!G.hasSave && pick == 1) pick = 2;
                    else if (G.hasSave && pick == 1) pick = 1;
                    if (pick == 0) { G.createIdx = 0; G.createField = 0; go_scene(&G, SCENE_CREATE); }
                    else if (pick == 1) {
                        if (save_read(&G)) {
                            world_enter_zone(&G, G.p.zone, G.p.tx, G.p.ty);
                            go_scene(&G, G.p.zone == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
                        } else ui_toast(&G, "Save file could not be read.");
                    } else go_scene(&G, SCENE_CREDITS);
                }
            }
            break;

        case SCENE_CREATE:
            if (!blocked) {
                if (G.createField == 0) {
                    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
                        G.createIdx = (G.createIdx + 1) % CLASS_COUNT;
                    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
                        G.createIdx = (G.createIdx + CLASS_COUNT - 1) % CLASS_COUNT;
                    if (IsKeyPressed(KEY_ENTER)) G.createField = 1;
                    if (IsKeyPressed(KEY_ESCAPE)) go_scene(&G, SCENE_TITLE);
                } else {
                    int c;
                    while ((c = GetCharPressed()) > 0)
                        if (c >= 32 && c < 127 && G.nameLen < (int)sizeof G.nameBuf - 1) {
                            G.nameBuf[G.nameLen++] = (char)c;
                            G.nameBuf[G.nameLen] = 0;
                        }
                    if (IsKeyPressed(KEY_BACKSPACE) && G.nameLen > 0) G.nameBuf[--G.nameLen] = 0;
                    if (IsKeyPressed(KEY_ESCAPE)) G.createField = 0;
                    if (IsKeyPressed(KEY_ENTER) && G.nameLen > 0) {
                        new_player(&G, (ClassId)G.createIdx, G.nameBuf);
                        world_enter_zone(&G, ZONE_VILLAGE, 10, 9);
                        go_scene(&G, SCENE_VILLAGE);
                    }
                }
            }
            break;

        case SCENE_VILLAGE:
        case SCENE_WORLD:
            if (!blocked) world_update(&G, dt);
            break;

        case SCENE_BATTLE:
            battle_update(&G, dt);
            break;

        case SCENE_MENU:
        case SCENE_SHOP:
        case SCENE_TRAIN:
        case SCENE_DIALOG:
            /* handled inside their draw/update helpers in ui.c            */
            break;

        case SCENE_GAMEOVER:
            if (!blocked && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) {
                if (save_read(&G)) {
                    world_enter_zone(&G, G.p.saveZone, G.p.saveX, G.p.saveY);
                    go_scene(&G, G.p.saveZone == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
                } else go_scene(&G, SCENE_TITLE);
            }
            break;

        case SCENE_CREDITS:
            if (!blocked && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)))
                go_scene(&G, SCENE_TITLE);
            break;
        default: break;
        }

        /* --------------------------------------------------------- draw */
        BeginTextureMode(target);
        ClearBackground(C_INK);
        switch (G.scene) {
        case SCENE_TITLE:    ui_scene_title(&G); break;
        case SCENE_CREATE:   ui_scene_create(&G); break;
        case SCENE_VILLAGE:
        case SCENE_WORLD:    world_draw(&G); break;
        case SCENE_BATTLE:   battle_draw(&G); break;
        case SCENE_MENU:     world_draw(&G); ui_scene_menu(&G); break;
        case SCENE_SHOP:     world_draw(&G); ui_scene_shop(&G); break;
        case SCENE_TRAIN:    world_draw(&G); ui_scene_train(&G); break;
        case SCENE_DIALOG:   world_draw(&G); ui_scene_dialog(&G); break;
        case SCENE_GAMEOVER: {
            art_draw_battle_bg(BG_DARK, G.time);
            DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 0, 0, 0, 190 });
            ui_text_c("YOU FELL", SCREEN_W / 2, 250, 64, C_BLOOD2);
            ui_text_c("The shadow takes what it is owed.", SCREEN_W / 2, 330, 22, C_PARCH2);
            ui_text_c("ENTER -- return to your last rest", SCREEN_W / 2, 420, 20, C_GOLD);
        } break;
        case SCENE_CREDITS: {
            art_draw_battle_bg(BG_ARENA, G.time);
            DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 0, 0, 0, 170 });
            ui_text_c("ABOUT", SCREEN_W / 2, 120, 48, C_GOLD);
            const char *lines[] = {
                "An unofficial fan remake, written in C with raylib.",
                "Every sprite, background and effect is drawn procedurally",
                "in art.c -- there are no image files in this project.",
                "",
                "Original Flash game: Sinjid: Shadow of the Warrior (2004)",
                "by Infrarift. This remake is not affiliated with it.",
                "",
                "Controls:  arrows / WASD move,  ENTER or SPACE confirm,",
                "ESC back,  I inventory,  T training,  F5 save at a shrine.",
                "",
                "ENTER -- back to the title",
            };
            for (unsigned i = 0; i < sizeof lines / sizeof lines[0]; i++)
                ui_text_c(lines[i], SCREEN_W / 2, 210 + i * 34, 22,
                          i == sizeof lines / sizeof lines[0] - 1 ? C_GOLD : C_PARCH);
        } break;
        default: break;
        }

        if (G.toastT > 0) {
            float a = G.toastT > 2.2f ? (2.6f - G.toastT) / 0.4f : (G.toastT > 0.5f ? 1 : G.toastT / 0.5f);
            float w = ui_text_w(G.toast, 20) + 40;
            DrawRectangleRounded((Rectangle){ SCREEN_W / 2 - w / 2, 24, w, 40 }, 0.4f, 8,
                                 (Color){ 18, 16, 22, (unsigned char)(210 * a) });
            DrawRectangleRoundedLines((Rectangle){ SCREEN_W / 2 - w / 2, 24, w, 40 }, 0.4f, 8,
                                      (Color){ 198, 160, 74, (unsigned char)(180 * a) });
            ui_text_c(G.toast, SCREEN_W / 2, 34,
                      20, (Color){ 206, 192, 164, (unsigned char)(255 * a) });
        }

        if (G.fade > 0.001f)
            DrawRectangle(0, 0, SCREEN_W, SCREEN_H,
                          (Color){ 0, 0, 0, (unsigned char)(G.fade * 255) });
        EndTextureMode();

        /* Letterboxed scale to whatever the window is. */
        BeginDrawing();
        ClearBackground(BLACK);
        float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
        float scale = fminf(sw / SCREEN_W, sh / SCREEN_H);
        Rectangle src = { 0, 0, (float)target.texture.width, -(float)target.texture.height };
        Rectangle dst = { (sw - SCREEN_W * scale) * 0.5f, (sh - SCREEN_H * scale) * 0.5f,
                          SCREEN_W * scale, SCREEN_H * scale };
        DrawTexturePro(target.texture, src, dst, (Vector2){ 0, 0 }, 0, WHITE);
        EndDrawing();
    }

    UnloadRenderTexture(target);
    if (G.fontLoaded) UnloadFont(G.font);
    CloseWindow();
    return 0;
}
