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
    "I keep the record here.  Speak to me and I will set down where\nyou stand, or give you a bed if you need one.", 0 },
  { 0, NPC_HEALER,    3, 5, "Healer",
    "Sit.  Life or mana, a little or all of it -- I mend what you can\nafford and no more.", 0 },
  { 0, NPC_VILLAGER,  6, 8, "Student",
    "This is the arena gate.  The man in white robes will patch you up\nfor very little; the old man with the stick keeps the records.", 0 },
  /* --- Arena1 --------------------------------------------------------- */
  { 1, NPC_VENDOR,   13, 4, "Food Vendor",
    "Eat before you walk.  Low energy bleeds your life and mana away\nwhile you stand still.", SHOP_MEALS },
  { 1, NPC_VENDOR,    6, 4, "Potion Vendor",
    "Life and mana, bottled.  Fill up before a gateway, not after.", SHOP_POTS },
  { 1, NPC_VILLAGER, 16, 9, "Ninja",
    "The first gateway is left of here.  Turn right instead if you want\nequipment.  And eat -- once your energy runs out, life and mana\nstart draining.", 0 },
  /* --- Arena2 --------------------------------------------------------- */
  { 2, NPC_VILLAGER,  4, 9, "Apprentice",
    "Step into the portal and you fight a human.  Check your potions,\nyour equipment and your energy before each one.", 0 },
  { 2, NPC_VILLAGER, 14, 5, "Guard",
    "Finding this gateway hard?  There is a training room upstairs.", 0 },
  /* --- Arena3 --------------------------------------------------------- */
  { 3, NPC_VENDOR,    6, 4, "Item Vendor",
    "Steel, leather, and a wrist guard if you have the sense.\nStrength first -- you cannot swing what you cannot lift.", SHOP_ITEMS0 },
  { 3, NPC_VILLAGER,  7, 9, "Apprentice",
    "You can buy gear here, and there is more of it upstairs.  Out of\nmoney?  Talk to the busy merchant over by the crates.", 0 },
  { 3, NPC_VENDOR,   14,10, "Item Vendor2",
    "I buy what you have no use for, and I keep white leaves in stock\nfor anyone who knows what to do with them.", SHOP_TRADE },
  /* --- Arena4 --------------------------------------------------------- */
  { 4, NPC_VILLAGER,  3,10, "Drinker",
    "No.. no.. YOU be quiet!", 0 },
  { 4, NPC_VILLAGER,  8,10, "Drunkard",
    "Give me back my bottle!", 0 },
  { 4, NPC_VILLAGER, 15, 5, "Drinker",
    "I love you.  *hic*", 0 },
  { 4, NPC_VILLAGER,  4, 3, "Ninja",
    "These two should not be in here.  Step around them.", 0 },
  /* --- Arena5 --------------------------------------------------------- */
  { 5, NPC_VENDOR,   14, 4, "Vendor2",
    "Relics.  Odd things.  They do more than they look like they do.", SHOP_ITEMS1 },
  { 5, NPC_VILLAGER, 11, 7, "Apprentice",
    "Check your strength before you buy.  Carry more than you can lift\nand the weight works against you.", 0 },
  { 5, NPC_VENDOR,    7, 9, "Vendor",
    "Supplies, same as the last room, worse light.", SHOP_ITEMS2 },
  /* --- Arena6 --------------------------------------------------------- */
  { 6, NPC_VILLAGER,  5, 4, "Scribe",
    "Welcome to the library.  The books and scrolls here hold a great\ndeal -- reading them may be what gets you through the monster\nportal and the dark rift.", 0 },
  { 6, NPC_QUEST,     7, 8, "Lady",
    "I study herbs here.  Bring me white leaves and I will work them\ndown into medicine for you.", 2 },
  { 6, NPC_VILLAGER, 15, 9, "Relaxing Ninja",
    "Most of these books are worth the time.  I have just finished the\none on shadow wolves.", 0 },
  /* --- Arena7, the hall of statues: the three gateways ----------------- */
  { 7, NPC_QUEST,    10, 5, "Statue",
    "A worn dish is set into the plinth, waiting for an offering.", 1 },
  { 7, NPC_PROP,      5, 5, "Statue2",
    "Carved mid-step, as though it meant to leave.", 0 },
  { 7, NPC_PROP,     14, 5, "Statue2",
    "The same face as the other, worn smoother.", 0 },
  /* --- Arena8 --------------------------------------------------------- */
  { 8, NPC_TRAINER2,  3, 2, "Posted Note",
    "Training Room -- step inside and choose your training.  Strike the\ntarget for experience.", 0 },
  { 8, NPC_VENDOR,   10, 4, "Vendor3",
    "Everything here is overpriced.  You will buy it anyway.", SHOP_ITEMS3 },
  { 8, NPC_VILLAGER, 11, 9, "Dark Ninja",
    "The training room can hurt you.  Stop the session if your life or\nenergy runs low -- you keep the experience either way.", 0 },
  { 8, NPC_VILLAGER, 17, 5, "Guard",
    "The monsters in the blue portal are strong.  Be around level 7\nbefore you go in.", 0 },
  /* --- Arena9 --------------------------------------------------------- */
  { 9, NPC_VILLAGER,  4, 6, "Dark Ninja",
    "That doorway leads to a chamber only the greatest warriors are\nallowed to enter.", 0 },
  { 9, NPC_QUEST,    8, 7, "Meditating Ninja",
    "I found a strange ring washed up on the beach.  I have no use for\nit -- take it.", 0 },
  /* --- Arena10 -------------------------------------------------------- */
  {10, NPC_VILLAGER, 13, 5, "Guard",
    "This is the temple's last portal.  Only five levels, and every one\nof them is brutal.  Learn what you are facing first.", 0 },
  {10, NPC_VENDOR,    3, 7, "Vendor4",
    "Last of the stock, and the last room that sells any.", SHOP_ITEMS4 },
  {10, NPC_VENDOR,   17, 7, "Shadow",
    "Stand and be counted, one after another.", SHOP_ITEMS5 },
  {10, NPC_VILLAGER,  5, 4, "Wounded Warrior",
    "I went into the dark rift and the blood spirit finished me.  Heavy\nmagic, no physical attack at all, and a shield I could not break.", 0 },
};

/* The room graph, recovered from the original's edge trigger clips.  Each
   clip fires NewStage(dir, x, y) on space and jumps to its own stagelabel.
   ty < 0 marks an edge exit that spans its whole column; entryY < 0 keeps
   the row you left on, which is what the original's Math.ceil does. */
static const Exit ROOM_EXITS[] = {
  /* dir, trigger cell, dest, entry, gatePortal, gateNeed, portal, doorX, doorY
     The doorX/doorY are the original's own doorway-clip placements, read off
     its room frames; the side exits have no clip and leave them at zero. */
  /* room 0 */ { EX_UP,    10, 4, ZONE_ARENA1,  10,  9, -1, 0, -1, 300.2f, 122.5f },
  /* room 1 */ { EX_DOWN,  10,10, ZONE_ARENA0,  10,  5, -1, 0, -1, 300.2f, 309.1f },
               { EX_LEFT,   0,-1, ZONE_ARENA2,  18, -1, -1, 0, -1, 0, 0 },
               { EX_RIGHT, 19,-1, ZONE_ARENA3,   1, -1, -1, 0, -1, 0, 0 },
  /* room 2 */ { EX_RIGHT, 19,-1, ZONE_ARENA1,   1, -1, -1, 0, -1, 0, 0 },
  /* room 3 */ { EX_LEFT,   0,-1, ZONE_ARENA1,  18, -1, -1, 0, -1, 0, 0 },
               { EX_UP,    15, 4, ZONE_ARENA4,  13,  9, -1, 0, -1, 449.8f, 126.2f },
  /* room 4 */ { EX_DOWN,  13,10, ZONE_ARENA3,  13,  5, -1, 0, -1, 418.4f, 307.6f },
               { EX_LEFT,   0,-1, ZONE_ARENA5,  18, -1, -1, 0, -1, 0, 0 },
  /* room 5 */ { EX_LEFT,   0,-1, ZONE_ARENA6,  18, -1, -1, 0, -1, 0, 0 },
               { EX_RIGHT, 19,-1, ZONE_ARENA4,   1, -1, -1, 0, -1, 0, 0 },
               { EX_UP,    10, 4, ZONE_ARENA7,  10,  9, -1, 0, -1, 300.2f, 123.5f },
  /* room 6 */ { EX_RIGHT, 19,-1, ZONE_ARENA5,   1, -1, -1, 0, -1, 0, 0 },
  /* room 7 */ { EX_DOWN,  10,10, ZONE_ARENA5,  10,  5, -1, 0, -1, 300.2f, 305.1f },
               { EX_RIGHT, 19,-1, ZONE_ARENA8,   1, -1, -1, 0, -1, 0, 0 },
               { EX_LEFT,   0,-1, ZONE_ARENA9,  18, -1, -1, 0, -1, 0, 0 },
  /* room 8 */ { EX_LEFT,   0,-1, ZONE_ARENA7,  18, -1, -1, 0, -1, 0, 0 },
  /* room 9 */ { EX_RIGHT, 19,-1, ZONE_ARENA7,   1, -1, -1, 0, -1, 0, 0 },
               { EX_UP,    10, 4, ZONE_ARENA10, 10,  9,  0,20, -1, 300.1f, 124.2f },
  /* room 10*/ { EX_DOWN,  10,10, ZONE_ARENA9,  10,  5, -1, 0, -1, 300.2f, 305.1f },
  /* The three gateways are doorway clips of the same kind, one per portal.
     Their rooms are fixed by the door that says "You have completed all the
     levels in this portal": Arena2 for the human road, Arena8 for the blue
     monster portal, Arena10 for the last one. */
  /* room 2 */ { EX_UP,     9, 5, ZONE_ARENA2,   9,  6, -1, 0,  0, 299.3f, 165.6f },
  /* room 8 */ { EX_UP,    15, 5, ZONE_ARENA8,  15,  6, -1, 0,  1, 466.3f, 157.6f },
  /* room 10*/ { EX_UP,    10, 5, ZONE_ARENA10, 10,  6, -1, 0,  2, 309.4f, 156.6f },
  /* The entrance room's south door is not a gateway at all: it is the way
     out of the arena, and it only opens once the human gateway's twenty
     levels are behind you.  Walking through it ends the game. */
  /* room 0 */ { EX_DOWN,  10,10, ZONE_ARENA0,  10,  9,  0,20, -2, 300.3f, 309.1f },
};
static const int EXIT_ROOM[] = { 0, 1,1,1, 2, 3,3, 4,4, 5,5,5, 6, 7,7,7, 8, 9,9, 10,
                                 2, 8, 10, 0 };

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
        z->encounterRate = 0;   /* rooms are safe; fights come from gateways */
        z->minLevel = 1 + DEPTH[i] * 2;
        z->maxLevel = z->minLevel + 2;
        z->enemyPoolCount = 5;
        for (int k = 0; k < 5; k++) z->enemyPool[k] = POOLS[i][k];

        /* Every room in the temple stands on one floor: the original places
           the same backdrop clip at depth 2 for all eleven and only swaps the
           clip, never the palette.  These are its three stone fills. */
        z->ground     = (Color){ 140, 120, 87, 255 };
        z->groundDark = (Color){ 123, 102, 68, 255 };
        z->groundEdge = (Color){  95,  79, 54, 255 };
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
        pr->col = ps->col;
        pr->colDark = ps->colDark;
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

/* Disarm whatever doorway the player is standing in, until they step out of
   it or walk on through.  This runs on arriving in a room, and again whenever
   a panel or a battle hands control back -- otherwise closing the gateway
   panel leaves you standing in the gateway with it still armed, and the next
   press drops you straight back in. */
void world_lock_exit_underfoot(Game *g)
{
    const Zone *dz = &g->zones[g->p.zone];
    const Exit *on = exit_near(dz, g->p.px, g->p.py);
    g->exitLock = -1;
    if (on) {
        g->exitLock = (int)(on - dz->exits);
        g->exitLockX = g->p.px;
        g->exitLockY = g->p.py;
    }
}

void world_enter_zone(Game *g, ZoneId z, int tx, int ty)
{
    g->p.zone = z;
    g->p.tx = tx; g->p.ty = ty;
    g->p.px = tx * 30.0f + 15.0f;    /* cell centre, in the original's px  */
    g->p.py = ty * 30.0f + 15.0f;
    g->fromX = tx; g->fromY = ty;
    g->moving = false; g->moveT = 0;
    g->stepsSinceFight = 0;
    world_lock_exit_underfoot(g);
}

/* The original has no wandering encounters: its movechar() is movement and
   collision only, its rooms mark cells walkable or occupied and nothing else,
   and the one "meet" flag it carries is cleared by every room and never set.
   Every fight comes from a gateway.  So walking a room is safe. */

static void interact(Game *g, Npc *n)
{
    Player *p = &g->p;
    switch (n->kind) {
    case NPC_GATE: {
        Zone *dst = &g->zones[n->arg];
        bool deeper = (int)n->arg > (int)p->zone;
        g->pendZone = (ZoneId)n->arg;
        g->pendX = deeper ? dst->entryX : dst->exitX;
        g->pendY = deeper ? dst->entryY : dst->exitY;
        g->pendMove = true;
        go_scene(g, n->arg == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
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
    case NPC_SAVE:
        /* The Elder's inventorytype is "Save": the original raises a panel
           with your character sheet and two separate buttons -- saving the
           game, which is free, and resting, which spends one of your rests.
           It does neither the moment you walk up. */
        g->saveSel = 0;
        g->savedFlash = 0.0f;
        go_panel(g, SCENE_SAVE);
        break;
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

/* Where a doorway actually stands, in the original's pixels.  Side exits
   have no clip of their own, so they fall back to the trigger cell. */
static Vector2 door_at(const Exit *e, float py)
{
    if (e->doorX > 0)
        return (Vector2){ e->doorX, e->doorY };
    return (Vector2){ e->tx * 30.0f + 15.0f, e->ty < 0 ? py : e->ty * 30.0f + 15.0f };
}

static const Exit *exit_near(const Zone *z, float px, float py)
{
    for (int i = 0; i < z->exitCount; i++) {
        const Exit *e = &z->exits[i];
        Vector2 d = door_at(e, py);
        if (e->ty < 0) {                       /* an edge strip */
            if (fabsf(px - d.x) <= 15.0f + 9.0f) return e;
            continue;
        }
        /* The doorway clip is 22.8 x 38.4 around its placement.  A portal is
           tested against the clip alone: it is a one-way trip into a gateway,
           so you have to be standing in the doorway, not merely brushing past
           the bottom of the room. */
        float body = e->portal >= 0 ? 6.0f : 9.0f;
        float mx = 11.4f + body, my = 19.2f + body;
        if (fabsf(px - d.x) <= mx && fabsf(py - d.y) <= my) return e;
    }
    return NULL;
}

/* Take an exit: NewStage(dir, x, y).  A horizontal exit keeps the row you
   walked in on, as the original's Math.ceil does. */
static void take_exit(Game *g, const Exit *e)
{
    Player *p = &g->p;
    if (e->gatePortal >= 0 && p->portalLevel[e->gatePortal] <= e->gateNeed) {
        ui_toast(g, e->portal == -2
                 ? "The gate holds. Clear all twenty human levels first."
                 : "Sealed. Finish the gateway below and come back.");
        return;
    }
    if (e->portal == -2) {
        /* The arena's own front gate.  In the original it clears the stage
           and jumps the root to 'theend'. */
        go_panel(g, SCENE_CREDITS);
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
    /* Hold the move until the wipe covers it -- doing it now would show the
       next room first and fade it in afterwards.  The original never names
       the room you walk into either, so there is no toast. */
    g->pendZone = e->dest; g->pendX = nx; g->pendY = ny; g->pendMove = true;
    go_scene(g, e->dest == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
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
        /* The original tints a figure you can talk to with a blue glow, the
           same cyan its doorways use (12,193,254).  It has to read as light
           coming off the whole model, so it is a stack of soft bands that
           follow the figure's silhouette -- head, torso and legs -- rather
           than a ring around the waist. */
        float pulse = 0.72f + 0.28f * sinf(t * 4.0f);
        Color blue = (Color){ 12, 193, 254, 255 };
        const float head = py - TILE * 0.80f;
        const float chest = py - TILE * 0.50f;
        const float shin  = py - TILE * 0.16f;
        for (int k = 3; k >= 0; k--) {
            float sp = 1.0f + k * 0.22f;                /* outer bands wider  */
            float a  = (0.46f - k * 0.09f) * pulse;
            DrawEllipse((int)px, (int)head,  TILE * 0.17f * sp, TILE * 0.19f * sp,
                        Fade(blue, a));
            DrawEllipse((int)px, (int)chest, TILE * 0.23f * sp, TILE * 0.26f * sp,
                        Fade(blue, a));
            DrawEllipse((int)px, (int)shin,  TILE * 0.19f * sp, TILE * 0.22f * sp,
                        Fade(blue, a));
        }
        DrawEllipse((int)px, (int)py, TILE * 0.40f, TILE * 0.15f,
                    Fade(blue, 0.34f * pulse));
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

    /* The way out of a room is the original's trigger clip (22.8 x 38.4)
       standing on the floor.  For an ordinary door that clip is the threshold
       under the archway the room's own scenery already draws -- drawing a
       second framed opening there reads as two exits -- so it is a plate on
       the ground that lights when you are standing in it.  A gateway has no
       arch above it and is drawn as the lit doorway it is. */
    {
        const Exit *arm = exit_near(z, p->px, p->py);
        if (arm && (int)(arm - z->exits) == g->exitLock) arm = NULL;
        for (int i = 0; i < z->exitCount; i++) {
            const Exit *e = &z->exits[i];
            bool lit = (e == arm);
            bool portal = (e->portal >= 0);
            bool sealed = (e->gatePortal >= 0 &&
                           p->portalLevel[e->gatePortal] <= e->gateNeed);
            float pulse = 0.6f + 0.4f * sinf(t * 4.2f);

            if (e->ty < 0) {
                /* a side exit: the room's edge, marked only when you reach it */
                if (!lit) continue;
                float ex = OX + (e->tx * 30.0f + 15.0f) * HUD_S;
                Color c = (Color){ 120, 224, 255, 255 };
                Rectangle bar = { ex - TILE * 0.26f, OY + TILE * 0.6f,
                                  TILE * 0.52f, 335.0f * HUD_S - TILE * 0.9f };
                DrawRectangleRec(bar, Fade(c, 0.10f + 0.10f * pulse));
                DrawRectangleLinesEx(bar, 2.0f, Fade(c, 0.40f + 0.35f * pulse));
                continue;
            }

            Vector2 d = door_at(e, p->py);
            float ex = OX + d.x * HUD_S, ey = OY + d.y * HUD_S;
            float hw = 11.4f * HUD_S, hh = 19.2f * HUD_S;
            Rectangle frame = { ex - hw, ey - hh, hw * 2, hh * 2 };

            if (portal) {
                Color jamb  = sealed ? (Color){ 78, 72, 66, 255 } : (Color){ 92, 62, 120, 255 };
                Color mouth = sealed ? (Color){ 20, 19, 18, 255 } : (Color){ 26, 12, 40, 255 };
                DrawRectangleRec(frame, mouth);
                DrawRectangleLinesEx(frame, 3.0f, jamb);
                DrawRectangleRec((Rectangle){ frame.x - 4, frame.y - 6,
                                              frame.width + 8, 8 }, art_shade(jamb, 1.15f));
                if (!sealed) {
                    for (int k = 0; k < 3; k++) {
                        float ph = t * 1.6f + k * 0.7f;
                        float yy = frame.y + frame.height *
                                   (0.15f + 0.7f * (0.5f + 0.5f * sinf(ph)));
                        DrawRectangle((int)(frame.x + 4), (int)yy,
                                      (int)(frame.width - 8), 2,
                                      Fade((Color){ 186, 140, 255, 255 }, 0.35f));
                    }
                    DrawRectangleLinesEx(frame, 1.5f,
                        Fade((Color){ 186, 140, 255, 255 }, 0.4f + 0.3f * pulse));
                } else {
                    ui_text_c("sealed", ex, frame.y + frame.height + 2, 15,
                              (Color){ 150, 140, 130, 255 });
                }
            } else {
                /* a threshold plate, flat on the floor under the archway */
                Rectangle plate = { frame.x, ey - hh * 0.30f, frame.width, hh * 0.60f };
                Color c = lit ? (Color){ 120, 224, 255, 255 } : (Color){ 118, 106, 84, 255 };
                float a2 = lit ? 0.22f + 0.20f * pulse : 0.13f;
                DrawRectangleRec(plate, Fade(c, a2));
                DrawRectangleLinesEx(plate, lit ? 2.0f : 1.0f,
                                     Fade(c, lit ? 0.55f + 0.35f * pulse : 0.28f));
            }

            if (lit) {
                Color glow = portal ? (Color){ 200, 150, 255, 255 }
                                    : (Color){ 120, 224, 255, 255 };
                ui_text_c(portal ? "ENTER  gateway" : "ENTER", ex,
                          frame.y - TILE * 0.62f, 16, Fade(glow, 0.7f + 0.3f * pulse));
            }
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
    gfx_scissor((Rectangle){ por.x + 2, por.y + 2, por.width - 4, por.height - 4 });
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
