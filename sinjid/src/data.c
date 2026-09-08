/* Content tables: items, skills, enemies, classes and zones.
   Every name, stat line and map in this file is original to this remake. */
#include "game.h"
#include <string.h>

const char *CLASS_NAMES[CLASS_COUNT] = { "Balanced", "Warrior", "Spell Caster", "Shadow Ninja" };

/* Recovered from the original's class table: each entry is
   [speed, toughness, magic damage, physical damage] gained per level. */
const ClassGrowth CLASS_GROWTH[CLASS_COUNT] = {
    { 2, 2, 10, 10 },   /* Balanced     */
    { 1, 3,  5, 15 },   /* Warrior      */
    { 2, 1, 15, 10 },   /* Spell Caster */
    { 3, 1, 10, 10 },   /* Shadow Ninja */
};
const char *CLASS_BLURB[CLASS_COUNT] = {
    "Even growth in every direction. Gains ground steadily and is\nnever badly wrong-footed by a fight.",
    "Toughest of the four, and hits hardest with steel. Gains the\nmost life per level and the most physical damage.",
    "Trades life for ki. The fastest-growing magic damage in the\ngame, and armour barely slows a spell down.",
    "Fastest of the four. Speed decides who strikes first and who\ngets missed, so the ninja does both.",
};


/* ------------------------------------------------------------------ items */
/* name, type, price, tier | life mana str pdmg pdef mdmg mdef |
   shdPts shdPDef shdMDef shdDmg | speed avoid | shape, tint, note           */
/* ------------------------------------------------------------------ items
   Names and every stat below are the original's own table, recovered from its
   AddItem() calls.  Prices, the three specials' effects (Medicine, White
   Leaves, Mendo's Ring -- zero-stat rows in the source table), the trailing
   food/drink rows, and all the one-line notes are ours.
   Columns: name, type, price, tier, strNeed | life, mana | pDmg, pDef% |
            mDmg, mDef% | shdPts, shdPdef%, shdMdef%, shdDmg | speed | shape  */
const ItemDef ITEMS[] = {
/* 0*/ { "Iron Knife", ITEM_WEAPON, 70, 1, 10, 0,0, 5,0, 0,0, 0,0,0,0, 0, WEAP_KNIFE, P_STEEL,
        "Cheap steel, honestly made." },
/* 1*/ { "Energy Knife", ITEM_WEAPON, 70, 1, 10, 0,0, 0,0, 5,0, 0,0,0,0, 0, WEAP_KNIFE, P_KI,
        "Channels more ki than steel." },
/* 2*/ { "Silver Knife", ITEM_WEAPON, 30, 1, 10, 0,0, 10,0, 0,0, 0,0,0,0, 0, WEAP_KNIFE, P_STEEL,
        "Cheap steel, honestly made." },
/* 3*/ { "Long Sword", ITEM_WEAPON, 190, 2, 15, 0,0, 12,0, 0,0, 0,0,0,2, 0, WEAP_BROAD, P_STEEL,
        "A working blade." },
/* 4*/ { "Spiked Axe", ITEM_WEAPON, 60, 2, 16, 0,0, 11,0, 0,0, 0,0,0,5, 0, WEAP_AXE, P_STEEL2,
        "A working blade." },
/* 5*/ { "Heavy Blade", ITEM_WEAPON, 90, 2, 16, 0,0, 16,0, 0,0, 0,0,0,0, 0, WEAP_BROAD, P_STEEL,
        "A working blade." },
/* 6*/ { "Katana", ITEM_WEAPON, 150, 3, 18, 0,0, 24,0, 0,0, 0,0,0,7, 0, WEAP_KATANA, P_STEEL,
        "A working blade." },
/* 7*/ { "Shield Breaker", ITEM_WEAPON, 200, 4, 22, 0,0, 17,0, 0,0, 0,0,0,40, 0, WEAP_BROAD, P_STEEL,
        "Made for taking guards apart." },
/* 8*/ { "Blaze Edge", ITEM_WEAPON, 330, 4, 23, 0,0, 20,0, 13,0, 0,0,0,5, 0, WEAP_KATANA, P_BLOOD2,
        "A working blade." },
/* 9*/ { "Fusion Edge", ITEM_WEAPON, 700, 4, 25, 0,20, 24,0, 18,0, 0,0,0,5, 0, WEAP_KATANA, P_KI,
        "Warm in the hand, and it keeps you full." },
/*10*/ { "Spike Sword", ITEM_WEAPON, 780, 4, 25, 0,0, 38,0, 12,0, 0,0,0,7, 0, WEAP_BROAD, P_STEEL2,
        "A working blade." },
/*11*/ { "Demon Sword", ITEM_WEAPON, 810, 3, 20, 0,0, 19,0, 32,0, 0,0,0,10, 0, WEAP_BROAD, P_BLOOD,
        "Channels more ki than steel." },
/*12*/ { "Blood Razor", ITEM_WEAPON, 920, 5, 26, 0,0, 58,0, 0,0, 0,0,0,10, 0, WEAP_KATANA, P_BLOOD,
        "Heavy to lift, worse to be hit by." },
/*13*/ { "Raider Sword", ITEM_WEAPON, 140, 2, 13, 0,0, 8,0, 0,0, 0,0,0,3, 0, WEAP_BROAD, P_STEEL,
        "Cheap steel, honestly made." },
/*14*/ { "Guard Blade", ITEM_WEAPON, 1640, 5, 40, 0,-40, 90,15, 0,0, 0,0,0,10, 0, WEAP_BROAD, P_STEEL,
        "Heavy to lift, worse to be hit by." },
/*15*/ { "Golden Blade", ITEM_WEAPON, 1100, 5, 28, 0,0, 46,0, 24,0, 0,0,0,12, 0, WEAP_BROAD, P_GOLD,
        "A working blade." },
/*16*/ { "Double Razor", ITEM_WEAPON, 360, 3, 18, 0,0, 13,0, 8,0, 0,0,0,7, 0, WEAP_KATANA, P_STEEL,
        "A working blade." },
/*17*/ { "Shadow Katana", ITEM_WEAPON, 1000, 4, 24, 0,0, 33,0, 32,0, 0,0,0,10, 15, WEAP_KATANA, P_VIOLET,
        "A working blade." },
/*18*/ { "Shadow Spirit", ITEM_WEAPON, 1000, 3, 19, 0,80, 21,0, 44,0, 0,0,0,5, 0, WEAP_BROAD, P_VIOLET,
        "Channels more ki than steel." },
/*19*/ { "Fallen Blade", ITEM_WEAPON, 1490, 5, 40, 0,0, 75,15, 10,0, 0,0,0,0, 0, WEAP_BROAD, P_VIOLET,
        "Heavy to lift, worse to be hit by." },
/*20*/ { "Wooden Rod", ITEM_WEAPON, 120, 2, 15, 0,30, 5,0, 16,0, 0,0,0,0, 0, WEAP_STAFF, P_BARK,
        "Channels more ki than steel." },
/*21*/ { "Golden Rod", ITEM_WEAPON, 400, 2, 15, 0,60, 7,0, 34,0, 0,0,0,0, 0, WEAP_STAFF, P_GOLD,
        "Channels more ki than steel." },
/*22*/ { "Wooden Staff", ITEM_WEAPON, 900, 2, 15, 0,100, 10,0, 30,0, 0,0,0,0, 0, WEAP_STAFF, P_BARK,
        "Channels more ki than steel." },
/*23*/ { "Ion Bo", ITEM_WEAPON, 900, 3, 20, 0,120, 10,0, 60,0, 0,0,0,0, 0, WEAP_STAFF, P_KI,
        "Channels more ki than steel." },
/*24*/ { "Rust Blade", ITEM_WEAPON, 120, 2, 15, 0,0, 18,0, 0,0, 0,0,0,0, 0, WEAP_BROAD, P_GOLD2,
        "A working blade." },
/*25*/ { "Leather Wrist", ITEM_SHIELD, 15, 1, 5, 0,0, 0,0, 0,0, 25,10,0,0, 0, SHLD_WRIST, P_BARK,
        "Barely more than a wrapped arm." },
/*26*/ { "Metal Wrist", ITEM_SHIELD, 30, 1, 5, 0,0, 0,0, 0,0, 40,20,0,0, 0, SHLD_WRIST, P_STEEL,
        "Barely more than a wrapped arm." },
/*27*/ { "Wooden Guard", ITEM_SHIELD, 40, 2, 15, 0,0, 0,0, 0,0, 50,15,0,0, 0, SHLD_BUCKLER, P_BARK,
        "Honest cover." },
/*28*/ { "Hand Blade", ITEM_SHIELD, 60, 3, 17, 0,0, 0,0, 0,0, 60,50,0,0, 0, SHLD_BLADE, P_STEEL,
        "Honest cover." },
/*29*/ { "Metal Guard", ITEM_SHIELD, 150, 3, 18, 0,0, 0,0, 0,0, 80,35,0,0, 0, SHLD_BUCKLER, P_STEEL,
        "Honest cover." },
/*30*/ { "Double Blade", ITEM_SHIELD, 200, 3, 19, 0,0, 0,0, 0,0, 75,80,0,0, 0, SHLD_BLADE, P_STEEL,
        "Honest cover." },
/*31*/ { "Wooden Shield", ITEM_SHIELD, 510, 2, 15, 0,0, 0,0, 0,0, 80,20,20,0, 0, SHLD_KITE, P_BARK,
        "Honest cover." },
/*32*/ { "Metal Shield", ITEM_SHIELD, 1020, 3, 21, 0,0, 0,0, 0,0, 160,40,40,0, 0, SHLD_KITE, P_STEEL,
        "Honest cover." },
/*33*/ { "Demon Shield", ITEM_SHIELD, 1150, 3, 21, 0,20, 0,0, 0,0, 200,30,40,0, 0, SHLD_TOWER, P_BLOOD,
        "Honest cover." },
/*34*/ { "Spike Shield", ITEM_SHIELD, 1380, 3, 21, 0,0, 20,0, 0,0, 180,40,40,0, 0, SHLD_SPIKE, P_STEEL2,
        "Parries, and bites on the way back." },
/*35*/ { "Claw Blade", ITEM_SHIELD, 400, 4, 23, 0,0, 0,0, 0,0, 100,85,0,0, 0, SHLD_BLADE, P_STEEL,
        "Honest cover." },
/*36*/ { "Shadow Wrist", ITEM_SHIELD, 400, 2, 16, 0,0, 0,0, 0,0, 100,40,60,0, 5, SHLD_WRIST, P_VIOLET,
        "Turns spellwork better than steel." },
/*37*/ { "Energy Wrist", ITEM_SHIELD, 560, 3, 18, 0,30, 0,0, 0,0, 120,20,80,0, 0, SHLD_WRIST, P_KI,
        "Turns spellwork better than steel." },
/*38*/ { "The Guardian", ITEM_SHIELD, 980, 4, 25, 0,0, 0,0, 0,0, 300,50,50,0, 0, SHLD_BUCKLER, P_GOLD,
        "A wall you happen to carry." },
/*39*/ { "Blade Shield", ITEM_SHIELD, 600, 4, 24, 0,0, 0,0, 0,0, 170,70,30,0, 0, SHLD_BLADE, P_STEEL,
        "Honest cover." },
/*40*/ { "Fallen Shield", ITEM_SHIELD, 1440, 4, 24, 0,0, 0,0, 0,0, 250,70,30,0, 0, SHLD_TOWER, P_VIOLET,
        "A wall you happen to carry." },
/*41*/ { "Voodoo Shield", ITEM_SHIELD, 800, 3, 18, 0,40, 0,0, 0,0, 120,0,92,0, 0, SHLD_KITE, P_JADE,
        "Turns spellwork better than steel." },
/*42*/ { "Bandana", ITEM_HELM, 10, 1, 1, 0,0, 0,7, 0,5, 0,0,0,0, 0, HELM_BANDANA, P_STEEL,
        "Keeps sweat out of your eyes." },
/*43*/ { "Metal Band", ITEM_HELM, 15, 1, 10, 0,0, 0,15, 0,3, 0,0,0,0, 0, HELM_CIRCLET, P_STEEL,
        "Dented, and still worth wearing." },
/*44*/ { "Focus Band", ITEM_HELM, 40, 1, 10, 0,30, 0,5, 0,12, 0,0,0,0, 0, HELM_HOOD, P_KI2,
        "Narrows the world to one point." },
/*45*/ { "Fire Band", ITEM_HELM, 310, 2, 15, 0,0, 0,6, 7,6, 0,0,0,0, 0, HELM_CIRCLET, P_BLOOD2,
        "Keeps sweat out of your eyes." },
/*46*/ { "Power Band", ITEM_HELM, 100, 3, 17, 0,0, 13,12, 0,3, 0,0,0,0, 0, HELM_CIRCLET, P_STEEL,
        "Keeps sweat out of your eyes." },
/*47*/ { "Golden Band", ITEM_HELM, 620, 3, 18, 0,0, 0,20, 0,15, 0,0,0,0, 0, HELM_CIRCLET, P_GOLD,
        "Dented, and still worth wearing." },
/*48*/ { "Demon Skin", ITEM_HELM, 220, 4, 22, 0,0, 8,15, 12,10, 0,0,0,0, 0, HELM_HOOD, P_BLOOD,
        "Dented, and still worth wearing." },
/*49*/ { "Shadow Band", ITEM_HELM, 400, 3, 19, 0,20, 0,10, 0,10, 0,0,0,0, 5, HELM_CIRCLET, P_VIOLET,
        "Keeps sweat out of your eyes." },
/*50*/ { "Snake Band", ITEM_HELM, 60, 2, 15, 0,10, 0,0, 0,0, 0,0,0,0, 6, HELM_CIRCLET, P_JADE,
        "Keeps sweat out of your eyes." },
/*51*/ { "Ki Band", ITEM_HELM, 210, 2, 15, 0,50, 0,15, 15,5, 0,0,0,0, 0, HELM_CIRCLET, P_KI,
        "Narrows the world to one point." },
/*52*/ { "Leather Armour", ITEM_ARMOUR, 100, 2, 13, 0,0, 0,20, 0,10, 0,0,0,0, 0, 0, P_BARK,
        "Layered cloth and patience." },
/*53*/ { "Metal Plates", ITEM_ARMOUR, 150, 3, 19, 0,0, 0,30, 0,15, 0,0,0,0, 0, 0, P_STEEL,
        "Layered cloth and patience." },
/*54*/ { "Metal Armour", ITEM_ARMOUR, 350, 3, 21, 0,0, 0,40, 0,20, 0,0,0,0, 0, 0, P_STEEL,
        "Layered cloth and patience." },
/*55*/ { "Golden Armour", ITEM_ARMOUR, 1600, 3, 20, 0,0, 0,60, 0,60, 0,0,0,0, 0, 0, P_GOLD,
        "Plated, and heavy with it." },
/*56*/ { "Demon Rags", ITEM_ARMOUR, 800, 5, 28, 0,0, 15,50, 0,15, 0,0,0,0, 0, 0, P_BLOOD,
        "Plated, and heavy with it." },
/*57*/ { "Focus Armour", ITEM_ARMOUR, 800, 3, 17, 0,100, 0,15, 10,30, 0,0,0,0, 0, 0, P_KI2,
        "Woven against the shadow, not the sword." },
/*58*/ { "Snake Skin", ITEM_ARMOUR, 170, 2, 15, 0,15, 0,10, 6,30, 0,0,0,0, 0, 0, P_JADE,
        "Woven against the shadow, not the sword." },
/*59*/ { "Shadow Armour", ITEM_ARMOUR, 1500, 4, 22, 0,0, 0,35, 0,50, 0,0,0,0, 13, 0, P_VIOLET,
        "Woven against the shadow, not the sword." },
/*60*/ { "Medicine", ITEM_CONSUMABLE, 150, 3, 0, 600,0, 0,0, 0,0, 0,0,0,0, 0, 0, P_STEEL,
        "Swallow it fast." },
/*61*/ { "Mendo's Ring", ITEM_RELIC, 900, 3, 0, 50,50, 0,5, 0,5, 20,5,5,5, 5, 0, P_STEEL,
        "Carried, never worn." },
/*62*/ { "White Leaves", ITEM_CONSUMABLE, 320, 3, 0, 300,150, 0,0, 0,0, 0,0,0,0, 0, 0, P_STEEL,
        "Swallow it fast." },
/*63*/ { "Rice Ball", ITEM_CONSUMABLE, 20, 1, 0, 120,0, 0,0, 0,0, 0,0,0,0, 0, 0, P_PARCH,
        "Restores 120 life." },
/*64*/ { "Broth Bowl", ITEM_CONSUMABLE, 60, 2, 0, 400,0, 0,0, 0,0, 0,0,0,0, 0, 0, P_GOLD2,
        "Restores 400 life." },
/*65*/ { "Field Dressing", ITEM_CONSUMABLE, 150, 3, 0, 1200,0, 0,0, 0,0, 0,0,0,0, 0, 0, P_BLOOD2,
        "Restores 1200 life." },
/*66*/ { "Green Tea", ITEM_CONSUMABLE, 30, 1, 0, 0,60, 0,0, 0,0, 0,0,0,0, 0, 0, P_JADE,
        "Restores 60 mana." },
/*67*/ { "Ki Draught", ITEM_CONSUMABLE, 110, 2, 0, 0,200, 0,0, 0,0, 0,0,0,0, 0, 0, P_KI,
        "Restores 200 mana." },
};
const int ITEM_COUNT = (int)(sizeof(ITEMS) / sizeof(ITEMS[0]));

/* ----------------------------------------------------------------- skills */
/* name, desc | tree, maxRank, reqLevel, mana, eng | power, perRank |
   dmgType, target, flags, anim, fx                                          */
/* ----------------------------------------------------------------- skills
   Structure and scaling are the original's, recovered from its skill scripts
   and skill-tree buttons: nineteen skills, max rank 10, gated by level and by
   prerequisite skills.  A skill scales as (stat/100) * (pctBase + pctPerRank *
   rank); passives instead add flatPerRank to a stat for every rank held.
   The names and the descriptions are ours.
   name, desc | tree, maxRank, reqLevel, prereq | mana, energy |
   pctBase, pctPerRank, flatPerRank | dmgType, target, flags, anim, fx        */
const SkillDef SKILLS[MAX_SKILLS] = {
 { "Iron Arm", "Conditioning. Every rank adds flat damage to every blow you land.",
   0, 10, 0, { -1, -1 }, 0, 0,   0, 0, 4,
   DMG_PHYSICAL, SK_TARGET_SELF, SKF_PASSIVE, ANIM_STAND, P_STEEL },
 { "Shield Breaker", "A heavy overhead meant for the guard, not the man. Never misses.",
   0, 10, 0, { -1, -1 }, 0, 12,  62, 8, 0,
   DMG_PHYSICAL, SK_TARGET_ONE_FOE, SKF_SHIELD_DMG | SKF_NEVER_MISS, ANIM_ATTACK, P_GOLD },
 { "Full Strike", "Everything at once -- arm, edge and ki behind a single cut.",
   0, 10, 0, { -1, -1 }, 0, 10,  52, 8, 0,
   DMG_PHYSICAL, SK_TARGET_ONE_FOE, SKF_NONE, ANIM_ATTACK, P_STEEL },
 { "Quick Cut", "Fast enough that the guard never comes up. Cannot miss.",
   0, 10, 0, { -1, -1 }, 0, 8,   45, 5, 0,
   DMG_PHYSICAL, SK_TARGET_ONE_FOE, SKF_NEVER_MISS, ANIM_ATTACK, P_STEEL },
 { "Ember Bolt", "A thrown coal of ki. Pure magic -- it ignores shields entirely.",
   1, 10, 0, { 1, -1 }, 14, 0,   40, 9, 0,
   DMG_MAGIC, SK_TARGET_ONE_FOE, SKF_PURE_MAGIC, ANIM_CAST, P_BLOOD2 },
 { "Ki Spark", "The cheap spell. Small, quick, and it never runs your mana dry.",
   1, 10, 0, { 1, -1 }, 5, 0,    7, 5, 0,
   DMG_MAGIC, SK_TARGET_ONE_FOE, SKF_PURE_MAGIC, ANIM_CAST, P_KI },
 { "Hard Training", "Rank on rank of drill. Adds to the strength behind your arm.",
   0, 10, 1, { 2, -1 }, 0, 0,    0, 0, 2,
   DMG_PHYSICAL, SK_TARGET_SELF, SKF_PASSIVE, ANIM_STAND, P_STEEL },
 { "Iron Stance", "Brace. Halves what reaches you this round and mends the guard.",
   0, 10, 1, { 3, -1 }, 0, 10,   0, 0, 0,
   DMG_PHYSICAL, SK_TARGET_SELF, SKF_SHIELD_UP | SKF_BUFF_DEF, ANIM_BLOCK, P_STEEL },
 { "Ki Focus", "Breath discipline. Every rank deepens the damage your spells do.",
   1, 10, 1, { 4, -1 }, 0, 0,    0, 0, 7,
   DMG_MAGIC, SK_TARGET_SELF, SKF_PASSIVE, ANIM_STAND, P_KI },
 { "Mend", "Close your own wounds with ki instead of thread.",
   1, 10, 1, { 4, -1 }, 16, 0,   0, 0, 0,
   DMG_MAGIC, SK_TARGET_SELF, SKF_HEAL, ANIM_HEAL, P_JADE },
 { "Blade Mastery", "The edge finds the seam by itself now. Flat damage, every rank.",
   0, 10, 5, { 6, -1 }, 0, 0,    0, 0, 5,
   DMG_PHYSICAL, SK_TARGET_SELF, SKF_PASSIVE, ANIM_STAND, P_STEEL },
 { "Frost Nail", "A spike of cold driven past the guard. Pure magic.",
   1, 10, 5, { 8, -1 }, 22, 0,   70, 7, 0,
   DMG_MAGIC, SK_TARGET_ONE_FOE, SKF_PURE_MAGIC | SKF_IGNORE_SHD, ANIM_CAST, P_KI2 },
 { "Ward", "Wrap yourself in ki. Magic barely touches you for three rounds.",
   1, 10, 5, { 9, -1 }, 18, 0,   0, 0, 0,
   DMG_MAGIC, SK_TARGET_SELF, SKF_BUFF_DEF, ANIM_HEAL, P_VIOLET },
 { "Last Breath", "The nearer death you are, the harder it lands.",
   0, 10, 10, { 10, -1 }, 0, 26, 60, 4, 0,
   DMG_PHYSICAL, SK_TARGET_ONE_FOE, SKF_MISSING_LIFE, ANIM_ATTACK, P_BLOOD },
 { "Execution", "Cuts away a share of whatever life is still standing in front of you.",
   0, 10, 10, { 10, -1 }, 0, 30, 10, 2, 0,
   DMG_PHYSICAL, SK_TARGET_ONE_FOE, SKF_TARGET_LIFE, ANIM_ATTACK, P_BLOOD2 },
 { "Chain Lightning", "Splits across the whole line and never misses one of them.",
   1, 10, 10, { 7, 11 }, 38, 0,  55, 6, 0,
   DMG_MAGIC, SK_TARGET_ALL_FOES, SKF_PURE_MAGIC | SKF_NEVER_MISS, ANIM_CAST, P_KI2 },
 { "Drain Soul", "Pull the life across the gap and keep half of what comes.",
   1, 10, 10, { 5, 12 }, 34, 0,  60, 6, 0,
   DMG_MAGIC, SK_TARGET_ONE_FOE, SKF_DRAIN | SKF_IGNORE_SHD, ANIM_CAST, P_VIOLET },
 { "Killing Breath", "Everything you have, in one downward cut.",
   0, 10, 15, { 13, 14 }, 0, 44, 60, 5, 0,
   DMG_PHYSICAL, SK_TARGET_ONE_FOE, SKF_SHIELD_DMG | SKF_NEVER_MISS, ANIM_ATTACK, P_BLOOD2 },
 { "Shadow Rend", "Tears the shape out from under them. Nothing blunts it.",
   1, 10, 15, { 15, -1 }, 50, 8, 90, 8, 0,
   DMG_PURE, SK_TARGET_ONE_FOE, SKF_IGNORE_SHD | SKF_NEVER_MISS, ANIM_CAST, P_VIOLET },
};


/* ---------------------------------------------------------------- enemies */
/* Look = { skin, cloth, clothDark, trim, hair, metal, body, weapon, shield,
            helm, weaponTint, shieldTint, scale, glow }                      */
/* Rows marked "exact" carry the original's own numbers, recovered from its
   per-encounter setup scripts; the rest are ours, scaled to sit between them.
   name, life | pDmg,mDmg,shdDmg | pDef%,mDef% | shdPts,shdPdef%,shdMdef% |
   str, speed | exp, gold | drop%, dropTier | look | ai skills               */
/* ---------------------------------------------------------------- enemies
   Every stat line below is the original's own, recovered from its per-encounter
   setup scripts (enemy1life/phydmg/phydef/...), including each fighter's weapon
   and shield.  The palettes, body shapes and AI skill picks are ours.
   name, life | pDmg,mDmg,shdDmg | pDef%,mDef% | shdPts,shdPdef%,shdMdef% |
   str, speed | exp, gold | drop%, dropTier | look | ai                      */
const EnemyDef ENEMIES[] = {
 { "Bandit", 40, 7,0,5, 15,10, 60,20,0, 0,15, 30,40, 28,1,
   { {178,142,110,255},{92,74,60,255},{62,50,40,255},{160,70,54,255},{38,30,26,255},P_STEEL,
     BODY_HUMAN, WEAP_KNIFE, SHLD_NONE, HELM_BANDANA, P_STEEL, P_STEEL, 1.02f, false }, {2,2},2 },
 { "Thief", 60, 5,0,5, 15,10, 20,20,0, 0,15, 30,30, 28,1,
   { {186,150,116,255},{106,84,66,255},{72,56,44,255},{150,60,50,255},{40,32,28,255},P_STEEL,
     BODY_HUMAN, WEAP_KNIFE, SHLD_NONE, HELM_BANDANA, P_STEEL, P_STEEL, 1.0f, false }, {2,2},2 },
 { "Training Ward", 80, 5,5,100, 0,50, 300,50,0, 0,15, 0,0, 28,1,
   { {150,120,80,255},{120,96,64,255},{88,68,44,255},{170,150,110,255},{90,70,50,255},P_STEEL,
     BODY_HUMAN, WEAP_NONE, SHLD_NONE, HELM_NONE, P_STEEL, P_STEEL, 0.95f, false }, {2,3,5},3 },
 { "Bandit", 100, 4,0,5, 15,10, 35,20,0, 0,15, 60,100, 28,1,
   { {178,142,110,255},{92,74,60,255},{62,50,40,255},{160,70,54,255},{38,30,26,255},P_STEEL,
     BODY_HUMAN, WEAP_KNIFE, SHLD_NONE, HELM_BANDANA, P_STEEL, P_STEEL, 1.02f, false }, {2,2},2 },
 { "Assailant", 100, 14,0,10, 30,5, 40,15,15, 0,50, 60,90, 28,1,
   { {172,140,112,255},{54,56,70,255},{34,36,46,255},{150,52,48,255},{30,28,26,255},P_INK2,
     BODY_HUMAN, WEAP_KATANA, SHLD_NONE, HELM_HOOD, P_INK2, P_INK2, 1.0f, false }, {2,2},2 },
 { "Assailant", 110, 13,0,15, 25,10, 80,25,10, 0,50, 150,260, 28,1,
   { {172,140,112,255},{54,56,70,255},{34,36,46,255},{150,52,48,255},{30,28,26,255},P_INK2,
     BODY_HUMAN, WEAP_KATANA, SHLD_NONE, HELM_HOOD, P_INK2, P_INK2, 1.0f, false }, {2,2},2 },
 { "Raider", 120, 9,0,5, 15,10, 15,20,0, 0,15, 40,50, 28,1,
   { {182,146,112,255},{104,80,52,255},{70,54,36,255},{170,120,60,255},{44,34,28,255},P_STEEL,
     BODY_HUMAN, WEAP_BROAD, SHLD_NONE, HELM_BANDANA, P_STEEL, P_STEEL, 1.03f, false }, {2,2},2 },
 { "Thief", 140, 7,0,5, 15,10, 35,20,0, 0,15, 60,100, 28,1,
   { {186,150,116,255},{106,84,66,255},{72,56,44,255},{150,60,50,255},{40,32,28,255},P_STEEL,
     BODY_HUMAN, WEAP_KNIFE, SHLD_NONE, HELM_BANDANA, P_STEEL, P_STEEL, 1.0f, false }, {2,2},2 },
 { "Warrior", 180, 8,0,10, 15,10, 200,20,0, 0,15, 40,60, 36,2,
   { {190,156,120,255},{86,86,96,255},{56,56,64,255},{160,140,90,255},{36,30,28,255},P_STEEL,
     BODY_HUMAN, WEAP_BROAD, SHLD_NONE, HELM_FULL, P_STEEL, P_STEEL, 1.05f, false }, {2,2},2 },
 { "Assailant", 180, 23,0,20, 40,15, 0,40,15, 0,60, 120,120, 36,2,
   { {172,140,112,255},{54,56,70,255},{34,36,46,255},{150,52,48,255},{30,28,26,255},P_INK2,
     BODY_HUMAN, WEAP_KATANA, SHLD_NONE, HELM_HOOD, P_INK2, P_INK2, 1.0f, false }, {2,3,5},3 },
 { "Training Ward", 180, 8,8,100, 0,50, 500,50,0, 0,30, 0,0, 36,2,
   { {150,120,80,255},{120,96,64,255},{88,68,44,255},{170,150,110,255},{90,70,50,255},P_STEEL,
     BODY_HUMAN, WEAP_NONE, SHLD_NONE, HELM_NONE, P_STEEL, P_STEEL, 0.95f, false }, {2,3,5},3 },
 { "Raider", 200, 5,0,10, 15,10, 70,20,0, 0,15, 80,130, 36,2,
   { {182,146,112,255},{104,80,52,255},{70,54,36,255},{170,120,60,255},{44,34,28,255},P_STEEL,
     BODY_HUMAN, WEAP_BROAD, SHLD_NONE, HELM_BANDANA, P_STEEL, P_STEEL, 1.03f, false }, {2,2},2 },
 { "Seer", 200, 2,8,10, 10,30, 170,15,60, 0,17, 40,70, 36,2,
   { {198,180,160,255},{74,66,104,255},{48,42,70,255},{150,130,190,255},{60,50,44,255},P_VIOLET,
     BODY_HUMAN, WEAP_STAFF, SHLD_NONE, HELM_HOOD, P_VIOLET, P_VIOLET, 1.0f, false }, {2,13,11,15},4 },
 { "Warrior", 230, 3,0,5, 15,10, 140,20,0, 0,15, 80,130, 36,2,
   { {190,156,120,255},{86,86,96,255},{56,56,64,255},{160,140,90,255},{36,30,28,255},P_STEEL,
     BODY_HUMAN, WEAP_BROAD, SHLD_NONE, HELM_FULL, P_STEEL, P_STEEL, 1.05f, false }, {2,2},2 },
 { "Seer", 260, 3,7,10, 10,40, 100,0,30, 0,17, 100,220, 36,2,
   { {198,180,160,255},{74,66,104,255},{48,42,70,255},{150,130,190,255},{60,50,44,255},P_VIOLET,
     BODY_HUMAN, WEAP_STAFF, SHLD_NONE, HELM_HOOD, P_VIOLET, P_VIOLET, 1.0f, false }, {2,13,11,15},4 },
 { "Warrior", 280, 6,0,10, 30,10, 320,30,0, 0,14, 100,220, 36,2,
   { {190,156,120,255},{86,86,96,255},{56,56,64,255},{160,140,90,255},{36,30,28,255},P_STEEL,
     BODY_HUMAN, WEAP_BROAD, SHLD_NONE, HELM_FULL, P_STEEL, P_STEEL, 1.05f, false }, {2,2},2 },
 { "Training Ward", 300, 12,12,100, 0,50, 700,50,0, 0,40, 0,0, 36,2,
   { {150,120,80,255},{120,96,64,255},{88,68,44,255},{170,150,110,255},{90,70,50,255},P_STEEL,
     BODY_HUMAN, WEAP_NONE, SHLD_NONE, HELM_NONE, P_STEEL, P_STEEL, 0.95f, false }, {2,3,5},3 },
 { "Skeleton", 400, 14,10,10, 10,10, 0,10,10, 0,25, 300,450, 36,2,
   { {206,200,182,255},{96,96,90,255},{60,60,56,255},{150,148,136,255},{30,30,30,255},P_STEEL2,
     BODY_UNDEAD, WEAP_AXE, SHLD_NONE, HELM_NONE, P_STEEL2, P_STEEL2, 1.0f, false }, {2,2,3},3 },
 { "Mercenary", 400, 12,0,10, 20,20, 150,70,70, 0,14, 60,80, 36,2,
   { {178,140,108,255},{92,70,58,255},{60,46,38,255},{170,120,60,255},{50,40,34,255},P_STEEL2,
     BODY_HUMAN, WEAP_BROAD, SHLD_NONE, HELM_HORNED, P_STEEL2, P_STEEL2, 1.1f, false }, {2,2,3},3 },
 { "Skeleton Mage", 400, 0,32,0, 10,30, 800,20,60, 0,22, 200,260, 36,2,
   { {198,192,172,255},{70,60,92,255},{46,40,62,255},{140,120,180,255},{30,30,30,255},P_VIOLET,
     BODY_UNDEAD, WEAP_NONE, SHLD_NONE, HELM_HOOD, P_VIOLET, P_VIOLET, 1.0f, true }, {2,13,11,15},4 },
 { "Mercenary", 420, 10,0,15, 30,0, 170,30,0, 0,10, 150,260, 36,2,
   { {178,140,108,255},{92,70,58,255},{60,46,38,255},{170,120,60,255},{50,40,34,255},P_STEEL2,
     BODY_HUMAN, WEAP_KATANA, SHLD_NONE, HELM_HORNED, P_STEEL2, P_STEEL2, 1.1f, false }, {2,2,3},3 },
 { "Skeleton", 450, 17,5,10, 20,20, 350,20,20, 0,27, 350,450, 44,3,
   { {206,200,182,255},{96,96,90,255},{60,60,56,255},{150,148,136,255},{30,30,30,255},P_STEEL2,
     BODY_UNDEAD, WEAP_BROAD, SHLD_NONE, HELM_NONE, P_STEEL2, P_STEEL2, 1.0f, false }, {2,2,3},3 },
 { "Skeleton Mage", 500, 5,28,10, 10,50, 0,30,50, 0,27, 350,450, 44,3,
   { {198,192,172,255},{70,60,92,255},{46,40,62,255},{140,120,180,255},{30,30,30,255},P_VIOLET,
     BODY_UNDEAD, WEAP_NONE, SHLD_NONE, HELM_HOOD, P_VIOLET, P_VIOLET, 1.0f, true }, {2,13,11,15},4 },
 { "Agent", 550, 17,0,15, 40,-15, 250,50,25, 0,19, 150,160, 44,3,
   { {168,138,112,255},{40,42,54,255},{26,28,36,255},{140,44,44,255},{28,26,24,255},P_INK2,
     BODY_HUMAN, WEAP_KATANA, SHLD_NONE, HELM_HOOD, P_INK2, P_INK2, 1.0f, false }, {2,2,3},3 },
 { "Poison Wasp", 550, 16,0,5, 15,10, 1,10,10, 0,35, 170,250, 44,3,
   { {168,150,60,255},{120,104,40,255},{84,72,28,255},{208,190,90,255},{60,52,24,255},P_STEEL,
     BODY_BEAST, WEAP_NONE, SHLD_NONE, HELM_NONE, P_STEEL, P_STEEL, 0.72f, false }, {2,2,3},3 },
 { "Seer", 600, 4,15,15, 15,60, 250,30,60, 0,19, 150,120, 44,3,
   { {198,180,160,255},{74,66,104,255},{48,42,70,255},{150,130,190,255},{60,50,44,255},P_VIOLET,
     BODY_HUMAN, WEAP_STAFF, SHLD_NONE, HELM_HOOD, P_VIOLET, P_VIOLET, 1.0f, false }, {2,13,11,15},4 },
 { "Shaman", 700, 7,14,7, 40,40, 1,0,0, 0,19, 200,70, 44,3,
   { {176,150,120,255},{86,70,52,255},{56,46,34,255},{120,160,110,255},{40,34,28,255},P_JADE,
     BODY_HUMAN, WEAP_STAFF, SHLD_NONE, HELM_HOOD, P_JADE, P_JADE, 1.02f, false }, {2,13,11,15},4 },
 { "Golem", 700, 28,3,10, 60,40, 200,80,80, 0,20, 200,270, 44,3,
   { {168,196,214,255},{110,140,164,255},{74,100,124,255},{200,226,240,255},{80,110,130,255},P_KI,
     BODY_BRUTE, WEAP_AXE, SHLD_NONE, HELM_NONE, P_KI, P_KI, 1.35f, false }, {2,2,3},3 },
 { "Ronin", 900, 10,13,20, 40,40, 1,0,0, 0,19, 120,120, 44,3,
   { {188,152,118,255},{70,74,86,255},{46,50,60,255},{160,140,90,255},{34,30,28,255},P_STEEL,
     BODY_HUMAN, WEAP_KATANA, SHLD_NONE, HELM_NONE, P_STEEL, P_STEEL, 1.05f, false }, {2,13,11,15},4 },
 { "Ronin", 1000, 10,11,10, 40,40, 1,0,0, 0,20, 500,320, 44,3,
   { {188,152,118,255},{70,74,86,255},{46,50,60,255},{160,140,90,255},{34,30,28,255},P_STEEL,
     BODY_HUMAN, WEAP_KATANA, SHLD_NONE, HELM_NONE, P_STEEL, P_STEEL, 1.05f, false }, {2,13,11,15},4 },
 { "Shaman", 1000, 9,8,8, 20,50, 1,0,0, 0,20, 500,320, 44,3,
   { {176,150,120,255},{86,70,52,255},{56,46,34,255},{120,160,110,255},{40,34,28,255},P_JADE,
     BODY_HUMAN, WEAP_STAFF, SHLD_NONE, HELM_HOOD, P_JADE, P_JADE, 1.02f, false }, {2,2,3},3 },
 { "Golem", 1000, 25,0,20, 50,30, 0,80,80, 0,27, 350,460, 44,3,
   { {168,196,214,255},{110,140,164,255},{74,100,124,255},{200,226,240,255},{80,110,130,255},P_KI,
     BODY_BRUTE, WEAP_AXE, SHLD_NONE, HELM_NONE, P_KI, P_KI, 1.35f, false }, {2,3,5},3 },
 { "Agent", 1000, 19,0,10, 40,-10, 0,90,90, 0,21, 120,140, 44,3,
   { {168,138,112,255},{40,42,54,255},{26,28,36,255},{140,44,44,255},{28,26,24,255},P_INK2,
     BODY_HUMAN, WEAP_KNIFE, SHLD_NONE, HELM_HOOD, P_INK2, P_INK2, 1.0f, false }, {2,2,3},3 },
 { "Shadow Reaper", 1000, 65,15,5, 85,85, 1,20,20, 0,60, 700,600, 44,3,
   { {110,100,130,255},{40,32,56,255},{24,18,36,255},{122,92,158,255},{20,16,28,255},P_VIOLET,
     BODY_WISP, WEAP_SCYTHE, SHLD_NONE, HELM_HOOD, P_VIOLET, P_VIOLET, 1.45f, true }, {2,2,3},3 },
 { "Samurai", 1100, 18,0,10, 40,30, 0,40,30, 0,18, 160,230, 52,4,
   { {190,156,120,255},{78,62,58,255},{50,40,38,255},{178,146,74,255},{34,30,28,255},P_GOLD,
     BODY_HUMAN, WEAP_KATANA, SHLD_NONE, HELM_HORNED, P_GOLD, P_GOLD, 1.08f, false }, {2,2,3},3 },
 { "Mountain Naga", 1100, 25,4,30, 40,40, 400,20,20, 0,22, 200,265, 52,4,
   { {150,132,86,255},{122,104,62,255},{86,72,42,255},{190,170,110,255},{70,60,36,255},P_GOLD2,
     BODY_BEAST, WEAP_NONE, SHLD_NONE, HELM_NONE, P_GOLD2, P_GOLD2, 1.15f, false }, {2,3,5},3 },
 { "Mountain Naga", 1150, 17,10,0, 30,30, 450,30,30, 0,27, 350,460, 52,4,
   { {150,132,86,255},{122,104,62,255},{86,72,42,255},{190,170,110,255},{70,60,36,255},P_GOLD2,
     BODY_BEAST, WEAP_NONE, SHLD_NONE, HELM_NONE, P_GOLD2, P_GOLD2, 1.15f, false }, {2,2,3},3 },
 { "Undead", 1200, 28,0,7, 40,10, 1,20,20, 0,15, 180,225, 52,4,
   { {126,140,110,255},{84,80,64,255},{54,52,42,255},{110,124,96,255},{44,44,36,255},P_JADE,
     BODY_UNDEAD, WEAP_AXE, SHLD_NONE, HELM_NONE, P_JADE, P_JADE, 1.05f, false }, {2,2,3},3 },
 { "Samurai", 1350, 14,10,10, 40,30, 0,40,30, 0,17, 550,750, 52,4,
   { {190,156,120,255},{78,62,58,255},{50,40,38,255},{178,146,74,255},{34,30,28,255},P_GOLD,
     BODY_HUMAN, WEAP_KATANA, SHLD_NONE, HELM_HORNED, P_GOLD, P_GOLD, 1.08f, false }, {2,2,3},3 },
 { "Samurai", 1350, 10,14,10, 40,30, 150,40,30, 0,17, 550,750, 52,4,
   { {190,156,120,255},{78,62,58,255},{50,40,38,255},{178,146,74,255},{34,30,28,255},P_GOLD,
     BODY_HUMAN, WEAP_BROAD, SHLD_NONE, HELM_HORNED, P_GOLD, P_GOLD, 1.08f, false }, {2,13,11,15},4 },
 { "Undead", 1500, 30,0,10, 60,20, 1,40,20, 0,17, 300,450, 52,4,
   { {126,140,110,255},{84,80,64,255},{54,52,42,255},{110,124,96,255},{44,44,36,255},P_JADE,
     BODY_UNDEAD, WEAP_AXE, SHLD_NONE, HELM_NONE, P_JADE, P_JADE, 1.05f, false }, {2,2,7,5},4 },
 { "Blood Spirit", 1800, 0,65,0, 0,65, 4000,20,50, 0,35, 400,300, 52,4,
   { {190,90,90,255},{120,40,52,255},{80,24,34,255},{220,120,110,255},{60,20,26,255},P_BLOOD,
     BODY_WISP, WEAP_NONE, SHLD_NONE, HELM_NONE, P_BLOOD, P_BLOOD, 1.0f, true }, {2,13,11,15},4 },
 { "Semi Demon", 2300, 25,10,10, 50,10, 200,30,30, 0,25, 300,300, 52,4,
   { {150,90,96,255},{80,40,60,255},{52,26,40,255},{190,80,70,255},{36,24,30,255},P_BLOOD,
     BODY_HUMAN, WEAP_BROAD, SHLD_NONE, HELM_HORNED, P_BLOOD, P_BLOOD, 1.15f, true }, {2,2,7,5},4 },
 { "Flesh Fiend", 2350, 30,10,10, 40,40, 350,60,60, 0,30, 500,500, 52,4,
   { {170,120,110,255},{124,70,64,255},{80,44,42,255},{200,150,140,255},{70,40,38,255},P_BLOOD,
     BODY_BRUTE, WEAP_BROAD, SHLD_NONE, HELM_NONE, P_BLOOD, P_BLOOD, 1.4f, false }, {2,2,7,5},4 },
 { "Liquid Metal", 3000, 60,0,10, 70,20, 350,30,30, 0,27, 300,300, 60,5,
   { {142,148,158,255},{86,92,104,255},{60,64,74,255},{190,196,206,255},{86,92,104,255},P_STEEL,
     BODY_BRUTE, WEAP_BROAD, SHLD_NONE, HELM_NONE, P_STEEL, P_STEEL, 1.2f, true }, {2,2,7,5},4 },
 { "Fallen Guardian", 4000, 50,20,10, 40,40, 2300,20,20, 0,38, 500,400, 60,5,
   { {160,150,140,255},{86,84,96,255},{56,54,64,255},{198,160,74,255},{40,40,44,255},P_GOLD,
     BODY_BRUTE, WEAP_BROAD, SHLD_NONE, HELM_FULL, P_GOLD, P_GOLD, 1.35f, false }, {2,2,7,5},4 },
 { "Anti Ninja", 4000, 65,15,5, 0,0, 1,20,20, 0,80, 600,500, 60,5,
   { {140,132,128,255},{36,34,44,255},{22,20,28,255},{150,40,40,255},{24,22,22,255},P_INK2,
     BODY_HUMAN, WEAP_KATANA, SHLD_NONE, HELM_HOOD, P_INK2, P_INK2, 1.05f, false }, {2,2,7,5},4 },
};
const int ENEMY_COUNT = (int)(sizeof(ENEMIES) / sizeof(ENEMIES[0]));

/* ---------------------------------------------------------------- classes */
Look data_class_look(ClassId c)
{
    Look lk;
    lk.skin  = (Color){ 206, 168, 132, 255 };
    lk.cloth = (Color){ 58, 72, 96, 255 };
    lk.clothDark = (Color){ 38, 48, 66, 255 };
    lk.trim  = (Color){ 168, 64, 58, 255 };
    lk.hair  = (Color){ 36, 30, 28, 255 };
    lk.metal = C_STEEL;
    lk.body  = BODY_HUMAN;
    lk.weapon = WEAP_NONE; lk.shield = SHLD_NONE; lk.helm = HELM_NONE;
    lk.weaponTint = C_STEEL; lk.shieldTint = C_STEEL;
    lk.scale = 1.0f; lk.glow = false;
    switch (c) {
    case CLASS_WARRIOR:
        lk.cloth = (Color){ 96, 62, 48, 255 }; lk.clothDark = (Color){ 64, 40, 32, 255 };
        lk.trim  = (Color){ 176, 140, 70, 255 }; break;
    case CLASS_SHADOW:
        lk.cloth = (Color){ 42, 44, 58, 255 }; lk.clothDark = (Color){ 26, 28, 38, 255 };
        lk.trim  = (Color){ 150, 52, 48, 255 }; break;
    case CLASS_SPELLCASTER:
        lk.cloth = (Color){ 62, 58, 96, 255 }; lk.clothDark = (Color){ 40, 38, 66, 255 };
        lk.trim  = (Color){ 132, 176, 200, 255 }; lk.glow = true; break;
    case CLASS_BALANCED:
        lk.cloth = (Color){ 132, 92, 52, 255 }; lk.clothDark = (Color){ 92, 62, 36, 255 };
        lk.trim  = (Color){ 200, 190, 160, 255 }; break;
    default: break;
    }
    return lk;
}

void data_class_base(Player *p, ClassId c)
{
    /* Every discipline starts from the same numbers in the original; only the
       per-level growth in CLASS_GROWTH differs. */
    (void)c;
    p->baseLife   = 75;
    p->baseMana   = 75;
    p->baseEng    = 50;
    p->baseStr    = 15;
    p->baseSpeed  = 15;
    p->basePhyDmg = 0;
    p->baseMagDmg = 0;
    p->basePhyDef = 0;
    p->baseMagDef = 0;
    p->baseShdPts = 0;
}

/* Shop stock (indices into ITEMS). */
static const int SHOP_SMITH[]  = { 0,1,2,25,26,42,43,44,           /* tier 1 */
                                   3,4,5,13,20,21,22,24,27,31,36,45,50,51,52,58, -1 };
static const int SHOP_SMITH2[] = { 6,11,16,18,23,28,29,30,32,33,34,37,41,46,47,49,53,54,55,57,
                                   7,8,9,10,17,35,38,39,40,48,59,
                                   12,14,15,19,56, -1 };
static const int SHOP_GOODS[]  = { 63,64,65,66,67,60,62, -1 };
static const int SHOP_RELIC[]  = { 61,60,62,63,66, -1 };

int data_shop_table(int vendor, const int **out)
{
    const int *t;
    switch (vendor) {
    case 0:  t = SHOP_SMITH;  break;
    case 1:  t = SHOP_GOODS;  break;
    case 2:  t = SHOP_RELIC;  break;
    default: t = SHOP_SMITH2; break;
    }
    *out = t;
    int n = 0; while (t[n] >= 0) n++;
    return n;
}
