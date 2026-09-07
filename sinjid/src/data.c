/* Content tables: items, skills, enemies, classes and zones.
   Every name, stat line and map in this file is original to this remake. */
#include "game.h"
#include <string.h>

const char *CLASS_NAMES[CLASS_COUNT] = { "Warrior", "Shadow", "Mystic", "Monk" };
const char *CLASS_BLURB[CLASS_COUNT] = {
    "Heavy steel and a stubborn guard. The most life and the deepest\nshield pool; grinds enemies down with raw strength.",
    "A ninja of the burnt village. Fast and evasive, strikes twice\nbefore the guard comes up. Fragile once a blow lands clean.",
    "Trained in the old ki disciplines. Weak arm, enormous mana, and\nspells that walk straight past armour and shields.",
    "Bare hands and breath control. Balanced, recovers energy fast,\nand mends its own wounds cheaply."
};

/* ------------------------------------------------------------------ items */
/* name, type, price, tier | life mana str pdmg pdef mdmg mdef |
   shdPts shdPDef shdMDef shdDmg | speed avoid | shape, tint, note           */
const ItemDef ITEMS[] = {
/*0*/{ "Bare Hands",    ITEM_WEAPON, 0,    0,  0,0,  0, 0,0, 0,0,    0,0,0,0,   0,0,  WEAP_NONE,   P_STEEL,  "Knuckles and nerve." },
/*1*/{ "Chipped Knife", ITEM_WEAPON, 40,   1,  0,0,  1, 3,0, 0,0,    0,0,0,0,   1,1,  WEAP_KNIFE,  P_STEEL,  "Village steel, badly kept." },
/*2*/{ "Hunting Dirk",  ITEM_WEAPON, 120,  1,  0,0,  2, 6,0, 0,0,    0,0,0,2,   2,1,  WEAP_KNIFE,  P_STEEL,  "Quick in the hand." },
/*3*/{ "Ash Bo",        ITEM_WEAPON, 150,  1,  0,8,  0, 5,1, 3,0,    0,0,0,0,   1,0,  WEAP_STAFF,  P_BARK,   "Hardwood, weighted at both ends." },
/*4*/{ "Field Katana",  ITEM_WEAPON, 260,  2,  0,0,  3,11,0, 0,0,    0,0,0,0,   1,0,  WEAP_KATANA, P_STEEL,  "Standard issue for the guard." },
/*5*/{ "Iron Cleaver",  ITEM_WEAPON, 300,  2,  0,0,  5,14,0, 0,0,    0,0,0,4,  -2,0,  WEAP_AXE,    P_STEEL2, "Slow, but it ruins shields." },
/*6*/{ "Prayer Staff",  ITEM_WEAPON, 340,  2,  0,20, 0, 4,0,12,2,    0,0,0,0,   0,0,  WEAP_STAFF,  P_JADE,   "Carved with the old syllables." },
/*7*/{ "Kaido Blade",   ITEM_WEAPON, 620,  3,  0,0,  6,20,0, 0,0,    0,0,0,3,   2,1,  WEAP_KATANA, P_KI,     "Folded until the edge sang." },
/*8*/{ "Reaver Axe",    ITEM_WEAPON, 700,  3,  0,0,  9,26,0, 0,0,    0,0,0,8,  -3,0,  WEAP_AXE,    P_BLOOD,  "Takes both hands and a grudge." },
/*9*/{ "Ki Focus Rod",  ITEM_WEAPON, 720,  3,  0,40, 0, 6,0,24,4,    0,0,0,0,   1,0,  WEAP_STAFF,  P_VIOLET, "Warm to the touch." },
/*10*/{ "Wolf Claws",   ITEM_WEAPON, 780,  3,  0,0,  4,17,0, 0,0,    0,0,0,0,   6,4,  WEAP_CLAW,   P_STEEL,  "Two cuts for every swing." },
/*11*/{ "Nightfall",    ITEM_WEAPON, 1500, 4,  0,0, 10,34,0, 6,0,    0,0,0,5,   4,2,  WEAP_KATANA, P_INK2,   "The blade the burnt village lost." },
/*12*/{ "Grave Scythe", ITEM_WEAPON, 1600, 4,  0,0, 12,38,0, 0,0,    0,0,0,10, -2,0,  WEAP_SCYTHE, P_VIOLET, "Reaps guard and man alike." },
/*13*/{ "Sunspear",     ITEM_WEAPON, 1700, 4,  0,30, 8,30,0,18,6,    0,0,0,4,   3,1,  WEAP_SPEAR,  P_GOLD,   "Long reach, longer memory." },
/*14*/{ "Shadow Fang",  ITEM_WEAPON, 3200, 5,  0,20,15,52,0,20,4,    0,0,0,12,  8,5,  WEAP_KATANA, P_VIOLET, "It drinks before you do." },

/*15*/{ "Cloth Wrap",   ITEM_SHIELD, 30,   1,  0,0,  0, 0,1, 0,0,    8,1,0,0,   1,1,  SHLD_WRIST,   P_BARK,   "Better than nothing." },
/*16*/{ "Iron Bracer",  ITEM_SHIELD, 130,  1,  0,0,  0, 0,2, 0,0,   22,3,1,0,   0,0,  SHLD_WRIST,   P_STEEL,  "Turns a knife aside." },
/*17*/{ "Round Buckler",ITEM_SHIELD, 280,  2,  0,0,  0, 0,3, 0,1,   48,6,3,0,  -1,0,  SHLD_BUCKLER, P_STEEL,  "Small, fast, honest." },
/*18*/{ "Warding Charm",ITEM_SHIELD, 340,  2,  0,15, 0, 0,1, 0,6,   40,2,10,0,  0,1,  SHLD_BUCKLER, P_JADE,   "Turns spellwork, not steel." },
/*19*/{ "Guard Kite",   ITEM_SHIELD, 640,  3,  0,0,  0, 0,6, 0,2,   96,11,5,0, -2,0,  SHLD_KITE,    P_STEEL,  "The house guard's wall." },
/*20*/{ "Blade Guard",  ITEM_SHIELD, 700,  3,  0,0,  2, 4,4, 0,1,   80,8,4,6,   1,1,  SHLD_BLADE,   P_BLOOD,  "Parries, and cuts on the way back." },
/*21*/{ "Tower Slab",   ITEM_SHIELD, 1400, 4,  0,0,  0, 0,10,0,4,  180,20,9,0, -4,-2, SHLD_TOWER,   P_STEEL2, "You do not move. Neither do they." },
/*22*/{ "Spiked Ward",  ITEM_SHIELD, 1500, 4,  0,0,  3, 6,7, 0,3,  150,15,7,14, 0,0,  SHLD_SPIKE,   P_BLOOD,  "Punishes the shield-breaker." },
/*23*/{ "Void Bracer",  ITEM_SHIELD, 3000, 5,  0,30, 0, 0,9, 8,14, 220,18,20,8, 4,4,  SHLD_WRIST,   P_VIOLET, "Drinks the blow before it lands." },

/*24*/{ "Rag Tunic",    ITEM_ARMOUR, 25,   1,  8,0,   0,0,1, 0,0,    0,0,0,0,   1,1,  0, P_BARK,   "Smells of smoke." },
/*25*/{ "Padded Coat",  ITEM_ARMOUR, 140,  1,  22,0,  0,0,3, 0,1,    0,0,0,0,   0,0,  0, P_BARK,   "Layered cotton and patience." },
/*26*/{ "Scale Vest",   ITEM_ARMOUR, 320,  2,  46,0,  1,0,7, 0,2,    0,0,0,0,  -1,0,  0, P_STEEL,  "Overlapping iron scales." },
/*27*/{ "Silk Kata",    ITEM_ARMOUR, 360,  2,  30,25, 0,0,4, 4,7,    0,0,0,0,   2,2,  0, P_JADE,   "The monastery weave." },
/*28*/{ "Steel Plates", ITEM_ARMOUR, 760,  3,  90,0,  2,0,14,0,4,   10,2,0,0,  -3,-1, 0, P_STEEL2, "Heavy as a bad decision." },
/*29*/{ "Night Weave",  ITEM_ARMOUR, 820,  3,  62,20, 0,0,9, 6,8,    0,0,0,0,   5,5,  0, P_INK2,   "Woven to swallow lamplight." },
/*30*/{ "Ancestor Mail",ITEM_ARMOUR, 1650, 4,  160,0, 4,0,22,0,9,   20,4,2,0,  -2,0,  0, P_GOLD,   "Names of the dead stitched inside." },
/*31*/{ "Demon Hide",   ITEM_ARMOUR, 3400, 5,  260,40,6,4,28,10,18,  0,0,0,0,   4,3,  0, P_BLOOD,  "Still warm. It should not be." },

/*32*/{ "Head Cloth",   ITEM_HELM, 20,   1,  4,0,   0,0,1, 0,0,      0,0,0,0,   1,1,  HELM_BANDANA, P_BARK,   "Keeps sweat out of the eyes." },
/*33*/{ "Iron Band",    ITEM_HELM, 120,  1,  12,0,  1,1,2, 0,0,      0,0,0,0,   0,0,  HELM_CIRCLET, P_STEEL,  "A simple circle of iron." },
/*34*/{ "Focus Hood",   ITEM_HELM, 300,  2,  10,30, 0,0,2, 6,4,      0,0,0,0,   1,1,  HELM_HOOD,    P_JADE,   "Narrows the world to one point." },
/*35*/{ "Guard Helm",   ITEM_HELM, 420,  2,  34,0,  1,0,7, 0,2,      0,0,0,0,  -1,0,  HELM_FULL,    P_STEEL,  "Dented on the left side." },
/*36*/{ "Horned Casque",ITEM_HELM, 900,  3,  56,0,  4,3,10,0,3,      0,0,0,4,  -1,0,  HELM_HORNED,  P_STEEL2, "Taken off something with horns." },
/*37*/{ "Crown of Ash", ITEM_HELM, 1800, 4,  70,60, 2,0,9, 14,12,    0,0,0,0,   2,2,  HELM_CIRCLET, P_VIOLET, "All that came out of the fire." },

/*38*/{ "Copper Ring",  ITEM_RELIC, 90,   1,  6,6,    1,1,1, 1,1,     4,0,0,0,   1,1,  0, P_GOLD2,  "Warm, cheap, lucky." },
/*39*/{ "Jade Bead",    ITEM_RELIC, 400,  2,  14,30,  0,0,2, 5,5,     8,1,2,0,   2,2,  0, P_JADE,   "Cool in any season." },
/*40*/{ "Iron Charm",   ITEM_RELIC, 450,  2,  40,0,   3,3,4, 0,1,    16,2,0,3,   0,0,  0, P_STEEL,  "A soldier's knot of iron." },
/*41*/{ "Wolf Totem",   ITEM_RELIC, 950,  3,  30,20,  4,4,3, 4,3,    12,1,1,2,   7,6,  0, P_BARK,   "Run first. Bite second." },
/*42*/{ "Master's Cord",ITEM_RELIC, 2000, 4,  80,80,  6,6,8, 10,10,  30,3,3,5,   5,4,  0, P_GOLD,   "Your teacher's, before the fire." },
/*43*/{ "Shadow Sigil", ITEM_RELIC, 4000, 5,  120,120,10,10,12,16,16,50,5,5,10,  9,8,  0, P_VIOLET, "The mark the reaper wears." },

/*44*/{ "Rice Ball",     ITEM_CONSUMABLE, 15,  1,  60,0,   0,0,0,0,0,   0,0,0,0, 0,0, 0, P_PARCH,  "Restores 60 life." },
/*45*/{ "Broth Bowl",    ITEM_CONSUMABLE, 45,  2,  180,0,  0,0,0,0,0,   0,0,0,0, 0,0, 0, P_GOLD2,  "Restores 180 life." },
/*46*/{ "Field Dressing",ITEM_CONSUMABLE, 120, 3,  500,0,  0,0,0,0,0,   0,0,0,0, 0,0, 0, P_BLOOD2, "Restores 500 life." },
/*47*/{ "Green Tea",     ITEM_CONSUMABLE, 25,  1,  0,40,   0,0,0,0,0,   0,0,0,0, 0,0, 0, P_JADE,   "Restores 40 mana." },
/*48*/{ "Ki Draught",    ITEM_CONSUMABLE, 90,  2,  0,140,  0,0,0,0,0,   0,0,0,0, 0,0, 0, P_KI,     "Restores 140 mana." },
/*49*/{ "White Leaf",    ITEM_CONSUMABLE, 200, 3,  240,120,0,0,0,0,0,   0,0,0,0, 0,0, 0, P_KI2,    "Restores 240 life and 120 mana." },
/*50*/{ "Whetstone",     ITEM_CONSUMABLE, 60,  2,  0,0,    0,0,0,0,0, 120,0,0,0, 0,0, 0, P_STEEL,  "Mends 120 shield points." },
};
const int ITEM_COUNT = (int)(sizeof(ITEMS) / sizeof(ITEMS[0]));

/* ----------------------------------------------------------------- skills */
/* name, desc | tree, maxRank, reqLevel, mana, eng | power, perRank |
   dmgType, target, flags, anim, fx                                          */
const SkillDef SKILLS[MAX_SKILLS] = {
 { "Strike", "A plain swing. Costs nothing and never runs dry.",
   0, 1, 1,  0,  0, 1.00f, 0.00f, DMG_PHYSICAL, SK_TARGET_ONE_FOE, SKF_NONE, ANIM_ATTACK, P_STEEL },
 { "Guard", "Brace. Halves damage taken this round and mends shield points.",
   0, 1, 1,  0,  0, 0.00f, 0.00f, DMG_PHYSICAL, SK_TARGET_SELF, SKF_SHIELD_UP, ANIM_BLOCK, P_KI },
 { "Double Cut", "Two fast strikes at reduced power each.",
   0, 5, 2,  0, 14, 0.60f, 0.07f, DMG_PHYSICAL, SK_TARGET_ONE_FOE, SKF_MULTI, ANIM_ATTACK, P_STEEL },
 { "Shield Breaker", "Heavy overhead. Triple damage to shield points.",
   0, 5, 3,  0, 18, 0.90f, 0.12f, DMG_PHYSICAL, SK_TARGET_ONE_FOE, SKF_SHIELD_DMG, ANIM_ATTACK, P_GOLD },
 { "Gut Thrust", "Slips inside the guard. Ignores shield points entirely.",
   0, 5, 5,  0, 26, 0.75f, 0.10f, DMG_PHYSICAL, SK_TARGET_ONE_FOE, SKF_IGNORE_SHD, ANIM_ATTACK, P_BLOOD },
 { "Wide Sweep", "One arc across every enemy still standing.",
   0, 5, 7,  0, 30, 0.55f, 0.08f, DMG_PHYSICAL, SK_TARGET_ALL_FOES, SKF_NONE, ANIM_ATTACK, P_STEEL },
 { "Iron Stance", "Physical defence sharply raised for three rounds.",
   0, 3, 8,  0, 20, 0.00f, 0.00f, DMG_PHYSICAL, SK_TARGET_SELF, SKF_BUFF_DEF, ANIM_BLOCK, P_STEEL },
 { "Bloodletter", "A deep cut that returns a third of the damage as life.",
   0, 5, 11, 0, 34, 0.85f, 0.11f, DMG_PHYSICAL, SK_TARGET_ONE_FOE, SKF_DRAIN, ANIM_ATTACK, P_BLOOD },
 { "Hilt Smash", "Cracks the skull. May stun for a round.",
   0, 3, 13, 0, 28, 0.60f, 0.09f, DMG_PHYSICAL, SK_TARGET_ONE_FOE, SKF_STUN, ANIM_ATTACK, P_GOLD },
 { "Killing Breath", "Everything you have, in one downward cut.",
   0, 5, 16, 0, 48, 1.80f, 0.22f, DMG_PHYSICAL, SK_TARGET_ONE_FOE, SKF_SHIELD_DMG|SKF_NEVER_MISS, ANIM_ATTACK, P_BLOOD2 },

 { "Ki Bolt", "A thrown spark of ki. Cheap and reliable.",
   1, 5, 1,  6,  0, 1.00f, 0.14f, DMG_MAGIC, SK_TARGET_ONE_FOE, SKF_NONE, ANIM_CAST, P_KI },
 { "Mend", "Knit your own wounds shut.",
   1, 5, 3,  12, 0, 0.00f, 0.00f, DMG_MAGIC, SK_TARGET_SELF, SKF_HEAL, ANIM_HEAL, P_JADE },
 { "Frost Nail", "A cold spike that bypasses the shield layer.",
   1, 5, 4,  16, 0, 1.10f, 0.16f, DMG_MAGIC, SK_TARGET_ONE_FOE, SKF_IGNORE_SHD, ANIM_CAST, P_KI2 },
 { "Ember Wave", "Fire washes over the whole line.",
   1, 5, 6,  26, 0, 0.70f, 0.11f, DMG_MAGIC, SK_TARGET_ALL_FOES, SKF_NONE, ANIM_CAST, P_BLOOD2 },
 { "Ward", "Wrap yourself in ki. Magic defence up for three rounds.",
   1, 3, 8,  18, 0, 0.00f, 0.00f, DMG_MAGIC, SK_TARGET_SELF, SKF_BUFF_DEF, ANIM_HEAL, P_VIOLET },
 { "Drain Soul", "Pull life across the gap and keep half of it.",
   1, 5, 10, 30, 0, 0.95f, 0.13f, DMG_MAGIC, SK_TARGET_ONE_FOE, SKF_DRAIN|SKF_IGNORE_SHD, ANIM_CAST, P_VIOLET },
 { "Focus", "Breathe. Restores mana and sharpens the next blows.",
   1, 3, 9,  0,  0, 0.00f, 0.00f, DMG_MAGIC, SK_TARGET_SELF, SKF_RESTORE_MP|SKF_BUFF_ATK, ANIM_HEAL, P_GOLD },
 { "Chain Lightning", "Splits across every enemy and never misses.",
   1, 5, 14, 44, 0, 1.05f, 0.15f, DMG_MAGIC, SK_TARGET_ALL_FOES, SKF_NEVER_MISS, ANIM_CAST, P_KI2 },
 { "Shadow Rend", "Tears the shape out from under them. Pure damage.",
   1, 5, 17, 60, 10, 1.90f, 0.25f, DMG_PURE, SK_TARGET_ONE_FOE, SKF_IGNORE_SHD|SKF_NEVER_MISS, ANIM_CAST, P_VIOLET },
};

/* ---------------------------------------------------------------- enemies */
/* Look = { skin, cloth, clothDark, trim, hair, metal, body, weapon, shield,
            helm, weaponTint, shieldTint, scale, glow }                      */
const EnemyDef ENEMIES[] = {
 { "Training Post", 1,3,   40,8,0,     2,1,   1,1,   0,0,  0,0,    0,0,   6,0,    8,0,    0,0,
   { {150,120,80,255},{120,96,64,255},{88,68,44,255},{170,150,110,255},{90,70,50,255},P_STEEL,
     BODY_HUMAN, WEAP_NONE, SHLD_NONE, HELM_NONE, P_STEEL, P_STEEL, 0.95f, false }, {0},1 },

 { "Marsh Stalker", 1,5,      34,7,0,     4,2,   0,0,   0,0,  1,1,    0,0,   12,10,  10,5,   25,1,
   { {168,150,60,255},{120,104,40,255},{84,72,28,255},{208,190,90,255},{60,52,24,255},P_STEEL,
     BODY_BEAST, WEAP_NONE, SHLD_NONE, HELM_NONE, P_STEEL, P_STEEL, 0.72f, false }, {0,0},2 },

 { "Road Bandit", 2,7,     60,12,0,    6,3,   3,2,   0,0,  1,1,    12,4,  8,6,    16,14,  35,1,
   { {186,150,116,255},{106,84,66,255},{72,56,44,255},{150,60,50,255},{40,32,28,255},P_STEEL,
     BODY_HUMAN, WEAP_KNIFE, SHLD_WRIST, HELM_BANDANA, P_STEEL, P_BARK, 1.0f, false }, {0,2},2 },

 { "Grave Skeleton", 3,9,  70,14,0,    8,4,   5,3,   0,0,  2,2,    20,6,  5,2,    22,16,  30,1,
   { {206,200,182,255},{96,96,90,255},{60,60,56,255},{150,148,136,255},{30,30,30,255},P_STEEL,
     BODY_UNDEAD, WEAP_BROAD, SHLD_BUCKLER, HELM_NONE, P_STEEL2, P_STEEL2, 1.0f, false }, {0,3},2 },

 { "Bone Shaman", 4,11,    62,11,60,   4,2,   3,2,   10,5, 6,4,    10,3,  6,4,    28,20,  40,2,
   { {198,192,172,255},{70,60,92,255},{46,40,62,255},{140,120,180,255},{30,30,30,255},P_VIOLET,
     BODY_UNDEAD, WEAP_STAFF, SHLD_NONE, HELM_HOOD, P_VIOLET, P_VIOLET, 1.0f, true }, {10,13,11},3 },

 { "Sand Naga", 5,13,      110,18,40,  11,5,  7,4,   6,3,  5,3,    26,8,  7,5,    36,26,  40,2,
   { {150,132,86,255},{122,104,62,255},{86,72,42,255},{190,170,110,255},{70,60,36,255},P_STEEL,
     BODY_BEAST, WEAP_SPEAR, SHLD_NONE, HELM_NONE, P_GOLD2, P_STEEL, 1.15f, false }, {0,4,12},3 },

 { "Iron Ronin", 6,15,     130,20,0,   14,6,  10,5,  0,0,  3,2,    60,14, 7,5,    44,34,  45,2,
   { {190,156,120,255},{70,74,86,255},{46,50,60,255},{160,140,90,255},{34,30,28,255},P_STEEL,
     BODY_HUMAN, WEAP_KATANA, SHLD_KITE, HELM_FULL, P_STEEL, P_STEEL, 1.05f, false }, {0,2,3,6},4 },

 { "Frost Golem", 7,16,    200,26,0,   16,7,  16,7,  0,0,  8,4,    80,18, 4,0,    54,38,  40,3,
   { {168,196,214,255},{110,140,164,255},{74,100,124,255},{200,226,240,255},{80,110,130,255},P_KI2,
     BODY_BRUTE, WEAP_NONE, SHLD_NONE, HELM_NONE, P_KI, P_KI, 1.35f, false }, {0,0,8},3 },

 { "Rot Walker", 5,14,     150,22,0,   12,5,  6,3,   0,0,  2,1,    12,4,  4,1,    38,24,  45,2,
   { {126,140,110,255},{84,80,64,255},{54,52,42,255},{110,124,96,255},{44,44,36,255},P_STEEL,
     BODY_UNDEAD, WEAP_CLAW, SHLD_NONE, HELM_NONE, P_JADE, P_STEEL, 1.05f, false }, {0,7},2 },

 { "Ash Mercenary", 8,17,  170,22,30,  17,7,  12,6,  4,2,  5,3,    90,18, 9,6,    62,48,  50,3,
   { {178,140,108,255},{92,70,58,255},{60,46,38,255},{170,120,60,255},{50,40,34,255},P_STEEL,
     BODY_HUMAN, WEAP_AXE, SHLD_SPIKE, HELM_HORNED, P_STEEL2, P_BLOOD, 1.1f, false }, {0,3,5,8},4 },

 { "Blood Spirit", 9,19,   140,18,120, 8,4,   4,2,   20,9, 14,7,   30,8,  13,12,  70,42,  40,3,
   { {190,90,90,255},{120,40,52,255},{80,24,34,255},{220,120,110,255},{60,20,26,255},P_BLOOD,
     BODY_WISP, WEAP_NONE, SHLD_NONE, HELM_NONE, P_BLOOD, P_BLOOD, 1.0f, true }, {10,15,13,11},4 },

 { "Fallen Ninja", 10,20,  180,22,60,  20,8,  12,6,  10,4, 8,4,    70,14, 18,16,  80,56,  55,3,
   { {170,140,112,255},{44,46,58,255},{28,30,40,255},{120,40,44,255},{28,26,24,255},P_INK2,
     BODY_HUMAN, WEAP_KATANA, SHLD_WRIST, HELM_HOOD, P_INK2, P_INK2, 1.0f, false }, {2,4,7,12},4 },

 { "Liquid Metal", 11,22,  210,24,80,  18,7,  20,9,  14,6, 12,6,   120,22,10,8,   90,66,  45,4,
   { P_STEEL,P_STEEL2,{60,64,74,255},{190,196,206,255},P_STEEL2,P_STEEL,
     BODY_BRUTE, WEAP_BROAD, SHLD_TOWER, HELM_NONE, P_STEEL, P_STEEL, 1.2f, true }, {0,3,6,12},4 },

 { "Flesh Fiend", 12,24,   300,30,60,  24,10, 14,7,  8,4,  8,4,    60,12, 6,2,    104,74, 55,4,
   { {170,120,110,255},{124,70,64,255},{80,44,42,255},{200,150,140,255},{70,40,38,255},P_BLOOD,
     BODY_BRUTE, WEAP_CLAW, SHLD_NONE, HELM_NONE, P_BLOOD, P_BLOOD, 1.4f, false }, {0,5,7,8},4 },

 { "Half Demon", 14,26,    280,30,140, 22,9,  16,8,  22,10,16,8,   100,18,13,10,  118,86, 55,4,
   { {150,90,96,255},{80,40,60,255},{52,26,40,255},{190,80,70,255},{36,24,30,255},P_BLOOD,
     BODY_HUMAN, WEAP_SCYTHE, SHLD_BLADE, HELM_HORNED, P_BLOOD, P_VIOLET, 1.15f, true }, {13,15,4,17},4 },

 { "Fallen Guardian", 16,28, 420,34,100, 26,11, 24,11, 14,7, 14,7, 200,28,9,5,    150,110,60,5,
   { {160,150,140,255},{86,84,96,255},{56,54,64,255},P_GOLD,{40,40,44,255},P_GOLD,
     BODY_BRUTE, WEAP_BROAD, SHLD_TOWER, HELM_FULL, P_GOLD, P_GOLD, 1.35f, false }, {0,3,5,6},4 },

 { "Anti Ninja", 18,30,    460,36,160, 30,12, 20,10, 24,11,20,10,  160,24,22,18,  260,180,100,5,
   { {140,132,128,255},{36,34,44,255},{22,20,28,255},{150,40,40,255},{24,22,22,255},P_INK2,
     BODY_HUMAN, WEAP_CLAW, SHLD_BLADE, HELM_HOOD, P_INK2, P_BLOOD, 1.05f, true }, {2,4,7,17},4 },

 { "Shadow Reaper", 20,40,  900,50,300, 38,14, 28,12, 34,14,26,12, 300,36,24,20,  800,500,100,5,
   { {110,100,130,255},{40,32,56,255},{24,18,36,255},P_VIOLET,{20,16,28,255},P_VIOLET,
     BODY_WISP, WEAP_SCYTHE, SHLD_NONE, HELM_HOOD, P_VIOLET, P_VIOLET, 1.45f, true }, {18,17,15,9},4 },
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
    case CLASS_MYSTIC:
        lk.cloth = (Color){ 62, 58, 96, 255 }; lk.clothDark = (Color){ 40, 38, 66, 255 };
        lk.trim  = (Color){ 132, 176, 200, 255 }; lk.glow = true; break;
    case CLASS_MONK:
        lk.cloth = (Color){ 132, 92, 52, 255 }; lk.clothDark = (Color){ 92, 62, 36, 255 };
        lk.trim  = (Color){ 200, 190, 160, 255 }; break;
    default: break;
    }
    return lk;
}

void data_class_base(Player *p, ClassId c)
{
    switch (c) {
    case CLASS_WARRIOR: p->baseLife=140; p->baseMana=20;  p->baseStr=12; p->baseDef=8;
                        p->baseMag=2;  p->baseMagDef=3; p->baseSpeed=6;  break;
    case CLASS_SHADOW:  p->baseLife=100; p->baseMana=40;  p->baseStr=10; p->baseDef=4;
                        p->baseMag=5;  p->baseMagDef=5; p->baseSpeed=14; break;
    case CLASS_MYSTIC:  p->baseLife=90;  p->baseMana=110; p->baseStr=4;  p->baseDef=3;
                        p->baseMag=14; p->baseMagDef=9; p->baseSpeed=8;  break;
    case CLASS_MONK:    p->baseLife=120; p->baseMana=70;  p->baseStr=9;  p->baseDef=6;
                        p->baseMag=8;  p->baseMagDef=7; p->baseSpeed=10; break;
    default: break;
    }
}

/* Shop stock (indices into ITEMS). */
static const int SHOP_SMITH[]  = { 1,2,3,4,5,6, 15,16,17,18, 24,25,26,27, 32,33,34,35, -1 };
static const int SHOP_SMITH2[] = { 7,8,9,10,11,12,13,14, 19,20,21,22,23, 28,29,30,31, 36,37, -1 };
static const int SHOP_GOODS[]  = { 44,45,46,47,48,49,50, 38,39, -1 };
static const int SHOP_RELIC[]  = { 38,39,40,41,42,43, -1 };

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
