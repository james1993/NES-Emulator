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

**The three portals.** `HUMAN`, `MONSTER` and `DARK`, each a ladder of the
original's own encounter definitions (20, 12 and 3 stages), with progress kept
per portal in `portallevel`. **Rests are limited** to the original's ten.
**Potions are counters** (`lifepots` / `manapots`, five each at the start), not
inventory items, and are the first two entries in the battle supply menu. The
pack is the original's **28-slot grid**. The **training ground** burns energy
for experience the way the original's does, and **hidden pickups** sit behind
searchable scenery, once each.

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

**The temple is a room graph, not a ladder.** The eleven screens are rooms on
the original's root timeline, reached with `gotoAndStop` on a per-exit
`stagelabel`. Each room carries edge trigger clips; standing on one and
pressing space calls `NewStage(dir, x, y)` and jumps to that exit's own
destination. All twenty exits are reproduced with their entry cell and facing:

```
Arena0 -up->    Arena1
Arena1 -down->  Arena0   -left->  Arena2   -right-> Arena3
Arena3 -up->    Arena4   -left->  Arena5
Arena5 -left->  Arena6   -up->    Arena7
Arena7 -right-> Arena8   -left->  Arena9
Arena9 -up->    Arena10  (gated on portallevel[0] > 20)
```

Horizontal exits keep the row you left on -- the original passes `Math.ceil`
of your position -- while vertical exits pass a fixed column. The entrance
room, labelled both `Arena` and `Arena0`, is the hub; there is no separate
village screen, and this remake no longer invents one.

**`type = 2` marks an occupied cell, not scenery.** `NewStage` resets every
cell to 1 and the stage script then blocks the cells something stands on, so
the scattered type-2 cells are props *and* the figures standing on them; only
the full-width row-3 band is a back wall. 24 of the 34 recovered clip
positions land exactly on a type-2 cell, against 1.8 expected by chance, which
both settles the meaning and independently confirms the pixel-to-cell
conversion. An NPC blocks its own cell here, so the floor under one is cleared
rather than drawn as rock with a figure on top.

**NPCs open an interface; they do not act on contact.** Every character clip
sets `persontype`, `objectnum` and `inventorytype`, and on space -- guarded by
`pause == false`, `talking != true`, `clearstage == false` -- it stops the walk
animation and runs `_root.inventory.gotoAndStop(inventorytype)` with
`_root.inventory._visible = true`, `_root.pause = true`, `_root.hold = true`.
The Healer's `inventorytype` is `"Heal"`, so talking to them opens a Heal panel
and pauses the game; it never deducts gold the moment you walk up. That panel
is reproduced here.

**Per-room scenery is placed, not scattered.** Every object in each room was
read out of the original's display list -- 197 of them across the eleven rooms,
from 11 in the sparsest to 39 in Arena4 -- and each is reproduced at its own
position and footprint, with the doorway, torches, pillars, windows, counters,
shelves, lamps, urns, crates, rocks, bamboo and floor glows identified by
footprint, palette and which rooms they appear in. `src/scenery_table.h` is
generated from that extraction; `art.c` draws every shape procedurally, so the
arrangement is the original's and none of the artwork is.

One caveat on the footprints: a sprite's bounds are the union over all of its
frames, so a box can overstate what is visible at any one moment. Shapes are
anchored within their box rather than filling it.

**The cast is scattered across the rooms**, at the cells its clips occupy:
Elder, Healer and Student at the entrance; Food and Potion Vendors in Arena1;
both Item Vendors in Arena3; the drinkers in Arena4; two more vendors in
Arena5; Scribe, Lady and Relaxing Ninja in Arena6; three statues in Arena7; a
vendor, Dark Ninja and Guard in Arena8; a Meditating Ninja in Arena9; and
Vendor4, a Guard, a Wounded Warrior and Shadow in Arena10. The graph and the
placements are in `docs/roomgraph.json`.

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

* **The second hero.** The engine supports two fighters a side and the original
  tracks `hero1`/`hero2`; only one is ever fielded.
## Seen in the running original, not yet reproduced

The original can be driven under Ruffle (see `docs/RUFFLE.md`), which settled
several things the bytecode alone could not:

* **An exit-direction indicator** sits in the room's top-right corner: a glowing
  plate with arrows for the directions that room can be left in. It changes from
  room to room. Nothing here draws it.
* **The exit trigger lights up** under the player when they stand on it -- a cyan
  marker on the floor. Here an exit is invisible until you press space on it.
* **Each class grants a starting skill**: Balanced/Shurikens, Warrior/Stab,
  Spell Caster/Charge, Shadow Ninja/Shadow Blend. This remake starts you with
  no skill at all.
* **The HUD** is a single bottom bar: portrait, class, gold, level, life/mana/
  energy/exp bars, life- and mana-potion counts with their tallies, and
  Inventory and Skills buttons.

The exit doorway itself is centred on the room's top wall, which matches the
recovered trigger position; the corner indicator is a separate object.

## Out of scope

**Art** is drawn procedurally in `art.c` and **audio is synthesised** in
`sound.c` -- the original's `playSound` vocabulary (sword hits, a blocked blow,
a shattering guard, coins, a picked-up item, a level, a spent point, a refusal,
thunder, a howl, footsteps) reproduced as generated waveforms. No art or audio
asset from the original is used or redistributed. **Dialogue** is written for this remake — the cast, their roles
and their mechanical function are the original's, the words are not.
