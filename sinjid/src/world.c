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

/* The original's stage is 600 wide: the room occupies the top 335 pixels and
   the walkmenu bar the 116 below it, on a 30px cell grid.  Everything here is
   that layout scaled by HUD_S, so the room and the bar keep their proportions.
   600 * 1.6 = 960 wide, and 451 * 1.6 = 722, which is the window height. */
#define TALK_REACH 70.0f   /* how far the original's person clips reach */
#define HUD_S  1.6f                     /* original pixels -> ours          */
#define TILE   48                       /* 30 * HUD_S                       */
#define OX     160                      /* (1280 - 20 * TILE) / 2           */
#define OY     0
#define HUD_Y  536                      /* 335 * HUD_S                      */
#define HUD_H  186                      /* 116 * HUD_S                      */
#define HX(v)  (OX + (int)((v) * HUD_S))   /* original x -> screen          */
#define HY(v)  ((int)((v) * HUD_S))        /* original y -> screen          */

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
      { "Statue2",         {118,114,108,255},{156,148,130,255},{ 82, 80, 76,255}, HELM_NONE,   WEAP_NONE,  1.05f },
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
  { 0, NPC_SAVE,     16, 7, "Elder",
    "You were away on the mountain when it came.  What is left of\nus is standing in this room.", 0 },
  { 0, NPC_HEALER,    3, 5, "Healer",
    "Sit.  Breathe out.  I can close most of what the road opens.", 0 },
  { 0, NPC_VILLAGER,  6, 8, "Student",
    "They say the deeper rooms are worse.  I have not gone far\nenough to argue.", 0 },
  /* --- Arena1 --------------------------------------------------------- */
  { 1, NPC_VENDOR,   13, 4, "Food Vendor",
    "Eat before you walk.  It is cheaper than bleeding.", SHOP_MEALS },
  { 1, NPC_VENDOR,    6, 4, "Potion Vendor",
    "Life and mana, bottled.  You will want both.", SHOP_POTS },
  { 1, NPC_VILLAGER, 16, 9, "Ninja",
    "West is quiet.  East is not.  Choose by what you can carry.", 0 },
  /* --- Arena2 --------------------------------------------------------- */
  { 2, NPC_VILLAGER,  4, 9, "Apprentice",
    "I sweep this room so I do not have to think about the next one.", 0 },
  { 2, NPC_VILLAGER, 14, 5, "Guard",
    "Nothing through here but dust.  Go back the way you came.", 0 },
  /* --- Arena3 --------------------------------------------------------- */
  { 3, NPC_VENDOR,    6, 4, "Item Vendor",
    "Steel, leather, and a wrist guard if you have the sense.\nStrength first -- you cannot swing what you cannot lift.", SHOP_ITEMS0 },
  { 3, NPC_VILLAGER,  7, 9, "Apprentice",
    "Two vendors in one room.  They argue about prices all day.", 0 },
  { 3, NPC_VENDOR,   14,10, "Item Vendor 2",
    "Better stock than his, and I will not pretend otherwise.", SHOP_TRADE },
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
    "Relics.  Odd things.  They do more than they look like they do.", SHOP_ITEMS1 },
  { 5, NPC_VILLAGER, 11, 7, "Apprentice",
    "Up from here is the hall of statues.  Mind your footing.", 0 },
  { 5, NPC_VENDOR,    7, 9, "Vendor",
    "Supplies, same as the last room, worse light.", SHOP_ITEMS2 },
  /* --- Arena6 --------------------------------------------------------- */
  { 6, NPC_VILLAGER,  5, 4, "Scribe",
    "Rest, and I will write down where you stood.", 0 },
  { 6, NPC_QUEST,     7, 8, "Lady",
    "My husband went east with the guard.  Three came back.\nHe was not one of the three.", 2 },
  { 6, NPC_VILLAGER, 15, 9, "Relaxing Ninja",
    "I have earned this floor and I intend to keep sitting on it.", 0 },
  /* --- Arena7, the hall of statues: the three gateways ----------------- */
  { 7, NPC_PROP,     10, 5, "Statue",
    "Three gateways, and a long walk behind each.", 0 },
  { 7, NPC_PROP,      5, 5, "Statue2",
    "Carved mid-step, as though it meant to leave.", 0 },
  { 7, NPC_QUEST,    13, 3, "Offering Stone",
    "A worn dish set into the stone, waiting for something.", 1 },
  { 7, NPC_PROP,     14, 5, "Statue2",
    "The same face as the other, worn smoother.", 0 },
  /* --- Arena8 --------------------------------------------------------- */
  { 8, NPC_TRAINER2,  3, 2, "Posted Note",
    "Training room.  Strike the ward for experience, and break its\nguard for more.", 0 },
  { 8, NPC_VENDOR,   10, 4, "Vendor 3",
    "Everything here is overpriced.  You will buy it anyway.", SHOP_ITEMS3 },
  { 8, NPC_VILLAGER, 11, 9, "Dark Ninja",
    "You smell like the shallow rooms.  That will change.", 0 },
  { 8, NPC_VILLAGER, 17, 5, "Guard",
    "Train first.  The rooms past here do not offer a second try.", 0 },
  /* --- Arena9 --------------------------------------------------------- */
  { 9, NPC_VILLAGER,  4, 6, "Dark Ninja",
    "The gate above opens for whoever finishes the human gateway.\nNot before.", 0 },
  { 9, NPC_QUEST,    8, 7, "Meditating Ninja",
    "Sit.  Spend what you have earned before you spend your life.", 0 },
  /* --- Arena10 -------------------------------------------------------- */
  {10, NPC_VILLAGER, 13, 5, "Guard",
    "You came up the stairs.  Few do.", 0 },
  {10, NPC_VENDOR,    3, 7, "Vendor 4",
    "Last of the stock, and the last room that sells any.", SHOP_ITEMS4 },
  {10, NPC_VENDOR,   17, 7, "Shadow",
    "Stand and be counted, one after another.", SHOP_ITEMS5 },
  {10, NPC_VILLAGER,  5, 4, "Wounded Warrior",
    "I got this far.  That is the whole of what I have to teach.", 0 },
};

/* The room graph, recovered from the original's edge trigger clips.  Each
   clip fires NewStage(dir, x, y) on space and jumps to its own stagelabel.
   ty < 0 marks an edge exit that spans its whole column; entryY < 0 keeps
   the row you left on, which is what the original's Math.ceil does. */
static const Exit ROOM_EXITS[] = {
  /* room 0 */ { EX_UP,    10, 4, ZONE_ARENA1,  10,  9, -1, 0, -1 },
  /* room 1 */ { EX_DOWN,  10,10, ZONE_ARENA0,  10,  5, -1, 0, -1 },
               { EX_LEFT,   0,-1, ZONE_ARENA2,  18, -1, -1, 0, -1 },
               { EX_RIGHT, 19,-1, ZONE_ARENA3,   1, -1, -1, 0, -1 },
  /* room 2 */ { EX_RIGHT, 19,-1, ZONE_ARENA1,   1, -1, -1, 0, -1 },
  /* room 3 */ { EX_LEFT,   0,-1, ZONE_ARENA1,  18, -1, -1, 0, -1 },
               { EX_UP,    15, 4, ZONE_ARENA4,  13,  9, -1, 0, -1 },
  /* room 4 */ { EX_DOWN,  13,10, ZONE_ARENA3,  13,  5, -1, 0, -1 },
               { EX_LEFT,   0,-1, ZONE_ARENA5,  18, -1, -1, 0, -1 },
  /* room 5 */ { EX_LEFT,   0,-1, ZONE_ARENA6,  18, -1, -1, 0, -1 },
               { EX_RIGHT, 19,-1, ZONE_ARENA4,   1, -1, -1, 0, -1 },
               { EX_UP,    10, 4, ZONE_ARENA7,  10,  9, -1, 0, -1 },
  /* room 6 */ { EX_RIGHT, 19,-1, ZONE_ARENA5,   1, -1, -1, 0, -1 },
  /* room 7 */ { EX_DOWN,  10,10, ZONE_ARENA5,  10,  5, -1, 0, -1 },
               { EX_RIGHT, 19,-1, ZONE_ARENA8,   1, -1, -1, 0, -1 },
               { EX_LEFT,   0,-1, ZONE_ARENA9,  18, -1, -1, 0, -1 },
  /* room 8 */ { EX_LEFT,   0,-1, ZONE_ARENA7,  18, -1, -1, 0, -1 },
  /* room 9 */ { EX_RIGHT, 19,-1, ZONE_ARENA7,   1, -1, -1, 0, -1 },
               { EX_UP,    10, 4, ZONE_ARENA10, 10,  9,  0,20, -1 },
  /* room 10*/ { EX_DOWN,  10,10, ZONE_ARENA9,  10,  5, -1, 0, -1 },
  /* The three portals are gated doorways of the same kind, not a statue:
     the entrance room's south door, one in Arena8 and one in Arena2, each
     behind its own portallevel threshold. */
  /* room 0 */ { EX_DOWN,  10,10, ZONE_ARENA0,  10,  9, -1, 0,  0 },
  /* room 8 */ { EX_UP,    15, 5, ZONE_ARENA8,  15,  6, -1, 0,  1 },
  /* room 2 */ { EX_UP,     9, 5, ZONE_ARENA2,   9,  6, -1, 0,  2 },
};
static const int EXIT_ROOM[] = { 0, 1,1,1, 2, 3,3, 4,4, 5,5,5, 6, 7,7,7, 8, 9,9, 10,
                                 0, 8, 2 };

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

static const Exit *exit_near(const Zone *z, float px, float py);

void world_enter_zone(Game *g, ZoneId z, int tx, int ty)
{
    g->p.zone = z;
    g->p.tx = tx; g->p.ty = ty;
    g->p.px = tx * 30.0f + 15.0f;    /* cell centre, in the original's px  */
    g->p.py = ty * 30.0f + 15.0f;
    g->fromX = tx; g->fromY = ty;
    g->moving = false; g->moveT = 0;
    g->stepsSinceFight = 0;
    /* Disarm the exit we land inside, if any, until the player steps out. */
    g->exitLock = -1;
    {
        const Zone *dz = &g->zones[z];
        const Exit *on = exit_near(dz, g->p.px, g->p.py);
        if (on) {
            g->exitLock = (int)(on - dz->exits);
            g->exitLockX = g->p.px;
            g->exitLockY = g->p.py;
        }
    }
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
    case NPC_VENDOR:
        /* Each merchant opens its own screen: the original gives every one of
           them its own frame in the interface clip, so the shop is arg, not a
           tier picked from the player's level. */
        g->shopVendor = n->arg;
        g->shopIdx = 0;
        g->shopMode = (n->arg == SHOP_TRADE) ? 1 : 0;   /* Trade opens on sell */
        g->dialogNpc = (int)(n - g->zones[p->zone].npcs);
        go_panel(g, SCENE_SHOP);
        break;
    case NPC_TRAINER:
        g->menuIdx = 0; g->menuTab = 0;
        go_panel(g, SCENE_TRAIN);
        break;
    case NPC_PORTAL:
        g->portalIdx = 0;
        go_panel(g, SCENE_PORTAL);
        break;
    case NPC_TRAINER2:
        g->trainGain = 0; g->trainT = 0;
        go_panel(g, SCENE_TRAINING);
        break;
    case NPC_QUEST: {
        /* The original's small fetch quests, run through SearchItem/GetItem:
           the Meditating Ninja hands over Mendo's Ring once (guarded by its
           moon_q flag), the offering stone in the hall of statues takes the
           ring for a skill point, and the Lady trades White Leaves for
           Medicine.  Each consumes the item it asks for. */
        switch (n->arg) {
        case 0:
            if (p->picked[0]) { ui_toast(g, "\"Go on. It is not mine to keep.\""); break; }
            if (player_add_item(p, IT_MENDOS_RING2) < 0) {
                ui_toast(g, "Your pack is full.");
                break;
            }
            p->picked[0] = true;
            sound_play(SFX_ITEM);
            ui_toast(g, "The ninja presses a ring into your hand.");
            break;
        case 1:
            if (player_take_item(p, IT_MENDOS_RING2)) {
                p->skillPts++;
                sound_play(SFX_LEVEL);
                ui_toast(g, "The ring settles into the dish. +1 skill point.");
            } else ui_toast(g, "The dish is empty, and stays empty.");
            break;
        default: {
            /* The original walks the pack and converts every White Leaves it
               finds, not one, so a whole gathering trip is turned in at once. */
            int made = 0;
            while (player_take_item(p, IT_WHITE_LEAVES)) {
                if (player_add_item(p, IT_MEDICINE) < 0) {
                    player_add_item(p, IT_WHITE_LEAVES);   /* no room; put it back */
                    ui_toast(g, "Your pack is full.");
                    break;
                }
                made++;
            }
            if (made > 0) {
                sound_play(SFX_ITEM);
                ui_toast(g, "She works the leaves down into %d medicine.", made);
            } else ui_toast(g, "\"Bring me white leaves and I will make something of them.\"");
        } break;
        }
    } break;
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
        go_panel(g, SCENE_HEAL);
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
        go_panel(g, SCENE_DIALOG);
        break;
    }
}

/* The exit trigger under a cell, if any.  Edge exits (ty < 0) span their
   whole column, which is how the original's tall edge clips behave. */
/* The original's exit triggers are clips with real area, tested against the
   player with hitTest, so a way out lights up as you come near it rather than
   on one exact cell.  Edge triggers are tall strips down their whole column. */
/* Is the character's footprint blocked at this pixel position?  Cells are 30
   pixels in the original, and a type-2 cell is impassable. */
static bool blocked_at(const Zone *z, float px, float py)
{
    /* A character standing there blocks the way, as in the original. */
    for (int i = 0; i < z->npcCount; i++) {
        const Npc *n = &z->npcs[i];
        if (n->kind == NPC_NONE) continue;
        float nx = n->tx * 30.0f + 15.0f, ny = n->ty * 30.0f + 15.0f;
        if (fabsf(px - nx) < 17.0f && fabsf(py - ny) < 15.0f) return true;
    }
    const float r = 8.0f;                      /* a small body, not a whole cell */
    const float ox[4] = { -r, r, -r, r }, oy[4] = { -r, -r, r, r };
    for (int i = 0; i < 4; i++) {
        int cx = (int)((px + ox[i]) / 30.0f), cy = (int)((py + oy[i]) / 30.0f);
        if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) return true;
        if (tile_solid(z->tiles[cy][cx])) return true;
    }
    return false;
}

static const Exit *exit_near(const Zone *z, float px, float py)
{
    for (int i = 0; i < z->exitCount; i++) {
        const Exit *e = &z->exits[i];
        float ex = e->tx * 30.0f + 15.0f;
        /* The trigger clip is 23x38; test it against the player's body as an
           overlap, the way hitTest does.  Sized generously it swallows most
           of a room, and pressing space to talk takes an exit instead --
           which is how a gateway got entered by accident. */
        const float bodyX = 9.0f, bodyY = 9.0f;
        if (e->ty < 0) {                       /* an edge strip */
            if (fabsf(px - ex) <= 15.0f + bodyX) return e;
        } else {
            /* The clip sits high in its cell: the extraction puts the up
               triggers at y~123 for a cell centred on 135, and the down ones
               at ~307 for 315, so the box is centred ten pixels above. */
            float ey = e->ty * 30.0f + 15.0f - 10.0f;
            if (fabsf(px - ex) <= 11.5f + bodyX &&
                fabsf(py - ey) <= 19.0f + bodyY) return e;
        }
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
    if (e->portal >= 0) {
        /* A portal doorway opens the gateway rather than moving you a room. */
        g->portalIdx = e->portal;
        go_panel(g, SCENE_PORTAL);
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
        go_panel(g, SCENE_MENU);
        return;
    }
    if (IsKeyPressed(KEY_T)) { g->menuIdx = 0; g->menuTab = 0; go_panel(g, SCENE_TRAIN); return; }

    /* Movement is free, not tile-stepped.  The original runs an onEnterFrame
       that walks the character by game.speed pixels: 6 a frame while energy
       lasts and 4 once it is spent, at 24fps, so 144 and 96 pixels a second.
       Each axis is read on its own chain, which is what makes a diagonal work. */
    Combatant cc;
    player_recalc(p, &cc);
    float speed = (p->curEnergy > 0 ? 144.0f : 96.0f) * dt;

    float vx = 0, vy = 0;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))     vx =  1;
    else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) vx = -1;
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))        vy = -1;
    else if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) vy =  1;

    if (vx < 0)      p->dir = 2;
    else if (vx > 0) p->dir = 3;
    else if (vy < 0) p->dir = 0;
    else if (vy > 0) p->dir = 1;

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        /* The original tests the player against the character's clip, so
           reach is a radius rather than the next cell along. */
        Npc *best = NULL; float bestD = TALK_REACH;   /* reaches over a counter */
        for (int i = 0; i < z->npcCount; i++) {
            Npc *n = &z->npcs[i];
            if (n->kind == NPC_NONE) continue;
            float dx2 = (n->tx * 30.0f + 15.0f) - p->px;
            float dy2 = (n->ty * 30.0f + 15.0f) - p->py;
            float d = sqrtf(dx2 * dx2 + dy2 * dy2);
            if (d < bestD) { bestD = d; best = n; }
        }
        if (best) { interact(g, best); return; }
        const Exit *e = exit_near(z, p->px, p->py);
        if (e && (int)(e - z->exits) != g->exitLock) { take_exit(g, e); return; }
    }

    /* The lock lifts once the player leaves that exit's box, or walks into
       the doorway rather than merely shuffling about in front of it -- so a
       step sideways on arrival does nothing, but walking on out still works. */
    if (g->exitLock >= 0) {
        const Exit *on = exit_near(z, p->px, p->py);
        if (!on || (int)(on - z->exits) != g->exitLock) g->exitLock = -1;
        else {
            const float slack = 6.0f;
            switch (on->dir) {
            case EX_UP:    if (p->py < g->exitLockY - slack) g->exitLock = -1; break;
            case EX_DOWN:  if (p->py > g->exitLockY + slack) g->exitLock = -1; break;
            case EX_LEFT:  if (p->px < g->exitLockX - slack) g->exitLock = -1; break;
            case EX_RIGHT: if (p->px > g->exitLockX + slack) g->exitLock = -1; break;
            }
        }
    }

    if (vx || vy) {
        if (vx && vy) { vx *= 0.7071f; vy *= 0.7071f; }   /* even diagonal pace */
        float nx = p->px + vx * speed, ny = p->py + vy * speed;
        /* Collide per axis so a wall is slid along rather than stuck on. */
        if (!blocked_at(z, nx, p->py)) p->px = nx;
        if (!blocked_at(z, p->px, ny)) p->py = ny;
        if (p->px < 6)   p->px = 6;
        if (p->py < 6)   p->py = 6;
        if (p->px > MAP_W * 30 - 6) p->px = MAP_W * 30 - 6;
        /* The room is masked to the top 335 pixels of the original's stage --
           the bar covers the rest -- so the floor ends there rather than at
           the bottom of the 13-row grid.  Without this you walk under the
           bar, and straight past the gateway standing at the room's foot. */
        if (p->py > 335.0f - 9.0f) p->py = 335.0f - 9.0f;
        g->walkT += dt;
        g->stepsSinceFight++;
        if ((g->stepsSinceFight % 24) == 0) roll_encounter(g, z);
    } else {
        g->walkT = 0;
    }
    p->tx = (int)(p->px / 30.0f);
    p->ty = (int)(p->py / 30.0f);
    if (p->tx < 0) p->tx = 0;
    if (p->ty < 0) p->ty = 0;
    if (p->tx >= MAP_W) p->tx = MAP_W - 1;
    if (p->ty >= MAP_H) p->ty = MAP_H - 1;
}

/* ------------------------------------------------------------------ draw */

/* True when the player is close enough to talk to this one.  The original
   plays the character's `yellow` state on hitTest, so the one you can speak
   to is lit rather than left to guesswork. */
static bool npc_in_reach(const Game *g, const Npc *n)
{
    float dx = (n->tx * 30.0f + 15.0f) - g->p.px;
    float dy = (n->ty * 30.0f + 15.0f) - g->p.py;
    return (dx * dx + dy * dy) < TALK_REACH * TALK_REACH;
}

static void draw_npc(Game *g, Npc *n, float t)
{
    float px = OX + n->tx * TILE + TILE * 0.5f;
    float py = OY + n->ty * TILE + TILE * 0.9f;

    /* The original lights the person you can talk to, so the highlight has to
       cover the whole model rather than ring it.  The puppet draws its own
       outlines and contact shadow, so it cannot simply be re-stamped in a flat
       colour; instead lay a warm aura shaped to the body -- roughly 46px tall
       and 30 wide at this scale -- behind the character, plus a glow at the feet. */
    if (n->kind != NPC_PROP && npc_in_reach(g, n)) {
        float pulse = 0.72f + 0.28f * sinf(t * 4.0f);
        Color warm = (Color){ 255, 226, 140, 255 };
        const float cy = py - TILE * 0.42f;             /* middle of the body */
        const float rx[3] = { 0.44f, 0.36f, 0.27f };    /* in tiles           */
        const float ry[3] = { 0.68f, 0.58f, 0.46f };
        const float al[3] = { 0.20f, 0.26f, 0.34f };
        for (int k = 0; k < 3; k++)
            DrawEllipse((int)px, (int)cy, TILE * rx[k], TILE * ry[k],
                        Fade(warm, al[k] * pulse));
        DrawEllipse((int)px, (int)py, TILE * 0.42f, TILE * 0.17f,
                    Fade(warm, 0.30f * pulse));
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

    /* The way out is lit on the doorway itself, not on the ground under the
       player: the original plays the trigger clip's own highlight. */
    {
        const Exit *e = exit_near(z, p->px, p->py);
        if (e && (int)(e - z->exits) == g->exitLock) e = NULL;   /* not armed yet */
        if (e) {
            float ex = OX + (e->tx * 30.0f + 15.0f) * HUD_S;
            float ey = OY + (e->ty < 0 ? p->py : e->ty * 30.0f + 15.0f) * HUD_S;
            float pulse = 0.6f + 0.4f * sinf(t * 4.2f);
            Color glow = (Color){ 120, 224, 255, 255 };
            /* the trigger cell sits just inside the doorway, so lift the
               marker onto the opening itself */
            if (e->dir == EX_UP)   ey -= TILE * 0.55f;
            if (e->dir == EX_DOWN) ey += TILE * 0.35f;
            Rectangle door = { ex - TILE * 0.62f, ey - TILE * 1.05f,
                               TILE * 1.24f, TILE * 1.45f };
            DrawRectangleRec(door, Fade(glow, 0.10f + 0.08f * pulse));
            DrawRectangleLinesEx(door, 2.0f, Fade(glow, 0.45f + 0.35f * pulse));
            const float ddx[4] = { 0, 0, -1, 1 }, ddy[4] = { -1, 1, 0, 0 };
            float ax = ex + ddx[e->dir] * TILE * 0.78f;
            float ay = ey + ddy[e->dir] * TILE * 0.90f - TILE * 0.30f;
            float s2 = TILE * 0.15f;
            Vector2 tip  = { ax + ddx[e->dir] * s2, ay + ddy[e->dir] * s2 };
            Vector2 side = { ddy[e->dir] * s2, ddx[e->dir] * s2 };
            Vector2 b1 = { ax - ddx[e->dir] * s2 + side.x, ay - ddy[e->dir] * s2 + side.y };
            Vector2 b2 = { ax - ddx[e->dir] * s2 - side.x, ay - ddy[e->dir] * s2 - side.y };
            Color cg = Fade(glow, 0.6f + 0.4f * pulse);
            if (e->dir == EX_UP || e->dir == EX_RIGHT) DrawTriangle(tip, b1, b2, cg);
            else                                      DrawTriangle(tip, b2, b1, cg);
        }
    }



    for (int i = 0; i < z->npcCount; i++)
        if (z->npcs[i].kind != NPC_NONE) draw_npc(g, &z->npcs[i], t);

    /* The player draws at the position it actually holds, which is a float in
       the original's pixel space; there is no tween between cells any more. */
    Vector2 at = { OX + p->px * HUD_S, OY + p->py * HUD_S + TILE * 0.42f };
    Look lk = p->look;
    for (int s = 0; s < SLOT_COUNT; s++) {
        int id = p->equip[s];
        if (id < 0) continue;
        if (ITEMS[id].type == ITEM_WEAPON) { lk.weapon = ITEMS[id].shape; lk.weaponTint = ITEMS[id].tint; }
        if (ITEMS[id].type == ITEM_HELM)   lk.helm = ITEMS[id].shape;
        if (ITEMS[id].type == ITEM_ARMOUR) { lk.cloth = ITEMS[id].tint;
                                             lk.clothDark = art_shade(ITEMS[id].tint, 0.62f); }
    }
    art_draw_walker(&lk, at, p->dir, g->walkT, 0.66f);

    /* dark edges so the 28x18 grid reads as a stage */
    /* Letterbox either side of the room, and mask the strip the bar covers. */
    DrawRectangle(0, 0, OX, SCREEN_H, (Color){ 12, 10, 16, 255 });
    DrawRectangle(OX + MAP_W * TILE, 0, SCREEN_W, SCREEN_H, (Color){ 12, 10, 16, 255 });
    DrawRectangleGradientV(OX, HUD_Y - 70, MAP_W * TILE, 70, (Color){ 12, 10, 16, 0 },
                           (Color){ 12, 10, 16, 220 });

    /* ------------------------------------------------------------ the bar
       The original's walkmenu: one panel across the foot of the stage, with
       the portrait and name at the left, the class / gold / level line above
       four bars, the two potion counts to their right, and Inventory and
       Skills at the far right.  Positions below are its own, in its 600x451
       pixel space, mapped through HX/HY. */
    Combatant c;
    player_recalc(p, &c);
    int life = p->curLife > 0 ? p->curLife : c.lifeMax;

    Rectangle bar = { (float)OX, (float)HUD_Y, (float)(MAP_W * TILE), (float)HUD_H };
    DrawRectangleRec(bar, (Color){ 26, 22, 18, 245 });
    DrawRectangleLinesEx(bar, 2, (Color){ 122, 100, 62, 255 });
    DrawRectangleLinesEx((Rectangle){ bar.x + 5, bar.y + 5, bar.width - 10, bar.height - 10 },
                         1, (Color){ 78, 64, 40, 255 });

    /* portrait, and the name under it */
    Rectangle por = { (float)HX(30.8f), (float)HY(355.5f),
                      59 * HUD_S, 54 * HUD_S };
    DrawRectangleRec(por, (Color){ 40, 34, 28, 255 });
    DrawRectangleLinesEx(por, 2, (Color){ 122, 100, 62, 255 });
    BeginScissorMode((int)por.x + 2, (int)por.y + 2, (int)por.width - 4, (int)por.height - 4);
    art_draw_walker(&p->look, (Vector2){ por.x + por.width * 0.5f,
                                         por.y + por.height * 1.25f }, 1, 0.0f, 1.15f);
    EndScissorMode();
    ui_text(p->name, HX(9.3f), HY(413.7f), 20, C_PARCH);

    /* class, gold and level share the line above the bars */
    ui_text(CLASS_NAMES[p->cls], HX(105.4f), HY(349.7f), 21, C_PARCH);
    char buf[64];
    ui_text("GOLD:", HX(208), HY(352), 18, C_GOLD2);
    snprintf(buf, sizeof buf, "%d", p->gold);
    ui_text(buf, HX(259), HY(352), 18, C_GOLD);
    ui_text("Level", HX(288), HY(352), 18, C_KI2);
    snprintf(buf, sizeof buf, "%d", p->level);
    ui_text(buf, HX(322), HY(352), 18, C_KI);

    /* the four bars, at the original's own rows */
    struct { const char *lab; float y; int cur, max; Color fill, back; } rows[4] = {
        { "LIFE", 377.3f, life,         c.lifeMax, C_BLOOD, { 46, 20, 20, 220 } },
        { "MANA", 392.6f, p->curMana,   c.manaMax, C_KI,    { 22, 34, 48, 220 } },
        { "ENG",  408.3f, p->curEnergy, c.engMax,  C_GOLD,  { 46, 38, 16, 220 } },
        { "EXP",  425.3f, p->exp,       p->expNext, C_JADE,  { 22, 40, 26, 220 } },
    };
    for (int i = 0; i < 4; i++) {
        ui_text(rows[i].lab, HX(104), HY(rows[i].y) - 5, 14, C_PARCH2);
        float frac = rows[i].max > 0 ? (float)rows[i].cur / rows[i].max : 0.0f;
        ui_bar((Rectangle){ (float)HX(144.7f), (float)HY(rows[i].y),
                            108 * HUD_S, 7 * HUD_S },
               frac, rows[i].fill, rows[i].back, NULL);
        snprintf(buf, sizeof buf, "%d/%d", rows[i].cur, rows[i].max);
        ui_text(buf, HX(256), HY(rows[i].y) - 5, 15, C_PARCH2);
    }

    /* potion counts */
    ui_text("Life Potion", HX(367.4f), HY(352.5f), 15, C_PARCH2);
    DrawCircle(HX(374), HY(371), 9, (Color){ 96, 176, 96, 255 });
    DrawCircleLines(HX(374), HY(371), 9, C_INK);
    snprintf(buf, sizeof buf, "X %d", p->lifePots);
    ui_text(buf, HX(390), HY(365), 17, C_PARCH);

    ui_text("Mana Potion", HX(367.4f), HY(392.5f), 15, C_PARCH2);
    DrawCircle(HX(374), HY(411), 9, (Color){ 96, 140, 200, 255 });
    DrawCircleLines(HX(374), HY(411), 9, C_INK);
    snprintf(buf, sizeof buf, "X %d", p->manaPots);
    ui_text(buf, HX(390), HY(405), 17, C_PARCH);

    /* the two buttons at the far right */
    ui_text("Inventory", HX(470), HY(374), 19, C_PARCH);
    ui_text("Skills",    HX(470), HY(399), 19, C_PARCH);
    ui_text("I", HX(452), HY(374), 17, C_GOLD2);
    ui_text("T", HX(452), HY(399), 17, C_GOLD2);
    if (p->statPts > 0 || p->skillPts > 0)
        ui_text("points to spend", HX(452), HY(424), 15, C_JADE);
}
