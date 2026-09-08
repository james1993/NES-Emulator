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

#define IMPACT_AT   0.46f     /* fraction of the attack animation           */
#define ANIM_LEN    0.85f

static const float HERO_X[MAX_HEROES] = { 330, 210 };
static const float FOE_X[MAX_FOES]    = { 900, 1040 };
static const float LINE_Y             = 540;

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
    if (idx < MAX_HEROES) return (Vector2){ HERO_X[idx], LINE_Y };
    return (Vector2){ FOE_X[idx - MAX_HEROES], LINE_Y };
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
    if (!(sk->flags & SKF_NEVER_MISS)) {
        int spdran1 = rnd(0, a->atkSpd / 2) + a->atkSpd / 2;
        int spdran2 = rnd(0, t->speed / 2);
        if (t->guarding) spdran2 += t->speed / 4;
        if (spdran2 >= spdran1) { h.missed = true; return h; }
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
            int heal = (int)(a->magDmg * (1.8f + 0.35f * (rank - 1))) + 20;
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
            battle_floater(b, (Vector2){ tp.x, tp.y - 150 }, C_PARCH2, 22, "miss");
            battle_log(b, "%s slips aside.", t->name);
            continue;
        }
        if (h.toShield > 0) {
            battle_floater(b, (Vector2){ tp.x - 26, tp.y - 170 }, C_KI2, 20, "-%d", h.toShield);
            battle_burst(b, (Vector2){ tp.x, tp.y - 80 }, C_KI2, 10, 130, 0);
        }
        if (h.broke) {
            battle_log(b, "%s's guard shatters!", t->name);
            battle_floater(b, (Vector2){ tp.x, tp.y - 200 }, C_GOLD, 22, "guard broken");
            battle_burst(b, (Vector2){ tp.x, tp.y - 80 }, C_GOLD, 26, 200, 0);
            b->shake = 0.6f;
            if (t->alive) { t->anim = ANIM_BLOCKBREAK; t->animT = 0; }
        }
        if (h.toLife > 0) {
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
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            begin_action(g, 0, b->pendingSkill, b->target);
        }
        return;
    }

    if (b->menuTab == 0) {
        const int N = 5;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) b->menuIdx = (b->menuIdx + 1) % N;
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) b->menuIdx = (b->menuIdx + N - 1) % N;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            switch (b->menuIdx) {
            case 0:                                   /* Attack             */
                b->pendingSkill = ID_ATTACK;
                b->phase = BP_PLAYER_TARGET;
                break;
            case 1: b->menuTab = 1; b->skillIdx = 0; break;
            case 2: b->menuTab = 2; b->menuIdx = 0; break;
            case 3:                                   /* Guard              */
                begin_action(g, 0, ID_GUARD, 0);
                break;
            case 4:                                   /* Flee               */
                if (!b->canFlee) { battle_log(b, "There is no way out of this one."); break; }
                if (rnd(0, 100) < 45 + a->speed) {
                    battle_log(b, "You break away.");
                    b->phase = BP_FLED; b->timer = 0;
                } else {
                    battle_log(b, "You could not break away!");
                    b->orderIdx++;
                    b->phase = BP_TURN_END; b->timer = 0.4f;
                }
                break;
            }
        }
    } else if (b->menuTab == 1) {
        int ids[MAX_SKILLS], n = 0;
        for (int i = 0; i < MAX_SKILLS; i++)
            if (g->p.skillRank[i] > 0 && !(SKILLS[i].flags & SKF_PASSIVE)) ids[n++] = i;
        if (n == 0) { b->menuTab = 0; return; }
        if (b->skillIdx >= n) b->skillIdx = n - 1;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) b->skillIdx = (b->skillIdx + 1) % n;
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) b->skillIdx = (b->skillIdx + n - 1) % n;
        if (IsKeyPressed(KEY_ESCAPE)) { b->menuTab = 0; return; }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            int id = ids[b->skillIdx];
            if (!skill_usable(g, a, id)) { battle_log(b, "Not enough left in the tank."); return; }
            b->pendingSkill = id;
            if (SKILLS[id].target == SK_TARGET_ONE_FOE) b->phase = BP_PLAYER_TARGET;
            else begin_action(g, 0, id, b->target);
        }
    } else {
        int ids[MAX_INVENTORY], n = 0;
        for (int i = 0; i < g->p.invCount; i++)
            if (ITEMS[g->p.inv[i].def].type == ITEM_CONSUMABLE) ids[n++] = i;
        if (IsKeyPressed(KEY_ESCAPE) || n == 0) { b->menuTab = 0; if (n == 0) battle_log(b, "No supplies left."); return; }
        if (b->menuIdx >= n) b->menuIdx = n - 1;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) b->menuIdx = (b->menuIdx + 1) % n;
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) b->menuIdx = (b->menuIdx + n - 1) % n;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            if (use_consumable(g, ids[b->menuIdx], a)) {
                a->anim = ANIM_HEAL; a->animT = 0;
                battle_burst(b, (Vector2){ HERO_X[0], LINE_Y - 70 }, C_JADE, 14, 80, 1);
                b->menuTab = 0;
                b->orderIdx++;
                b->phase = BP_TURN_END;
                b->timer = 0.6f;
            }
        }
    }
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
            if (b->isArena) {
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

/* One HUD stat: caps label on the left, value on the right, bar underneath. */
static void stat_row(float x, float y, float w, const char *tag, int cur, int max,
                     Color fill, float barH)
{
    char v[32];
    snprintf(v, sizeof v, "%d/%d", cur, max);
    ui_text(tag, x, y, 14, C_PARCH2);
    ui_text(v, x + w - ui_text_w(v, 14), y, 14, C_PARCH);
    ui_bar((Rectangle){ x, y + 16, w, barH }, max > 0 ? (float)cur / max : 0, fill,
           (Color){ 24, 22, 28, 220 }, NULL);
}

static void draw_nameplate(Battle *b, Combatant *c, Vector2 at, bool targeted)
{
    if (!c->alive) return;
    bool hasGuard = c->shdMax > 5;   /* most enemies carry a token 1 point */
    float w = 200, x = at.x - w / 2, y = at.y - 205;
    DrawRectangleRounded((Rectangle){ x, y, w, hasGuard ? 54.0f : 44.0f }, 0.25f, 8,
                         (Color){ 18, 16, 22, 200 });
    if (targeted)
        DrawRectangleRoundedLines((Rectangle){ x, y, w, hasGuard ? 54.0f : 44.0f }, 0.25f, 8,
                                  C_GOLD);
    char nm[48];
    snprintf(nm, sizeof nm, "%s  Lv%d", c->name, c->level);
    ui_text(nm, x + 8, y + 4, 16, C_PARCH);
    ui_bar((Rectangle){ x + 8, y + 23, w - 16, 8 }, (float)c->life / c->lifeMax,
           C_BLOOD, (Color){ 40, 20, 20, 255 }, NULL);
    if (hasGuard)
        ui_bar((Rectangle){ x + 8, y + 34, w - 16, 6 }, (float)c->shd / c->shdMax,
               C_KI, (Color){ 22, 34, 44, 255 }, NULL);
    if (targeted) {
        float bob = sinf((float)GetTime() * 6) * 4;
        Vector2 tip = { at.x, at.y - 168 + bob };
        DrawTriangle((Vector2){ tip.x - 12, tip.y - 16 }, (Vector2){ tip.x + 12, tip.y - 16 },
                     tip, C_GOLD);
    }
}

void battle_draw(Game *g)
{
    Battle *b = &g->b;
    Camera2D cam = { 0 };
    cam.zoom = 1.0f;
    cam.offset = (Vector2){ frnd(-1, 1) * b->shake * 9, frnd(-1, 1) * b->shake * 7 };

    BeginMode2D(cam);
    art_draw_battle_bg(b->bgStyle, g->time);

    /* fighters, back to front */
    for (int i = b->nFoes - 1; i >= 0; i--) {
        Combatant *c = &b->foes[i];
        Vector2 p = { FOE_X[i], LINE_Y - (i == 1 ? 46 : 0) };
        art_draw_puppet(c, p, -1.0f, c->animT, i == 1 ? 0.92f : 1.0f);
    }
    for (int i = 0; i < b->nHeroes; i++) {
        Combatant *c = &b->heroes[i];
        art_draw_puppet(c, (Vector2){ HERO_X[i], LINE_Y }, 1.0f, c->animT, 1.0f);
    }

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &b->parts[i];
        if (p->life <= 0) continue;
        float a = p->life / p->maxLife;
        Color c = p->col;
        c.a = (unsigned char)(255 * (a > 1 ? 1 : a));
        DrawCircleV(p->pos, p->size * (0.4f + a * 0.6f), c);
    }
    EndMode2D();

    /* nameplates and floaters sit above the shake so text stays readable */
    for (int i = 0; i < b->nFoes; i++)
        draw_nameplate(b, &b->foes[i], (Vector2){ FOE_X[i], LINE_Y - (i == 1 ? 46 : 0) },
                       b->phase == BP_PLAYER_TARGET && b->target == MAX_HEROES + i);

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

    /* ------------------------------------------------------------- HUD */
    Combatant *h = &b->heroes[0];
    Rectangle hp = { 20, SCREEN_H - 172, 430, 152 };
    ui_panel(hp, NULL);
    char nm[64];
    snprintf(nm, sizeof nm, "%s   Lv%d %s", h->name, h->level, CLASS_NAMES[g->p.cls]);
    ui_text(nm, hp.x + 16, hp.y + 8, 20, C_GOLD);
    float cw = hp.width - 32;
    stat_row(hp.x + 16, hp.y + 38, cw, "LIFE", h->life, h->lifeMax, C_BLOOD, 16);
    stat_row(hp.x + 16, hp.y + 76, cw, "MANA", h->mana, h->manaMax, C_KI, 12);
    stat_row(hp.x + 16, hp.y + 110, cw * 0.47f, "ENERGY", h->eng, h->engMax, C_GOLD, 10);
    if (h->shdMax > 5)
        stat_row(hp.x + 16 + cw * 0.53f, hp.y + 110, cw * 0.47f, "GUARD",
                 h->shd, h->shdMax, C_STEEL, 10);

    /* battle log, in its own panel between the HUD and the commands */
    Rectangle lp = { 466, SCREEN_H - 172, 470, 152 };
    ui_panel(lp, NULL);
    for (int i = 0; i < b->logCount; i++) {
        float a = 0.42f + 0.58f * (i + 1) / (float)b->logCount;
        ui_text(b->log[i], lp.x + 14, lp.y + 12 + i * 23, 17,
                (Color){ 206, 192, 164, (unsigned char)(255 * a) });
    }

    /* command panel */
    if (b->phase == BP_PLAYER_CHOOSE || b->phase == BP_PLAYER_TARGET) {
        Rectangle cp = { SCREEN_W - 330, SCREEN_H - 300, 310, 280 };
        ui_panel(cp, b->menuTab == 0 ? "COMMAND" : (b->menuTab == 1 ? "SKILLS" : "SUPPLIES"));
        if (b->menuTab == 0) {
            const char *opts[5] = { "Attack", "Skills", "Items", "Guard", "Flee" };
            for (int i = 0; i < 5; i++) {
                bool en = !(i == 4 && !b->canFlee);
                ui_button((Rectangle){ cp.x + 14, cp.y + 44 + i * 44, cp.width - 28, 38 },
                          opts[i], b->menuIdx == i && b->phase == BP_PLAYER_CHOOSE, en);
            }
        } else if (b->menuTab == 1) {
            int ids[MAX_SKILLS], n = 0;
            for (int i = 0; i < MAX_SKILLS; i++)
            if (g->p.skillRank[i] > 0 && !(SKILLS[i].flags & SKF_PASSIVE)) ids[n++] = i;
            int top = b->skillIdx - 4; if (top < 0) top = 0;
            for (int i = top; i < n && i < top + 5; i++) {
                const SkillDef *sk = &SKILLS[ids[i]];
                char lab[64];
                if (sk->manaCost) snprintf(lab, sizeof lab, "%s  %dmp", sk->name, sk->manaCost);
                else if (sk->engCost) snprintf(lab, sizeof lab, "%s  %den", sk->name, sk->engCost);
                else snprintf(lab, sizeof lab, "%s", sk->name);
                ui_button((Rectangle){ cp.x + 14, cp.y + 44 + (i - top) * 44, cp.width - 28, 38 },
                          lab, b->skillIdx == i, skill_usable(g, h, ids[i]));
            }
            if (n > 0) {
                const SkillDef *sel = &SKILLS[ids[b->skillIdx < n ? b->skillIdx : 0]];
                Rectangle tip = { 466, SCREEN_H - 300, 470, 120 };
                DrawRectangleRounded(tip, 0.12f, 8, (Color){ 18, 16, 22, 232 });
                DrawRectangleRoundedLines(tip, 0.12f, 8, (Color){ 122, 98, 46, 200 });
                ui_text(sel->name, tip.x + 14, tip.y + 10, 19, C_GOLD);
                /* wrap the description by hand at ~40 chars */
                const char *d = sel->desc;
                char line[64]; int li = 0, ly = 0;
                for (const char *p = d;; p++) {
                    if (*p == 0 || (li > 34 && *p == ' ')) {
                        line[li] = 0;
                        ui_text(line, tip.x + 14, tip.y + 38 + ly * 22, 17, C_PARCH2);
                        ly++; li = 0;
                        if (*p == 0) break;
                        continue;
                    }
                    if (li < 62) line[li++] = *p;
                }
            }
        } else {
            int ids[MAX_INVENTORY], n = 0;
            for (int i = 0; i < g->p.invCount; i++)
                if (ITEMS[g->p.inv[i].def].type == ITEM_CONSUMABLE) ids[n++] = i;
            int top = b->menuIdx - 4; if (top < 0) top = 0;
            for (int i = top; i < n && i < top + 5; i++) {
                char lab[64];
                snprintf(lab, sizeof lab, "%s  x%d", ITEMS[g->p.inv[ids[i]].def].name,
                         g->p.inv[ids[i]].count);
                ui_button((Rectangle){ cp.x + 14, cp.y + 44 + (i - top) * 44, cp.width - 28, 38 },
                          lab, b->menuIdx == i, true);
            }
            if (n == 0) ui_text("Nothing to use.", cp.x + 18, cp.y + 50, 18, C_PARCH2);
        }
        if (b->phase == BP_PLAYER_TARGET)
            ui_text_c("choose a target -- ESC to go back", SCREEN_W / 2, 120, 20, C_GOLD);
    }

    /* turn order strip */
    {
        float x = SCREEN_W / 2 - 120;
        ui_text("TURN", x - 60, 24, 16, C_PARCH2);
        for (int i = b->orderIdx; i < b->orderCount && i < b->orderIdx + 5; i++) {
            Combatant *c = slot(b, b->order[i]);
            if (!c->alive) continue;
            bool now = (i == b->orderIdx);
            DrawRectangleRounded((Rectangle){ x, 20, 46, 26 }, 0.3f, 6,
                                 now ? (Color){ 198, 160, 74, 220 } : (Color){ 18, 16, 22, 190 });
            char t[8];
            snprintf(t, sizeof t, "%.3s", c->name);
            ui_text_c(t, x + 23, 25, 15, now ? C_INK : C_PARCH2);
            x += 52;
        }
    }

    /* outcome overlays */
    if (b->phase == BP_WIN) {
        DrawRectangle(0, 190, SCREEN_W, 250, (Color){ 18, 16, 22, 215 });
        ui_text_c("VICTORY", SCREEN_W / 2, 210, 52, C_GOLD);
        char l1[96];
        snprintf(l1, sizeof l1, "%d experience     %d gold", b->expGain, b->goldGain);
        ui_text_c(l1, SCREEN_W / 2, 280, 24, C_PARCH);
        if (b->dropItem >= 0) {
            char l2[96];
            snprintf(l2, sizeof l2, "Picked up: %s", ITEMS[b->dropItem].name);
            ui_text_c(l2, SCREEN_W / 2, 312, 22, C_JADE);
        }
        char l3[96];
        snprintf(l3, sizeof l3, "%d / %d to the next level", g->p.exp, g->p.expNext);
        ui_text_c(l3, SCREEN_W / 2, 346, 20, C_PARCH2);
        ui_text_c("ENTER to continue", SCREEN_W / 2, 396, 20, C_GOLD);
    } else if (b->phase == BP_LOSE) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H,
                      (Color){ 0, 0, 0, (unsigned char)(160 * (b->timer > 1 ? 1 : b->timer)) });
        ui_text_c("DEFEAT", SCREEN_W / 2, 300, 56, C_BLOOD2);
    }
}
