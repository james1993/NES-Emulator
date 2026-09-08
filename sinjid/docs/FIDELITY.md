# Fidelity audit

How close this remake is to the 2004 original, **excluding art and dialogue**
(both deliberately original here — see the last section).

Method: the game's ActionScript was disassembled out of the SWF across all four
code surfaces — frame scripts (`DoAction`), sprite init (`DoInitAction`), button
bodies (`DefineButton2`) and instance handlers (`PlaceObject2` ClipActions) —
and the recovered tables were then diffed field-by-field against the shipped C
source. The numbers below are diff results, not claims.

## Verified by diff

| table | result |
| --- | --- |
| Items | 63 / 63 present, **0 stat mismatches** |
| Item prices | 42 / 42 shop prices, **0 mismatches** |
| Enemies | 47 / 47 present, **0 stat mismatches** across 13 fields each |
| Skill tree | 19 / 19, **0 mismatches** on level gates, prerequisites, rank cap, mana cost, passive flag |
| Stage layouts | 11 / 11, **2860 / 2860 cells exact** |

The audit scripts parse the C tables directly, so these can be re-run against
any future edit.

## Exact, from the bytecode

**Shield damage** is its own channel. An attack passes a `dmgtoshd` argument
alongside the three damage components: normally the fighter's raw shield-damage
stat (which items supply -- the Shield Breaker weapon carries 40, Guard Blade
10, Katana 7), zero for the pure-magic bolts, and scaled by
`(shddmg / 100) * (62 + 8 * rank)` for the shield-breaking skill. All three
cases are implemented.

**Combat.** Defence is percentage reduction, `dmg - ceil(dmg/100 * def)`, applied
to three separate components (weapon, strength, magic) each metered by its
matching defence. While shield points remain, the whole blow is spent on them
using the shield's own two percentages plus the attacker's shield damage; when
the guard breaks the overflow is **discarded**, so a broken shield still takes no
life damage that turn. Hit/miss is a speed roll —
`random(atkspd/2) + atkspd/2` against `random(target.speed/2)` — and the damage
spread is the original's `random(phydmg/3)`. Turn order sorts purely on
descending speed.

**Progression.** `expmax = 50 * level`. A level grants +1 stat point, +1 skill
point, +5 life, +5 mana, +3 energy; every fifth level adds +2 Strength, +2 Speed
and a bonus skill point. Leftover experience carries, capped at `expmax - 1`.

**Character.** Four disciplines (Balanced, Warrior, Spell Caster, Shadow Ninja)
differing only by per-level growth in `[speed, toughness, magic, physical]`.
Start: 75 life, 75 mana, 50 energy, 15 Strength, 15 Speed, 75 gold, 50 exp to
level 2, an Iron Knife and nothing else worn.

**Items.** All 63 with their real stats, including the Strength requirement
(`strneed`) that gates every piece of gear.

**Skills.** Nineteen, max rank 10, with the real level gates and prerequisite
graph. Recovered scaling is implemented for ten of them: the percentage attacks
(52%+8%/rank, 60%+5%/rank, 62%+8%/rank against shields), the flat-bonus magic
bolts (`magdmg + 40 + 9/rank` and `+ 7 + 5/rank`), the four passives
(+4, +5 physical, +7 magic, +2 strength per rank), the missing-life scaler and
the percentage-of-target-life scaler. Casting costs are the original's
`manareq` values. Guard and the plain Attack are stances offered alongside the
nineteen rather than members of the table, as in the original.

## Ours, where the original had nothing to copy

* **Scaling for 7 of the 19 skills** — Mend, Ward, Hard Training, Iron Skin,
  Frost Nail, Chain Lightning, Drain Soul and Shadow Rend have no damage script
  or only a partial one, so their magnitudes are ours. Their tree position,
  rank cap, level gate, prerequisites and mana cost are the original's.
* **Skill names and descriptions.**
* **Prices for 21 items** the shops never stock, the effects of the three
  zero-stat rows (Medicine, White Leaves, Mendo's Ring), and five added
  food/drink items.
* **The Guard stance's mechanics** (halves damage, mends shields) and the
  guarding modifiers in the hit roll.
* **Enemy AI** — which skill an enemy picks on a given turn.
* **The village screen's NPC placement.** The cast and their roles are the
  original's; where each one stands is ours, because its hub is laid out on the
  timeline rather than in a stage script.
* **A defence cap of 95%**, to stop total immunity. The original applies
  percentages raw; its highest is 85%, so nothing is actually clipped.

## Present in the original, not implemented here

* **The three portals of twenty stages each** (`HUMAN`, `MONSTER`, `DARK`, over
  Snow / Coast / Grass / Desert). This remake uses the eleven recovered hub
  layouts joined in a line instead. This is the largest structural gap.
* **Limited rests** (`rests = 10`).
* **Potions as counters** (`lifepots` / `manapots`, 5 each at start) — here they
  are ordinary inventory items.
* **The 28-slot `itemstats` inventory grid**, and buying into slots 4–11.
* **A second hero.** The engine supports two per side and the original tracks
  `hero1`/`hero2`; only one is ever fielded here.
* **The training minigame**, which spends energy for experience
  (`engb -= engrate`, `expup += ceil(exprate * 2)`). Energy is consequently
  vestigial in this build, since skills cost mana only.
* **Hidden searchable pickups** (`SearchItem` / `Searched`).
* **One guard-destroying attack** that passes `dmgtoshd = 999` with every other
  component zeroed. It is not tied to any player skill rank in the scripts, so
  it has not been attributed to one here.
* **Sound.** The original calls `playSound` throughout; there is no audio here.
* Saving uses a local file rather than a Flash `SharedObject`.

## Out of scope

**Art** is drawn procedurally in `art.c`; no asset from the original is used or
redistributed. **Dialogue** is written for this remake — the cast, their roles
and their mechanical function are the original's, the words are not.
