/* ===========================================================================
   world.c -- the temple: rooms, walking, NPCs, encounters.

   The original's temple is eleven single-screen rooms on its root timeline,
   joined by edge trigger clips: stand on one, press space, and it calls
   NewStage(dir, x, y) and jumps to that exit's own destination.  That is
   reproduced here -- the graph, the entry cell and facing for all twenty
   exits, and the gate on the last one.  Rooms roll random encounters
   against a per-room enemy pool as you walk.
   =========================================================================== */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
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
           t == T_VOID || t == T_LAVA || t == T_OCCUPIED;
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
  { "Arena4", BG_ARENA3, {
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
  { "Arena5", BG_ARENA3, {
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
  { "Arena6", BG_ARENA3, {
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
  { "Arena7", BG_ARENA, {
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
/* The original's own cast, placed room by room exactly where its clips sit.
   The roles are the original's; every line of dialogue below is written for
   this remake.  Cell coordinates are floor(pixel / 30) on the 20x13 grid. */
typedef struct {
    int room; NpcKind kind; int tx, ty; const char *name; const char *line; int arg;
} NpcSeed;

static const NpcSeed ROOM_NPCS[] = {
  /* --- Arena0, the entrance, and the closest thing to a hub ------------ */
  { 0, NPC_ELDER,    16, 7, "Elder",
    "You were away on the mountain when it came.  What is left of\nus is standing in this room.", 0 },
  { 0, NPC_HEALER,    3, 5, "Healer",
    "Sit.  Breathe out.  I can close most of what the road opens.", 0 },
  { 0, NPC_VILLAGER,  6, 8, "Student",
    "They say the deeper rooms are worse.  I have not gone far\nenough to argue.", 0 },
  /* --- Arena1 --------------------------------------------------------- */
  { 1, NPC_VENDOR,   13, 4, "Food Vendor",
    "Eat before you walk.  It is cheaper than bleeding.", 1 },
  { 1, NPC_VENDOR,    6, 4, "Potion Vendor",
    "Life and mana, bottled.  You will want both.", 1 },
  { 1, NPC_VILLAGER, 16, 9, "Ninja",
    "West is quiet.  East is not.  Choose by what you can carry.", 0 },
  /* --- Arena2 --------------------------------------------------------- */
  { 2, NPC_VILLAGER,  4, 9, "Apprentice",
    "I sweep this room so I do not have to think about the next one.", 0 },
  { 2, NPC_VILLAGER, 14, 5, "Guard",
    "Nothing through here but dust.  Go back the way you came.", 0 },
  /* --- Arena3 --------------------------------------------------------- */
  { 3, NPC_SMITH,     6, 4, "Item Vendor",
    "Steel, leather, and a wrist guard if you have the sense.\nStrength first -- you cannot swing what you cannot lift.", 0 },
  { 3, NPC_VILLAGER,  7, 9, "Apprentice",
    "Two vendors in one room.  They argue about prices all day.", 0 },
  { 3, NPC_SMITH,    14,10, "Item Vendor 2",
    "Better stock than his, and I will not pretend otherwise.", 3 },
  /* --- Arena4 --------------------------------------------------------- */
  { 4, NPC_VILLAGER,  3,10, "Drinker",
    "One more and I will go home.  I have said that four times.", 0 },
  { 4, NPC_VILLAGER,  8,10, "Drunkard",
    "The statues move.  I have watched them.  Nobody believes me.", 0 },
  { 4, NPC_VILLAGER, 15, 5, "Drinker",
    "Do not mind him.  He is right, but do not mind him.", 0 },
  { 4, NPC_VILLAGER,  4, 3, "Ninja",
    "Drink here if you must.  Do not drink past this room.", 0 },
  /* --- Arena5 --------------------------------------------------------- */
  { 5, NPC_VENDOR,   14, 4, "Vendor 2",
    "Relics.  Odd things.  They do more than they look like they do.", 2 },
  { 5, NPC_VILLAGER, 11, 7, "Apprentice",
    "Up from here is the hall of statues.  Mind your footing.", 0 },
  { 5, NPC_VENDOR,    7, 9, "Vendor",
    "Supplies, same as the last room, worse light.", 1 },
  /* --- Arena6 --------------------------------------------------------- */
  { 6, NPC_SAVE,      5, 4, "Scribe",
    "Rest, and I will write down where you stood.", 0 },
  { 6, NPC_VILLAGER,  7, 8, "Lady",
    "My husband went east with the guard.  Three came back.\nHe was not one of the three.", 0 },
  { 6, NPC_VILLAGER, 15, 9, "Relaxing Ninja",
    "I have earned this floor and I intend to keep sitting on it.", 0 },
  /* --- Arena7, the hall of statues: the three gateways ----------------- */
  { 7, NPC_PORTAL,   10, 5, "Statue",
    "Three gateways, and a long walk behind each.", 0 },
  { 7, NPC_PROP,      5, 5, "Statue",
    "Carved mid-step, as though it meant to leave.", 0 },
  { 7, NPC_PROP,     14, 5, "Statue",
    "The same face as the other, worn smoother.", 0 },
  /* --- Arena8 --------------------------------------------------------- */
  { 8, NPC_TRAINER2,  3, 2, "Posted Note",
    "Training room.  Strike the ward for experience, and break its\nguard for more.", 0 },
  { 8, NPC_VENDOR,   10, 4, "Vendor 3",
    "Everything here is overpriced.  You will buy it anyway.", 2 },
  { 8, NPC_VILLAGER, 11, 9, "Dark Ninja",
    "You smell like the shallow rooms.  That will change.", 0 },
  { 8, NPC_VILLAGER, 17, 5, "Guard",
    "Train first.  The rooms past here do not offer a second try.", 0 },
  /* --- Arena9 --------------------------------------------------------- */
  { 9, NPC_VILLAGER,  4, 6, "Dark Ninja",
    "The gate above opens for whoever finishes the human gateway.\nNot before.", 0 },
  { 9, NPC_TRAINER,   8, 7, "Meditating Ninja",
    "Sit.  Spend what you have earned before you spend your life.", 0 },
  /* --- Arena10 -------------------------------------------------------- */
  {10, NPC_VILLAGER, 13, 5, "Guard",
    "You came up the stairs.  Few do.", 0 },
  {10, NPC_VENDOR,    3, 7, "Vendor 4",
    "Last of the stock, and the last room that sells any.", 3 },
  {10, NPC_ARENA,    17, 7, "Shadow",
    "Stand and be counted, one after another.", 0 },
  {10, NPC_VILLAGER,  5, 4, "Wounded Warrior",
    "I got this far.  That is the whole of what I have to teach.", 0 },
};

/* The room graph, recovered from the original's edge trigger clips.  Each
   clip fires NewStage(dir, x, y) on space and jumps to its own stagelabel.
   ty < 0 marks an edge exit that spans its whole column; entryY < 0 keeps
   the row you left on, which is what the original's Math.ceil does. */
static const Exit ROOM_EXITS[] = {
  /* room 0 */ { EX_UP,    10, 4, ZONE_ARENA1,  10,  9, -1, 0 },
  /* room 1 */ { EX_DOWN,  10,10, ZONE_ARENA0,  10,  5, -1, 0 },
               { EX_LEFT,   0,-1, ZONE_ARENA2,  18, -1, -1, 0 },
               { EX_RIGHT, 19,-1, ZONE_ARENA3,   1, -1, -1, 0 },
  /* room 2 */ { EX_RIGHT, 19,-1, ZONE_ARENA1,   1, -1, -1, 0 },
  /* room 3 */ { EX_LEFT,   0,-1, ZONE_ARENA1,  18, -1, -1, 0 },
               { EX_UP,    15, 4, ZONE_ARENA4,  13,  9, -1, 0 },
  /* room 4 */ { EX_DOWN,  13,10, ZONE_ARENA3,  13,  5, -1, 0 },
               { EX_LEFT,   0,-1, ZONE_ARENA5,  18, -1, -1, 0 },
  /* room 5 */ { EX_LEFT,   0,-1, ZONE_ARENA6,  18, -1, -1, 0 },
               { EX_RIGHT, 19,-1, ZONE_ARENA4,   1, -1, -1, 0 },
               { EX_UP,    10, 4, ZONE_ARENA7,  10,  9, -1, 0 },
  /* room 6 */ { EX_RIGHT, 19,-1, ZONE_ARENA5,   1, -1, -1, 0 },
  /* room 7 */ { EX_DOWN,  10,10, ZONE_ARENA5,  10,  5, -1, 0 },
               { EX_RIGHT, 19,-1, ZONE_ARENA8,   1, -1, -1, 0 },
               { EX_LEFT,   0,-1, ZONE_ARENA9,  18, -1, -1, 0 },
  /* room 8 */ { EX_LEFT,   0,-1, ZONE_ARENA7,  18, -1, -1, 0 },
  /* room 9 */ { EX_RIGHT, 19,-1, ZONE_ARENA7,   1, -1, -1, 0 },
               { EX_UP,    10, 4, ZONE_ARENA10, 10,  9,  0,20 },
  /* room 10*/ { EX_DOWN,  10,10, ZONE_ARENA9,  10,  5, -1, 0 },
};
static const int EXIT_ROOM[] = { 0, 1,1,1, 2, 3,3, 4,4, 5,5,5, 6, 7,7,7, 8, 9,9, 10 };

#include "scenery_table.h"

void data_init_zones(Zone *zones)
{
    memset(zones, 0, sizeof(Zone) * ZONE_COUNT);

    static const int POOLS[11][5] = {
        { 0, 1, 3, 6, 7 }, { 1, 3, 6, 7, 8 }, { 4, 8,11,12,13 },
        {11,12,13,14,15 }, {14,15,17,18,21 }, {17,18,19,20,22 },
        {19,20,22,23,24 }, {25,26,27,28,31 }, {28,31,34,35,37 },
        {37,40,41,42,43 }, {43,44,45,46,46 },
    };
    /* The temple is a graph, not a ladder, so difficulty follows the walk
       out from the entrance rather than the room number. */
    static const int DEPTH[11] = { 0, 1, 2, 2, 3, 4, 5, 5, 6, 6, 7 };

    for (int i = 0; i < 11; i++) {
        Zone *z = &zones[ZONE_ARENA0 + i];
        z->id = (ZoneId)(ZONE_ARENA0 + i);
        z->name = STAGES[i].name;
        z->bgStyle = STAGES[i].bg;
        /* The entrance room is safe; it is where the Elder and Healer stand. */
        z->encounterRate = (i == 0) ? 0 : 8 + DEPTH[i];
        z->minLevel = 1 + DEPTH[i] * 2;
        z->maxLevel = z->minLevel + 2;
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
        /* Row 2 is the full-width back wall in every layout.  Every other
           type-2 cell is blocked because something stands on it, and that
           something is now drawn from the scenery table, so those cells read
           as floor while staying impassable. */
        for (int y = 0; y < MAP_H; y++)
            for (int x = 0; x < MAP_W; x++)
                if (z->tiles[y][x] == T_WALL)
                    z->tiles[y][x] = (y == 2) ? T_ROCK : T_OCCUPIED;

        z->entryX = 10; z->entryY = 5;
        z->exitX  = 10; z->exitY  = 9;
    }

    /* the exits */
    for (unsigned e = 0; e < sizeof ROOM_EXITS / sizeof ROOM_EXITS[0]; e++) {
        Zone *z = &zones[ZONE_ARENA0 + EXIT_ROOM[e]];
        if (z->exitCount < 4) z->exits[z->exitCount++] = ROOM_EXITS[e];
    }

    /* Scenery, at the original's own positions.  Its 600x390 pixel space maps
       onto this one at TILE/30 per pixel. */
    for (unsigned e = 0; e < sizeof ROOM_PROPS / sizeof ROOM_PROPS[0]; e++) {
        const PropSeed *ps = &ROOM_PROPS[e];
        Zone *z = &zones[ZONE_ARENA0 + ps->room];
        if (z->propCount >= MAX_PROPS) continue;
        const float k = TILE / 30.0f;
        Prop *pr = &z->props[z->propCount++];
        pr->kind = ps->kind;
        pr->x = OX + ps->x * k;
        pr->y = OY + ps->y * k;
        pr->w = ps->w * k;
        pr->h = ps->h * k;
    }

    /* The cast.  `type = 2` in the original's stage scripts marks a cell as
       *occupied*, not as scenery: NewStage resets every cell to 1 and the
       script then blocks the cells that something stands on.  24 of these 34
       clip positions land on a type-2 cell, against 1.8 expected by chance,
       so the standing figure is what blocks it.  An NPC already makes its own
       cell impassable here, so clear the floor under one rather than drawing
       a rock and standing the NPC on top of it. */
    for (unsigned i = 0; i < sizeof ROOM_NPCS / sizeof ROOM_NPCS[0]; i++) {
        const NpcSeed *s = &ROOM_NPCS[i];
        Zone *z = &zones[ZONE_ARENA0 + s->room];
        if (s->tx >= 0 && s->tx < MAP_W && s->ty >= 0 && s->ty < MAP_H)
            z->tiles[s->ty][s->tx] = T_GRASS;
        add_npc(z, s->kind, s->tx, s->ty, s->name, s->line, s->arg,
                npc_look_for(s->name));
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
    if (getenv("SJ_NOFIGHT")) return;   /* deterministic scripted walkthroughs */
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
    case NPC_PORTAL:
        g->portalIdx = 0;
        go_scene(g, SCENE_PORTAL);
        break;
    case NPC_TRAINER2:
        g->trainGain = 0; g->trainT = 0;
        go_scene(g, SCENE_TRAINING);
        break;
    case NPC_PICKUP: {
        /* The original hides items behind searchable scenery; each is once only. */
        int slot = n->arg & 15;
        if (p->picked[slot]) { ui_toast(g, "Nothing more here."); break; }
        p->picked[slot] = true;
        if (slot == 0) {
            p->skillPts++;
            ui_toast(g, "A ring, and the sense to use it. +1 skill point.");
            player_add_item(p, IT_MENDOS_RING);
        } else {
            p->gold += 120;
            ui_toast(g, "Someone's hidden purse. +120 gold.");
        }
    } break;
    case NPC_HEALER:
        /* The original opens the Heal panel and pauses; it does not charge
           you the moment you walk up. */
        g->healSel = 0;
        go_scene(g, SCENE_HEAL);
        break;
    case NPC_SAVE: {
        Combatant c;
        player_recalc(p, &c);
        if (p->rests <= 0) {
            ui_toast(g, "You have no rests left. The shrine only gives so much.");
            break;
        }
        p->rests--;
        p->curLife = c.lifeMax;
        p->curMana = c.manaMax;
        p->curEnergy = c.engMax;
        p->saveZone = p->zone; p->saveX = p->tx; p->saveY = p->ty;
        if (save_write(g)) { g->hasSave = true;
            ui_toast(g, "Rested and saved. %d rests left.", p->rests); }
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

/* The exit trigger under a cell, if any.  Edge exits (ty < 0) span their
   whole column, which is how the original's tall edge clips behave. */
static const Exit *exit_at(const Zone *z, int tx, int ty)
{
    for (int i = 0; i < z->exitCount; i++) {
        const Exit *e = &z->exits[i];
        if (e->ty < 0) { if (tx == e->tx) return e; }
        else if (tx == e->tx && ty == e->ty) return e;
    }
    return NULL;
}

/* Take an exit: NewStage(dir, x, y).  A horizontal exit keeps the row you
   walked in on, as the original's Math.ceil does. */
static void take_exit(Game *g, const Exit *e)
{
    Player *p = &g->p;
    if (e->gatePortal >= 0 && p->portalLevel[e->gatePortal] <= e->gateNeed) {
        ui_toast(g, "Sealed. Finish the gateway below and come back.");
        return;
    }
    int nx = e->entryX;
    int ny = (e->entryY < 0) ? p->ty : e->entryY;
    if (ny < 0) ny = 0;
    if (ny >= MAP_H) ny = MAP_H - 1;
    /* the original drops you on the entry cell; nudge off a blocked one */
    Zone *dst = &g->zones[e->dest];
    if (tile_solid(dst->tiles[ny][nx])) {
        for (int r = 1; r < MAP_H && tile_solid(dst->tiles[ny][nx]); r++) {
            if (ny + r < MAP_H && !tile_solid(dst->tiles[ny + r][nx])) { ny += r; break; }
            if (ny - r >= 0    && !tile_solid(dst->tiles[ny - r][nx])) { ny -= r; break; }
        }
    }
    world_enter_zone(g, e->dest, nx, ny);
    go_scene(g, e->dest == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
    ui_toast(g, "%s", dst->name);
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
        const Exit *e = exit_at(z, p->tx, p->ty);
        if (e) { take_exit(g, e); return; }
    }

    if (dx || dy) {
        int nx = p->tx + dx, ny = p->ty + dy;
        if (walkable(g, z, nx, ny)) {
            g->fromX = p->tx; g->fromY = p->ty;
            p->tx = nx; p->ty = ny;
            g->moving = true;
            g->moveT = 0;
            sound_play(SFX_STEP);
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

    /* Scenery, at the original's own positions rather than scattered. */
    for (int i = 0; i < z->propCount; i++)
        art_draw_scenery(&z->props[i], z, t);

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
