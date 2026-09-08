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

#define TILE   52
#define OX     120         /* 20 x 13 cells of 52px, centred */
#define OY     22

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

static void fill_zone(Zone *z, const char *const rows[MAP_H])
{
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            char c = rows[y][x] ? rows[y][x] : ' ';
            z->tiles[y][x] = (unsigned char)tile_from_char(c);
        }
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


/* A palette per cast member.  The names are the original's; the colours,
   body shapes and gear choices are ours. */
static Look npc_look_for(const char *name)
{
    struct { const char *n; Color cloth, trim, hair; int helm, weapon; float sc; } T[] = {
      { "Elder",           { 74, 74, 88,255},{160,150,120,255},{200,198,194,255}, HELM_NONE,   WEAP_NONE,  0.95f },
      { "Scribe",          { 96, 92,120,255},{190,180,150,255},{ 60, 54, 50,255}, HELM_NONE,   WEAP_NONE,  0.92f },
      { "Healer",          {200,196,186,255},{ 92,146,100,255},{ 34, 30, 28,255}, HELM_HOOD,   WEAP_NONE,  0.92f },
      { "Item Vendor",     { 96, 70, 48,255},{160, 60, 44,255},{ 40, 34, 30,255}, HELM_BANDANA,WEAP_NONE,  0.95f },
      { "Item Vendor 2",   { 70, 62, 54,255},{176,140, 70,255},{ 36, 30, 26,255}, HELM_BANDANA,WEAP_NONE,  1.0f  },
      { "Food Vendor",     {120, 92,130,255},{200,180,120,255},{ 60, 40, 34,255}, HELM_NONE,   WEAP_NONE,  0.92f },
      { "Potion Vendor",   { 62, 92, 96,255},{140,190,180,255},{ 44, 38, 34,255}, HELM_NONE,   WEAP_NONE,  0.92f },
      { "Ninja",           { 42, 44, 58,255},{150, 52, 48,255},{ 28, 26, 24,255}, HELM_HOOD,   WEAP_KATANA,1.0f  },
      { "Dark Ninja",      { 30, 30, 40,255},{120, 36, 36,255},{ 24, 22, 22,255}, HELM_HOOD,   WEAP_CLAW,  1.02f },
      { "Lady",            {130, 96,120,255},{210,190,150,255},{ 50, 40, 36,255}, HELM_NONE,   WEAP_NONE,  0.9f  },
      { "Drunkard",        {110, 92, 70,255},{160,140,100,255},{ 54, 46, 40,255}, HELM_NONE,   WEAP_NONE,  0.94f },
      { "Drinker",         { 96, 84, 66,255},{150,130, 96,255},{ 48, 42, 36,255}, HELM_NONE,   WEAP_NONE,  0.92f },
      { "Relaxing Ninja",  { 54, 58, 74,255},{130, 60, 56,255},{ 30, 28, 26,255}, HELM_BANDANA,WEAP_NONE,  0.95f },
      { "Meditating Ninja",{ 48, 52, 66,255},{120, 70, 60,255},{ 30, 28, 26,255}, HELM_NONE,   WEAP_NONE,  0.95f },
      { "Wounded Warrior", { 86, 86, 96,255},{150, 44, 40,255},{ 40, 34, 30,255}, HELM_NONE,   WEAP_NONE,  0.95f },
      { "Apprentice",      { 82, 96, 78,255},{170,160,120,255},{ 40, 34, 30,255}, HELM_NONE,   WEAP_NONE,  0.86f },
      { "Student",         { 90,100, 86,255},{180,170,130,255},{ 38, 32, 28,255}, HELM_NONE,   WEAP_KNIFE, 0.86f },
      { "Statue",          {130,126,118,255},{170,160,140,255},{ 90, 88, 84,255}, HELM_NONE,   WEAP_NONE,  1.0f  },
      { "Guard",           { 70, 74, 86,255},{176,140, 70,255},{ 34, 30, 28,255}, HELM_FULL,   WEAP_NONE,  1.0f  },
    };
    Look lk;
    lk.skin  = (Color){ 202, 164, 128, 255 };
    lk.cloth = (Color){ 96, 92, 88, 255 };
    lk.clothDark = art_shade(lk.cloth, 0.62f);
    lk.trim  = C_GOLD; lk.hair = (Color){ 40, 34, 30, 255 };
    lk.metal = C_STEEL; lk.body = BODY_HUMAN;
    lk.weapon = WEAP_NONE; lk.shield = SHLD_NONE; lk.helm = HELM_NONE;
    lk.weaponTint = C_STEEL; lk.shieldTint = C_STEEL;
    lk.scale = 0.92f; lk.glow = false;
    for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++) {
        if (strcmp(T[i].n, name) != 0) continue;
        lk.cloth = T[i].cloth; lk.clothDark = art_shade(T[i].cloth, 0.62f);
        lk.trim = T[i].trim;   lk.hair = T[i].hair;
        lk.helm = T[i].helm;   lk.weapon = T[i].weapon;
        lk.scale = T[i].sc;
        break;
    }
    return lk;
}

typedef struct { const char *name; int bg; const char *rows[MAP_H]; } StageDef;

/* 11 stage layouts, exactly as the original's stage scripts set them:
   game.cell{x}_{y}.type = 2 marks a blocked cell on a 20-wide grid. */
static const StageDef STAGES[] = {
  { "Arena0", BG_ARENA2, {
      "....................",
      "....................",
      "####################",
      "....................",
      ".......#....#...##..",
      "...#................",
      "....................",
      "................#...",
      "......#.............",
      "....................",
      "....................",
      "....................",
      "....................",
  } },
  { "Arena1", BG_ARENA, {
      "....................",
      "....................",
      "####################",
      "...#....#..#....#...",
      "..##....#..#....##..",
      "...######..######...",
      "....................",
      "....................",
      "....................",
      "................#...",
      "....................",
      "....................",
      "....................",
  } },
  { "Arena2", BG_ARENA, {
      "....................",
      "....................",
      "####################",
      ".......##..##.......",
      "......#.####........",
      "..............#.....",
      "....................",
      "....................",
      "....#...............",
      "....................",
      "....................",
      "....................",
      "....................",
  } },
  { "Arena3", BG_ARENA, {
      "....................",
      "....................",
      "####################",
      "...#.#.#............",
      ".....###............",
      "....................",
      "....................",
      "....................",
      "................#...",
      ".......#......###...",
      "....................",
      "....................",
      "....................",
  } },
  { "Arena0", BG_ARENA3, {
      "....................",
      "....................",
      "####################",
      "....#...............",
      "....................",
      "...........#####....",
      "....................",
      "....................",
      "....................",
      "...#######..........",
      "....................",
      "....................",
      "....................",
  } },
  { "Arena0", BG_ARENA3, {
      "....................",
      "....................",
      "####################",
      "....................",
      ".......#....#.#.....",
      "....................",
      "....................",
      "...........#........",
      "....................",
      "...#####............",
      "....................",
      "....................",
      "....................",
  } },
  { "Arena0", BG_ARENA3, {
      "....................",
      "....................",
      "####################",
      "....#..........###..",
      "##################..",
      "..#.................",
      "....................",
      "....................",
      ".......#.......#....",
      "....................",
      "....................",
      "....................",
      "....................",
  } },
  { "Arena0", BG_ARENA, {
      "....................",
      "....................",
      "####################",
      "....................",
      "..#..............#..",
      ".....#..####..#.....",
      ".........##.........",
      "....................",
      "....................",
      "....................",
      "....................",
      "....................",
      "....................",
  } },
  { "Arena8", BG_ARENA, {
      "....................",
      "....................",
      "####################",
      ".........#####...#..",
      ".............#####..",
      "....................",
      "....................",
      "....................",
      ".................#..",
      "...........#........",
      "..............#.#...",
      "....................",
      "....................",
  } },
  { "Arena9", BG_ARENA, {
      "....................",
      "....................",
      "####################",
      "......#......#......",
      "....................",
      "....................",
      "....#...#...........",
      "....................",
      "......####..........",
      "....................",
      "....................",
      "....................",
      "....................",
  } },
  { "Arena10", BG_DARK, {
      "....................",
      "....................",
      "####################",
      "........#...#.......",
      "...###...###........",
      ".............#......",
      "....................",
      "...#.............#..",
      "....................",
      "....................",
      "....................",
      "....................",
      "....................",
  } },
};

/* The original's own NPC and prop cast, taken from its clip labels.  Their
   roles match the roles they fill in the original; every line of dialogue
   below is written for this remake. */
typedef struct {
    NpcKind kind; int tx, ty; const char *name; const char *line; int arg;
} NpcSeed;

static const NpcSeed VILLAGE_NPCS[] = {
  { NPC_ELDER,   10, 3, "Elder",
    "You were away on the mountain when it came. Now the road\nsouth belongs to the reaper, and we are what is left.", 0 },
  { NPC_HEALER,  16, 3, "Healer",
    "Sit. Breathe out. I can close most of what the road opens.", 0 },
  { NPC_SMITH,    4, 3, "Item Vendor",
    "Steel, leather, and a wrist guard if you have the sense.\nStrength first -- you cannot swing what you cannot lift.", 0 },
  { NPC_VILLAGER, 7, 3, "Lady",
    "My husband went south with the guard. Three of them came\nback. He was not one of the three.", 0 },
  { NPC_VILLAGER, 2, 3, "Statue",
    "A stone warrior, worn smooth. Someone keeps the moss off it.", 0 },
  { NPC_VILLAGER,18, 3, "Wounded Warrior",
    "Their guard soaks up everything you have. Break it first,\nor you will never touch the man behind it.", 0 },
  { NPC_SMITH,    5, 4, "Item Vendor 2",
    "The heavy stock. Come back when your arm is worth it.", 3 },
  { NPC_VENDOR,  14, 4, "Food Vendor",
    "Rice and broth. It is not a blade, but it keeps you\nstanding long enough to use one.", 1 },
  { NPC_VILLAGER,18, 4, "Drunkard",
    "You are the one from the mountain. Ha. They will feel\nbetter now. I will not, but they will.", 0 },
  { NPC_VILLAGER, 1, 4, "Apprentice",
    "The Elder says you trained where the air is thin.\nIs it true you never once came down?", 0 },
  { NPC_VENDOR,  16, 5, "Potion Vendor",
    "Medicine, tea, white leaves. Buy more than you think\nyou need; everyone always does.", 2 },
  { NPC_TRAINER, 10, 5, "Ninja",
    "Points are worth nothing in your pocket. Put them into\nthe arm, the guard, or the breath -- but put them in.", 0 },
  { NPC_PROP,     8, 5, "Posted Note",
    "A notice: the south road is closed. Nobody has taken it down.", 0 },
  { NPC_VILLAGER, 5, 6, "Drinker",
    "Cheapest cup in the village and it still costs too much.", 0 },
  { NPC_VILLAGER,12, 6, "Relaxing Ninja",
    "Rest while the gate holds. It will not hold long.", 0 },
  { NPC_PROP,    18, 6, "Barrel", "Rainwater, and a drowned moth.", 0 },
  { NPC_SAVE,     4, 7, "Scribe",
    "I keep the names of the dead and the deeds of the living.\nRest here and I will write yours down.", 0 },
  { NPC_ARENA,   14, 7, "Dark Ninja",
    "The ward is through here. It hits back, and it does not\nstop when you are tired.", 0 },
  { NPC_VILLAGER, 8, 7, "Meditating Ninja",
    "Speed is not hurry. The fast fighter is the one who is\nalready where the blade is going.", 0 },
  { NPC_VILLAGER,12, 8, "Student",
    "I can hold a knife. That is not the same as using one,\nthe Ninja keeps telling me.", 0 },
  { NPC_PROP,     3, 8, "Crate", "A crate, nailed shut.", 0 },
  { NPC_PROP,     6, 9, "Urn", "Chipped at the lip. Empty.", 0 },
  { NPC_PROP,    15, 9, "Bamboo", "Cut stalks, drying in a bundle.", 0 },
  { NPC_GATE,    10,11, "South Road", "The road out of the valley.", ZONE_STAGE0 },
};

void data_init_zones(Zone *zones)
{
    memset(zones, 0, sizeof(Zone) * ZONE_COUNT);

    /* ------------------------------------------------------- the village
       The original's village is laid out on its timeline rather than in a
       stage script, so this screen is ours; the cast standing in it is not. */
    {
        Zone *z = &zones[ZONE_VILLAGE];
        z->id = ZONE_VILLAGE; z->name = "Kaido Village";
        z->encounterRate = 0; z->bgStyle = BG_VILLAGE;
        z->ground = (Color){ 104, 122, 82, 255 };
        z->groundDark = (Color){ 78, 94, 62, 255 };
        z->propA = (Color){ 150, 132, 96, 255 };
        z->propB = (Color){ 126, 110, 80, 255 };
        /* The hub screen uses the original's own Arena0 layout. */
        fill_zone(z, STAGES[0].rows);
        z->entryX = 10; z->entryY = 10;
        z->exitX  = 10; z->exitY  = 10;
        for (unsigned i = 0; i < sizeof VILLAGE_NPCS / sizeof VILLAGE_NPCS[0]; i++) {
            const NpcSeed *s = &VILLAGE_NPCS[i];
            Look lk = npc_look_for(s->name);
            add_npc(z, s->kind, s->tx, s->ty, s->name, s->line, s->arg, lk);
        }
    }

    /* ------------------------------- the original's eleven stage layouts */
    static const int POOLS[11][5] = {
        { 0, 1, 3, 6, 7 }, { 1, 3, 6, 7, 8 }, { 4, 8,11,12,13 },
        {11,12,13,14,15 }, {14,15,17,18,21 }, {17,18,19,20,22 },
        {19,20,22,23,24 }, {25,26,27,28,31 }, {28,31,34,35,37 },
        {37,40,41,42,43 }, {43,44,45,46,46 },
    };
    for (int i = 0; i < 11; i++) {
        Zone *z = &zones[ZONE_STAGE0 + i];
        z->id = (ZoneId)(ZONE_STAGE0 + i);
        z->name = STAGES[i].name;
        z->bgStyle = STAGES[i].bg;
        z->encounterRate = 10 + i / 2;
        z->enemyPoolCount = 5;
        for (int k = 0; k < 5; k++) z->enemyPool[k] = POOLS[i][k];
        switch (STAGES[i].bg) {
        case BG_ARENA2:
            z->ground = (Color){ 150, 138, 104, 255 };
            z->groundDark = (Color){ 118, 108, 80, 255 }; break;
        case BG_ARENA3:
            z->ground = (Color){ 122, 118, 110, 255 };
            z->groundDark = (Color){ 92, 88, 82, 255 }; break;
        case BG_DARK:
            z->ground = (Color){ 52, 36, 58, 255 };
            z->groundDark = (Color){ 34, 24, 40, 255 }; break;
        default:
            z->ground = (Color){ 96, 120, 72, 255 };
            z->groundDark = (Color){ 70, 92, 56, 255 }; break;
        }
        z->propA = art_shade(z->ground, 1.15f);
        z->propB = art_shade(z->ground, 0.85f);
        fill_zone(z, STAGES[i].rows);
        for (int y = 0; y < MAP_H; y++)
            for (int x = 0; x < MAP_W; x++)
                if (z->tiles[y][x] == T_WALL) z->tiles[y][x] = T_ROCK;

        /* Row 3 of the original's grid is a solid wall, so the gates go on
           the first and last walkable rows of the play area. */
        int bx = -1, fx = -1;
        for (int x = 1; x < MAP_W - 1; x++)
            if (!tile_solid(z->tiles[3][x])) { bx = x; break; }
        for (int x = MAP_W - 2; x > 0; x--)
            if (!tile_solid(z->tiles[MAP_H - 3][x])) { fx = x; break; }
        if (bx < 0) bx = 1;
        if (fx < 0) fx = MAP_W - 2;
        z->entryX = bx; z->entryY = 4;
        z->exitX  = fx; z->exitY  = MAP_H - 4;

        ZoneId back = (i == 0) ? ZONE_VILLAGE : (ZoneId)(ZONE_STAGE0 + i - 1);
        add_npc(z, NPC_GATE, bx, 3, "Back", "The way you came.", back,
                npc_look_for("Guard"));
        if (i < 10)
            add_npc(z, NPC_GATE, fx, MAP_H - 3, "Onward", "Deeper in.",
                    (ZoneId)(ZONE_STAGE0 + i + 1), npc_look_for("Guard"));
    }
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
    case NPC_GATE: {
        Zone *dst = &g->zones[n->arg];
        bool deeper = (int)n->arg > (int)p->zone;
        int tx = deeper ? dst->entryX : dst->exitX;
        int ty = deeper ? dst->entryY : dst->exitY;
        world_enter_zone(g, (ZoneId)n->arg, tx, ty);
        go_scene(g, n->arg == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
        ui_toast(g, "%s", dst->name);
    } break;
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
        ui_text_c(n->name, px, (n->ty <= 3) ? py + 6 : py - TILE * 1.5f, 15, C_PARCH);
        return;
    }
    if (n->kind == NPC_PROP) {
        int kind = (int)(n->name[0] + n->name[1]) % 6;   /* stable per name */
        art_draw_prop(kind, (Vector2){ px, py }, 0.8f, n->look.cloth, n->look.trim);
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
