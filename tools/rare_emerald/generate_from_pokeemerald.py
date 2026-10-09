#!/usr/bin/env python3
"""Generate Rare Emerald manifests from a pokeemerald source checkout.

The Emerald repository remains the canonical gameplay/story authority. This
script converts only source data into compact intermediate JSON consumed by the
Platinum-side Rare Emerald importer.
"""
from __future__ import annotations
import argparse, json, re
from pathlib import Path

ap=argparse.ArgumentParser()
ap.add_argument('pokeemerald', type=Path)
ap.add_argument('--out', type=Path, default=Path('rare_emerald/manifests'))
a=ap.parse_args()
em=a.pokeemerald.resolve(); out=a.out.resolve(); out.mkdir(parents=True,exist_ok=True)

def dump(name,obj):
    (out/name).write_text(json.dumps(obj,separators=(',',':'),ensure_ascii=False)+'\n',encoding='utf-8')

# Every canonical Emerald map directory.
mapdirs=sorted(p.name for p in (em/'data/maps').iterdir() if p.is_dir())
dump('emerald_maps.json',{'count':len(mapdirs),'maps':mapdirs})

# Canonical Emerald wild encounters.
wild=json.loads((em/'src/data/wild_encounters.json').read_text(encoding='utf-8'))
encounters=wild['wild_encounter_groups'][0]['encounters']
dump('emerald_wild_encounters.json',{'count':len(encounters),'encounters':encounters})

# Trainer parties.
party_text=(em/'src/data/trainer_parties.h').read_text(encoding='utf-8')
party_pat=re.compile(r'static const struct (TrainerMon\w+)\s+(sParty_\w+)\[\]\s*=\s*\{(.*?)\n\};',re.S)
parties={}
for typ,label,body in party_pat.findall(party_text):
    mons=[]
    for c in re.findall(r'\{\s*(.*?)\n\s*\}',body,re.S):
        if '.species' not in c: continue
        def get(field):
            m=re.search(r'\.'+field+r'\s*=\s*([^,\n}]+)',c)
            return m.group(1).strip() if m else None
        mm=re.search(r'\.moves\s*=\s*\{([^}]*)\}',c,re.S)
        moves=[x.strip() for x in mm.group(1).split(',') if x.strip() and x.strip()!='MOVE_NONE'] if mm else []
        mons.append({'iv':int(get('iv') or 0),'level':int(get('lvl') or 1),'species':get('species'),'item':get('heldItem'),'moves':moves})
    parties[label]={'party_type':typ,'pokemon':mons}

# Trainer metadata.
train_text=(em/'src/data/trainers.h').read_text(encoding='utf-8')
starts=list(re.finditer(r'\[TRAINER_([A-Z0-9_]+)\]\s*=\s*\{',train_text))
trainers=[]
for i,m in enumerate(starts):
    constant='TRAINER_'+m.group(1)
    start=m.end(); end=starts[i+1].start() if i+1<len(starts) else train_text.rfind('};')
    block=train_text[start:end]
    tm=re.search(r'\.trainerName\s*=\s*_\("([^"]*)"\)',block)
    cm=re.search(r'\.trainerClass\s*=\s*([A-Z0-9_]+)',block)
    db=re.search(r'\.doubleBattle\s*=\s*(TRUE|FALSE)',block)
    im=re.search(r'\.items\s*=\s*\{([^}]*)\}',block,re.S)
    pm=re.search(r'\.party\s*=\s*(?:\{\.\w+\s*=\s*)?(sParty_\w+)',block) or re.search(r'(sParty_\w+)\)',block)
    items=[x.strip() for x in im.group(1).split(',') if x.strip() and x.strip()!='ITEM_NONE'] if im else []
    party_label=pm.group(1) if pm else None
    trainers.append({'emerald_id':i,'constant':constant,'name':tm.group(1) if tm else '', 'class':cm.group(1) if cm else None,'double_battle':db.group(1)=='TRUE' if db else False,'items':items,'party_label':party_label,'party':parties.get(party_label,{}).get('pokemon',[]) if party_label else []})
dump('emerald_trainers.json',{'count':len(trainers),'trainers':trainers})

# Stable full-campaign chapter routing. Map geometry is converted separately;
# these chapter IDs stay stable so save files survive later geometry upgrades.
chapters=[
('INTRO',['LittlerootTown','InsideOfTruck']),('LITTLEROOT',['LittlerootTown']),('ROUTE_101',['Route101']),('BIRCH_RESCUE',['Route101']),('STARTER_SELECTED',['Route101']),('FIRST_BATTLE',['Route101']),('BIRCH_LAB',['LittlerootTown_ProfessorBirchsLab']),('ROUTE_103',['OldaleTown','Route103']),('RIVAL_BATTLE',['Route103']),('POKEDEX',['LittlerootTown_ProfessorBirchsLab']),('OLDALE',['OldaleTown']),('PETALBURG',['Route102','PetalburgCity']),('WALLY_TUTORIAL',['PetalburgCity_Gym']),('PETALBURG_WOODS',['Route104','PetalburgWoods']),('RUSTBORO',['RustboroCity']),('BADGE_STONE',['RustboroCity_Gym']),('DEVON_GOODS',['Route116','RusturfTunnel','RustboroCity_DevonCorp_3F']),('DEWFORD',['DewfordTown']),('BADGE_KNUCKLE',['DewfordTown_Gym']),('GRANITE_CAVE',['GraniteCave_1F','GraniteCave_B1F','GraniteCave_B2F','GraniteCave_StevensRoom']),('SLATEPORT',['SlateportCity']),('OCEANIC_MUSEUM',['SlateportCity_OceanicMuseum_1F','SlateportCity_OceanicMuseum_2F']),('MAUVILLE',['MauvilleCity']),('BADGE_DYNAMO',['MauvilleCity_Gym']),('VERDANTURF',['Route117','VerdanturfTown']),('FALLARBOR',['Route111','Route112','Route113','FallarborTown']),('METEOR_FALLS',['Route114','MeteorFalls_1F_1R']),('MT_CHIMNEY',['MtChimney']),('LAVARIDGE',['JaggedPass','LavaridgeTown']),('BADGE_HEAT',['LavaridgeTown_Gym_1F','LavaridgeTown_Gym_B1F']),('PETALBURG_GYM',['PetalburgCity_Gym']),('BADGE_BALANCE',['PetalburgCity_Gym']),('SURF_UNLOCKED',['PetalburgCity']),('WEATHER_INSTITUTE',['Route118','Route119','Route119_WeatherInstitute_1F','Route119_WeatherInstitute_2F']),('FORTREE',['FortreeCity']),('BADGE_FEATHER',['FortreeCity_Gym']),('MT_PYRE',['Route120','Route121','MtPyre_1F','MtPyre_Exterior','MtPyre_Summit']),('MAGMA_HIDEOUT',['MagmaHideout_1F','MagmaHideout_2F_1R','MagmaHideout_2F_2R','MagmaHideout_3F_1R','MagmaHideout_3F_2R','MagmaHideout_4F','MagmaHideout_B1F']),('AQUA_HIDEOUT',['AquaHideout_1F','AquaHideout_B1F','AquaHideout_B2F']),('LILYCOVE',['LilycoveCity']),('MOSSDEEP',['MossdeepCity']),('BADGE_MIND',['MossdeepCity_Gym']),('SPACE_CENTER',['MossdeepCity_SpaceCenter_1F','MossdeepCity_SpaceCenter_2F']),('DIVE_UNLOCKED',['MossdeepCity_StevensHouse']),('SEAFLOOR_CAVERN',['Underwater_Route128','SeafloorCavern_Entrance','SeafloorCavern_Room1','SeafloorCavern_Room2','SeafloorCavern_Room3','SeafloorCavern_Room4','SeafloorCavern_Room5','SeafloorCavern_Room6','SeafloorCavern_Room7','SeafloorCavern_Room8','SeafloorCavern_Room9']),('GROUDON_KYOGRE_CRISIS',['SeafloorCavern_Room9','SootopolisCity']),('SOOTOPOLIS_CRISIS',['SootopolisCity']),('SKY_PILLAR',['SkyPillar_Outside','SkyPillar_1F','SkyPillar_2F','SkyPillar_3F','SkyPillar_4F','SkyPillar_5F','SkyPillar_Top']),('RAYQUAZA_AWAKENED',['SkyPillar_Top','SootopolisCity']),('SOOTOPOLIS_RESOLVED',['SootopolisCity']),('BADGE_RAIN',['SootopolisCity_Gym_1F']),('VICTORY_ROAD',['EverGrandeCity','VictoryRoad_1F','VictoryRoad_B1F','VictoryRoad_B2F']),('ELITE_FOUR',['EverGrandeCity_PokemonLeague_1F','EverGrandeCity_SidneysRoom','EverGrandeCity_PhoebesRoom','EverGrandeCity_GlaciasRoom','EverGrandeCity_DrakesRoom']),('CHAMPION_WALLACE',['EverGrandeCity_ChampionsRoom']),('HALL_OF_FAME',['EverGrandeCity_HallOfFame']),('POSTGAME',['LittlerootTown']),('BATTLE_FRONTIER',['BattleFrontier_ReceptionGate','BattleFrontier_OutsideEast','BattleFrontier_OutsideWest']),('COMPLETE',['BattleFrontier_OutsideEast'])]
mapset=set(mapdirs)
chapter_json=[{'id':i,'name':name,'canonical_maps':maps,'all_maps_present_in_pokeemerald':all(x in mapset for x in maps)} for i,(name,maps) in enumerate(chapters)]
dump('emerald_campaign.json',{'chapter_count':len(chapter_json),'chapters':chapter_json})

dump('source_identity.json',{'repository':'RealMCera/POKE_EMERALD','map_count':len(mapdirs),'trainer_count':len(trainers),'wild_encounter_map_count':len(encounters)})
print(f'[Rare Emerald] Emerald manifests: {len(mapdirs)} maps, {len(trainers)} trainers, {len(encounters)} encounter maps, {len(chapter_json)} chapters')
