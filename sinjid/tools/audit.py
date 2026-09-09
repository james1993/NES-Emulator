#!/usr/bin/env python3
"""Audit the remake's tables against what was recovered from the original.

Ground truth lives in tools/groundtruth/ as JSON, extracted from the Flash
file's own bytecode and display list (see docs/RUFFLE.md).  Nothing here needs
the original to run.  Exits non-zero if anything disagrees.
"""
import json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GT   = os.path.join(ROOT, 'tools', 'groundtruth')
SRC  = lambda f: open(os.path.join(ROOT, 'src', f), encoding='utf-8').read()
gt   = lambda f: json.load(open(os.path.join(GT, f), encoding='utf-8'))

# ---------------------------------------------------------------- C parsing
def strip_comments(s):
    s = re.sub(r'/\*.*?\*/', '', s, flags=re.S)
    return re.sub(r'//[^\n]*', '', s)

def struct_fields(header, name):
    """Field names of `typedef struct { ... } NAME;`, in declaration order."""
    s = strip_comments(header)
    end = s.index('} ' + name + ';')
    start = s.rindex('typedef struct {', 0, end)
    out = []
    for line in s[start + len('typedef struct {'):end].split('\n'):
        line = line.strip().rstrip(';')
        if not line or line.startswith('#'):
            continue
        m = re.match(r'(?:const\s+)?[A-Za-z_]\w*\s+(.*)$', line)
        if not m:
            continue
        for f in m.group(1).split(','):
            f = f.strip().lstrip('*')
            f = re.sub(r'\[.*?\]', '', f)
            if f:
                out.append(f)
    return out

def split_top(s, sep=','):
    """Split on `sep` at brace depth 0, respecting strings and escapes."""
    out, buf, depth, i, instr = [], '', 0, 0, False
    while i < len(s):
        c = s[i]
        if instr:
            buf += c
            if c == '\\':
                buf += s[i+1]; i += 2; continue
            if c == '"':
                instr = False
            i += 1; continue
        if c == '"':
            instr = True; buf += c; i += 1; continue
        if c in '{[(':
            depth += 1
        elif c in '}])':
            depth -= 1
        if c == sep and depth == 0:
            out.append(buf.strip()); buf = ''; i += 1; continue
        buf += c; i += 1
    if buf.strip():
        out.append(buf.strip())
    return out

def table_rows(src, decl):
    """Top-level `{...}` rows of an array initialiser."""
    s = strip_comments(src)
    i = s.index(decl)
    i = s.index('{', i)
    depth, j = 0, i
    while True:
        if s[j] == '{': depth += 1
        elif s[j] == '}':
            depth -= 1
            if depth == 0: break
        j += 1
    body = s[i+1:j]
    rows = []
    for part in split_top(body):
        part = part.strip()
        if part.startswith('{') and part.endswith('}'):
            rows.append(split_top(part[1:-1].strip()))
    return rows

def unq(v):
    v = v.strip()
    return v[1:-1] if len(v) >= 2 and v[0] == '"' and v[-1] == '"' else v

def num(v):
    v = v.strip()
    try:    return int(v)
    except Exception: return None

FAIL = []
def check(area, ok, detail):
    if not ok:
        FAIL.append((area, detail))

# --------------------------------------------------------------------- run
hdr  = SRC('game.h')
data = SRC('data.c')

# ---- items -----------------------------------------------------------
ifields = struct_fields(hdr, 'ItemDef')
irows   = table_rows(data, 'const ItemDef ITEMS[]')
items   = [dict(zip(ifields, r)) for r in irows]
by_name = {unq(r['name']): r for r in items}
print("items in code: %d   fields: %d" % (len(items), len(ifields)))

swf_items = {r['name']: r for r in gt('items.json')}
FIELDMAP = {  # our ItemDef field  ->  the original's column
    'phyDmg':'pdmg', 'magDmg':'mdmg', 'shdDmg':'sdmg', 'phyDef':'pdef',
    'magDef':'mdef', 'shdPhyDef':'spdef', 'shdMagDef':'smdef',
    'shdPts':'spts', 'speed':'spd', 'lifeMax':'life', 'manaMax':'mana',
    'strNeed':'strneed',
}
for nm, row in by_name.items():
    s = swf_items.get(nm)
    if not s:
        continue
    for ours, theirs in FIELDMAP.items():
        a, b = num(row.get(ours, '')), s.get(theirs)
        if a is None or b is None:
            continue
        check('items', a == b, "%s.%s ours=%s theirs=%s" % (nm, ours, a, b))
missing = [n for n in swf_items if n not in by_name]
check('items', not missing, "absent from code: %s" % missing)

# ---- buy prices ------------------------------------------------------
for nm, price in gt('prices.json').items():
    row = by_name.get(nm)
    if row is None:
        check('prices', False, "%s not in item table" % nm); continue
    check('prices', num(row['price']) == price,
          "%s price ours=%s theirs=%s" % (nm, num(row['price']), price))

# ---- skills ----------------------------------------------------------
sfields = struct_fields(hdr, 'SkillDef')
srows   = table_rows(data, 'const SkillDef SKILLS[MAX_SKILLS]')
skills  = [dict(zip(sfields, r)) for r in srows]
print("skills in code: %d" % len(skills))
mana, lvl = {}, {}
for _, e in gt('skill_costs.json'):
    i = e.get('skilllabel')
    if 'manareq'  in e: mana[i] = e['manareq']
    if 'levelreq' in e: lvl[i]  = e['levelreq']
for i, sk in enumerate(skills):
    nm = unq(sk['name'])
    if i in mana:
        check('skills', num(sk['manaCost']) == mana[i],
              "%s manaCost ours=%s theirs=%s" % (nm, num(sk['manaCost']), mana[i]))
    if i in lvl:
        check('skills', num(sk['reqLevel']) == lvl[i],
              "%s reqLevel ours=%s theirs=%s" % (nm, num(sk['reqLevel']), lvl[i]))
    check('skills', num(sk['maxRank']) == 10,
          "%s maxRank ours=%s theirs=10" % (nm, num(sk['maxRank'])))

# ---- enemies ---------------------------------------------------------
efields = struct_fields(hdr, 'EnemyDef')
erows   = table_rows(data, 'const EnemyDef ENEMIES[]')
enemies = [dict(zip(efields, r)) for r in erows]
print("enemies in code: %d" % len(enemies))
swf_en = gt('enemies_full.json')
swf_en = {e['name']: e for e in (swf_en if isinstance(swf_en, list) else swf_en.values())}
ecode  = {unq(e['name']): e for e in enemies}
for nm, s in swf_en.items():
    e = ecode.get(nm)
    if e is None:
        check('enemies', False, "%s absent from code" % nm); continue
    for ours, theirs in (('life','life'), ('phyDmg','phydmg'), ('magDmg','magdmg'),
                         ('phyDef','phydef'), ('magDef','magdef'), ('shdPts','shdpts'),
                         ('speed','speed'), ('str','str')):
        a, b = num(e.get(ours, '')), s.get(theirs)
        if a is None or b is None: continue
        check('enemies', a == b, "%s.%s ours=%s theirs=%s" % (nm, ours, a, b))

# ---- classes ---------------------------------------------------------
cls = gt('classes.json')
base = data[data.index('void data_class_base'):]
base = base[:base.index('\n}\n')]
order = ['Balanced', 'Warrior', 'Spell Caster', 'Shadow Ninja']
crows = table_rows(base, 'START[CLASS_COUNT]')
for i, nm in enumerate(order):
    want, got = cls[nm], crows[i]
    keys = ['life','mana','str','speed','phyDmg','magDmg','phyDef','magDef']
    for k, v in zip(keys, got):
        check('classes', num(v) == want[k],
              "%s.%s ours=%s theirs=%s" % (nm, k, num(v), want[k]))

# ---- shops -----------------------------------------------------------
names = [unq(r['name']) for r in items]
shops = gt('shops.json')
for si, key in enumerate(sorted(shops, key=int)):
    want = [shops[key][s] for s in sorted(shops[key], key=int)]
    m = re.search(r'SHOP_ITEMS_%d\[\]\s*=\s*\{([^}]*)\}' % si, strip_comments(data))
    if not m:
        check('shops', False, "SHOP_ITEMS_%d missing" % si); continue
    got = [names[int(x)] for x in re.findall(r'-?\d+', m.group(1)) if int(x) >= 0]
    check('shops', got == want, "Items_%d ours=%s theirs=%s" % (si, got, want))

# ---- sell model ------------------------------------------------------
sell = gt('sell.json')
check('sell', 'data_sell_price' in data, "no data_sell_price")
fn = data[data.index('int data_sell_price'):]
fn = fn[:fn.index('\n}\n')]
check('sell', 'return 3' in fn, "Drink should sell for 3")
check('sell', 'return 0' in fn, "equipment should sell for 0")
check('sell', '/ 2' not in SRC('ui.c').split('data_sell_price')[0][-400:],
      "a half-price sale path survives in ui.c")

# ---- rooms -----------------------------------------------------------
rg = gt('roomgraph.json')
world = SRC('world.c')
exits = table_rows(world, 'static const Exit ROOM_EXITS[]')
want_n = sum(len(v) for v in rg['graph'].values())
travel = [r for r in exits
          if not (len(r) > 8 and num(r[8]) is not None and num(r[8]) >= 0)]
check('rooms', len(travel) == want_n,
      "exit count ours=%d theirs=%d" % (len(travel), want_n))
npcs = table_rows(world, 'static const NpcSeed ROOM_NPCS[]')
want_npc = sum(len(v) for v in rg['npcs'].values())
check('rooms', len(npcs) >= want_npc,
      "npc count ours=%d theirs=%d" % (len(npcs), want_npc))
props = table_rows(SRC('scenery_table.h'), 'static const PropSeed ROOM_PROPS[]')
check('rooms', len(props) == 197, "scenery count ours=%d theirs=197" % len(props))

# ============================ deeper checks =================================
# Counts prove little; these compare the actual values.

# ---- every exit: direction, destination, entry cell -------------------
DIRS = {'EX_UP':'up', 'EX_DOWN':'down', 'EX_LEFT':'left', 'EX_RIGHT':'right'}
ROOMNAME = ['Arena'] + ['Arena%d' % i for i in range(1, 11)]
exit_room = re.search(r'static const int EXIT_ROOM\[\]\s*=\s*\{([^}]*)\}',
                      strip_comments(world))
exit_room = [int(x) for x in re.findall(r'\d+', exit_room.group(1))]
ours = {}
portal_rows = []
for ridx, row in zip(exit_room, exits):
    if len(row) > 8 and num(row[8]) is not None and num(row[8]) >= 0:
        portal_rows.append((ridx, num(row[1]), num(row[2]), num(row[8])))
        continue                      # a portal doorway, not a way to a room
    d = DIRS[row[0].strip()]
    dest = row[3].strip()
    ours.setdefault(ROOMNAME[ridx], []).append((d, dest))
for room, want in rg['graph'].items():
    got = ours.get(room, [])
    want_set = sorted((w[0], 'ZONE_ARENA' + re.sub(r'^Arena', '', w[1]) if w[1] != 'Arena0'
                       else 'ZONE_ARENA0') for w in want)
    got_set = sorted(got)
    check('exits', got_set == want_set,
          "%s ours=%s theirs=%s" % (room, got_set, want_set))

# ---- every NPC: room and cell ----------------------------------------
npc_ours = {}
for row in npcs:
    r = int(row[0]); nm = unq(row[4])
    npc_ours.setdefault(ROOMNAME[r], set()).add((nm, int(row[2]), int(row[3])))
for room, want in rg['npcs'].items():
    got = npc_ours.get(room, set())
    for x, y, nm in want:
        cell = (int(x), int(y))
        hit = any(abs(gx - cell[0]) <= 0 and abs(gy - cell[1]) <= 0
                  for gn, gx, gy in got if gn.replace(' ', '') == nm.replace(' ', ''))
        check('npc-cells', hit, "%s %s expected at %s, got %s" %
              (room, nm, cell, sorted((gx, gy) for gn, gx, gy in got
                                      if gn.replace(' ', '') == nm.replace(' ', ''))))

# ---- skill tree: level gate and prerequisites -------------------------
for i, (lvl_, pre) in ((int(k), v) for k, v in gt('skilltree.json').items()):
    sk = skills[i]; nm = unq(sk['name'])
    check('skilltree', num(sk['reqLevel']) == lvl_,
          "%s reqLevel ours=%s theirs=%s" % (nm, num(sk['reqLevel']), lvl_))
    got = [num(x) for x in split_top(sk['prereq'].strip().lstrip('{').rstrip('}'))]
    check('skilltree', got == pre, "%s prereq ours=%s theirs=%s" % (nm, got, pre))

# ---- merchants open their own panel -----------------------------------
PANEL2SHOP = {'Items_0':'SHOP_ITEMS0','Items_1':'SHOP_ITEMS1','Items_2':'SHOP_ITEMS2',
              'Items_3':'SHOP_ITEMS3','Items_4':'SHOP_ITEMS4','Items_5':'SHOP_ITEMS5',
              'Trade':'SHOP_TRADE'}
for nm, panel in gt('npc_panels.json').items():
    rows_ = [r for r in npcs if unq(r[4]).replace(' ', '') == nm.replace(' ', '')]
    if not rows_:
        check('panels', False, "%s not placed" % nm); continue
    kind, arg = rows_[0][1].strip(), rows_[0][-1].strip()
    if panel in PANEL2SHOP:
        check('panels', arg == PANEL2SHOP[panel],
              "%s should open %s, opens %s" % (nm, PANEL2SHOP[panel], arg))
    elif panel == 'Save':
        check('panels', kind == 'NPC_SAVE', "%s should be the save point, is %s" % (nm, kind))
    elif panel == 'Heal':
        check('panels', kind == 'NPC_HEALER', "%s should be the healer, is %s" % (nm, kind))

# ---- progression -------------------------------------------------------
prog = gt('progression.json')
main = SRC('main.c')
m_exp = re.search(r'int player_exp_for_level\([^)]*\)\s*\{\s*return([^;]*);', main)
check('progression', m_exp is not None and '50' in m_exp.group(1)
      and '*' in m_exp.group(1),
      "exp for next level should be 50 * level, got %r"
      % (m_exp.group(1).strip() if m_exp else None))

# ---- starting state, level-up rewards, battle constants ---------------
rules = gt('rules.json')
st = rules['start']
ng = main[main.index('static void new_player'):]
ng = ng[:ng.index('\n}\n')] if '\n}\n' in ng else ng
for label, pat, want in (
    ('gold',      r'p->gold\s*=\s*(\d+)',        st['gold']),
    ('rests',     r'p->rests\s*=\s*(\d+)',       st['rests']),
    ('lifePots',  r'p->lifePots\s*=\s*(\d+)',    st['lifePots']),
    ('manaPots',  r'p->manaPots\s*=\s*(\d+)',    st['manaPots']),
):
    m = re.search(pat, ng)
    check('start', m is not None and int(m.group(1)) == want,
          "%s ours=%s theirs=%s" % (label, m.group(1) if m else None, want))
check('start', re.search(r'portalLevel\[i\]\s*=\s*1', ng) is not None,
      "each portalLevel should start at 1")

lu = rules['levelUp']
lvlfn = main[main.index('player_gain_exp'):]
lvlfn = lvlfn[:lvlfn.index('\n}\n')]
for label, want in (('life', lu['life']), ('mana', lu['mana']), ('eng', lu['eng'])):
    check('levelup', str(want) in lvlfn, "level-up should add %d %s" % (want, label))
check('levelup', '5' in lvlfn and '%' in lvlfn,
      "every fifth level should give the extra strength, speed and skill point")

defcap = re.search(r'#define\s+DEF_CAP\s+(\d+)', hdr)
check('battle', defcap is not None and int(defcap.group(1)) == rules['battle']['defCap'],
      "DEF_CAP ours=%s theirs=%s" % (defcap.group(1) if defcap else None,
                                     rules['battle']['defCap']))
battle = SRC('battle.c')
check('battle', 'atkSpd / 2' in battle.replace('  ', ' '),
      "the hit roll should be a speed roll")
check('battle', re.search(r'phyDmg\s*/\s*3', battle) is not None,
      "the random damage term should be random(phydmg / 3)")

# ---- scenery table integrity ------------------------------------------
rooms_seen = set()
for r in props:
    ri = int(r[0]); rooms_seen.add(ri)
    x, y, w, h = (int(r[2]), int(r[3]), int(r[4]), int(r[5]))
    check('scenery', 0 <= ri < 11, "prop in room %d" % ri)
    check('scenery', -60 <= x <= 620 and -60 <= y <= 420,
          "prop out of the stage at (%d,%d)" % (x, y))
    check('scenery', w > 0 and h > 0, "prop with an empty box at (%d,%d)" % (x, y))
check('scenery', rooms_seen == set(range(11)),
      "rooms with no scenery: %s" % sorted(set(range(11)) - rooms_seen))

# ---- the three portal doorways ----------------------------------------
want_p = {(p_['room'], p_['cell'][0], p_['cell'][1], p_['index'])
          for p_ in gt('portals.json')['portals']}
check('portals', set(portal_rows) == want_p,
      "ours=%s theirs=%s" % (sorted(portal_rows), sorted(want_p)))

# ---- shop panel layout ------------------------------------------------
sl = gt('shop_layout.json')
ui = SRC('ui.c')
check('shop', 'ui_scene_shop' in ui, "no shop scene")
check('shop', re.search(r'for \(int i = 0; i < 8; i\+\+\)', ui) is not None,
      "the pack grid should hold eight cells (slot4..slot11)")
check('shop', re.search(r'for \(int i = 0; i < 10; i\+\+\)', ui) is not None,
      "the stock grid should hold ten cells (itemstats[20..29])")
check('shop', 'WORN[4]' in ui, "the worn figure should hold four slots")
mi = re.search(r'#define MAX_INVENTORY\s+(\d+)', hdr)
check('shop', mi is not None and int(mi.group(1)) == 8,
      "pack size ours=%s theirs=8" % (mi.group(1) if mi else None))

# ---- battle interface layout ------------------------------------------
bl = gt('battle_layout.json')
bat = SRC('battle.c')

# the skill grid: slot -> skill index, exactly the original's button set
want_grid = [sl_["skill"] for sl_ in bl['commandBar']['grid']['slots']]
m = re.search(r'GRID_SKILL\[GRID_N\]\s*=\s*\{(.*?)\};', bat, re.S)
ours_grid = [int(x) for x in re.findall(r'-?\d+', m.group(1))] if m else []
check('battle', ours_grid == want_grid,
      "skill grid ours=%s theirs=%s" % (ours_grid, want_grid))

g = bl['commandBar']['grid']
check('battle', ('#define GRID_COLS   %d' % g['cols']) in bat,
      "grid should be %d columns" % g['cols'])
check('battle', ('#define GRID_ROWS   %d' % g['rows']) in bat,
      "grid should be %d rows" % g['rows'])

# the three round buttons on the left, and no Guard/Flee among them
for lab in ("Attack", "Life P.", "Mana P."):
    check('battle', '"%s"' % lab in bat, "missing the %s button" % lab)
check('battle', 'case 4:' not in bat and 'canFlee) { battle_log' not in bat,
      "the original's command bar has no Flee button")

# key coordinates, in the original's stage space
for tag, val in (("grid x0", g['x0']), ("grid dx", g['dx']),
                 ("grid row0", g['y'][0]), ("grid row1", g['y'][1])):
    check('battle', ("%.1ff" % val) in bat, "%s should be %.1f" % (tag, val))
for name, xy in bl['fighters'].items():
    if name == 'enemyFacing':
        continue
    check('battle', ("%.1ff" % xy[0]) in bat and ("%.1ff" % xy[1]) in bat,
          "%s should stand at %s" % (name, xy))
st = bl['status']
for tag, val in (("hero life bar", st['hero']['lifeBarCx']),
                 ("hero shield bar", st['hero']['shdBarCx']),
                 ("enemy life bar", st['enemy']['lifeBarCx']),
                 ("enemy shield bar", st['enemy']['shdBarCx']),
                 ("hero portrait", st['hero']['portraitX']),
                 ("enemy portrait", st['enemy']['portraitX'])):
    check('battle', ("%.1ff" % val) in bat, "%s should sit at x=%.1f" % (tag, val))
check('battle', ("%.1ff" % st['barSlot'][0]) in bat and ("%.1ff" % st['barSlot'][1]) in bat,
      "bar slots should be %gx%g" % tuple(st['barSlot']))

# ---- the Elder's save panel -------------------------------------------
sv = gt('save_layout.json')
npcp = gt('npc_panels.json')
check('save', npcp.get('Elder') == 'Save',
      "the Elder's panel type ours=%s theirs=Save" % npcp.get('Elder'))
check('save', 'SCENE_SAVE' in hdr and 'ui_scene_save' in ui,
      "talking to the Elder should raise a panel, not act on contact")
wl = SRC('world.c')
check('save', 'go_panel(g, SCENE_SAVE)' in wl,
      "NPC_SAVE should open the panel")
check('save', 'p->rests--' not in wl,
      "resting is the panel's own button, not something walking up does")
# the panel carries both of the original's actions, separately
check('save', 'save_write(g)' in ui, "the panel should hold the save action")
check('save', 'p->rests--' in ui, "the panel should hold the rest action")
check('save', ('%.2ff' % sv['clipScale']) in ui,
      "the interface clip is placed at %.2f scale" % sv['clipScale'])
for a in sv['actions']:
    for v in a['plate']:
        check('save', ("%.1ff" % v) in ui,
              "%s plate edge %.1f missing" % (a['name'], v))
for v in sv['sheet']:
    check('save', ("%.1ff" % v) in ui, "sheet edge %.1f missing" % v)
for bar in sv['bars']:
    check('save', ("%.1ff" % bar['y']) in ui,
          "%s bar should sit at y=%.1f" % (bar['var'], bar['y']))
check('save', ("%.1ff" % sv['statsLeft']['valueX']) in ui
          and ("%.1ff" % sv['statsRight']['labelX']) in ui
          and ("%.1ff" % sv['statsRight']['valueX']) in ui,
      "the two stat columns should sit at the original's x")
check('save', ("%.1ff" % sv['gold'][0]) in ui, "gold should sit at x=%.1f" % sv['gold'][0])

print()
if not FAIL:
    print("PASS -- deep checks clean too.")
else:
    areas = {}
    for a, d in FAIL:
        areas.setdefault(a, []).append(d)
    print("MISMATCHES: %d" % len(FAIL))
    for a in sorted(areas):
        print("\n[%s] %d" % (a, len(areas[a])))
        for d in areas[a][:20]:
            print("   ", d)
        if len(areas[a]) > 20:
            print("    ... and %d more" % (len(areas[a]) - 20))
sys.exit(1 if FAIL else 0)
