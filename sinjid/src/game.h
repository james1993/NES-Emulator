/* ===========================================================================
   Sinjid: Shadow of the Warrior -- unofficial raylib/C fan remake
   ---------------------------------------------------------------------------
   All art in this project is generated procedurally at runtime from the code
   in art.c; no assets from the original Flash game are used or redistributed.
   The design (speed-ordered 2v2 turn combat, a separate shield-point layer,
   split physical/magic damage, energy+mana skill costs, tile overworld with
   random encounters) is modelled on the 2004 original.
   =========================================================================== */
#ifndef SINJID_GAME_H
#define SINJID_GAME_H

#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>

#define SCREEN_W 1280
#define SCREEN_H 720

#define MAX_HEROES     2
#define MAX_FOES       2
#define MAX_INVENTORY 28    /* the original's itemstats grid */
#define INV_FIRST_FREE 4    /* purchases land in slots 4..11        */
#define MAX_SKILLS    19
#define MAX_LOG        6
#define MAX_FLOATERS  24
#define MAX_PARTICLES 256
/* The original applies defence percentages raw -- its Shadow Reaper carries
   85% -- so the cap only exists to stop total immunity. */
#define DEF_CAP        95
/* The original's overworld grid: 20 cells wide, addressed cell{x}_{y}. */
#define MAP_W         20
#define MAP_H         13
#define MAX_NPCS      24
#define MAX_PROPS     64

/* ---------------------------------------------------------------- palette */
/* A muted 2004-Flash palette: ink outlines, desaturated earth and steel, one
   hot accent for blood/fire and one cold accent for ki/magic.  Each colour has
   a P_ form (brace initialiser, usable in file-scope tables) and a C_ form
   (compound literal, usable in expressions).                                 */
#define P_INK      {  18,  16,  22, 255 }
#define P_INK2     {  34,  31,  40, 255 }
#define P_PARCH    { 206, 192, 164, 255 }
#define P_PARCH2   { 150, 138, 116, 255 }
#define P_GOLD     { 198, 160,  74, 255 }
#define P_GOLD2    { 122,  98,  46, 255 }
#define P_BLOOD    { 158,  44,  44, 255 }
#define P_BLOOD2   { 206,  74,  62, 255 }
#define P_KI       {  86, 150, 186, 255 }
#define P_KI2      { 138, 205, 232, 255 }
#define P_JADE     {  92, 146, 100, 255 }
#define P_STEEL    { 142, 148, 158, 255 }
#define P_STEEL2   {  86,  92, 104, 255 }
#define P_VIOLET   { 122,  92, 158, 255 }
#define P_BARK     { 104,  76,  52, 255 }

#define C_INK    CLITERAL(Color)P_INK
#define C_INK2   CLITERAL(Color)P_INK2
#define C_PARCH  CLITERAL(Color)P_PARCH
#define C_PARCH2 CLITERAL(Color)P_PARCH2
#define C_GOLD   CLITERAL(Color)P_GOLD
#define C_GOLD2  CLITERAL(Color)P_GOLD2
#define C_BLOOD  CLITERAL(Color)P_BLOOD
#define C_BLOOD2 CLITERAL(Color)P_BLOOD2
#define C_KI     CLITERAL(Color)P_KI
#define C_KI2    CLITERAL(Color)P_KI2
#define C_JADE   CLITERAL(Color)P_JADE
#define C_STEEL  CLITERAL(Color)P_STEEL
#define C_STEEL2 CLITERAL(Color)P_STEEL2
#define C_VIOLET CLITERAL(Color)P_VIOLET
#define C_BARK   CLITERAL(Color)P_BARK

/* ------------------------------------------------------------------ enums */
typedef enum {
    SCENE_TITLE, SCENE_CREATE, SCENE_VILLAGE, SCENE_WORLD, SCENE_BATTLE,
    SCENE_MENU, SCENE_SHOP, SCENE_TRAIN, SCENE_ARENA, SCENE_DIALOG,
    SCENE_GAMEOVER, SCENE_CREDITS, SCENE_PORTAL, SCENE_TRAINING, SCENE_HEAL
} Scene;

/* The original runs three portals of stages -- HUMAN, MONSTER and DARK -- each
   a ladder of its own encounter definitions, with progress kept per portal. */
typedef struct { int enemyA, enemyB; } PortalStage;
typedef struct { const char *name; int first, count; } Portal;
extern const PortalStage PORTAL_STAGES[];
extern const Portal PORTALS[3];
int  portal_stage_count(int portal);

/* The original's own four disciplines. */
typedef enum { CLASS_BALANCED, CLASS_WARRIOR, CLASS_SPELLCASTER, CLASS_SHADOW,
               CLASS_COUNT } ClassId;

/* Per-level growth, from the original's class table:
   [speed, toughness, magic damage, physical damage]. */
typedef struct { int speed, tough, magDmg, phyDmg; } ClassGrowth;
extern const ClassGrowth CLASS_GROWTH[CLASS_COUNT];

typedef enum {
    ANIM_STAND, ANIM_WALK, ANIM_ATTACK, ANIM_CAST, ANIM_BLOCK, ANIM_BLOCKBREAK,
    ANIM_HIT, ANIM_HEAL, ANIM_MISS, ANIM_DIE, ANIM_DEAD, ANIM_VICTORY
} AnimState;

typedef enum { DMG_PHYSICAL, DMG_MAGIC, DMG_PURE } DamageType;

/* One id per room.  The original's temple is eleven rooms on its root
   timeline, labelled Arena0 (also just "Arena", the entrance) through
   Arena10, reached with gotoAndStop on a per-exit `stagelabel`.  There is no
   separate village screen: the entrance room *is* the hub. */
typedef enum {
    ZONE_ARENA0, ZONE_ARENA1, ZONE_ARENA2, ZONE_ARENA3, ZONE_ARENA4,
    ZONE_ARENA5, ZONE_ARENA6, ZONE_ARENA7, ZONE_ARENA8, ZONE_ARENA9,
    ZONE_ARENA10,
    ZONE_COUNT
} ZoneId;
#define ZONE_VILLAGE ZONE_ARENA0    /* the entrance room is the hub */

/* Backdrop styles, kept separate from the map so a stage can pick any of them. */
typedef enum {
    BG_VILLAGE, BG_ARENA, BG_ARENA2, BG_ARENA3, BG_DARK, BG_COUNT
} BgStyle;

typedef enum {
    ITEM_NONE, ITEM_WEAPON, ITEM_SHIELD, ITEM_ARMOUR, ITEM_HELM,
    ITEM_RELIC, ITEM_CONSUMABLE
} ItemType;

typedef enum { SLOT_WEAPON, SLOT_SHIELD, SLOT_ARMOUR, SLOT_HELM, SLOT_RELIC, SLOT_COUNT } SlotId;

/* Silhouette families the puppet renderer knows how to draw. */
typedef enum {
    WEAP_NONE, WEAP_KNIFE, WEAP_KATANA, WEAP_BROAD, WEAP_AXE, WEAP_STAFF,
    WEAP_SCYTHE, WEAP_CLAW, WEAP_SPEAR, WEAP_COUNT
} WeaponShape;

typedef enum {
    SHLD_NONE, SHLD_WRIST, SHLD_BUCKLER, SHLD_KITE, SHLD_TOWER, SHLD_BLADE,
    SHLD_SPIKE, SHLD_COUNT
} ShieldShape;

typedef enum {
    HELM_NONE, HELM_BANDANA, HELM_HOOD, HELM_CIRCLET, HELM_HORNED, HELM_FULL,
    HELM_COUNT
} HelmShape;

typedef enum {
    BODY_HUMAN, BODY_BRUTE, BODY_UNDEAD, BODY_BEAST, BODY_WISP, BODY_COUNT
} BodyShape;

/* ------------------------------------------------------------------ items */
typedef struct {
    const char *name;
    ItemType    type;
    int         price, tier, strNeed;   /* strNeed: Strength the item demands */
    int lifeMax, manaMax;
    int phyDmg, phyDef;                 /* defences are percentages           */
    int magDmg, magDef;
    int shdPts, shdPhyDef, shdMagDef, shdDmg;
    int speed;
    int shape;              /* WeaponShape / ShieldShape / HelmShape        */
    Color tint;
    const char *note;
} ItemDef;

typedef struct { int def; int count; } ItemStack;

/* ----------------------------------------------------------------- skills */
typedef enum {
    SK_TARGET_ONE_FOE, SK_TARGET_ALL_FOES, SK_TARGET_SELF, SK_TARGET_ALLY
} SkillTarget;

typedef enum {
    SKF_NONE       = 0,
    SKF_HEAL       = 1 << 0,
    SKF_RESTORE_MP = 1 << 1,
    SKF_SHIELD_UP  = 1 << 2,
    SKF_IGNORE_SHD = 1 << 3,   /* bypasses the shield-point layer entirely  */
    SKF_MULTI      = 1 << 5,   /* strikes twice                             */
    SKF_BUFF_ATK   = 1 << 6,
    SKF_BUFF_DEF   = 1 << 7,
    SKF_DRAIN      = 1 << 8,
    SKF_STUN       = 1 << 9,
    SKF_NEVER_MISS = 1 << 10,
    /* Recovered from the original's own skill scripts. */
    SKF_PASSIVE      = 1 << 11,  /* never chosen in battle; folds into stats */
    SKF_PURE_MAGIC   = 1 << 12,  /* magic only: no strength, no shield damage */
    SKF_MISSING_LIFE = 1 << 13,  /* adds (rank+1) x the life you are missing  */
    SKF_TARGET_LIFE  = 1 << 14   /* adds a percentage of the target's life    */
} SkillFlags;

typedef struct {
    const char *name;
    const char *desc;
    int         tree;          /* 0 = combat, 1 = magic                      */
    int         maxRank;
    int         reqLevel;
    int         prereq[2];     /* skills that must be learned first, -1 = none */
    int         manaCost, engCost;
    /* The original scales a skill as a percentage of the relevant stat:
       (stat / 100) * (pctBase + pctPerRank * rank).  Passives instead add
       flatPerRank to a stat for every rank held. */
    int         pctBase, pctPerRank;
    /* Some skills add a flat amount on top of the stat instead of scaling it:
       the original's magic bolts are magdmg + flatBase + flatPerRank * rank. */
    int         flatBase, flatPerRank;
    /* Shield damage is its own channel in the original: an attack passes
       dmgtoshd, normally the fighter's raw shield-damage stat, but a
       shield-breaking skill scales it by (shdPctBase + shdPctPerRank * rank).
       Pure-magic skills pass zero. */
    int         shdPctBase, shdPctPerRank;
    DamageType  dmgType;
    SkillTarget target;
    unsigned    flags;
    AnimState   anim;
    Color       fx;
} SkillDef;

/* ------------------------------------------------------------- combatants */
typedef struct {
    Color skin, cloth, clothDark, trim, hair, metal;
    int   body;    /* BodyShape  */
    int   weapon;  /* WeaponShape */
    int   shield;  /* ShieldShape */
    int   helm;    /* HelmShape   */
    Color weaponTint, shieldTint;
    float scale;
    bool  glow;
} Look;

typedef struct {
    char  name[28];
    bool  isHero, alive;
    int   level;
    int   life, lifeMax, mana, manaMax, eng, engMax;
    /* Damage is three components (weapon, strength, magic); each defence is a
       PERCENTAGE reduction of the matching component, as in the original. */
    int   str, phyDmg, strDmg, magDmg;
    int   phyDef, magDef;            /* percent, 0..DEF_CAP                  */
    int   shd, shdMax, shdPhyDef, shdMagDef, shdDmg;
    int   speed, atkSpd;
    int   engRate;
    Look  look;
    /* volatile battle state */
    AnimState anim;
    float     animT;
    bool      guarding;
    int       stunned;
    float     atkBuff, defBuff;
    int       buffTurns;
    Vector2   pos, homePos;
    float     flash, shake;
    /* AI */
    int   aiSkill[4], aiSkillCount;
    int   expReward, goldReward, dropChance, dropTier;
} Combatant;

/* ---------------------------------------------------------------- enemies
   The original defines each encounter's fighters with absolute stats rather
   than a level curve, so this table does the same.  Defences are percentages. */
typedef struct {
    const char *name;
    int life;
    int phyDmg, magDmg, shdDmg;
    int phyDef, magDef;              /* percent */
    int shdPts, shdPhyDef, shdMagDef;
    int str, speed;
    int exp, gold;
    int dropChance, dropTier;
    Look look;
    int aiSkill[4], aiSkillCount;
} EnemyDef;

/* ------------------------------------------------------------------ world */
typedef enum {
    T_FLOOR, T_GRASS, T_PATH, T_WALL, T_WATER, T_TREE, T_ROCK, T_SAND,
    T_SNOW, T_LAVA, T_MAT, T_PORTAL, T_DOOR, T_VOID,
    T_OCCUPIED   /* blocked because scenery stands there; drawn as floor */
} TileId;

typedef enum {
    NPC_NONE, NPC_ELDER, NPC_SMITH, NPC_VENDOR, NPC_HEALER, NPC_TRAINER,
    NPC_ARENA, NPC_SAVE, NPC_VILLAGER, NPC_GATE, NPC_PROP, NPC_PORTAL, NPC_PICKUP, NPC_TRAINER2
} NpcKind;

typedef struct {
    NpcKind kind;
    int     tx, ty;
    const char *name;
    const char *line;
    int     arg;          /* zone for gates, shop table for vendors        */
    Look    look;
    float   bob;
} Npc;

/* An edge exit.  In the original each room carries trigger clips at its
   edges; standing on one and pressing space calls NewStage(dir, x, y) and
   jumps to that exit's own destination room.  Horizontal exits keep your
   row (the original passes Math.ceil of your position), so entryY < 0 here
   means "keep the row you walked in on". */
typedef enum { EX_UP, EX_DOWN, EX_LEFT, EX_RIGHT } ExitDir;

/* Scenery.  The original furnishes each room with 15-45 placed objects; their
   positions and footprints are reproduced from its display list, and art.c
   draws each kind procedurally.  See src/scenery_table.h. */
typedef enum {
    PROP_GENERIC, PROP_DOORWAY, PROP_TORCH, PROP_PILLAR, PROP_PLANT,
    PROP_BAMBOO, PROP_CRATE, PROP_COUNTER, PROP_SHELF, PROP_LAMP,
    PROP_WINDOW, PROP_URN, PROP_ROCKS, PROP_GLOW, PROP_SHADOW,
    PROP_EXITSIGN, PROP_EXITARROW, PROP_KIND_COUNT
} PropKind;

typedef struct { int kind; float x, y, w, h; } Prop;

typedef struct {
    int    dir;              /* ExitDir                                    */
    int    tx, ty;           /* trigger cell; edge exits span their column */
    ZoneId dest;
    int    entryX, entryY;   /* entryY < 0 => keep the current row         */
    int    gatePortal;       /* -1, else the portal that must be cleared   */
    int    gateNeed;
} Exit;

typedef struct {
    ZoneId id;
    const char *name;
    unsigned char tiles[MAP_H][MAP_W];
    Npc  npcs[MAX_NPCS];
    int  npcCount;
    int  encounterRate;      /* 0 = safe                                    */
    int  minLevel, maxLevel;
    int  enemyPool[8], enemyPoolCount;
    int  entryX, entryY, exitX, exitY;   /* where travel drops you */
    Exit exits[4];
    int  exitCount;
    Prop props[MAX_PROPS];
    int  propCount;
    Color skyTop, skyBot, ground, groundDark, fog, propA, propB;
    int  bgStyle;
} Zone;

/* ------------------------------------------------------------- player run */
typedef struct {
    char      name[24];
    ClassId   cls;
    int       level, exp, expNext, gold;
    int       statPts, skillPts;
    /* Trainable stats, mirroring the original's own stat list. */
    int       baseLife, baseMana, baseEng, baseStr, baseSpeed;
    int       basePhyDmg, baseMagDmg, basePhyDef, baseMagDef, baseShdPts;
    int       skillRank[MAX_SKILLS];
    ItemStack inv[MAX_INVENTORY];
    int       invCount;
    int       equip[SLOT_COUNT];   /* index into item table, -1 = empty     */
    Look      look;
    ZoneId    zone;
    int       tx, ty, dir;
    int       curLife, curMana, curEnergy;  /* carried between fights     */
    ZoneId    saveZone; int saveX, saveY;
    int       arenaWave;
    /* Portal progress, limited rests and potion counts, as the original keeps
       them (portallevel, rests, lifepots, manapots). */
    int       portalLevel[3];
    int       rests;
    int       lifePots, manaPots;
    bool      picked[16];          /* hidden pickups already taken          */
    bool      zoneCleared[ZONE_COUNT];
    int       playtime;
} Player;

/* ---------------------------------------------------------------- battle */
typedef struct {
    char  text[24];
    Vector2 pos;
    float t, life;
    Color col;
    float size;
} Floater;

typedef struct {
    Vector2 pos, vel;
    float   life, maxLife, size;
    Color   col;
    int     kind;
} Particle;

typedef enum {
    BP_INTRO, BP_TURN_START, BP_PLAYER_CHOOSE, BP_PLAYER_TARGET, BP_ACTING,
    BP_RESOLVE, BP_TURN_END, BP_WIN, BP_LOSE, BP_FLED
} BattlePhase;

typedef struct {
    Combatant heroes[MAX_HEROES], foes[MAX_FOES];
    int       nHeroes, nFoes;
    BattlePhase phase;
    float     timer;
    int       order[MAX_HEROES + MAX_FOES];  /* +side encoded: 0..1 hero, 2..3 foe */
    int       orderCount, orderIdx, turn;
    int       actor, target;
    int       menuIdx, menuTab, skillIdx, targetIdx;
    int       pendingSkill;
    char      log[MAX_LOG][96];
    int       logCount;
    Floater   floats[MAX_FLOATERS];
    Particle  parts[MAX_PARTICLES];
    float     shake, flashScreen;
    Color     flashCol;
    bool      isArena, canFlee, isBoss, isPortal;
    int       expGain, goldGain, dropItem;
    int       bgStyle;
} Battle;

/* -------------------------------------------------------------- game glue */
typedef struct {
    Scene   scene, prevScene;
    Player  p;
    Battle  b;
    Zone    zones[ZONE_COUNT];
    float   time;
    float   fade;         /* 1 = black                                     */
    int     fadeDir;
    Scene   fadeTarget;
    Font    font, fontBig;
    bool    fontLoaded;
    /* menus */
    int     menuTab, menuIdx, menuScroll;
    int     shopIdx, shopMode, shopVendor;
    int     dialogNpc;
    int     healSel;      /* the Heal panel's cursor: 0 = accept, 1 = leave */
    char    toast[96];
    float   toastT;
    int     createIdx, createField;
    char    nameBuf[24];
    int     nameLen;
    float   walkT;
    float   moveT;              /* 0..1 tween between tiles                */
    int     fromX, fromY;
    bool    moving;
    int     stepsSinceFight;
    int     interactNpc;
    int     titleIdx;
    bool    hasSave;
    float   camShakeT;
    int     credits;
    int     portalIdx, portalStage;
    float   trainT;
    int     trainGain;
} Game;

/* ------------------------------------------------------------------- data */
extern const ItemDef  ITEMS[];
extern const int      ITEM_COUNT;
extern const SkillDef SKILLS[MAX_SKILLS];
extern const EnemyDef ENEMIES[];
extern const int      ENEMY_COUNT;
extern const char    *CLASS_NAMES[CLASS_COUNT];
extern const char    *CLASS_BLURB[CLASS_COUNT];

void  data_init_zones(Zone *zones);
Look  data_class_look(ClassId c);
void  data_class_base(Player *p, ClassId c);
int   data_shop_table(int vendor, const int **out);

/* ------------------------------------------------------------------- art */
void  art_draw_puppet(const Combatant *c, Vector2 at, float facing, float t, float scale);
void  art_draw_walker(const Look *lk, Vector2 at, int dir, float walkT, float scale);
void  art_draw_zone_bg(const Zone *z, float t, float camX);
void  art_draw_battle_bg(int bgStyle, float t);
void  art_draw_scenery(const Prop *pr, const Zone *z, float t);
void  art_draw_tile(int tile, int px, int py, int size, const Zone *z, int wx, int wy);
void  art_draw_prop(int kind, Vector2 at, float scale, Color a, Color b);
void  art_weapon_shape(int shape, Vector2 grip, float ang, float scale, Color tint, Color edge);
void  art_shield_shape(int shape, Vector2 grip, float ang, float scale, Color tint);
Color art_shade(Color c, float f);

/* -------------------------------------------------------------------- ui */
void  ui_panel(Rectangle r, const char *title);
void  ui_bar(Rectangle r, float frac, Color fill, Color back, const char *label);
void  ui_text(const char *s, float x, float y, float size, Color col);
void  ui_text_c(const char *s, float cx, float y, float size, Color col);
float ui_text_w(const char *s, float size);
bool  ui_button(Rectangle r, const char *label, bool selected, bool enabled);
void  ui_tooltip_item(const ItemDef *it, Rectangle at);
void  ui_scene_menu(Game *g);
void  ui_scene_shop(Game *g);
void  ui_scene_train(Game *g);
void  ui_scene_title(Game *g);
void  ui_scene_create(Game *g);
void  ui_scene_dialog(Game *g);
void  ui_scene_heal(Game *g);
void  ui_scene_portal(Game *g);
void  ui_scene_training(Game *g);
void  ui_toast(Game *g, const char *fmt, ...);

/* ---------------------------------------------------------------- player */
void  player_recalc(Player *p, Combatant *out);
int   player_add_item(Player *p, int def);
int   player_pack_free(const Player *p);
void  player_equip(Player *p, int invIndex);
bool  player_can_equip(const Player *p, int def);
bool  skill_prereqs_met(const Player *p, int id);
const char *skill_lock_reason(const Player *p, int id);

/* Indices into the original's item table, for starting gear. */
#define IT_IRON_KNIFE     0
#define IT_ENERGY_KNIFE   1
#define IT_SILVER_KNIFE   2
#define IT_LEATHER_WRIST  25
#define IT_BANDANA        42
#define IT_LEATHER_ARMOUR 52
#define IT_RICE_BALL      63
#define IT_GREEN_TEA      66
#define IT_MENDOS_RING    61
void  player_gain_exp(Game *g, int exp);
int   player_exp_for_level(int lvl);
bool  save_write(const Game *g);
bool  save_read(Game *g);
bool  save_exists(void);
bool  use_consumable(Game *g, int invIdx, Combatant *on);
void  go_scene(Game *g, Scene s);

/* ---------------------------------------------------------------- battle */
void  battle_start(Game *g, const int *enemyDefs, int count, int level, bool arena, bool boss);
void  battle_update(Game *g, float dt);
void  battle_draw(Game *g);
void  battle_log(Battle *b, const char *fmt, ...);
void  battle_floater(Battle *b, Vector2 at, Color c, float size, const char *fmt, ...);
void  battle_burst(Battle *b, Vector2 at, Color c, int n, float spd, int kind);

/* ----------------------------------------------------------------- world */
void  world_update(Game *g, float dt);
void  world_draw(Game *g);
void  world_enter_zone(Game *g, ZoneId z, int tx, int ty);

/* ----------------------------------------------------------------- sound
   The original's playSound vocabulary, synthesised rather than sampled. */
typedef enum {
    SFX_SWORD1, SFX_SWORD2, SFX_BLOCK, SFX_BREAK, SFX_COINS, SFX_ITEM,
    SFX_LEVEL, SFX_SKILL, SFX_NO, SFX_THUNDER, SFX_HOWL, SFX_STEP,
    SFX_HEAL, SFX_SPELL, SFX_DIE, SFX_COUNT
} SfxId;

void  sound_init(void);
void  sound_play(int id);
void  sound_close(void);

/* ------------------------------------------------------------------ util */
int   rnd(int lo, int hi);
float frnd(float lo, float hi);
float approach(float v, float target, float rate);
Color lerp_col(Color a, Color b, float t);

extern Game G;

/* ---------------------------------------------------------- input shim --
   Scripted input for screenshots and CI (see --script in main.c).  With no
   script running these forward straight to raylib, so normal play is
   untouched.  The macros below intentionally shadow the raylib calls in all
   game code; raylib.h is already included above.                            */
bool sj_key_pressed(int key);
bool sj_key_down(int key);
int  sj_char_pressed(void);

#define IsKeyPressed(k)  sj_key_pressed(k)
#define IsKeyDown(k)     sj_key_down(k)
#define GetCharPressed() sj_char_pressed()

#endif /* SINJID_GAME_H */
