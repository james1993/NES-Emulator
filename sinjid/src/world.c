/* ===========================================================================
   world.c -- the tile overworld: zone maps, walking, NPCs, encounters.

   Each zone is a single 28x18 screen (the original's overworld worked the
   same way), joined by portal tiles.  Wild zones roll random encounters
   against a per-zone enemy pool as you walk.
   =========================================================================== */
#include "game.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define TILE   40
#define OX     80          /* map origin on screen */
#define OY     0

/* Map legend:  . grass   , path   # wall   D door   ~ water   T tree
                R rock    s sand   w snow   m mat    = floor   L lava
                P portal  space void                                        */
static int tile_from_char(char c)
{
    switch (c) {
    case '.': return T_GRASS;
    case ',': return T_PATH;
    case '#': return T_WALL;
    case 'D': return T_DOOR;
    case '~': return T_WATER;
    case 'T': return T_TREE;
    case 'R': return T_ROCK;
    case 's': return T_SAND;
    case 'w': return T_SNOW;
    case 'm': return T_MAT;
    case '=': return T_FLOOR;
    case 'L': return T_LAVA;
    case 'P': return T_PORTAL;
    default:  return T_VOID;
    }
}

static bool tile_solid(int t)
{
    return t == T_WALL || t == T_WATER || t == T_TREE || t == T_ROCK ||
           t == T_VOID || t == T_LAVA;
}

static void fill_zone(Zone *z, const char *rows[MAP_H])
{
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            char c = rows[y][x] ? rows[y][x] : ' ';
            z->tiles[y][x] = (unsigned char)tile_from_char(c);
        }
}

static Look npc_look(Color cloth, Color trim, Color hair, int helm, int weapon)
{
    Look lk;
    lk.skin = (Color){ 202, 164, 128, 255 };
    lk.cloth = cloth;
    lk.clothDark = art_shade(cloth, 0.62f);
    lk.trim = trim;
    lk.hair = hair;
    lk.metal = C_STEEL;
    lk.body = BODY_HUMAN;
    lk.weapon = weapon; lk.shield = SHLD_NONE; lk.helm = helm;
    lk.weaponTint = C_STEEL; lk.shieldTint = C_STEEL;
    lk.scale = 0.92f; lk.glow = false;
    return lk;
}

static void add_npc(Zone *z, NpcKind kind, int tx, int ty, const char *name,
                    const char *line, int arg, Look lk)
{
    if (z->npcCount >= MAX_NPCS) return;
    Npc *n = &z->npcs[z->npcCount++];
    n->kind = kind; n->tx = tx; n->ty = ty;
    n->name = name; n->line = line; n->arg = arg;
    n->look = lk;
    n->bob = (float)(z->npcCount) * 0.7f;
}

void data_init_zones(Zone *zones)
{
    memset(zones, 0, sizeof(Zone) * ZONE_COUNT);

    /* ------------------------------------------------------- the village */
    {
        Zone *z = &zones[ZONE_VILLAGE];
        z->id = ZONE_VILLAGE; z->name = "Kaido Village";
        z->encounterRate = 0;
        z->minLevel = 1; z->maxLevel = 1;
        z->skyTop = (Color){ 70, 92, 120, 255 };
        z->ground = (Color){ 104, 122, 82, 255 };
        z->groundDark = (Color){ 78, 94, 62, 255 };
        z->propA = (Color){ 150, 132, 96, 255 };
        z->propB = (Color){ 126, 110, 80, 255 };
        static const char *rows[MAP_H] = {
        /*   0123456789012345678901234567 */
            "TTTTTTTTTTTTTTTTTTTTTTTTTTTT",
            "T..........TTTT............T",
            "T..#####...............####.T",
            "T..#===#....,,,,,,....#===#.T",
            "T..#===D...,,,,,,,,...D===#.T",
            "T..#####...,,,,,,,,...#####.T",
            "T..........,,,,,,,,........,T",
            "T...####...,,,,,,,,...####..T",
            "T...#==D...,,,,,,,,...D==#..T",
            "T...####...,,,,,,,,...####..T",
            "T..........,,,,,,,,........,T",
            "T....RR....,,,,,,,,....RR...T",
            "T..........,,,,,,,,.........T",
            "T...####...,,,,,,,,...####..T",
            "T...#mmD...,,,,,,,,...D==#..T",
            "T...####...,,,,,,,,...####..T",
            "T..........,,,,,,,,.........T",
            "TTTTTTTTTT,,,,,,,,,TTTTTTTTT",
        };
        fill_zone(z, rows);
        add_npc(z, NPC_SMITH, 7, 4, "Toshi the Smith",
                "Steel is honest. Bring gold and I will make you\nharder to kill.", 0,
                npc_look((Color){ 96, 70, 48, 255 }, (Color){ 160, 60, 44, 255 },
                         (Color){ 40, 34, 30, 255 }, HELM_BANDANA, WEAP_NONE));
        add_npc(z, NPC_VENDOR, 22, 4, "Mira the Trader",
                "Rice, tea, bandages. Everything a fool needs to\nlast one more fight.", 1,
                npc_look((Color){ 120, 92, 130, 255 }, (Color){ 200, 180, 120, 255 },
                         (Color){ 60, 40, 34, 255 }, HELM_NONE, WEAP_NONE));
        add_npc(z, NPC_HEALER, 7, 8, "Sister Ayu",
                "Sit. Breathe. I will close what I can.", 0,
                npc_look((Color){ 200, 196, 186, 255 }, (Color){ 92, 146, 100, 255 },
                         (Color){ 34, 30, 28, 255 }, HELM_HOOD, WEAP_NONE));
        add_npc(z, NPC_TRAINER, 22, 8, "Master Renjiro",
                "You carry what the fire left you. Let us shape it\ninto something with an edge.", 0,
                npc_look((Color){ 62, 68, 92, 255 }, (Color){ 176, 140, 70, 255 },
                         (Color){ 180, 178, 172, 255 }, HELM_NONE, WEAP_STAFF));
        add_npc(z, NPC_SAVE, 6, 14, "Ancestor Shrine",
                "The names of the dead are cut into the stone.\nResting here restores you.", 0,
                npc_look((Color){ 130, 126, 118, 255 }, C_GOLD,
                         (Color){ 90, 88, 84, 255 }, HELM_NONE, WEAP_NONE));
        add_npc(z, NPC_ARENA, 22, 14, "Arena Keeper",
                "Ten waves. No running, no supplies from outside.\nHow much are you worth?", 0,
                npc_look((Color){ 88, 52, 52, 255 }, (Color){ 190, 160, 70, 255 },
                         (Color){ 40, 30, 26, 255 }, HELM_HORNED, WEAP_AXE));
        add_npc(z, NPC_ELDER, 14, 2, "Elder Sokan",
                "The reaper took the village while you were away\ntraining. Everything past the south road is his now.", 0,
                npc_look((Color){ 74, 74, 88, 255 }, (Color){ 160, 150, 120, 255 },
                         (Color){ 200, 198, 194, 255 }, HELM_NONE, WEAP_NONE));
        add_npc(z, NPC_GATE, 14, 17, "South Road", "The fern road out of the valley.",
                ZONE_GRASS, npc_look(C_STEEL, C_GOLD, C_INK2, HELM_NONE, WEAP_NONE));
    }

    /* ------------------------------------------------------ the wild zones */
    struct { ZoneId id; const char *name; int rate, lo, hi; Color g, gd, pa, pb;
             int pool[6], pn; } W[] = {
        { ZONE_GRASS,  "Fern Road",    9,  1,  6,
          { 96,120,72,255 }, { 70,92,56,255 }, { 150,132,96,255 }, { 126,110,80,255 },
          { 1, 2, 3, 4, 0, 0 }, 4 },
        { ZONE_COAST,  "Grey Coast",   10, 5, 11,
          { 150,138,104,255 }, { 118,108,80,255 }, { 168,156,124,255 }, { 140,128,98,255 },
          { 3, 4, 5, 8, 2, 0 }, 5 },
        { ZONE_DESERT, "Ash Flats",    11, 9, 16,
          { 190,158,108,255 }, { 158,128,86,255 }, { 202,174,124,255 }, { 176,148,104,255 },
          { 5, 6, 7, 8, 10, 0 }, 5 },
        { ZONE_SNOW,   "White Pass",   12, 14, 21,
          { 214,224,236,255 }, { 176,190,208,255 }, { 200,212,226,255 }, { 178,192,210,255 },
          { 9, 10, 11, 12, 6, 0 }, 5 },
        { ZONE_CAVE,   "Hollow Deep",  13, 18, 25,
          { 66,58,62,255 }, { 44,38,42,255 }, { 74,66,68,255 }, { 56,50,54,255 },
          { 11, 13, 14, 15, 12, 0 }, 5 },
        { ZONE_SHADOW, "Shadow Realm", 14, 22, 30,
          { 52,36,58,255 }, { 34,24,40,255 }, { 60,44,66,255 }, { 44,32,50,255 },
          { 15, 16, 17, 18, 14, 0 }, 5 },
    };

    static const char *GRASS_ROWS[MAP_H] = {
        "TTTTTTTTTT,,,,,,,,TTTTTTTTTT",
        "TT.....TTT,,,,,,,,TTT......T",
        "T.......T..........T.......T",
        "T..TT...............RR.....T",
        "T..TT.........TT...........T",
        "T............TTTT..........T",
        "T....RR......TTTT....TT....T",
        "T.....................TT...T",
        "T........TT................T",
        "T........TT.....RR.........T",
        "T..........................T",
        "T...TT..............TT.....T",
        "T...TT......RR......TT.....T",
        "T..........................T",
        "T.....TT...........TT......T",
        "T.....TT...........TT......T",
        "T..........................T",
        "TTTTTTTTTTTT,,,,TTTTTTTTTTTT",
    };
    static const char *COAST_ROWS[MAP_H] = {
        "TTTTTTTTTTTT,,,,TTTTTTTTTTTT",
        "T.........TT....TT.........T",
        "T..RR......................T",
        "T.....................RR...T",
        "T.......sssssssss..........T",
        "T......sssssssssss.........T",
        "T.....ssssssssssssss.......T",
        "T....sssssssssssssssss.....T",
        "T...ssssssssssssssssssss...T",
        "T..sssssssss~~~ssssssssss..T",
        "T.ssssssss~~~~~~~sssssssss.T",
        "Tsssssss~~~~~~~~~~~ssssssssT",
        "Tssssss~~~~~~~~~~~~~~ssssssT",
        "Tsssss~~~~~~~~~~~~~~~~~ssssT",
        "Tssss~~~~~~~~~~~~~~~~~~~~ssT",
        "Tsss~~~~~~~~~~~~~~~~~~~~~~sT",
        "Tss~~~~~~~~~~~~~~~~~~~~~~~sT",
        "TTTTTTTTTTTT,,,,TTTTTTTTTTTT",
    };
    static const char *DESERT_ROWS[MAP_H] = {
        "RRRRRRRRRRRR,,,,RRRRRRRRRRRR",
        "Rsssssssssssssssssssssssss.R",
        "Rss.RR.ssssssssss.RR.sssss.R",
        "Rsssssssssssssssssssssssss.R",
        "Rssssss.RRRR.sssssssssssss.R",
        "Rsssssssssssssss.RR.ssssss.R",
        "Rss.RR.sssssssssssssssssss.R",
        "Rssssssssssss.RRRR.sssssss.R",
        "Rsssssssssssssssssssssssss.R",
        "Rssss.RR.sssssssssss.RR.ss.R",
        "Rsssssssssssssssssssssssss.R",
        "Rss.RRRR.ssssssssssssssss..R",
        "Rsssssssssss.RR.ssssssssss.R",
        "Rssssssssssssssssssssssss..R",
        "Rss.RR.sssssssssss.RRRR.ss.R",
        "Rsssssssssssssssssssssssss.R",
        "Rsssssssssssssssssssssssss.R",
        "RRRRRRRRRRRR,,,,RRRRRRRRRRRR",
    };
    static const char *SNOW_ROWS[MAP_H] = {
        "RRRRRRRRRRRR,,,,RRRRRRRRRRRR",
        "RwwwwwwwwwwwwwwwwwwwwwwwwwwR",
        "Rww.TT.wwwwwwwwww.TT.wwwwwwR",
        "RwwwwwwwwwwRRwwwwwwwwwwwwwwR",
        "Rwwwww.TT.wwwwwwwwww.TT.wwwR",
        "RwwwwwwwwwwwwwwRRwwwwwwwwwwR",
        "Rww.TT.wwwwwwwwwwwwwww.TT.wR",
        "RwwwwwwwwRRwwwwwwwwwwwwwwwwR",
        "Rwwwwwwwwwwwwww.TT.wwwwwwwwR",
        "Rww.TT.wwwwwwwwwwwwRRwwwwwwR",
        "RwwwwwwwwwwwwwwwwwwwwwwwwwwR",
        "RwwwwRRwwwww.TT.wwwwwww.TT.R",
        "RwwwwwwwwwwwwwwwwwwwwwwwwwwR",
        "Rww.TT.wwwwwwwwRRwwwwwwwwwwR",
        "Rwwwwwwwwwwwwwwwwwww.TT.wwwR",
        "RwwwwwwwwRRwwwwwwwwwwwwwwwwR",
        "RwwwwwwwwwwwwwwwwwwwwwwwwwwR",
        "RRRRRRRRRRRR,,,,RRRRRRRRRRRR",
    };
    static const char *CAVE_ROWS[MAP_H] = {
        "############,,,,############",
        "#=========#########========#",
        "#===RR====#########===RR===#",
        "#=========D=======D========#",
        "#####=#####=======#####=####",
        "#===========================",
        "#==RR===#########===RR=====#",
        "#=======#########==========#",
        "#=======#########=====LL===#",
        "#===========================",
        "#####=#####=======#####=####",
        "#=========D=======D========#",
        "#==RR=====#########==RR====#",
        "#=========#########========#",
        "#=========#########========#",
        "#===LL====#########===LL===#",
        "#=========#########========#",
        "############,,,,############",
    };
    static const char *SHADOW_ROWS[MAP_H] = {
        "############,,,,############",
        "#=========================.#",
        "#=.RR.=========.RR.========#",
        "#==========================#",
        "#====.RRRR.================#",
        "#==============.RR.========#",
        "#=.RR.=====================#",
        "#=========.RRRR.===========#",
        "#==========================#",
        "#====.RR.============.RR.==#",
        "#==========================#",
        "#=.RRRR.===================#",
        "#===========.RR.===========#",
        "#==========================#",
        "#=.RR.==========.RRRR.=====#",
        "#==========================#",
        "#=============P============#",
        "############,,,,############",
    };
    const char **MAPS[] = { GRASS_ROWS, COAST_ROWS, DESERT_ROWS, SNOW_ROWS, CAVE_ROWS, SHADOW_ROWS };

    for (unsigned i = 0; i < sizeof W / sizeof W[0]; i++) {
        Zone *z = &zones[W[i].id];
        z->id = W[i].id;
        z->name = W[i].name;
        z->encounterRate = W[i].rate;
        z->minLevel = W[i].lo; z->maxLevel = W[i].hi;
        z->ground = W[i].g; z->groundDark = W[i].gd;
        z->propA = W[i].pa; z->propB = W[i].pb;
        z->enemyPoolCount = W[i].pn;
        for (int k = 0; k < W[i].pn; k++) z->enemyPool[k] = W[i].pool[k];
        fill_zone(z, MAPS[i]);

        /* North gate goes back, south gate goes deeper. */
        ZoneId back = (W[i].id == ZONE_GRASS) ? ZONE_VILLAGE : (ZoneId)(W[i].id - 1);
        add_npc(z, NPC_GATE, 13, 0, "North Road", "Back the way you came.", back,
                npc_look(C_STEEL, C_GOLD, C_INK2, HELM_NONE, WEAP_NONE));
        if (W[i].id != ZONE_SHADOW)
            add_npc(z, NPC_GATE, 14, 17, "South Road", "Deeper in.", (ZoneId)(W[i].id + 1),
                    npc_look(C_STEEL, C_GOLD, C_INK2, HELM_NONE, WEAP_NONE));
    }
    zones[ZONE_SHADOW].npcs[zones[ZONE_SHADOW].npcCount - 1].kind = NPC_NONE;
}

/* ---------------------------------------------------------------- travel */

void world_enter_zone(Game *g, ZoneId z, int tx, int ty)
{
    g->p.zone = z;
    g->p.tx = tx; g->p.ty = ty;
    g->fromX = tx; g->fromY = ty;
    g->moving = false; g->moveT = 0;
    g->stepsSinceFight = 0;
}

static Npc *npc_at(Zone *z, int tx, int ty)
{
    for (int i = 0; i < z->npcCount; i++)
        if (z->npcs[i].kind != NPC_NONE && z->npcs[i].tx == tx && z->npcs[i].ty == ty)
            return &z->npcs[i];
    return NULL;
}

static bool walkable(Game *g, Zone *z, int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) return false;
    if (tile_solid(z->tiles[ty][tx])) return false;
    Npc *n = npc_at(z, tx, ty);
    if (n && n->kind != NPC_GATE) return false;    /* gates are walk-through */
    (void)g;
    return true;
}

static void roll_encounter(Game *g, Zone *z)
{
    if (z->encounterRate <= 0 || z->enemyPoolCount <= 0) return;
    g->stepsSinceFight++;
    if (g->stepsSinceFight < 4) return;
    if (rnd(0, 100) >= z->encounterRate) return;

    g->stepsSinceFight = 0;
    int defs[MAX_FOES];
    int n = (g->p.level >= z->minLevel + 3 && rnd(0, 100) < 45) ? 2 : 1;
    for (int i = 0; i < n; i++)
        defs[i] = z->enemyPool[rnd(0, z->enemyPoolCount - 1)];
    int lvl = rnd(z->minLevel, z->maxLevel);
    battle_start(g, defs, n, lvl, false, false);
}

static void interact(Game *g, Npc *n)
{
    Player *p = &g->p;
    switch (n->kind) {
    case NPC_GATE:
        world_enter_zone(g, (ZoneId)n->arg, n->tx == 13 ? 14 : 14, n->ty == 0 ? 16 : 1);
        go_scene(g, n->arg == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
        ui_toast(g, "%s", g->zones[n->arg].name);
        break;
    case NPC_SMITH:
        g->shopVendor = (p->level >= 8) ? 3 : 0;
        g->shopIdx = 0; g->shopMode = 0;
        g->dialogNpc = (int)(n - g->zones[p->zone].npcs);
        go_scene(g, SCENE_SHOP);
        break;
    case NPC_VENDOR:
        g->shopVendor = 1;
        g->shopIdx = 0; g->shopMode = 0;
        g->dialogNpc = (int)(n - g->zones[p->zone].npcs);
        go_scene(g, SCENE_SHOP);
        break;
    case NPC_TRAINER:
        g->menuIdx = 0; g->menuTab = 0;
        go_scene(g, SCENE_TRAIN);
        break;
    case NPC_HEALER: {
        Combatant c;
        player_recalc(p, &c);
        int cost = 10 + p->level * 4;
        if (p->curLife >= c.lifeMax && p->curMana >= c.manaMax) {
            ui_toast(g, "You are already whole.");
        } else if (p->gold >= cost) {
            p->gold -= cost;
            p->curLife = c.lifeMax;
            p->curMana = c.manaMax;
            ui_toast(g, "Ayu closes your wounds. (-%d gold)", cost);
        } else ui_toast(g, "You need %d gold.", cost);
    } break;
    case NPC_SAVE: {
        Combatant c;
        player_recalc(p, &c);
        p->curLife = c.lifeMax;
        p->curMana = c.manaMax;
        p->saveZone = p->zone; p->saveX = p->tx; p->saveY = p->ty;
        if (save_write(g)) { g->hasSave = true; ui_toast(g, "Rested. Progress saved."); }
        else ui_toast(g, "The shrine is silent. (save failed)");
    } break;
    case NPC_ARENA: {
        int wave = p->arenaWave;
        int defs[MAX_FOES];
        int n2 = (wave >= 3) ? 2 : 1;
        int base = wave < ENEMY_COUNT - 2 ? 1 + wave : ENEMY_COUNT - 2;
        for (int i = 0; i < n2; i++) {
            int pick = base - i;
            if (pick < 1) pick = 1;
            if (pick >= ENEMY_COUNT) pick = ENEMY_COUNT - 1;
            defs[i] = pick;
        }
        ui_toast(g, "Arena wave %d!", wave + 1);
        battle_start(g, defs, n2, p->level + wave / 2, true, false);
    } break;
    case NPC_ELDER:
    case NPC_VILLAGER:
    default:
        g->dialogNpc = (int)(n - g->zones[p->zone].npcs);
        go_scene(g, SCENE_DIALOG);
        break;
    }
}

/* ---------------------------------------------------------------- update */

void world_update(Game *g, float dt)
{
    Player *p = &g->p;
    Zone *z = &g->zones[p->zone];

    if (IsKeyPressed(KEY_I) || IsKeyPressed(KEY_ESCAPE)) {
        g->menuTab = 0; g->menuIdx = 0; g->menuScroll = 0;
        go_scene(g, SCENE_MENU);
        return;
    }
    if (IsKeyPressed(KEY_T)) { g->menuIdx = 0; g->menuTab = 0; go_scene(g, SCENE_TRAIN); return; }

    if (g->moving) {
        g->moveT += dt * 5.4f;
        g->walkT += dt;
        if (g->moveT >= 1.0f) {
            g->moving = false;
            g->moveT = 0;
            /* stepping onto a gate tile travels immediately */
            Npc *n = npc_at(z, p->tx, p->ty);
            if (n && n->kind == NPC_GATE) { interact(g, n); return; }
            if (z->tiles[p->ty][p->tx] == T_PORTAL) {
                ui_toast(g, "The way down is sealed. The reaper waits.");
            }
            roll_encounter(g, z);
        }
        return;
    }

    int dx = 0, dy = 0;
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) { dy = -1; p->dir = 0; }
    else if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) { dy = 1; p->dir = 1; }
    else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) { dx = -1; p->dir = 2; }
    else if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) { dx = 1; p->dir = 3; }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        const int ddx[4] = { 0, 0, -1, 1 }, ddy[4] = { -1, 1, 0, 0 };
        Npc *n = npc_at(z, p->tx + ddx[p->dir], p->ty + ddy[p->dir]);
        if (!n) n = npc_at(z, p->tx, p->ty);
        if (n) { interact(g, n); return; }
    }

    if (dx || dy) {
        int nx = p->tx + dx, ny = p->ty + dy;
        if (walkable(g, z, nx, ny)) {
            g->fromX = p->tx; g->fromY = p->ty;
            p->tx = nx; p->ty = ny;
            g->moving = true;
            g->moveT = 0;
        } else {
            g->walkT += dt;   /* keep the legs moving while pushing a wall */
        }
    } else {
        g->walkT = 0;
    }
}

/* ------------------------------------------------------------------ draw */

static void draw_npc(Game *g, Npc *n, float t)
{
    float px = OX + n->tx * TILE + TILE * 0.5f;
    float py = OY + n->ty * TILE + TILE * 0.9f;

    if (n->kind == NPC_SAVE) {                       /* a stone shrine      */
        DrawEllipse((int)px, (int)py, 20, 6, (Color){ 0, 0, 0, 90 });
        DrawRectangleRec((Rectangle){ px - 15, py - 44, 30, 44 }, (Color){ 128, 124, 116, 255 });
        DrawRectangleRec((Rectangle){ px - 19, py - 52, 38, 10 }, (Color){ 146, 142, 134, 255 });
        DrawRectangleLinesEx((Rectangle){ px - 15, py - 44, 30, 44 }, 2, (Color){ 74, 72, 68, 255 });
        DrawCircleV((Vector2){ px, py - 24 }, 7 + sinf(t * 2 + n->bob) * 1.5f,
                    (Color){ 198, 160, 74, 60 });
        return;
    }
    if (n->kind == NPC_GATE) {
        float a = 0.35f + 0.25f * sinf(t * 2.5f + n->bob);
        DrawRectangleRec((Rectangle){ px - TILE * 0.5f, py - TILE, TILE, TILE },
                         (Color){ 198, 160, 74, (unsigned char)(60 * a) });
        const char *arrow = (n->ty == 0) ? "^" : "v";
        ui_text_c(arrow, px, py - TILE * 0.9f, 26, C_GOLD);
        /* keep the label on screen: below the tile for the north gate */
        ui_text_c(n->name, px, (n->ty == 0) ? py + 4 : py - TILE * 1.5f, 15, C_PARCH);
        return;
    }
    Look lk = n->look;
    art_draw_walker(&lk, (Vector2){ px, py }, 1, 0.0f, 0.62f);
    /* an interaction pip so the player can tell who talks */
    float bob = sinf(t * 2.2f + n->bob) * 3;
    ui_text_c("!", px, py - 78 + bob, 22, C_GOLD);
    (void)g;
}

void world_draw(Game *g)
{
    Player *p = &g->p;
    Zone *z = &g->zones[p->zone];
    float t = g->time;

    /* sky band behind the map, so the screen edges are not flat black */
    ClearBackground(C_INK);
    DrawRectangleGradientV(0, 0, SCREEN_W, SCREEN_H,
                           art_shade(z->groundDark, 0.5f), art_shade(z->groundDark, 0.25f));

    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            art_draw_tile(z->tiles[y][x], OX + x * TILE, OY + y * TILE, TILE, z, x, y);

    /* props scattered deterministically on empty ground */
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            int tl = z->tiles[y][x];
            if (tl != T_GRASS && tl != T_FLOOR) continue;
            unsigned h = (unsigned)((x * 7919 + y * 104729) ^ (z->id * 31337));
            if ((h % 37) != 0) continue;
            if (npc_at(z, x, y)) continue;
            art_draw_prop((int)(h / 37) % 6,
                          (Vector2){ OX + x * TILE + TILE * 0.5f, OY + y * TILE + TILE * 0.9f },
                          0.55f, z->propA, z->propB);
        }

    for (int i = 0; i < z->npcCount; i++)
        if (z->npcs[i].kind != NPC_NONE) draw_npc(g, &z->npcs[i], t);

    /* the player, tweened between tiles */
    float fx = g->moving ? (g->fromX + (p->tx - g->fromX) * g->moveT) : (float)p->tx;
    float fy = g->moving ? (g->fromY + (p->ty - g->fromY) * g->moveT) : (float)p->ty;
    Vector2 at = { OX + fx * TILE + TILE * 0.5f, OY + fy * TILE + TILE * 0.95f };
    Look lk = p->look;
    for (int s = 0; s < SLOT_COUNT; s++) {
        int id = p->equip[s];
        if (id < 0) continue;
        if (ITEMS[id].type == ITEM_WEAPON) { lk.weapon = ITEMS[id].shape; lk.weaponTint = ITEMS[id].tint; }
        if (ITEMS[id].type == ITEM_HELM)   lk.helm = ITEMS[id].shape;
        if (ITEMS[id].type == ITEM_ARMOUR) { lk.cloth = ITEMS[id].tint;
                                             lk.clothDark = art_shade(ITEMS[id].tint, 0.62f); }
    }
    art_draw_walker(&lk, at, p->dir, g->moving ? g->walkT : 0.0f, 0.66f);

    /* dark edges so the 28x18 grid reads as a stage */
    DrawRectangle(0, 0, OX, SCREEN_H, (Color){ 12, 10, 16, 255 });
    DrawRectangle(SCREEN_W - OX, 0, OX, SCREEN_H, (Color){ 12, 10, 16, 255 });
    DrawRectangleGradientV(OX, 0, SCREEN_W - OX * 2, 60, (Color){ 12, 10, 16, 190 },
                           (Color){ 12, 10, 16, 0 });
    DrawRectangleGradientV(OX, SCREEN_H - 90, SCREEN_W - OX * 2, 90, (Color){ 12, 10, 16, 0 },
                           (Color){ 12, 10, 16, 210 });

    /* status strip */
    Combatant c;
    player_recalc(p, &c);
    int life = p->curLife > 0 ? p->curLife : c.lifeMax;
    int mana = p->curMana;
    ui_text(z->name, 20, 20, 26, C_GOLD);
    char sub[96];
    snprintf(sub, sizeof sub, "%s  Lv%d  %s", p->name, p->level, CLASS_NAMES[p->cls]);
    ui_text(sub, 20, 52, 17, C_PARCH2);

    ui_bar((Rectangle){ SCREEN_W - 250, 22, 230, 16 }, (float)life / c.lifeMax, C_BLOOD,
           (Color){ 40, 20, 20, 220 }, NULL);
    ui_bar((Rectangle){ SCREEN_W - 250, 42, 230, 12 }, c.manaMax ? (float)mana / c.manaMax : 0,
           C_KI, (Color){ 22, 34, 44, 220 }, NULL);
    char g1[64];
    snprintf(g1, sizeof g1, "%d gold", p->gold);
    ui_text(g1, SCREEN_W - 250, 60, 18, C_GOLD);
    if (p->statPts > 0 || p->skillPts > 0)
        ui_text("T -- points to spend", SCREEN_W - 250, 84, 16, C_JADE);

    ui_text("ENTER talk    I inventory    T training", 20, SCREEN_H - 34, 17,
            (Color){ 206, 192, 164, 150 });
}
