/* ===========================================================================
   battle.c -- the turn engine.

   Order of play is by speed with a per-round jitter, so a fast fighter can
   act twice in a row but is not guaranteed to.  Damage runs through a shield
   layer before it ever touches life: while a fighter has shield points, blows
   are spent breaking them down (reduced by the shield's own defence, boosted
   by the attacker's shield damage), and only the overflow bleeds through.
   =========================================================================== */
#include "game.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "enemy_suit_table.h"

#define IMPACT_AT   0.46f     /* fraction of the attack animation           */
#define ANIM_LEN    0.85f

/* ------------------------------------------------------------------ layout
   The battle screen is the original's, taken off its main timeline: a status
   band across the top (heroes left, enemies right, two rows deep) and a
   command bar along the bottom holding three round buttons -- Attack, Life P.,
   Mana P. -- beside an 8x2 grid of round skill buttons.  Coordinates below are
   the SWF's own, on its 600x470 stage; BX/BY put them on ours with the same
   1.6x the overworld HUD uses.  See tools/groundtruth/battle_layout.json.   */
#define BS       1.6f
#define BOX      160.0f
#define BX(v)    (BOX + (v) * BS)
#define BY(v)    ((v) * BS)
#define BW(v)    ((v) * BS)

/* Fighters stand where the original's clips stand; the second of each pair is
   set back and to the inside.  Enemies are drawn mirrored. */
static const float HERO_X[MAX_HEROES] = { BX(195.9f), BX(225.8f) };
static const float HERO_Y[MAX_HEROES] = { BY(247.5f), BY(236.2f) };
static const float FOE_X[MAX_FOES]    = { BX(422.3f), BX(392.3f) };
static const float FOE_Y[MAX_FOES]    = { BY(247.1f), BY(237.2f) };
#define LINE_Y   BY(247.5f)

/* The command bar is flat -- no submenus.  Slots 0..2 are the round buttons on
   the left; 3.. are the skill grid, whose slot->skill mapping is the
   original's own (7, 8 and 10 are passives and have no button). */
#define GRID_COLS   8
#define GRID_ROWS   2
#define GRID_N      (GRID_COLS * GRID_ROWS)
#define CMD_ATTACK  0
#define CMD_LIFEPOT 1
#define CMD_MANAPOT 2
#define CMD_GRID0   3
#define CMD_N       (CMD_GRID0 + GRID_N)
static const int GRID_SKILL[GRID_N] = {
     0,  1,  2,  3,  4,  5,  6,  9,
    11, 12, 13, 14, 15, 16, 17, 18,
};

/* the original's own bar and plate colours */
#define C_LIFE_HI  ((Color){  21, 202,  21, 255 })
#define C_LIFE_LO  ((Color){  15, 124,  14, 255 })
#define C_MANA_HI  ((Color){  37, 149, 186, 255 })
#define C_MANA_LO  ((Color){  26,  77, 113, 255 })
#define C_SHD_TXT  ((Color){ 102, 153, 255, 255 })
#define C_LIFE_TXT ((Color){  36, 224,  36, 255 })
#define C_PLATE    ((Color){  78,  63,  50, 236 })
#define C_PLATE_HI ((Color){ 175, 153, 133, 255 })
#define C_PLATE_LN ((Color){ 115,  89,  66, 255 })
#define C_GROOVE   ((Color){  32,  30,  28, 255 })

/* --------------------------------------------------------------- helpers */

void battle_log(Battle *b, const char *fmt, ...)
{
    if (b->logCount >= MAX_LOG) {
        for (int i = 0; i + 1 < MAX_LOG; i++) memcpy(b->log[i], b->log[i + 1], sizeof b->log[0]);
        b->logCount = MAX_LOG - 1;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b->log[b->logCount], sizeof b->log[0], fmt, ap);
    va_end(ap);
    b->logCount++;
}

void battle_floater(Battle *b, Vector2 at, Color c, float size, const char *fmt, ...)
{
    for (int i = 0; i < MAX_FLOATERS; i++) {
        if (b->floats[i].life > 0) continue;
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(b->floats[i].text, sizeof b->floats[i].text, fmt, ap);
        va_end(ap);
        b->floats[i].pos = at;
        b->floats[i].pos.x += frnd(-10, 10);
        b->floats[i].life = b->floats[i].t = 1.25f;
        b->floats[i].col = c;
        b->floats[i].size = size;
        return;
    }
}

void battle_burst(Battle *b, Vector2 at, Color c, int n, float spd, int kind)
{
    for (int k = 0; k < n; k++) {
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (b->parts[i].life > 0) continue;
            float a = frnd(0, 6.2832f);
            float v = frnd(spd * 0.3f, spd);
            b->parts[i].pos = at;
            b->parts[i].vel = (Vector2){ cosf(a) * v, sinf(a) * v - (kind == 1 ? 40 : 0) };
            b->parts[i].maxLife = b->parts[i].life = frnd(0.3f, 0.8f);
            b->parts[i].size = frnd(2, 5);
            b->parts[i].col = c;
            b->parts[i].kind = kind;
            break;
        }
    }
}

static Combatant *slot(Battle *b, int idx)
{
    return (idx < MAX_HEROES) ? &b->heroes[idx] : &b->foes[idx - MAX_HEROES];
}
static int slot_count(Battle *b) { return MAX_HEROES + MAX_FOES; }
static bool slot_live(Battle *b, int idx)
{
    if (idx < MAX_HEROES) return idx < b->nHeroes && b->heroes[idx].alive;
    int f = idx - MAX_HEROES;
    return f < b->nFoes && b->foes[f].alive;
}
static Vector2 slot_pos(Battle *b, int idx)
{
    (void)b;
    if (idx < MAX_HEROES) return (Vector2){ HERO_X[idx], HERO_Y[idx] };
    return (Vector2){ FOE_X[idx - MAX_HEROES], FOE_Y[idx - MAX_HEROES] };
}

static int first_live_foe(Battle *b)
{
    for (int i = 0; i < b->nFoes; i++) if (b->foes[i].alive) return MAX_HEROES + i;
    return -1;
}
static int first_live_hero(Battle *b)
{
    for (int i = 0; i < b->nHeroes; i++) if (b->heroes[i].alive) return i;
    return -1;
}

/* --------------------------------------------------------------- set-up */

static void build_enemy(Combatant *c, const EnemyDef *d, int level)
{
    memset(c, 0, sizeof *c);
    snprintf(c->name, sizeof c->name, "%s", d->name);
    c->level   = level < 1 ? 1 : level;
    c->lifeMax = c->life = d->life;
    c->manaMax = c->mana = 200;
    c->engMax  = 100; c->eng = 60; c->engRate = 25;
    c->str     = d->str;
    c->strDmg  = d->str * 2;
    c->phyDmg  = d->phyDmg;
    c->magDmg  = d->magDmg;
    c->phyDef  = d->phyDef > DEF_CAP ? DEF_CAP : d->phyDef;
    c->magDef  = d->magDef > DEF_CAP ? DEF_CAP : d->magDef;   /* may be negative */
    c->shdMax  = c->shd = d->shdPts;
    c->shdPhyDef = d->shdPhyDef;
    c->shdMagDef = d->shdMagDef;
    c->shdDmg  = d->shdDmg;
    c->speed   = d->speed;
    c->atkSpd  = d->speed;
    c->look    = d->look;
    /* Dress the fighter in the original's own palette for its suit, keeping
       our shapes: the suit name is what its part clips are keyed on. */
    {
        int idx = (int)(d - ENEMIES);
        if (idx >= 0 && idx < (int)(sizeof ENEMY_SUIT / sizeof ENEMY_SUIT[0]))
            data_dress(&c->look, ENEMY_SUIT[idx]);
    }
    c->alive   = true;
    c->anim    = ANIM_STAND;
    c->atkBuff = c->defBuff = 1.0f;
    c->expReward  = d->exp;
    c->goldReward = d->gold;
    c->dropChance = d->dropChance;
    c->dropTier   = d->dropTier;
    c->aiSkillCount = d->aiSkillCount;
    for (int i = 0; i < d->aiSkillCount && i < 4; i++) c->aiSkill[i] = d->aiSkill[i];
}

static void roll_order(Battle *b)
{
    int idx[MAX_HEROES + MAX_FOES], key[MAX_HEROES + MAX_FOES], n = 0;
    for (int i = 0; i < slot_count(b); i++) {
        if (!slot_live(b, i)) continue;
        Combatant *c = slot(b, i);
        idx[n] = i;
        key[n] = c->speed;      /* the original sorts purely on speed */
        n++;
    }
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (key[j] > key[i]) {
                int t = key[i]; key[i] = key[j]; key[j] = t;
                t = idx[i]; idx[i] = idx[j]; idx[j] = t;
            }
    for (int i = 0; i < n; i++) b->order[i] = idx[i];
    b->orderCount = n;
    b->orderIdx = 0;
}

void battle_start(Game *g, const int *enemyDefs, int count, int level, bool arena, bool boss)
{
    Battle *b = &g->b;
    memset(b, 0, sizeof *b);
    b->isArena = arena;
    b->isBoss  = boss;
    b->canFlee = !arena && !boss;
    b->bgStyle = g->zones[g->p.zone].bgStyle;

    player_recalc(&g->p, &b->heroes[0]);
    if (g->p.curLife > 0) {
        b->heroes[0].life = g->p.curLife > b->heroes[0].lifeMax ? b->heroes[0].lifeMax : g->p.curLife;
        b->heroes[0].mana = g->p.curMana > b->heroes[0].manaMax ? b->heroes[0].manaMax : g->p.curMana;
    }
    b->heroes[0].homePos = slot_pos(b, 0);
    b->heroes[0].pos = b->heroes[0].homePos;
    b->nHeroes = 1;

    b->nFoes = count > MAX_FOES ? MAX_FOES : count;
    for (int i = 0; i < b->nFoes; i++) {
        build_enemy(&b->foes[i], &ENEMIES[enemyDefs[i]], level);
        b->foes[i].homePos = b->foes[i].pos = slot_pos(b, MAX_HEROES + i);
        b->expGain  += b->foes[i].expReward;
        b->goldGain += b->foes[i].goldReward;
    }
    b->dropItem = -1;
    b->phase = BP_INTRO;
    b->timer = 0;
    b->menuIdx = 0; b->menuTab = 0; b->skillIdx = 0;
    b->target = first_live_foe(b);
    battle_log(b, "%s stands ready.", b->heroes[0].name);
    roll_order(b);
    go_scene(g, SCENE_BATTLE);
}

/* -------------------------------------------------------------- damage */

typedef struct { int toShield, toLife; bool missed, broke, killed; } Hit;

/* The original's plain attack passes the fighter's stats through untouched;
   only skills scale them.  Selected in the menu as "Attack" (id -1). */
static const SkillDef BASIC_ATTACK = {
    "Attack", "A plain swing with what you are holding.",
    0, 1, 0, { -1, -1 }, 0, 0,  100, 0, 0, 0, 0, 0,
    DMG_PHYSICAL, SK_TARGET_ONE_FOE, SKF_NONE, ANIM_ATTACK, P_STEEL
};

/* Guard is a stance the original offers alongside the skills, not one of the
   nineteen, so it lives here rather than in the table. */
static const SkillDef BASIC_GUARD = {
    "Guard", "Brace behind the shield and mend it.",
    0, 1, 0, { -1, -1 }, 0, 0,  0, 0, 0, 0, 0, 0,
    DMG_PHYSICAL, SK_TARGET_SELF, SKF_SHIELD_UP, ANIM_BLOCK, P_KI
};

#define ID_ATTACK (-1)
#define ID_GUARD  (-2)

static const SkillDef *skill_def(int id)
{
    if (id == ID_GUARD) return &BASIC_GUARD;
    if (id < 0 || id >= MAX_SKILLS) return &BASIC_ATTACK;
    return &SKILLS[id];
}

/* Percentage damage reduction, exactly as the original applies it:
   dmg - ceil(dmg / 100 * defence).  Capped so armour never fully negates. */
static int cut(int dmg, int defPct)
{
    /* A few of the original's fighters carry negative defence, which amplifies
       the blow rather than blunting it, so only the upper bound is clamped. */
    if (defPct < -100) defPct = -100;
    if (defPct > DEF_CAP) defPct = DEF_CAP;
    return dmg - (int)ceilf(dmg / 100.0f * defPct);
}

static Hit resolve_hit(Battle *b, Combatant *a, Combatant *t, const SkillDef *sk, int rank)
{
    Hit h = { 0, 0, false, false, false };
    /* (stat / 100) * (pctBase + pctPerRank * rank), as the original scales it */
    float power = (sk->pctBase + sk->pctPerRank * rank) / 100.0f;
    if (sk->pctBase == 0 && sk->pctPerRank == 0) power = 1.0f;

    /* Hit check: the attacker's attack speed rolled against the target's
       speed.  A guarding target is harder to catch. */
    /* The original's own check, from its damage function:
           spdran1 = random(atkspd / 2) + atkspd / 2
           spdran2 = random(enemytarget.speed / 2)
           if (nomiss) spdran2 = -1
       and the blow misses only when spdran2 is strictly greater.  A tie is a
       hit, and "nomiss" is expressed by putting the defender's roll below
       every possible attacker roll rather than by skipping the test. */
    {
        int spdran1 = rnd(0, a->atkSpd / 2) + a->atkSpd / 2;
        int spdran2 = (sk->flags & SKF_NEVER_MISS) ? -1 : rnd(0, t->speed / 2);
        if (spdran2 > spdran1) { h.missed = true; return h; }
    }

    /* Three damage components, scaled by the skill and any attack buff. */
    int phy = (int)(a->phyDmg * power * a->atkBuff);
    int str = (int)(a->strDmg * power * a->atkBuff);
    int mag = (int)(a->magDmg * power * a->atkBuff);
    if (sk->dmgType == DMG_MAGIC)         { phy = 0; str = 0; }
    else if (sk->dmgType == DMG_PHYSICAL) { mag = 0; }
    if (sk->flags & SKF_PURE_MAGIC)       { phy = 0; str = 0; }
    /* Flat bonus on top of the stat, as the original's bolts are written:
       magdmg + flatBase + flatPerRank * rank. */
    if (!(sk->flags & SKF_PASSIVE)) {
        int flat = sk->flatBase + sk->flatPerRank * rank;
        if (flat > 0) {
            if (sk->dmgType == DMG_MAGIC) mag += flat;
            else                          phy += flat;
        }
    }
    /* Two scalers the original carries: one off your own missing life, one
       off a share of whatever life the target still has. */
    if (sk->flags & SKF_MISSING_LIFE)
        phy += (rank + 1) * (a->lifeMax - a->life);
    if (sk->flags & SKF_TARGET_LIFE)
        phy += t->life * (sk->pctBase + sk->pctPerRank * rank) / 100;
    int ran = rnd(0, (a->phyDmg / 3) + 1);   /* the original's random(phydmg/3) */

    /* Shield damage is its own argument in the original: normally the raw
       shield-damage stat, zero for the pure-magic bolts, and scaled by a
       shield-breaking skill's rank. */
    int toShd = a->shdDmg;
    if (sk->flags & SKF_PURE_MAGIC) toShd = 0;
    else if (sk->shdPctBase > 0)
        toShd = a->shdDmg * (sk->shdPctBase + sk->shdPctPerRank * rank) / 100;

    bool shieldLayer = (t->shd > 0) && !(sk->flags & SKF_IGNORE_SHD) &&
                       sk->dmgType != DMG_PURE;
    if (shieldLayer) {
        float d = (float)(cut(phy, t->shdPhyDef) + cut(mag, t->shdMagDef) +
                          cut(str, t->shdPhyDef) + toShd);
        if (t->guarding) d *= 0.5f;
        h.toShield = (int)(d + 0.5f) + ran;
        if (h.toShield < 1) h.toShield = 1;
        if (h.toShield >= t->shd) {
            h.toShield = t->shd;
            t->shd = 0;
            h.broke = true;
            /* The original spends the whole blow on the guard: once the shield
               breaks the overflow is discarded rather than carried to life. */
        } else {
            t->shd -= h.toShield;
        }
    } else {
        int total;
        if (sk->dmgType == DMG_PURE) total = phy + str + mag;   /* ignores armour */
        else total = cut(phy, t->phyDef) + cut(mag, t->magDef) + cut(str, t->phyDef);
        total += ran;
        if (t->guarding) total /= 2;
        if (total < 1) total = 1;
        h.toLife = total;
    }

    if (h.toLife > 0) {
        t->life -= h.toLife;
        t->flash = 1.0f;
        t->shake = 1.0f;
        if (t->life <= 0) { t->life = 0; t->alive = false; h.killed = true; }
    }
    return h;
}

/* ------------------------------------------------------- action pipeline */

typedef struct {
    int   actor, target, skill, rank;
    bool  applied;
    int   strikesLeft;
    float nextStrikeAt;
} Action;

static Action ACT;

static void begin_action(Game *g, int actor, int skillId, int target)
{
    Battle *b = &g->b;
    Combatant *a = slot(b, actor);
    const SkillDef *sk = skill_def(skillId);
    int rank = (skillId < 0) ? 1
             : (a->isHero ? g->p.skillRank[skillId] : 1 + a->level / 6);
    if (rank < 1) rank = 1;
    if (rank < 1) rank = 1;

    ACT.actor = actor; ACT.target = target; ACT.skill = skillId; ACT.rank = rank;
    ACT.applied = false;
    ACT.strikesLeft = (sk->flags & SKF_MULTI) ? 2 : 1;
    ACT.nextStrikeAt = IMPACT_AT;

    a->mana -= sk->manaCost;
    a->eng  -= sk->engCost;
    if (a->mana < 0) a->mana = 0;
    if (a->eng < 0) a->eng = 0;

    a->anim = sk->anim;
    a->animT = 0;
    a->guarding = false;
    b->phase = BP_ACTING;
    b->timer = 0;
}

static void apply_skill(Game *g, Combatant *a, const SkillDef *sk, int rank, int target)
{
    Battle *b = &g->b;
    Vector2 ap = slot_pos(b, ACT.actor);

    /* --- self / support --- */
    if (sk->target == SK_TARGET_SELF) {
        if (sk->flags & SKF_SHIELD_UP) {
            a->guarding = true;
            int mend = a->shdMax / 4 + 5;
            if (a->shd + mend > a->shdMax) mend = a->shdMax - a->shd;
            if (mend > 0) {
                a->shd += mend;
                battle_floater(b, (Vector2){ ap.x, ap.y - 150 }, C_KI2, 22, "+%d shield", mend);
            }
            battle_log(b, "%s braces behind the guard.", a->name);
        }
        if (sk->flags & SKF_HEAL) {
            sound_play(SFX_HEAL);
            /* The original heals a flat amount, not a multiple of magic damage. */
            int heal = sk->flatBase + sk->flatPerRank * rank;
            if (a->life + heal > a->lifeMax) heal = a->lifeMax - a->life;
            a->life += heal;
            battle_floater(b, (Vector2){ ap.x, ap.y - 160 }, C_JADE, 26, "+%d", heal);
            battle_burst(b, (Vector2){ ap.x, ap.y - 70 }, C_JADE, 18, 90, 1);
            battle_log(b, "%s mends %d life.", a->name, heal);
        }
        if (sk->flags & SKF_RESTORE_MP) {
            int m = a->manaMax / 4 + 10, e = 40;
            if (a->mana + m > a->manaMax) m = a->manaMax - a->mana;
            a->mana += m;
            a->eng += e; if (a->eng > a->engMax) a->eng = a->engMax;
            battle_floater(b, (Vector2){ ap.x, ap.y - 160 }, C_KI, 22, "+%d mana", m);
            battle_log(b, "%s breathes and centres.", a->name);
        }
        if (sk->flags & SKF_BUFF_ATK) { a->atkBuff = 1.35f + 0.1f * (rank - 1); a->buffTurns = 3; }
        if (sk->flags & SKF_BUFF_DEF) {
            a->defBuff = 1.6f + 0.2f * (rank - 1); a->buffTurns = 3;
            battle_floater(b, (Vector2){ ap.x, ap.y - 160 }, sk->fx, 22, "defence up");
            battle_log(b, "%s sets a hard stance.", a->name);
        }
        battle_burst(b, (Vector2){ ap.x, ap.y - 60 }, sk->fx, 14, 70, 1);
        return;
    }

    /* --- offensive --- */
    int targets[MAX_FOES + MAX_HEROES], n = 0;
    if (sk->target == SK_TARGET_ALL_FOES) {
        if (a->isHero) { for (int i = 0; i < b->nFoes; i++) if (b->foes[i].alive) targets[n++] = MAX_HEROES + i; }
        else           { for (int i = 0; i < b->nHeroes; i++) if (b->heroes[i].alive) targets[n++] = i; }
    } else {
        if (target >= 0 && slot_live(b, target)) targets[n++] = target;
        else {
            int t = a->isHero ? first_live_foe(b) : first_live_hero(b);
            if (t >= 0) targets[n++] = t;
        }
    }

    int drained = 0;
    for (int i = 0; i < n; i++) {
        Combatant *t = slot(b, targets[i]);
        Vector2 tp = slot_pos(b, targets[i]);
        Hit h = resolve_hit(b, a, t, sk, rank);

        if (h.missed) {
            sound_play(SFX_NO);
            battle_floater(b, (Vector2){ tp.x, tp.y - 150 }, C_PARCH2, 22, "miss");
            battle_log(b, "%s slips aside.", t->name);
            continue;
        }
        if (h.toShield > 0) {
            sound_play(SFX_BLOCK);
            battle_floater(b, (Vector2){ tp.x - 26, tp.y - 170 }, C_KI2, 20, "-%d", h.toShield);
            battle_burst(b, (Vector2){ tp.x, tp.y - 80 }, C_KI2, 10, 130, 0);
        }
        if (h.broke) {
            sound_play(SFX_BREAK);
            battle_log(b, "%s's guard shatters!", t->name);
            battle_floater(b, (Vector2){ tp.x, tp.y - 200 }, C_GOLD, 22, "guard broken");
            battle_burst(b, (Vector2){ tp.x, tp.y - 80 }, C_GOLD, 26, 200, 0);
            b->shake = 0.6f;
            if (t->alive) { t->anim = ANIM_BLOCKBREAK; t->animT = 0; }
        }
        if (h.toLife > 0) {
            sound_play(sk->dmgType == DMG_MAGIC ? SFX_SPELL : (rnd(0,1) ? SFX_SWORD1 : SFX_SWORD2));
            battle_floater(b, (Vector2){ tp.x, tp.y - 150 }, sk->dmgType == DMG_MAGIC ? sk->fx : C_BLOOD2,
                           30, "%d", h.toLife);
            battle_burst(b, (Vector2){ tp.x, tp.y - 70 }, sk->dmgType == DMG_PHYSICAL ? C_BLOOD : sk->fx,
                         18, 190, 0);
            b->shake = 0.45f;
            drained += h.toLife;
            if (t->alive) { t->anim = ANIM_HIT; t->animT = 0; }
        } else if (!h.broke && h.toShield > 0) {
            if (t->alive) { t->anim = ANIM_BLOCK; t->animT = 0; }
        }
        if (sk->flags & SKF_STUN) {
            if (rnd(0, 100) < 35 + rank * 8) {
                t->stunned = 1;
                battle_floater(b, (Vector2){ tp.x, tp.y - 200 }, C_GOLD, 20, "stunned");
            }
        }
        if (h.killed) {
            sound_play(SFX_DIE);
            t->anim = ANIM_DIE; t->animT = 0;
            battle_log(b, "%s falls.", t->name);
            battle_burst(b, (Vector2){ tp.x, tp.y - 60 }, C_BLOOD, 30, 220, 0);
        }
    }

    if ((sk->flags & SKF_DRAIN) && drained > 0) {
        int gain = drained / 3 + (sk->dmgType == DMG_MAGIC ? drained / 6 : 0);
        if (a->life + gain > a->lifeMax) gain = a->lifeMax - a->life;
        if (gain > 0) {
            a->life += gain;
            battle_floater(b, (Vector2){ ap.x, ap.y - 170 }, C_JADE, 22, "+%d", gain);
        }
    }
}

/* ------------------------------------------------------------------- AI */

static int ai_pick_skill(Battle *b, Combatant *c)
{
    int best = 2;              /* Full Strike -- a real attack, never a passive */
    int tries = 0;
    /* Heal when badly hurt and able. */
    for (int i = 0; i < c->aiSkillCount; i++) {
        const SkillDef *sk = &SKILLS[c->aiSkill[i]];
        if (sk->flags & SKF_PASSIVE) continue;
        if ((sk->flags & SKF_HEAL) && c->life < c->lifeMax / 3 && c->mana >= sk->manaCost)
            return c->aiSkill[i];
    }
    while (tries++ < 12) {
        int id = c->aiSkill[rnd(0, c->aiSkillCount - 1)];
        const SkillDef *sk = &SKILLS[id];
        if (sk->flags & SKF_PASSIVE) continue;      /* passives are not actions */
        if (sk->manaCost > c->mana || sk->engCost > c->eng) continue;
        if ((sk->flags & SKF_HEAL) && c->life > c->lifeMax * 3 / 4) continue;
        if (sk->target == SK_TARGET_ALL_FOES && b->nHeroes < 2 && rnd(0, 100) < 50) continue;
        best = id;
        break;
    }
    return best;
}

/* --------------------------------------------------------------- update */

static void end_battle_win(Game *g)
{
    Battle *b = &g->b;
    b->phase = BP_WIN;
    b->timer = 0;
    b->heroes[0].anim = ANIM_VICTORY;
    b->heroes[0].animT = 0;
    /* rewards */
    g->p.gold += b->goldGain;
    for (int i = 0; i < b->nFoes; i++) {
        Combatant *f = &b->foes[i];
        if (rnd(0, 100) < f->dropChance) {
            int pool[64], n = 0;
            for (int it = 1; it < ITEM_COUNT; it++)
                if (ITEMS[it].tier > 0 && ITEMS[it].tier <= f->dropTier &&
                    ITEMS[it].tier >= f->dropTier - 1)
                    pool[n++] = it;
            if (n > 0) {
                int pick = pool[rnd(0, n - 1)];
                if (player_add_item(&g->p, pick) >= 0) b->dropItem = pick;
            }
        }
    }
    sound_play(SFX_COINS);
    if (b->isPortal && g->portalIdx >= 0 && g->portalIdx < 3) {
        int *lvl = &g->p.portalLevel[g->portalIdx];
        if (*lvl == g->portalStage && *lvl < portal_stage_count(g->portalIdx)) (*lvl)++;
    }
    player_gain_exp(g, b->expGain);
    g->p.curLife = b->heroes[0].life;
    g->p.curMana = b->heroes[0].mana;
}

static void start_next_turn(Game *g)
{
    Battle *b = &g->b;

    /* win / lose checks */
    bool anyFoe = false, anyHero = false;
    for (int i = 0; i < b->nFoes; i++) if (b->foes[i].alive) anyFoe = true;
    for (int i = 0; i < b->nHeroes; i++) if (b->heroes[i].alive) anyHero = true;
    if (!anyFoe) { end_battle_win(g); return; }
    if (!anyHero) {
        b->phase = BP_LOSE; b->timer = 0;
        return;
    }

    if (b->orderIdx >= b->orderCount) {
        /* round boundary: regenerate, tick buffs, rebuild the speed order */
        b->turn++;
        for (int i = 0; i < slot_count(b); i++) {
            if (!slot_live(b, i)) continue;
            Combatant *c = slot(b, i);
            c->eng += c->engRate;
            if (c->eng > c->engMax) c->eng = c->engMax;
            c->mana += c->manaMax / 25 + 1;
            if (c->mana > c->manaMax) c->mana = c->manaMax;
            if (c->buffTurns > 0 && --c->buffTurns == 0) { c->atkBuff = 1.0f; c->defBuff = 1.0f; }
        }
        roll_order(b);
    }

    while (b->orderIdx < b->orderCount && !slot_live(b, b->order[b->orderIdx])) b->orderIdx++;
    if (b->orderIdx >= b->orderCount) { start_next_turn(g); return; }

    b->actor = b->order[b->orderIdx];
    Combatant *a = slot(b, b->actor);
    a->guarding = false;

    if (a->stunned > 0) {
        a->stunned--;
        battle_log(b, "%s is reeling and loses the turn.", a->name);
        battle_floater(b, (Vector2){ slot_pos(b, b->actor).x, LINE_Y - 190 }, C_GOLD, 20, "stunned");
        b->orderIdx++;
        b->phase = BP_TURN_END;
        b->timer = 0.5f;
        return;
    }

    if (a->isHero) {
        b->phase = BP_PLAYER_CHOOSE;
        b->menuIdx = 0;
        if (!slot_live(b, b->target)) b->target = first_live_foe(b);
    } else {
        b->phase = BP_TURN_START;
        b->timer = 0.35f;
    }
}

static bool skill_usable(Game *g, Combatant *a, int id)
{
    const SkillDef *sk = &SKILLS[id];
    if (a->isHero && g->p.skillRank[id] <= 0) return false;
    if (a->mana < sk->manaCost) return false;
    if (a->eng < sk->engCost) return false;
    return true;
}

/* Player's command menu. Tabs: 0 = actions, 1 = skills, 2 = items. */
/* The original's bar has no submenus: Attack and the two potions sit beside a
   fixed 8x2 grid of skill buttons, every slot always shown and greyed out
   until its skill is learned.  There is no Guard and no Flee button. */
static void cmd_move(Battle *b, int dx, int dy)
{
    int i = b->menuIdx;
    if (dy) {
        if (i < CMD_GRID0) { b->menuIdx = CMD_GRID0 + (dy > 0 ? 0 : GRID_COLS); return; }
        int s = i - CMD_GRID0, c = s % GRID_COLS, r = s / GRID_COLS;
        r = (r + GRID_ROWS + (dy > 0 ? 1 : -1)) % GRID_ROWS;
        b->menuIdx = CMD_GRID0 + r * GRID_COLS + c;
        return;
    }
    if (!dx) return;
    if (i < CMD_GRID0) {
        int n = i + dx;
        if (n < 0)            b->menuIdx = CMD_GRID0 + GRID_N - 1;   /* wrap right */
        else if (n > 2)       b->menuIdx = CMD_GRID0;
        else                  b->menuIdx = n;
        return;
    }
    int s = i - CMD_GRID0, c = s % GRID_COLS, r = s / GRID_COLS;
    c += dx;
    if (c < 0)               b->menuIdx = CMD_MANAPOT;
    else if (c >= GRID_COLS) b->menuIdx = CMD_ATTACK;
    else                     b->menuIdx = CMD_GRID0 + r * GRID_COLS + c;
}

/* Feedback the original shows over the fighter rather than in a log. */
static void cmd_note(Battle *b, Color col, const char *msg)
{
    battle_floater(b, (Vector2){ HERO_X[0], LINE_Y - 150 }, col, 20, "%s", msg);
}

static bool quaff_life(Game *g)
{
    Battle *b = &g->b;
    Combatant *a = &b->heroes[0];
    if (g->p.lifePots <= 0)      { cmd_note(b, C_PARCH2, "no life potions"); return false; }
    if (a->life >= a->lifeMax)   { cmd_note(b, C_PARCH2, "already whole");   return false; }
    g->p.lifePots--;
    int h2 = a->lifeMax / 3;
    if (a->life + h2 > a->lifeMax) h2 = a->lifeMax - a->life;
    a->life += h2;
    battle_log(b, "%s drinks a life potion (+%d).", a->name, h2);
    battle_floater(b, (Vector2){ HERO_X[0], LINE_Y - 160 }, C_JADE, 26, "+%d", h2);
    return true;
}

static bool quaff_mana(Game *g)
{
    Battle *b = &g->b;
    Combatant *a = &b->heroes[0];
    if (g->p.manaPots <= 0)     { cmd_note(b, C_PARCH2, "no mana potions"); return false; }
    if (a->mana >= a->manaMax)  { cmd_note(b, C_PARCH2, "already full");    return false; }
    g->p.manaPots--;
    int m2 = a->manaMax / 3;
    if (a->mana + m2 > a->manaMax) m2 = a->manaMax - a->mana;
    a->mana += m2;
    battle_log(b, "%s drinks a mana potion (+%d).", a->name, m2);
    battle_floater(b, (Vector2){ HERO_X[0], LINE_Y - 160 }, C_KI, 26, "+%d", m2);
    return true;
}

static void update_player_menu(Game *g)
{
    Battle *b = &g->b;
    Combatant *a = &b->heroes[0];

    if (b->phase == BP_PLAYER_TARGET) {
        int live[MAX_FOES], n = 0;
        for (int i = 0; i < b->nFoes; i++) if (b->foes[i].alive) live[n++] = MAX_HEROES + i;
        if (n == 0) { b->phase = BP_PLAYER_CHOOSE; return; }
        int cur = 0;
        for (int i = 0; i < n; i++) if (live[i] == b->target) cur = i;
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) || IsKeyPressed(KEY_DOWN)) cur = (cur + 1) % n;
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_UP)) cur = (cur + n - 1) % n;
        b->target = live[cur];
        if (IsKeyPressed(KEY_ESCAPE)) { b->phase = BP_PLAYER_CHOOSE; return; }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
            begin_action(g, 0, b->pendingSkill, b->target);
        return;
    }

    if (b->menuIdx < 0 || b->menuIdx >= CMD_N) b->menuIdx = CMD_ATTACK;

    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) cmd_move(b,  1, 0);
    if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) cmd_move(b, -1, 0);
    if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)) cmd_move(b, 0,  1);
    if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)) cmd_move(b, 0, -1);

    if (!(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) return;

    if (b->menuIdx == CMD_ATTACK) {
        b->pendingSkill = ID_ATTACK;
        b->phase = BP_PLAYER_TARGET;
        return;
    }
    if (b->menuIdx == CMD_LIFEPOT || b->menuIdx == CMD_MANAPOT) {
        bool used = (b->menuIdx == CMD_LIFEPOT) ? quaff_life(g) : quaff_mana(g);
        if (!used) return;
        a->anim = ANIM_HEAL; a->animT = 0;
        battle_burst(b, (Vector2){ HERO_X[0], LINE_Y - 70 }, C_JADE, 14, 80, 1);
        b->orderIdx++;
        b->phase = BP_TURN_END;
        b->timer = 0.6f;
        return;
    }

    int id = GRID_SKILL[b->menuIdx - CMD_GRID0];
    if (g->p.skillRank[id] <= 0) { cmd_note(b, C_PARCH2, "not learned"); return; }
    if (!skill_usable(g, a, id)) { cmd_note(b, C_KI, "not enough mana");  return; }
    b->pendingSkill = id;
    if (SKILLS[id].target == SK_TARGET_ONE_FOE) b->phase = BP_PLAYER_TARGET;
    else begin_action(g, 0, id, b->target);
}

void battle_update(Game *g, float dt)
{
    Battle *b = &g->b;

    /* animation + effect bookkeeping runs in every phase */
    for (int i = 0; i < slot_count(b); i++) {
        Combatant *c = slot(b, i);
        c->animT += dt;
        c->flash = approach(c->flash, 0, dt * 3.0f);
        c->shake = approach(c->shake, 0, dt * 2.5f);
        if (!c->alive && c->anim == ANIM_DIE && c->animT > 1.0f) c->anim = ANIM_DEAD;
        if (c->alive && c->animT > ANIM_LEN &&
            (c->anim == ANIM_HIT || c->anim == ANIM_ATTACK || c->anim == ANIM_CAST ||
             c->anim == ANIM_HEAL || c->anim == ANIM_MISS || c->anim == ANIM_BLOCKBREAK)) {
            c->anim = c->guarding ? ANIM_BLOCK : ANIM_STAND;
            c->animT = 0;
        }
    }
    for (int i = 0; i < MAX_FLOATERS; i++) {
        if (b->floats[i].life <= 0) continue;
        b->floats[i].life -= dt;
        b->floats[i].pos.y -= dt * 42;
    }
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &b->parts[i];
        if (p->life <= 0) continue;
        p->life -= dt;
        p->pos.x += p->vel.x * dt;
        p->pos.y += p->vel.y * dt;
        p->vel.y += (p->kind == 1 ? -120 : 420) * dt;
        p->vel.x *= 0.98f;
    }
    b->shake = approach(b->shake, 0, dt * 2.0f);
    if (b->flashScreen > 0) b->flashScreen = approach(b->flashScreen, 0, dt * 2.5f);

    if (g->fadeDir > 0) return;

    switch (b->phase) {
    case BP_INTRO:
        b->timer += dt;
        if (b->timer > 0.5f || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            b->timer = 0;
            start_next_turn(g);
        }
        break;

    case BP_TURN_START:                       /* short pause before AI acts */
        b->timer -= dt;
        if (b->timer <= 0) {
            Combatant *a = slot(b, b->actor);
            int id = ai_pick_skill(b, a);
            int tgt = first_live_hero(b);
            if (tgt < 0) { start_next_turn(g); break; }
            battle_log(b, "%s uses %s.", a->name, SKILLS[id].name);
            begin_action(g, b->actor, id, tgt);
        }
        break;

    case BP_PLAYER_CHOOSE:
    case BP_PLAYER_TARGET:
        update_player_menu(g);
        break;

    case BP_ACTING: {
        b->timer += dt;
        Combatant *a = slot(b, ACT.actor);
        const SkillDef *sk = skill_def(ACT.skill);
        float phase = b->timer / ANIM_LEN;
        if (!ACT.applied && phase >= ACT.nextStrikeAt) {
            apply_skill(g, a, sk, ACT.rank, ACT.target);
            ACT.strikesLeft--;
            if (ACT.strikesLeft > 0) {
                ACT.nextStrikeAt = phase + 0.22f;
                a->animT = 0.12f;               /* replay the swing         */
            } else {
                ACT.applied = true;
            }
        }
        if (b->timer > ANIM_LEN + (ACT.strikesLeft > 0 ? 0.3f : 0.0f)) {
            b->orderIdx++;
            b->phase = BP_TURN_END;
            b->timer = 0.28f;
        }
    } break;

    case BP_TURN_END:
        b->timer -= dt;
        if (b->timer <= 0) start_next_turn(g);
        break;

    case BP_WIN:
        b->timer += dt;
        if (b->timer > 0.6f && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) {
            g->p.curLife = b->heroes[0].life;
            g->p.curMana = b->heroes[0].mana;
            if (b->isPortal) {
                go_scene(g, SCENE_PORTAL);
            } else if (b->isArena) {
                g->p.arenaWave++;
                go_scene(g, SCENE_ARENA);
                g->scene = SCENE_VILLAGE;      /* arena flow returns to town */
                go_scene(g, SCENE_VILLAGE);
            } else {
                go_scene(g, g->p.zone == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
            }
        }
        break;

    case BP_LOSE:
        b->timer += dt;
        if (b->timer > 1.2f) go_scene(g, SCENE_GAMEOVER);
        break;

    case BP_FLED:
        b->timer += dt;
        if (b->timer > 0.5f) {
            g->p.curLife = b->heroes[0].life;
            g->p.curMana = b->heroes[0].mana;
            go_scene(g, g->p.zone == ZONE_VILLAGE ? SCENE_VILLAGE : SCENE_WORLD);
        }
        break;

    default: break;
    }
}

/* ----------------------------------------------------------------- draw */

/* A recessed groove with a gradient fill -- the original's bars are a 56x7
   slot (its cid 1763) holding a two-stop gradient. */
static void bar_slot(float cx, float cy, float frac, Color hi, Color lo)
{
    float w = BW(56.0f), h = BW(7.0f);
    Rectangle r = { cx - w * 0.5f, cy - h * 0.5f, w, h };
    DrawRectangleRec(r, C_GROOVE);
    if (frac > 0) {
        if (frac > 1) frac = 1;
        DrawRectangleGradientV((int)(r.x + 1), (int)(r.y + 1),
                               (int)((r.width - 2) * frac), (int)(r.height - 2), hi, lo);
    }
    DrawRectangleLinesEx(r, 1, (Color){ 199, 199, 199, 110 });
}

/* The head-and-shoulders plate the original keeps at the outside of each row. */
static void portrait(const Combatant *c, float cx, float cy)
{
    float w = BW(62.0f), h = BW(58.0f);
    Rectangle r = { cx - w * 0.5f, cy - h * 0.5f, w, h };
    DrawRectangleRec(r, (Color){ 30, 26, 23, 255 });
    gfx_scissor((Rectangle){ r.x + 1, r.y + 1, r.width - 2, r.height - 2 });
    Combatant t = *c;
    t.anim = ANIM_STAND; t.animT = 0;
    art_draw_puppet(&t, (Vector2){ cx, r.y + h * 1.22f }, 1.0f, 0.0f, 1.30f);
    EndScissorMode();
    DrawRectangleLinesEx(r, 1.5f, C_PLATE_LN);
}

/* One combatant's strip: portrait, name, life number and bar, shield bar and
   number -- heroes reading left to right, enemies mirrored. */
static void status_row(const Combatant *c, bool hero, int row, bool targeted, float t)
{
    if (!c->alive && c->life <= 0 && c->lifeMax <= 0) return;
    const float nameY = row ? 19.6f : 65.2f;
    const float numY  = row ? 38.5f : 83.2f;
    const float barY  = row ? 44.8f : 90.9f;
    const float portY = hero ? (row ? 31.5f : 84.8f) : (row ? 30.4f : 83.8f);

    /* the plate behind the strip */
    float px0 = hero ? 19.3f : 332.8f, px1 = hero ? 278.8f : 585.5f;
    Rectangle plate = { BX(px0), BY(nameY - 18.0f), BW(px1 - px0), BW(56.0f) };
    DrawRectangleRec(plate, C_PLATE);
    DrawRectangleLinesEx(plate, 1.5f, targeted ? C_GOLD : C_PLATE_LN);

    portrait(c, BX(hero ? 59.0f : 547.0f), BY(portY));

    char buf[48];
    float nameX = hero ? 87.0f : 332.0f;
    ui_text(c->name, BX(nameX), BY(nameY), 19, C_PLATE_HI);
    snprintf(buf, sizeof buf, "%d", c->life < 0 ? 0 : c->life);
    ui_text(buf, BX(hero ? 87.4f : 335.4f), BY(numY), 19, C_LIFE_TXT);
    bar_slot(BX(hero ? 162.0f : 409.9f), BY(barY),
             c->lifeMax > 0 ? (float)c->life / c->lifeMax : 0, C_LIFE_HI, C_LIFE_LO);
    bar_slot(BX(hero ? 218.7f : 466.9f), BY(barY),
             c->shdMax > 0 ? (float)c->shd / c->shdMax : 0, C_MANA_HI, C_MANA_LO);
    snprintf(buf, sizeof buf, "%d", c->shd < 0 ? 0 : c->shd);
    ui_text(buf, BX(hero ? 240.9f : 488.9f), BY(numY), 19, C_SHD_TXT);

    if (!c->alive) {
        DrawRectangleRec(plate, (Color){ 10, 8, 12, 150 });
        ui_text("down", BX(nameX), BY(numY), 19, C_PARCH2);
    }
    (void)t;
}

/* A command button: the original draws them as plain discs. */
static void round_button(float cx, float cy, bool sel, bool enabled, float t)
{
    float r = BW(20.0f);
    DrawCircle((int)cx, (int)cy, r,
               enabled ? (Color){ 56, 48, 40, 255 } : (Color){ 34, 31, 29, 235 });
    DrawCircleLines((int)cx, (int)cy, r, enabled ? C_PLATE_LN : (Color){ 60, 54, 48, 255 });
    if (sel) {
        float pulse = 0.65f + 0.35f * sinf(t * 5.0f);
        DrawCircleLines((int)cx, (int)cy, r,       Fade(C_GOLD, pulse));
        DrawCircleLines((int)cx, (int)cy, r - 1.5f, Fade(C_GOLD, pulse));
        DrawCircleLines((int)cx, (int)cy, r + 1.5f, Fade(C_GOLD, pulse * 0.5f));
    }
}


void battle_draw(Game *g)
{
    Battle *b = &g->b;
    Combatant *h = &b->heroes[0];
    float t = g->time;
    gfx_shake_begin(frnd(-1, 1) * b->shake * 9, frnd(-1, 1) * b->shake * 7);
    art_draw_battle_bg(b->bgStyle, t);

    /* fighters, back row first */
    for (int i = b->nFoes - 1; i >= 0; i--)
        art_draw_puppet(&b->foes[i], (Vector2){ FOE_X[i], FOE_Y[i] }, -1.0f,
                        b->foes[i].animT, i == 1 ? 0.94f : 1.0f);
    for (int i = b->nHeroes - 1; i >= 0; i--)
        art_draw_puppet(&b->heroes[i], (Vector2){ HERO_X[i], HERO_Y[i] }, 1.0f,
                        b->heroes[i].animT, i == 1 ? 0.94f : 1.0f);

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &b->parts[i];
        if (p->life <= 0) continue;
        float a = p->life / p->maxLife;
        Color c = p->col;
        c.a = (unsigned char)(255 * (a > 1 ? 1 : a));
        DrawCircleV(p->pos, p->size * (0.4f + a * 0.6f), c);
    }
    gfx_shake_end();

    /* The target marker sits over the enemy, where the original's pointer clip
       does, rather than on a nameplate. */
    if (b->phase == BP_PLAYER_TARGET) {
        int fi = b->target - MAX_HEROES;
        if (fi >= 0 && fi < b->nFoes && b->foes[fi].alive) {
            /* The original's pointer sits at a fixed stage point, but that
               offset is relative to its own clip registration, which is not
               centred the way our puppets are -- so keep the intent (just over
               the target's head) and anchor it to the fighter. */
            float px = FOE_X[fi];
            float py = FOE_Y[fi] - BW(49.0f) + sinf(t * 6.0f) * 4.0f;
            /* raylib culls a clockwise triangle, and screen y runs down */
            DrawTriangle((Vector2){ px, py }, (Vector2){ px + 13, py - 18 },
                         (Vector2){ px - 13, py - 18 }, C_GOLD);
            DrawTriangle((Vector2){ px, py + 2 }, (Vector2){ px + 15, py - 20 },
                         (Vector2){ px - 15, py - 20 }, Fade(C_GOLD, 0.35f));
        }
    }

    /* floating counters -- the original's per-fighter message line */
    for (int i = 0; i < MAX_FLOATERS; i++) {
        Floater *f = &b->floats[i];
        if (f->life <= 0) continue;
        float a = f->life / f->t;
        Color c = f->col;
        c.a = (unsigned char)(255 * (a > 0.7f ? 1 : a / 0.7f));
        float sz = f->size * (1.0f + (1 - a) * 0.25f);
        ui_text_c(f->text, f->pos.x + 1, f->pos.y + 1, sz, (Color){ 10, 8, 12, c.a });
        ui_text_c(f->text, f->pos.x, f->pos.y, sz, c);
    }

    /* ------------------------------------------------------ status band */
    for (int i = b->nHeroes - 1; i >= 0; i--)
        status_row(&b->heroes[i], true, i, false, t);
    for (int i = b->nFoes - 1; i >= 0; i--)
        status_row(&b->foes[i], false, i, b->phase == BP_PLAYER_TARGET
                                         && b->target == MAX_HEROES + i, t);

    /* ------------------------------------------------------ command bar */
    {
        Rectangle outer = { BX(9.0f), BY(331.1f), BW(582.0f), BW(112.0f) };
        Rectangle inner = { BX(25.9f), BY(344.1f), BW(537.5f), BW(92.4f) };
        DrawRectangleRec(outer, (Color){ 115, 89, 66, 245 });
        DrawRectangleLinesEx(outer, 2, (Color){ 175, 153, 133, 220 });
        DrawRectangleRec(inner, (Color){ 44, 40, 36, 250 });
        DrawRectangleLinesEx(inner, 1.5f, (Color){ 78, 63, 50, 255 });

        bool live = (b->phase == BP_PLAYER_CHOOSE);
        int sel = live ? b->menuIdx : -1;

        /* the three round buttons on the left */
        const char *LAB[3] = { "Attack", "Life P.", "Mana P." };
        const float LCX[3] = { 46.3f, 93.1f, 141.2f };
        for (int i = 0; i < 3; i++) {
            float cx = BX(LCX[i]), cy = BY(360.7f);
            bool en = (i == 0) || (i == 1 ? g->p.lifePots > 0 : g->p.manaPots > 0);
            round_button(cx, cy, sel == i, en, t);
            if (i == 0) {
                float r = BW(13.0f);
                DrawLineEx((Vector2){ cx - r * 0.6f, cy + r * 0.7f },
                           (Vector2){ cx + r * 0.7f, cy - r * 0.7f }, 3.2f,
                           en ? C_STEEL : (Color){ 80, 76, 70, 255 });
                DrawLineEx((Vector2){ cx - r * 0.8f, cy + r * 0.35f },
                           (Vector2){ cx - r * 0.25f, cy + r * 0.9f }, 3.0f,
                           en ? (Color){ 150, 116, 70, 255 } : (Color){ 80, 76, 70, 255 });
            } else {
                Color pc = (i == 1) ? (Color){ 190, 48, 48, 255 } : (Color){ 46, 132, 190, 255 };
                if (!en) pc = (Color){ 74, 70, 66, 255 };
                float r = BW(9.0f);
                DrawCircle((int)cx, (int)(cy + 2), r, pc);
                DrawRectangle((int)(cx - r * 0.42f), (int)(cy - r * 1.5f),
                              (int)(r * 0.84f), (int)(r * 0.9f), art_shade(pc, 0.7f));
                char n[8];
                snprintf(n, sizeof n, "x%d", i == 1 ? g->p.lifePots : g->p.manaPots);
                ui_text_c(n, cx, cy + BW(11.0f), 17, en ? C_PARCH : C_PARCH2);
            }
            ui_text_c(LAB[i], cx, BY(380.8f) + 2, 16, sel == i ? C_GOLD : C_PLATE_HI);
        }

        /* mana readout under them */
        char mb[64];
        ui_text("Mana", BX(25.9f), BY(411.2f), 17, C_PLATE_HI);
        snprintf(mb, sizeof mb, "%d", h->mana);
        ui_text(mb, BX(86.2f), BY(414.1f), 18, (Color){ 4, 204, 254, 255 });
        snprintf(mb, sizeof mb, "/ %d", h->manaMax);
        ui_text(mb, BX(134.5f), BY(414.2f), 18, C_PARCH2);
        {
            Rectangle mr = { BX(27.6f), BY(431.6f), BW(134.3f), BW(5.8f) };
            DrawRectangleRec(mr, C_GROOVE);
            float f = h->manaMax > 0 ? (float)h->mana / h->manaMax : 0;
            DrawRectangleGradientV((int)(mr.x + 1), (int)(mr.y + 1),
                                   (int)((mr.width - 2) * f), (int)(mr.height - 2),
                                   C_MANA_HI, C_MANA_LO);
            DrawRectangleLinesEx(mr, 1, (Color){ 199, 199, 199, 110 });
        }

        /* the 8x2 skill grid */
        for (int i = 0; i < GRID_N; i++) {
            int id = GRID_SKILL[i];
            const SkillDef *sk = &SKILLS[id];
            float cx = BX(231.5f + (i % GRID_COLS) * 45.0f);
            float cy = BY((i / GRID_COLS) ? 415.6f : 375.6f);
            bool known = g->p.skillRank[id] > 0;
            bool en = known && skill_usable(g, h, id);
            round_button(cx, cy, sel == CMD_GRID0 + i, en, t);
            art_skill_glyph(sk, cx, cy, BW(9.5f), en);
            if (known) {
                char rk[8];
                snprintf(rk, sizeof rk, "%d", g->p.skillRank[id]);
                ui_text(rk, cx + BW(11.0f), cy + BW(5.0f), 15,
                        en ? C_GOLD : (Color){ 110, 104, 96, 255 });
            }
        }

        /* the rollover readout the original prints above the grid */
        if (live && b->menuIdx >= CMD_GRID0) {
            const SkillDef *sk = &SKILLS[GRID_SKILL[b->menuIdx - CMD_GRID0]];
            char cost[32];
            ui_text("Mana Cost: ", BX(245.6f), BY(336.8f), 17, C_PLATE_HI);
            snprintf(cost, sizeof cost, "%d", sk->manaCost);
            ui_text(cost, BX(314.4f), BY(336.8f), 17, C_GOLD);
            ui_text(sk->name, BX(360.0f), BY(336.8f), 17, C_PARCH);
        } else if (live) {
            const char *n = b->menuIdx == CMD_ATTACK ? "Attack"
                          : b->menuIdx == CMD_LIFEPOT ? "Life Potion" : "Mana Potion";
            ui_text(n, BX(245.6f), BY(336.8f), 17, C_PARCH);
        }

        if (b->phase == BP_PLAYER_TARGET)
            ui_text_c("choose a target -- ESC to go back", SCREEN_W / 2, BY(336.8f), 18, C_GOLD);
    }

    /* outcome overlays */
    if (b->phase == BP_WIN) {
        DrawRectangle(0, (int)BY(150.0f), SCREEN_W, (int)BW(150.0f), (Color){ 18, 16, 22, 225 });
        ui_text_c("VICTORY", SCREEN_W / 2, BY(158.0f), 48, C_GOLD);
        char l1[96];
        snprintf(l1, sizeof l1, "%d experience     %d gold", b->expGain, b->goldGain);
        ui_text_c(l1, SCREEN_W / 2, BY(190.0f), 22, C_PARCH);
        if (b->dropItem >= 0) {
            char l2[96];
            snprintf(l2, sizeof l2, "Picked up: %s", ITEMS[b->dropItem].name);
            ui_text_c(l2, SCREEN_W / 2, BY(212.0f), 20, C_JADE);
        }
        char l3[96];
        snprintf(l3, sizeof l3, "%d / %d to the next level", g->p.exp, g->p.expNext);
        ui_text_c(l3, SCREEN_W / 2, BY(234.0f), 19, C_PARCH2);
        ui_text_c("ENTER to continue", SCREEN_W / 2, BY(262.0f), 19, C_GOLD);
    } else if (b->phase == BP_LOSE) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H,
                      (Color){ 0, 0, 0, (unsigned char)(160 * (b->timer > 1 ? 1 : b->timer)) });
        ui_text_c("DEFEAT", SCREEN_W / 2, 300, 56, C_BLOOD2);
    }
}
