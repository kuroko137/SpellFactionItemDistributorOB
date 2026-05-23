About this mod
OBSE framework for dynamically distributing items, spells, factions, and AI packages on the fly during runtime.

Spell Faction Item Distributor

Inspired by Spell Perk Item Distributor by po3 for Skyrim, this mod was built upon my other po3 port (Base Object Swapper), and uses similar syntax. Using SFID you can distribute Items, Leveled Items, Spells, Leveled Spells, Factions, and AI Packages to any NPC dynamically at runtime without the need for .esp patches or record conflicts.

Source

With this mod, you could... 

assign items to NPCs without an .esp
distribute mod added spells or items to increase enemy variety
Add factions to NPCs to function similar to Skyrim's Keywords
Add factions to NPCs to include or exclude dialogue topics
Add AI Packages to NPC to increase schedule immersion
Requirements
xOBSE
EditorIDMapper﻿﻿

How to Use
Similar to BOS, SFID loads config files from Data/OBSE/Plugins/SpellFactionItemDistributor that are in a familiar format:

[FormType|PatternType:PatternToLookFor]
NPCToAddTo|FormToAdd|amount(x),chance(x)


Here are some explanations:
FormType - The type of form you want to add. Currently SFID supports
  ﻿Items - any non-equippable item (supports Leveled Lists)
  Equippables - any item that can be equipped (supports Leveled Lists)
  ﻿Spells - any kind of spell (supports Leveled Lists)
  ﻿Factions - Any faction
  ﻿Packages - any AI package
  ﻿Keywords - Keywords from OBSE Keywords﻿
PatternType - The type of record to check against, currently supports Cell, EditorID, Race, Class, Faction, Item, Name, Mod, Worldspace, Region, Keyword
PatternToLookFor - A word/part of a word or specific formID to compare against when deciding what NPCs to distribute to. As of 1.1.0, SFID now only checks against the specified PatternType. Both the formID, EditorID, and strings can be used here. 'Item' will check if an NPC has a specific item in their inventory. 'Mod' will compare to the file name the NPC comes from. A hyphen/minus sign will do the opposite and apply to every NPC except those that match the pattern. You can also use a comma to distinguish separate conditions that will be evaluated individually. 
As of 1.1.0, SFID now supports chaining multiple patterns using &. See examples below.
NPCToAddTo - Either a specific NPC's formID (both baseID and refID work), or the word ALL. ALL will distribute to any and all NPCs that match the pattern(s).
FormToAdd -  the formID of the object you want to distribute that matches the FormType.
Amount - only used for items; how many of the item you want to add
Chance - number between 0 and 100 representing the chance something will be distributed. If not specified will default to 100 (100%).

NOTE: when using formIDs, the first two digits (the load order prefix) is not needed. You can cut these out and simply put 0x + the last 6 digits + ~TheModFilenameHere.esp. See examples below.

Examples
Adds a "Glarthir" keyword to Glarthir's reference﻿
[Keywords]
0x0x294C8~Oblivion.esm|Keywords(Glarthir)

Adds a Daedric Warhammer to any NPC with the Glarthir keyword
[Items|Keyword:Glarthir]
0x294C8~Oblivion.esm|0x35E79~Oblivion.esm|amount(1)

Adds 500 gold to any NPC who matches the ICTalosPlaza pattern, distributing to all NPCs in cells in that district
[Items|Cell:ICTalosPlaza]
ALL|0xF~Oblivion.esm|amount(500)


Adds a Daedric Longsword to any NPC with the Dremora race
[Items|Race:0x38010~Oblivion.esm]
ALL|0x35E76~Oblivion.esm|amount(1)

Puts Glarthir (by reference) into the Mages' Guild faction.
[Factions]
0x294C8~Oblivion.esm|0x22296~Oblivion.esm

A '&' sign separates conditions that will be evaluated together (AND) while a ',' separates conditions that will be evaluated separately (OR).

Adds an item from the ArenaBlueArmor Leveled List to every NPC in the ICArena district except those already in the Arena faction.
[Equippables|Cell:ICArena&-Faction:ArenaCombatants]
ALL|0x2ACD5~Oblivion.esm|amount(1)
Adds the flames spell to any NPC whose Class contains "Mage" and any NPC in the ArcaneUniversity faction.

[Spells|Class:Mage,Faction:0x4D16F~Oblivion.esm]
ALL|0x3C405~Oblivion.esm

ActorType, Trait, and Stats
ActorType distinguishes between NPCs and creatures. Use "NPC" or "Creature".
Trait checks built-in NPC properties: Female, Male, Unique, Summonable, Leveled, TeamMate, or Dead.
Stats checks numeric actor values using comparison operators (>=, <=, >, <, =, !=). Supported fields include "Level" (actual level with PC offset), "BaseLevel" (base record level), or any base actor value name such as Strength, Intelligence, Speed, etc.

Adds Immunity to Paralysis ability to any actor that is level 30 or higher, or has 500 or more health.
[Spells|Stats:Level>=30,Stats:Health>=500]
ALL|0x143F09~Oblivion.esm|amount(1)

Adds gold to any creature whose EditorID contains "Boss".
[Items|EditorID:Boss&ActorType:Creature]
ALL|0x00000F~Oblivion.esm|amount(300)

Adds all unique NPCs (non-respawning, non-dynamic) to the Imperial Legion faction.
[Factions|Trait:Unique]
ALL|0x18B117~Oblivion.esm

I hope these examples give you some idea of the possible uses for this mod and I looked forward to seeing what you guys can come up with! 

Keep an eye out for my SFID Integrations, similar to my BOS Integrations!

Credits
llde and the xOBSE team
powerofThree for Spell Perk Item Distributor
Bethesda for Oblivion, which often gets neglected in favor of Skyrim or Morrowind. I hope that by continuing to put out mods for my favorite game, others can enjoy and do the same.﻿