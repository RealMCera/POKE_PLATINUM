#!/usr/bin/env python3
from __future__ import annotations
import argparse,json,re
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('root',nargs='?',default='.');p.add_argument('--manifests',default='rare_emerald/manifests');a=p.parse_args()
r=Path(a.root).resolve(); md=r/a.manifests
J=lambda n:json.loads((md/n).read_text())
def patch(path,old,new,mark):
 s=path.read_text()
 if mark in s:return
 if old not in s:raise SystemExit(f'missing Rare Emerald anchor: {path}')
 path.write_text(s.replace(old,new,1))

# Platinum-native chooser and rival logic, Emerald trio.
patch(r/'src/choose_starter/choose_starter_app.c',
'#define NUM_STARTER_OPTIONS 3\n#define STARTER_OPTION_0    SPECIES_TURTWIG\n#define STARTER_OPTION_1    SPECIES_CHIMCHAR\n#define STARTER_OPTION_2    SPECIES_PIPLUP',
'#define NUM_STARTER_OPTIONS 3\n// RARE_EMERALD_STARTERS\n#define STARTER_OPTION_0    SPECIES_TREECKO\n#define STARTER_OPTION_1    SPECIES_TORCHIC\n#define STARTER_OPTION_2    SPECIES_MUDKIP','RARE_EMERALD_STARTERS')
s=r/'src/system_vars.c'
patch(s,'if (playerStarter == SPECIES_TURTWIG) {\n        rivalStarter = SPECIES_CHIMCHAR;\n    } else if (playerStarter == SPECIES_CHIMCHAR) {\n        rivalStarter = SPECIES_PIPLUP;\n    } else {\n        rivalStarter = SPECIES_TURTWIG;\n    }','// RARE_EMERALD_RIVAL\n    if (playerStarter == SPECIES_TREECKO) {\n        rivalStarter = SPECIES_TORCHIC;\n    } else if (playerStarter == SPECIES_TORCHIC) {\n        rivalStarter = SPECIES_MUDKIP;\n    } else {\n        rivalStarter = SPECIES_TREECKO;\n    }','RARE_EMERALD_RIVAL')
patch(s,'if (playerStarter == SPECIES_TURTWIG) {\n        counterpartStarter = SPECIES_PIPLUP;\n    } else if (playerStarter == SPECIES_CHIMCHAR) {\n        counterpartStarter = SPECIES_TURTWIG;\n    } else {\n        counterpartStarter = SPECIES_CHIMCHAR;\n    }','// RARE_EMERALD_COUNTERPART\n    if (playerStarter == SPECIES_TREECKO) {\n        counterpartStarter = SPECIES_MUDKIP;\n    } else if (playerStarter == SPECIES_TORCHIC) {\n        counterpartStarter = SPECIES_TREECKO;\n    } else {\n        counterpartStarter = SPECIES_TORCHIC;\n    }','RARE_EMERALD_COUNTERPART')

# Full Emerald trainer roster. Retain each Platinum slot's existing visual class
# unless a major Emerald boss has a deliberate Gen-IV class mapping.
T=J('emerald_trainers.json')['trainers']; slots=(r/'generated/trainers.txt').read_text().splitlines()[:len(T)]
boss={'ROXANNE':'TRAINER_CLASS_LEADER_ROARK','BRAWLY':'TRAINER_CLASS_LEADER_MAYLENE','WATTSON':'TRAINER_CLASS_LEADER_VOLKNER','FLANNERY':'TRAINER_CLASS_LEADER_CANDICE','NORMAN':'TRAINER_CLASS_LEADER_WAKE','WINONA':'TRAINER_CLASS_LEADER_GARDENIA','TATE_AND_LIZA':'TRAINER_CLASS_LEADER_FANTINA','JUAN':'TRAINER_CLASS_LEADER_BYRON','SIDNEY':'TRAINER_CLASS_ELITE_FOUR_AARON','PHOEBE':'TRAINER_CLASS_ELITE_FOUR_BERTHA','GLACIA':'TRAINER_CLASS_ELITE_FOUR_FLINT','DRAKE':'TRAINER_CLASS_ELITE_FOUR_LUCIAN','WALLACE':'TRAINER_CLASS_CHAMPION_CYNTHIA','ARCHIE':'TRAINER_CLASS_GALACTIC_BOSS','MAXIE':'TRAINER_CLASS_GALACTIC_BOSS'}
ma={'MOVE_SELF_DESTRUCT':'MOVE_SELFDESTRUCT','MOVE_SMOKESCREEN':'MOVE_SMOKE_SCREEN'};ia={'ITEM_SILVER_POWDER':'ITEM_SILVERPOWDER','ITEM_NONE':None}; tm=[]
for i,t in enumerate(T):
 fn=slots[i].removeprefix('TRAINER_').lower()+'.json'; dst=r/'res/trainers/data'/fn
 old=json.loads(dst.read_text()); cls=old['class']
 for k,v in boss.items():
  if k in t['constant']:cls=v;break
 cm=any(x['moves'] for x in t['party']); hi=any(x.get('item') not in (None,'ITEM_NONE') for x in t['party']);party=[]
 for x in t['party']:
  moves=[ma.get(y,y) for y in x['moves']] if cm else None
  item=ia.get(x.get('item'),x.get('item')) if hi else None
  party.append({'species':x['species'],'form':0,'level':x['level'],'item':item,'moves':moves,'iv_scale':x['iv'],'ball_seal':0})
 ai=['AI_FLAG_BASIC'];
 if any(k in t['constant'] for k in boss):ai+=['AI_FLAG_EVAL_ATTACK','AI_FLAG_EXPERT']
 dst.write_text(json.dumps({'name':t['name'].title(),'class':cls,'items':[ia.get(x,x) for x in t['items'] if ia.get(x,x)],'ai_flags':ai,'double_battle':t['double_battle'],'party':party,'messages':[]},indent=4)+'\n')
 tm.append({'slot':i,'platinum':slots[i],'emerald':t['constant'],'file':fn})

# Full Emerald grass/surf/fishing data into deterministic Platinum encounter slots.
E=J('emerald_wild_encounters.json')['encounters']; mes=(r/'res/field/encounters/meson.build').read_text(); block=re.search(r'pl_enc_data_srcs\s*=\s*files\((.*?)\n\)',mes,re.S).group(1); efiles=re.findall(r"'([^']+\.json)'",block)
def five(a):
 if not a:return [{'level_max':0,'level_min':0,'species':'SPECIES_NONE'} for _ in range(5)]
 z=[{'level_max':x['max_level'],'level_min':x['min_level'],'species':x['species']} for x in a['mons'][:5]]
 return z+[{'level_max':0,'level_min':0,'species':'SPECIES_NONE'}]*(5-len(z))
def rods(e):
 f=e.get('fishing_mons')
 if not f:return five(None),five(None),five(None),0
 m=f['mons']; cv=lambda q:[{'level_max':m[i]['max_level'],'level_min':m[i]['min_level'],'species':m[i]['species']} for i in q]+[{'level_max':0,'level_min':0,'species':'SPECIES_NONE'}]*(5-len(q))
 return cv([0,1]),cv([2,3,4]),cv([5,6,7,8,9]),f['encounter_rate']
em=[]
for i,e in enumerate(E):
 land=e.get('land_mons'); le=[{'level':x['min_level'],'species':x['species']} for x in land['mons'][:12]] if land else [];le += [{'level':0,'species':'SPECIES_NONE'}]*(12-len(le)); old,good,sup,fr=rods(e); surf=e.get('water_mons')
 d={'land_rate':land['encounter_rate'] if land else 0,'land_encounters':le,'swarms':['SPECIES_NONE']*2,'day':['SPECIES_NONE']*2,'night':['SPECIES_NONE']*2,'radar':['SPECIES_NONE']*4,'rate_form0':0,'rate_form1':0,'rate_form2':0,'rate_form3':0,'rate_form4':0,'unown_table':0,'ruby':['SPECIES_NONE']*2,'sapphire':['SPECIES_NONE']*2,'emerald':['SPECIES_NONE']*2,'firered':['SPECIES_NONE']*2,'leafgreen':['SPECIES_NONE']*2,'surf_rate':surf['encounter_rate'] if surf else 0,'surf_encounters':five(surf),'old_rod_rate':fr,'old_rod_encounters':old,'good_rod_rate':fr,'good_rod_encounters':good,'super_rod_rate':fr,'super_rod_encounters':sup,'map_category':{'map_type':'field','map_number':0}}
 (r/'res/field/encounters'/efiles[i]).write_text(json.dumps(d,indent=4)+'\n');em.append({'slot':i,'file':efiles[i],'emerald':e['map']})

# Emerald rod probabilities.
w=r/'src/overlay006/wild_encounters.c'
patch(w,'''    case FISHING_TYPE_OLD_ROD:\n        if (roll < 60) {\n            encSlot = 0;\n        } else if (roll < 90) {\n            encSlot = 1;\n        } else if (roll < 95) {\n            encSlot = 2;\n        } else if (roll < 99) {\n            encSlot = 3;\n        } else {\n            encSlot = 4;\n        }\n        break;\n    case FISHING_TYPE_GOOD_ROD:\n        if (roll < 40) {\n            encSlot = 0;\n        } else if (roll < 80) {\n            encSlot = 1;\n        } else if (roll < 95) {\n            encSlot = 2;\n        } else if (roll < 99) {\n            encSlot = 3;\n        } else {\n            encSlot = 4;\n        }\n        break;''','''    case FISHING_TYPE_OLD_ROD:\n        // RARE_EMERALD_FISHING\n        encSlot = (roll < 70) ? 0 : 1;\n        break;\n    case FISHING_TYPE_GOOD_ROD:\n        encSlot = (roll < 60) ? 0 : ((roll < 80) ? 1 : 2);\n        break;''','RARE_EMERALD_FISHING')

# Stable campaign state and Emerald badge semantics.
C=J('emerald_campaign.json')['chapters']; h=r/'include/constants/rare_emerald.h'; lines=['#ifndef POKEPLATINUM_CONSTANTS_RARE_EMERALD_H','#define POKEPLATINUM_CONSTANTS_RARE_EMERALD_H','#include "generated/badges.h"','#include "generated/vars_flags.h"','#define VAR_RE_CHAPTER VAR_UNUSED_0x40C0','#define VAR_RE_STARTER VAR_UNUSED_0x40C9','#define VAR_RE_RIVAL_SPECIES VAR_UNUSED_0x40D2','#define BADGE_RE_STONE BADGE_ID_COAL','#define BADGE_RE_KNUCKLE BADGE_ID_FOREST','#define BADGE_RE_DYNAMO BADGE_ID_COBBLE','#define BADGE_RE_HEAT BADGE_ID_FEN','#define BADGE_RE_BALANCE BADGE_ID_RELIC','#define BADGE_RE_FEATHER BADGE_ID_MINE','#define BADGE_RE_MIND BADGE_ID_ICICLE','#define BADGE_RE_RAIN BADGE_ID_BEACON','enum RareEmeraldChapter {']+[f'    RE_CHAPTER_{x["name"]} = {x["id"]},' for x in C]+['};','#endif'];h.write_text('\n'.join(lines)+'\n')
f=r/'src/field_move_tasks.c';z=f.read_text();
if 'constants/rare_emerald.h' not in z:z=z.replace('\n','#include "constants/rare_emerald.h"\n',1)
for x,y in [('BADGE_ID_FOREST','BADGE_RE_STONE'),('BADGE_ID_COBBLE','BADGE_RE_FEATHER'),('BADGE_ID_FEN','BADGE_RE_BALANCE'),('BADGE_ID_MINE','BADGE_RE_HEAT'),('BADGE_ID_COAL','BADGE_RE_DYNAMO'),('BADGE_ID_BEACON','BADGE_RE_RAIN')]:z=z.replace(f'PlayerHasRequiredBadge(fieldMoveContext, {x})',f'PlayerHasRequiredBadge(fieldMoveContext, {y})',1)
fa='''    if (FieldMoves_IsMoveUsable(fieldMoveContext, FIELD_MOVE_FLASH)) {'''
if 'BADGE_RE_KNUCKLE' not in z:z=z.replace(fa,'''    if (PlayerHasRequiredBadge(fieldMoveContext, BADGE_RE_KNUCKLE) == FALSE) {\n        return FIELD_MOVE_ERROR_BADGE;\n    }\n\n'''+fa,1)
f.write_text(z)

# Build/debug manifests; 518 physical maps are routed in the next geometry phase.
g=r/'rare_emerald/generated';g.mkdir(parents=True,exist_ok=True);maps=J('emerald_maps.json')['maps'];
(g/'trainer_slots.json').write_text(json.dumps(tm));(g/'encounter_slots.json').write_text(json.dumps(em));(g/'status.json').write_text(json.dumps({'engine':'Pokemon Platinum','maps':len(maps),'trainers':len(T),'encounters':len(E),'chapters':len(C),'starters':['Treecko','Torchic','Mudkip'],'geometry':'Magma Ruby donor bridge pending'},indent=2)+'\n')
print(f'[Rare Emerald] {len(maps)} maps / {len(T)} trainers / {len(E)} encounters / {len(C)} chapters generated')
