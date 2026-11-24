# OpenNeoUA - Parameter Guide

> The public reference for modders: every parameter the engine understands, vanilla Urban Assault and OpenNeoUA extensions, grouped by script file and block.

## Legend

Each parameter is one line: marker, `name = value`, em dash, short description. A line of dashes separates the entries.

- 🔵 **Vanilla**: exists in the original Urban Assault and behaves as in the original.
- 🟢 **OpenNeoUA**: new, added by the engine. A vanilla parameter reused inside an OpenNeoUA-only block is also green.
- 🔵🟢 **Vanilla extended**: an original parameter whose syntax, accepted values or behavior were changed by OpenNeoUA. The name is the same as in vanilla.

## Contents

- [Introduction and general rules](#introduction-and-general-rules)
  - [Compatibility rules](#compatibility-rules)
  - [Value syntax](#value-syntax)
  - [Paths and visual models](#paths-and-visual-models)
  - [Sound pitch, effect radius and fades](#sound-pitch-effect-radius-and-fades)
- [Vehicles (Vehicles.cfg)](#vehicles-vehiclescfg)
  - [Identity, class and production](#identity-class-and-production)
  - [Stats and physics](#stats-and-physics)
  - [Weapons and firing](#weapons-and-firing)
  - [AI behavior](#ai-behavior)
  - [Guns, flaks and radar](#guns-flaks-and-radar)
  - [Host Stations and mounted guns](#host-stations-and-mounted-guns)
  - [Appearance (models, VP, 3DS)](#appearance-models-vp-3ds)
  - [Destruction and effects](#destruction-and-effects)
  - [Buffs](#buffs)
  - [Sounds, palette flashes and shakes](#sounds-palette-flashes-and-shakes)
  - [OpenNeoUA special features](#openneoua-special-features)
- [Weapons (Weapons.cfg)](#weapons-weaponscfg)
  - [Identity and class](#identity-and-class)
  - [Special projectile classes](#special-projectile-classes)
  - [Damage and targets](#damage-and-targets)
  - [Projectile flight and physics](#projectile-flight-and-physics)
  - [Rate of fire and bursts](#rate-of-fire-and-bursts)
  - [AI fire volumes](#ai-fire-volumes)
  - [Energy cost and progressive fire rate](#energy-cost-and-progressive-fire-rate)
  - [Collisions](#collisions)
  - [Appearance (models, VP, 3DS, trails)](#appearance-models-vp-3ds-trails)
  - [Impact effects](#impact-effects)
  - [Debuffs](#debuffs)
  - [Sounds, palette flashes and shakes](#sounds-palette-flashes-and-shakes-1)
  - [OpenNeoUA special features](#openneoua-special-features-1)
- [Buildings (Buildings.cfg)](#buildings-buildingscfg)
  - [Identity and cost](#identity-and-cost)
  - [Stats](#stats)
  - [Briefing model settings for buildings](#briefing-model-settings-for-buildings)
  - [Flaks mounted on the building](#flaks-mounted-on-the-building)
  - [Appearance](#appearance)
  - [External sector files (.sector)](#external-sector-files-sector)
  - [Sounds](#sounds)
- [Bruch (Bruch.cfg)](#bruch-bruchcfg)
- [Buffs (Buffs.cfg)](#buffs-buffscfg)
- [Debuffs (Debuffs.cfg)](#debuffs-debuffscfg)
- [Clouds (Clouds.cfg)](#clouds-cloudscfg)
- [SuperItems (SuperItems.cfg)](#superitems-superitemscfg)
- [Ground Decals (Ground_Decals.cfg)](#ground-decals-ground_decalscfg)
- [Startup and Game Config (Startup.cfg)](#startup-and-game-config-startupcfg)
- [OpenNeoUA.ini](#openneouaini)
  - [Game and debug](#game-and-debug)
  - [AI](#ai)
  - [Combat and physics](#combat-and-physics)
  - [Deformation and craters](#deformation-and-craters)
  - [Player controls](#player-controls)
  - [Host Station](#host-station)
  - [GEM popup](#gem-popup)
  - [Elite units](#elite-units)
  - [Plasma](#plasma)
  - [Audio and effect falloff](#audio-and-effect-falloff)
  - [Graphics](#graphics)
  - [Regen and Drain effects](#regen-and-drain-effects)
  - [World HP and shield bars](#world-hp-and-shield-bars)
  - [User interface](#user-interface)
- [Nucleus.ini](#nucleusini)
  - [Graphics](#graphics-1)
  - [Drivers and leftovers of the original engine](#drivers-and-leftovers-of-the-original-engine)
  - [Audio](#audio)
  - [Network and multiplayer](#network-and-multiplayer)
  - [Game and menu](#game-and-menu)
  - [Palette (color effects)](#palette-color-effects)
- [World.ini](#worldini)
  - [General settings (begin_misc)](#general-settings-begin_misc)
  - [Interface colors (begin_colors)](#interface-colors-begin_colors)
  - [Menu and map backgrounds (begin_bg)](#menu-and-map-backgrounds-begin_bg)
  - [Movies (begin_movies)](#movies-begin_movies)
  - [Vanilla SuperItems (begin_superitem)](#vanilla-superitems-begin_superitem)
  - [Menu sounds and music (begin_shellsounds, begin_shelltracks)](#menu-sounds-and-music-begin_shellsounds-begin_shelltracks)
- [USER.TXT (player profile)](#usertxt-player-profile)
  - [Player data (new_user)](#player-data-new_user)
  - [Controls and keys (new_input)](#controls-and-keys-new_input)
  - [Audio (new_sound)](#audio-new_sound)
  - [Video (new_video)](#video-new_video)
  - [Completed levels and buddies](#completed-levels-and-buddies)
  - [Menu interface (new_shell)](#menu-interface-new_shell)
- [Graphic Profiles](#graphic-profiles)
- [Levels (LDF files)](#levels-ldf-files)
  - [begin_level block](#begin_level-block)
  - [Host Stations (begin_robo)](#host-stations-begin_robo)
  - [Squads (begin_squad)](#squads-begin_squad)
  - [Beam gates (begin_gate)](#beam-gates-begin_gate)
  - [Gems and upgrades (begin_gem)](#gems-and-upgrades-begin_gem)
  - [Enables (begin_enable)](#enables-begin_enable)
  - [Level SuperItems (begin_item)](#level-superitems-begin_item)
  - [Lua API (lua_script)](#lua-api-lua_script)
  - [Briefing and maps](#briefing-and-maps)
  - [Level locations (OpenNeoUA only, in LDF files)](#level-locations-openneoua-only-in-ldf-files)
- [Debug controls (F1-F12)](#debug-controls-f1-f12)
- [Retired and ignored keys](#retired-and-ignored-keys)
- [OpenNeoUA Studio notes](#openneoua-studio-notes)
  - [Models](#models)
  - [Entities and effects](#entities-and-effects)
  - [Gun points](#gun-points)
  - [Levels](#levels)

## Introduction and general rules

OpenNeoUA is an independent, open-source evolution of the Urban Assault UA_source/OpenUA engine. It extends the engine while keeping vanilla data, levels, scripts, saves and game feel working.

This guide is a reference catalog, not a configuration to copy: add only the parameters your mod needs. Example values show the syntax, not a recommended balance. The running engine is the final authority, so test every combination that changes a vanilla system.

### Compatibility rules

- An omitted OpenNeoUA parameter uses the documented fallback or the vanilla behavior. Most extensions are opt-in.
- `0`, absent or invalid usually means "off" or "vanilla"; each entry states its own fallback.
- Invalid values are ignored and logged. A group with the wrong number of parts is ignored.
- Original Urban Assault parameters keep their name and syntax (`robo_gun_pos_x`, `sbact_pos_x`, `dest_fx`, ...).
- New damage parameters use energy names (`energy`, `Blast_energy`, `impact_energy`, ...). Former damage names are not aliases. Existing `damaged_*` settings and vanilla save formats are unchanged.
- Removed or renamed OpenNeoUA parameters have no fallback.
- Units whose collision shapes overlap at Genesis separate in a consistent direction with bounded movement; a brief initial overlap may stay visible.

### Value syntax

- `~` means "random between": `min~max` picks a value in the range (`count = 2~4`, `snd_launch_pitch = 900~1100`). A single value is fixed.
- `|` separates the fields of a group, in the order of the name: `offset_xyz = x|y|z`, `fade_inout = in|out`, `tint = R|G|B|A`, `vp_model = 138|193`, `trigger = detonate|impact_world`.
- Both can combine: `launch_xyz = -15~15|-20|-15~15`. Check each entry for the syntax it supports.
- `_` appears only inside names (`impact_world`, `air_destroyed`). Original Urban Assault parameters keep their underscore syntax (`dest_fx = death_46_0_0_0`) and also accept `|`.
- Compact parameters need every part: `spin_xyz = x|y|z`, `fade_inout = in|out`, `lifetime_minmax = min~max`. The old split names (`spin_x`, `fade_in`, ...) no longer exist.
- `%` is a percentage only where the entry says so; some parameters accept both `5%` and a fixed number.
- Time values are in milliseconds unless stated. "World units" is the engine distance scale (a sector is 1200 x 1200 units).
- Yes/no values: scripts (Vehicles, Weapons, Buildings, Levels and similar) are written with `0` and `1`; configuration files (`OpenNeoUA.ini`, `Nucleus.ini`, `World.ini`, `USER.TXT`) with `yes` and `no`. Original Urban Assault keys keep their original form. Every yes/no value in every file is read the same way: `1`, `yes`, `true`, `on` or any non-zero number is yes; `0`, `no`, `false` or `off` is no; anything else counts as no (an invalid ini value uses the key's default).

### Paths and visual models

- Use `/` in paths. Data-relative paths start from the game's Data folder; many model and sound paths already begin with `Data/`, so never add a second prefix. Example: `Data/Models/3ds/Tracer.3ds`. Old backslashes and older relative paths are still accepted as a fallback.
- Visual priority is `3ds_*` first, then `base_*`, then `vp_*`. A `3ds_*` asset replaces the matching `base_*` and `vp_*` when it loads; a `base_*` asset falls back to its VP.
- A `base_*` path points to one `.BASE` file, the entry point of a model family. The loader resolves KIDS/SKLT, ILBM textures, ANM/VANM animations and embedded assets from the file's folder and its subfolders.

### Sound pitch, effect radius and fades

Any pitch parameter accepts `min~max`, so one event can pick a different pitch each time without duplicating assets. Radius parameters localize sound, palette and shake events in world space; `0` or absent keeps the legacy behavior.

------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `snd_<event>_pitch = 900~1100` — Pitch of a sound event; accepts `min~max`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_<event>_radius = 4000` — Maximum hearing radius of the event. Volume falls linearly to 0 at the limit. 0 or absent = legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_<event>_radius = 4000` — Maximum radius of a palette flash. Intensity falls linearly to 0 at the limit. 0 or absent = legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_<event>_radius = 4000` — Maximum radius of a camera shake. Intensity falls linearly to 0 at the limit. 0 or absent = legacy behavior based on `shk_<event>_mute`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_<event>_fade_inout = 150|300` — Volume fade-in and fade-out of the event sound in milliseconds (format in|out). A one-shot sample fades out over its own length; a looping sound only fades in. If both fades are longer than the sound they shrink together. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_<event>_fade_inout = 150|300` — Fade-in and fade-out of a palette flash in milliseconds (format in|out). The fade-out ends together with `pal_<event>_time`; a looping effect only fades in. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_<event>_fade_inout = 150|300` — Fade-in and fade-out of a camera shake in milliseconds (format in|out). The fade-out ends together with `shk_<event>_time`; a looping effect only fades in. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------

## Vehicles (Vehicles.cfg)

### Identity, class and production

------------------------------------------------------------------------------------------------------------------------
- 🔵 `new_vehicle = 21` — Opens the definition of a new Vehicle with the given numeric ID. If the ID already exists, the prototype is rebuilt from scratch, so every value must be written again.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `modify_vehicle = 21` — Reopens an existing Vehicle so that only the listed values change. Typical use is inside level files and tech-upgrade (begin_gem / begin_action) blocks. Values not mentioned stay untouched.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `end` — Closes the current new_vehicle / modify_vehicle block.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `enable = 1` — Makes the Vehicle available to a faction (1 Resistance, 2 Sulgogar, 3 Mykonian, 4 Taerkasten, 5 Black Sect, 6 Ghorkov, 7 neutral/tutorial). One line per faction; a level uses `begin_enable` instead.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `disable = 1` — Removes the Vehicle from the given faction (same numbers as enable).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `model = tank` — Vehicle class: tank and car (ground), heli, plane, glider and zeppelin (aircraft), ufo (hovering craft), gun (fixed turret), robo (Host Station). Decides the movement model. An unknown value invalidates the whole block.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `name = Tiger_Tank` — Display name of the Vehicle. Underscores are shown as spaces.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `type_icon = A` — Icon glyph key of the unit in lists and on the map, mapped through TYPE_NS.FON into the H_FINDER graphic. Vanilla reads only the first byte, so use a plain ASCII character (letter, digit or a symbol such as `]`). A Unicode symbol or `U+XXXX` (for example `U+2206`) is OpenNeoUA-only: vanilla reads it as `U`, the Radar Station icon. Prefer `U+XXXX`, because a pasted symbol can be corrupted by the editor encoding. `;`, `=`, `#` and `\` cannot stand bare in a script: write `U+003B`, `U+003D`, `U+0023` and `U+005C`. An unreadable icon falls back to `A`; `U+FFFD` is rejected. In TYPE_NS.FON write new keys as a single byte and keep each `U+XXXX` line above the `U` line. OpenNeoUA also reads a second icon page, `Graphics/UnitIcons/H_FINDER_OpenNeoUA.PNG`, defined in `H_FINDER_OpenNeoUA.FON` (same line format: `key x y width`; keys as `U+XXXX` or `#number`). Its keys replace the same keys of TYPE_NS.FON and may be any `U+XXXX`; vanilla never reads that page. If the file is missing, only the original page is used. The value `I` keeps the unit out of the Database.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `map_direction = 1` — 0 hides the view triangle (facing direction) of the unit on the map and minimap, for static units such as spawners. The unit icon stays. Missing or 1 = vanilla.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vo_type = 1` — Selects the voice set used for the unit's spoken messages. It is read as a hexadecimal number.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `wireframe = wireless/tiger.skl` — Wireframe outline of the unit used by the in-game interface (unit list and wireless panel). Path to a skeleton file.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_wireframe = wireless/tigerhud.skl` — Wireframe used for the cockpit / HUD overlay while the player controls the unit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `mg_wireframe = wireless/mg.skl` — Wireframe of the machine gun shown in the interface.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `wpn_wireframe_1 = wireless/wpn1.skl` — Wireframe of weapon 1 shown in the interface.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `wpn_wireframe_2 = wireless/wpn2.skl` — Wireframe of weapon 2 shown in the interface.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `model = cruiser` — Airborne vehicle model with smooth up/down response and comparatively rigid left/right response.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `model = module` — Alternate alias for model = gun; behavior is the same.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `model = mimic` — Mimic vehicle type. Creates a copy of itself using another eligible unit already present on the map as the visual/template source; excludes itself, robo and gun classes. Add at_death_spawn_vehicle to generate units when it dies.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mimic_tint = 60|255|90|255` — RGBA multiplier applied to the visual copied by the Mimic, for both VP and 3DS visuals.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mimic_energy_cost = 20000~30000` — Energy required to create the mimic. Single value = fixed cost; `min~max` = random cost.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_mimic_sample = sounds/sulgi.wav` — Mimic-specific sound used during disguise.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_mimic_volume = 120` — Mimic sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_mimic_pitch = 4500~5500` — Mimic sound pitch. Single value = fixed; `min~max` = random pitch selected on each playback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_mimic_fade_inout = 150|300` — Volume fade-in and fade-out of the mimic sound in milliseconds (format in|out). A one-shot sample fades out over its own length; a looping sound only fades in. If both fades are longer than the sound they shrink together. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `speech_voicepack = Data/Sounds/Speech/Voicepacks/resistance-tank-medium` — Unit event voices inside new_vehicle: point directly to the final pack directory. Direct pack path under Data. Missing or 0 uses vanilla voices.
------------------------------------------------------------------------------------------------------------------------

**Voice events.** `speech_voicepack` points to a folder of WAV files named after the 16 supported events: unit_ready, unit_idle, order_ack_friendly, order_ack_enemy, unit_control_entered, enemy_sector_entered, unit_recovered, enemy_unit_spotted, enemy_host_found, request_support, unit_retreating, unit_lost, enemy_unit_engaged, enemy_unit_destroyed, enemy_host_destroyed, powerstation_captured.

- Numbered variants (`unit_ready_01.wav`, `unit_ready_02.wav`) are picked at random without an immediate repeat.
- A missing file falls back to the vanilla voice (`vo_type`). A Mimic uses the voice pack of the vehicle it copies.
- WAV files may be mono or stereo, 8 or 16 bit PCM; voice filters are not applied. Restart after changing scripts or audio.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `max_active_at_once = 15` — Maximum number of active units of this type, not counting units placed by the level. GEM unlocks can raise it. 0 or absent = no limit (vanilla). The classic GEM popup shows a MAX row only when a limit is set (not in network games).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `add_max_active_at_once = 3` — GEM upgrade value used inside modify_vehicle to increase and persist max_active_at_once.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `production_cost = 150` — Displayed Genesis price for this vehicle. Overrides the vanilla energy-derived price (energy x 2 / 100) without changing hit points. The Genesis list is sorted by this final price. 0 or absent = vanilla behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `building_time = 4.4` — Construction time in real seconds, 0-100, decimals with a dot. 0 completes on the first simulation update; absent, negative, above 100 or invalid = vanilla timing. Vehicles use it for Genesis and the production wait, and their Genesis sound pitch follows it (4.4 = pitch of cost 440, 10 = cost 1000; above 10 only the duration grows). Buildings use it to emerge from the terrain and become operational, with no pitch. `disable_genesis = 1` still creates instantly; preplaced scripted buildings stay instant. Cost, hit points and death plasma are unchanged.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vehicle_description = Heavy assault unit` — Optional vehicle description shown in the Database. If absent, the Description row is not shown.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `purchase_cost = 1000` — Plasma cost to unlock the vehicle in the Workshop. If absent, the vehicle is not offered. Zero allows a free unlock. Purchases and the equipped Host Station are stored in the player profile and carry over to later levels.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `briefing_size = 100` — Size of the unit in the briefing and debriefing preview, 1-10000: larger = bigger model. 0, absent or invalid = classic radius framing. Gameplay geometry, collisions and saves are unchanged.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `briefing_xyz = 0|0|0` — Preview offset in camera axes (X right, Y down, Z away from the camera), independent of `briefing_size`. Applied after screen placement, so it does not rotate with the model. Each axis -100000..100000; an invalid group keeps the previous value; absent or 0|0|0 = classic placement.
------------------------------------------------------------------------------------------------------------------------

### Stats and physics

------------------------------------------------------------------------------------------------------------------------
- 🔵 `energy = 1000` — Hit points (energy) of the Vehicle. In vanilla this value is also the Genesis cost of the unit; production_cost overrides the price without touching the hit points.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shield = 20` — Armor as a percentage from 0 to 100: it reduces incoming damage. 100 makes the unit immune to the regular damage formula.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `mass = 400` — Mass of the Vehicle. Together with force and airconst it decides acceleration and how hard the unit is to push around.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `force = 20000` — Engine thrust. Higher values accelerate the unit faster and raise its top speed together with airconst.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `maxrot = 1.0` — Maximum turn rate of the unit. Higher values let it steer and rotate faster. For `gun_type = radar` and `gun_type = spawner`, it sets continuous rotation around the mount axis in radians per second; zero, negative, or non-finite values stop continuous rotation. `gun_type = spawner` additionally stays immobile when maxrot is not authored at all, instead of inheriting the vanilla default turn rate; `gun_type = radar` keeps that default when maxrot is missing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `airconst = 500` — Air resistance / drag. Higher values brake the unit harder and lower its top speed; top speed is roughly where drag balances force.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `height = 200` — Preferred flight height above the ground for flying classes (heli, plane, glider, zeppelin, ufo).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `radius = 20` — Physical collision radius of the unit. It decides collisions, hit detection and how much space the unit occupies.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `overeof = 10` — Vertical offset used by ground collision: the distance between the unit origin and the ground.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vwr_radius = 20` — Collision radius used by the viewer / player camera while the unit is controlled or observed.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vwr_overeof = 30` — Vertical offset used by the viewer. In practice it sets the camera height when the player enters the unit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `max_pitch = 1.2` — Scales the engine sound pitch with speed. Despite the name it does not tilt the model; it is used by the movement-sound code.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `radar = 2` — Radius, in sectors, of the area the unit reveals around itself on the map (fog of war). 0 means no radar.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `unhide_radar = 1` — Radius, in sectors, in which the unit reveals enemy units that use hidden. Sectors uncovered this way make hidden enemies visible to this faction.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hidden = yes` — Makes the unit invisible to other factions. It is revealed only inside the area covered by another unit's unhide_radar.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_energy = 200` — Upgrade-only (gem / script): adds the value to the current energy instead of replacing it.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_shield = 10` — Upgrade-only: adds the value to the current shield.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_radar = 1` — Upgrade-only: adds the value to the current radar radius.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_unhide_radar = 1` — Upgrade-only: adds the value to unhide_radar.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `disable_collision = 0` — Vehicle and Weapon: 1 disables physical contacts with units, projectiles, terrain and buildings (radius, compound spheres and `collision_shape`). Gravity, lifetime expiry, timed explosions and area damage still work, it grants no invulnerability and map bounds still apply. 0, absent or invalid = normal collisions.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `push_resistance = 0.5` — Resistance to weapon and event push, 0-1 (above 1 is clamped). Push intensity is multiplied by (1 - push_resistance): 0 = full push, 0.5 = half intensity (a quarter of the displacement). Also applies to Host Stations; no use for static guns. Reference values: tiny scouts 0-0.10, light 0.15-0.30, medium 0.30-0.60, heavy 0.50-0.65, super-heavy 0.85-0.90.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `disable_genesis = 1` — Vehicle only: `1` removes the Genesis plasma on both creation and death (the unit appears instantly and fully formed with no plasma, scale-in or sound, though its `dest_fx` create effects still play, and on death it releases no plasma while the wreck still falls and explodes); `at_death` (case-insensitive) removes only the death plasma while keeping normal creation Genesis. Works for every class, Host Stations included. `0`, absent or invalid = vanilla.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `air_dead = 1` — Flying Vehicles only (Flyer and UFO): `1` skips the falling wreck phase (`CrashOrLand`) on death so the unit triggers its terminal `DEATH2` state directly in mid-air at its death position (preserving the player destroyed-unit camera/message in the sky). Combine with `disable_genesis = at_death` to also suppress the mid-air death plasma. Ground vehicles ignore it; `0`, absent or invalid = vanilla.
------------------------------------------------------------------------------------------------------------------------

Mobile Power turns a vehicle into an energy source for itself and nearby allies.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `power = 130` — Amount of mobile energy generated for this vehicle and nearby allied units.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `power_radius = 2400` — Radius within which the vehicle supplies energy to itself and nearby allies.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `power_falloff = 1` — 1 reduces Power effect strength with distance; 0 keeps it uniform within power_radius.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `power_cloud_id = 3` — ID of the visual/audio Cloud associated with this Vehicle mobile Power. -1, absent or unknown ID = no Power Cloud.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_force_malus = -50%` — Force modifier while damaged. Example: -50% reduces force by 50%.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_maxrot_malus = -50%` — maxrot modifier while damaged. Example: -50% reduces maxrot by 50%.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_mgun_shot_time_malus = -50%` — MGUN fire-rate modifier while damaged. Example: -50% reduces MGUN rate of fire by 50%; 0 or absent = unchanged.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_shot_time_malus = -50%` — Weapon fire-rate modifier while damaged. Example: -50% reduces Weapon rate of fire by 50%; 0 or absent = unchanged.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_snd_pitch = -50%` — Pitch modifier for sounds emitted by the damaged unit. Negative values lower pitch; positive values raise it. 0 or absent = unchanged.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `collision_shape = Data/Collisions/Tiger_Set1.collision` — Model-derived collision generated by OpenNeoUA Studio. A valid profile replaces the physical spheres for movement, vehicle contacts and direct projectile, MGUN and laser hits. Empty, 0, absent or invalid = radius / `coll_*` fallback. Classes: heli, tank, car, flyer and UFO. Limits: 64 convex hulls, 128 vertices each. It must match the visual scale and rotation it was baked from, applies to every texture SET, excludes rotors and FX, keeps turrets in their authored pose and needs regeneration when the model changes. AoE and proximity keep their own logic.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `collision_shape = Data/Collisions/Rocket_Set1.collision` — Weapon projectile body generated by Studio. A valid profile replaces the weapon's spheres for direct hits on units: its hulls are swept against the target's shape, or against the target's spheres when it has none. Terrain, building and weapon-vs-weapon impacts are unchanged. Fallback, limits and path rules as for Vehicles. Ignored by `model = laser` (the beam is not a physical projectile): lasers use collision spheres, and Studio greys out the Collision Shape tab for them.
------------------------------------------------------------------------------------------------------------------------

Up to 256 manually placed spheres replace the single legacy radius, which helps long, asymmetric or multi-part units.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `coll_num = 3` — Number of manually defined `coll_*` spheres. Range 0-256; negative values become 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `coll_act = 0` — Selects the sphere being defined (0-255). Repeat the three `coll_*` lines for each sphere.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `coll_xyz = 50|0|0` — Sphere X|Y|Z position relative to unit center. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `coll_radius = 200` — Sphere radius.
------------------------------------------------------------------------------------------------------------------------

### Weapons and firing

------------------------------------------------------------------------------------------------------------------------
- 🔵 `weapon = 5` — ID of the primary Weapon, taken from the weapon list (new_weapon).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `mgun = 3` — ID of the secondary machine-gun Weapon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `num_targets = 3` — Vehicle only: how many targets a missile, homing_bomb or laser salvo can spread over, capped by `num_weapons` of the selected slot. 0, 1 or absent = every shot goes to the primary target. 2-255 = shots are shared round-robin among valid targets (fewer targets are reused). Incompatible weapons ignore it. Also applies to weapon_2/3/4.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `fire_x = 0` — Muzzle position of the projectile on the X axis (right positive), relative to the unit center.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `fire_y = 0` — Muzzle position on the Y axis. The engine Y axis points down, so negative values are higher.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `fire_z = 50` — Muzzle position on the Z axis (forward positive).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gun_radius = 5` — Machine gun: radius of the hit test against targets.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gun_power = 4000` — Machine gun: power (damage) of each hit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gun_angle = 0.1` — Machine gun: width of the aiming cone. Higher values make the gun more permissive about firing off-axis.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `kill_after_shot = 1` — The unit dies right after firing. Useful for one-shot or suicide units.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `kamikaze = 5000` — Car class only: the unit explodes on contact. The value is the energy of the blast. It is not the same as kill_after_shot.
------------------------------------------------------------------------------------------------------------------------

MGUN is the shared hit-scan machine-gun path. Its range is `game.mgun_range`. MGUN impacts never damage buildings or sectors (units are still damaged), and AI sector attack or capture orders use the primary weapon and stop MGUN fire.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_name = MGUN` — Display name of the MGUN.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_wireframe = wireless/otlw38.sklt` — Wireframe skeleton of the MGUN shown in the interface.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_spread_xy = 20|20` — MGUN spread (format x|y): X horizontal left/right, Y vertical up/down.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_cockpit_camera_recoil = 1.5` — 0-10 first-person MGUN shake intensity. One shake per shot, so it follows mgun_shot_time. Only active in the player's cockpit camera and never moves the vehicle. 0, absent or invalid = no shake.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_energy = 900` — Energy removed by each MGUN shot. When present, the MGUN fires one shot every mgun_shot_time: sound, FIRE state, spread hit scan and energy hit all happen together. Absent, 0 or invalid = vanilla MGUN (continuous damage from gun_power).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_angle = 0` — MGUN angle. If absent, falls back to legacy gun_angle.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_vp_dead = 99` — Legacy VP for the MGUN dead impact; final fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_base_dead = Data/Models/Base/FX/MGUN_DEAD.BASE` — BASE family for the MGUN dead impact; falls back to mgun_vp_dead.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_3ds_dead = Data/Models/3ds/FX/mgun_dead.3ds` — 3DS model for the dead impact; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_vp_megadeth = 99` — Legacy VP for the MGUN megadeth impact; final fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_base_megadeth = Data/Models/Base/FX/MGUN_MEGADETH.BASE` — BASE family for the megadeth impact; falls back to mgun_vp_megadeth.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_3ds_megadeth = Data/Models/3ds/FX/mgun_megadeth.3ds` — 3DS model for the megadeth impact; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_shot_time = 100` — Time between two MGUN shots, shared by player and AI. With mgun_energy it sets the real fire rate (sound, FIRE state, hit scan and energy hit); without mgun_energy it only paces sound, animation and cost.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_shot_time_user = 100` — Player-specific time between two MGUN shots; absent or zero uses mgun_shot_time.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_fire_energy_cost = 0.05%` — Energy cost per MGUN shot. 0 or absent = no cost.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_ground_decal_id = 4` — ID of the new_decal profile used for MGUN world impacts; set its diameter with `mgun_ground_decal_size`. -1 or absent = no decal.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_ground_decal_size = 3~7` — Manual diameter range in world units, 0..12000. Absent, zero or invalid disables the decal.
------------------------------------------------------------------------------------------------------------------------

**Spread by weapon class.** The axes of `weapon_spread_xyz` that apply depend on the class being fired:

- Bullet, rocket, missile, grenade, arc_grenade (every non-bomb class): X and Y rotate the aim direction; Z is ignored.
- Bomb and homing_bomb: X (left/right) and Z (forward/back) push each bomb on the ground plane relative to the carrier heading, independent of pitch; Y is ignored. Bombs leave at 20% of the carrier speed, so the push uses a fixed reference launch speed and the scatter grows with the drop height.
- Laser, downward lasers included: no spread.
- Absent, 0 or invalid = no spread on that axis. The Hand Brake reduction applies to all classes; `ramp_up_max_weapon_spread_xy` applies to non-bomb weapons (its X also affects the X of a bomb's spread).

------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_spread_xyz = 20|20|20` — Random spread of each projectile, in degrees (x|y|z): X horizontal, Y vertical, Z forward/back (bombs and homing bombs only). See the note above for what each weapon class uses. 0, absent or invalid = none. Also applied to player fire.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_arc_xy = 0|0` — Fixed arc offset for each projectile (format x|y): X horizontal, Y vertical. Applied the same way for the player.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_cone_xy = 0` — XY cone width for each generated projectile, using a uniform conical distribution.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `scope_angle_min_max = -100|100` — Tank/car only: inclusive min|max limits of vanilla aim travel (-100 down, 0 parallel, 100 up). Absent = vanilla; outside range, red reticle and firing/lock blocked; MG unchanged.
------------------------------------------------------------------------------------------------------------------------

`fire_x_*` controls a virtual firing rack. It is useful for weapons that should visibly walk across multiple muzzle positions or alternate entire salvos.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `fire_x_mode = sequence` — Projectile X mode: sequence = orderly X cycle; random = random X slot; salve_sequence = each whole salvo starts from one side and the next from the other; salve_mirror = splits one salvo and fires half simultaneously from each side.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fire_x_start = -45` — Initial X position of the firing rack used by sequence/random.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fire_x_step = 10` — X distance between adjacent firing-rack slots.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fire_x_slots = 10` — Total number of X slots before the rack cycle resets.
------------------------------------------------------------------------------------------------------------------------

A vehicle can expose up to four Weapon slots. Player and AI switching policies are independent, so the same vehicle may be manual for the player and smart or sequenced for AI control.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_2 = 2` — Optional second Weapon slot used by the unit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_3 = 8` — Optional third Weapon slot used by the unit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_4 = 7` — Optional fourth Weapon slot used by the unit.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `num_weapons = 2~4` — Projectiles fired by each shot (side-by-side salvo), not a list of different weapons. Single value = fixed; `min~max` = random count 1-255 per shot. 0 or absent = vanilla single projectile.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `num_weapons_snd_events = 2` — Maximum number of complete weapon sound events generated for a multi-projectile shot, preventing excessive overlap when num_weapons is greater than one.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `num_weapons_2 = 0` — Projectile count used by weapon_2. Single value = fixed; `min~max` = random 1-255; 0 or absent = inherits num_weapons.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `num_weapons_3 = 0` — Projectile count used by weapon_3. Single value = fixed; `min~max` = random 1-255; 0 or absent = inherits num_weapons.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `num_weapons_4 = 0` — Projectile count used by weapon_4. Single value = fixed; `min~max` = random 1-255; 0 or absent = inherits num_weapons.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_player_switch_mode = manual` — Player changes Weapon manually using the mapped dedicated control.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_player_switch_mode = sequence` — Player Weapon automatically cycles through weapon, weapon_2, weapon_3, ...
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_player_switch_mode = random` — Player Weapon automatically changes to a random configured slot.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_ai_switch_mode = smart` — AI picks the Weapon with the highest base energy times the target's `energy_*` multiplier; ties are random. A cluster Weapon whose own `ai_fire_trigger_*` volume holds more than one enemy (or one enemy plus a neighbour, see `cluster_ai_group_radius`) is always preferred; if several qualify, the best one wins. Weapons without a cluster or fire volume are unaffected.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_ai_switch_mode = sequence` — AI uses its configured Weapons in sequence.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_ai_switch_mode = random` — AI selects a random configured Weapon.
------------------------------------------------------------------------------------------------------------------------

### AI behavior

------------------------------------------------------------------------------------------------------------------------
- 🔵 `adist_sector = 1200` — Maximum distance at which the AI wants to attack or interact with a sector.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sdist_sector = 300` — Minimum safe distance the AI keeps from a sector target.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `adist_bact = 1200` — Maximum distance at which the AI wants to engage another unit (a bact is any actor: vehicles, Host Stations, guns).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sdist_bact = 300` — Minimum safe distance from another unit. The ideal band is sdist <= target distance <= adist; too far and the AI approaches, too close and it repositions.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `job_fightrobo = 6` — AI suitability score against Host Stations. Job values do not change damage or speed; they only tell the AI and the Host Station how well the unit fits a task.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `job_fightflyer = 6` — AI suitability score against aircraft (plane, glider, zeppelin, ufo).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `job_fighthelicopter = 6` — AI suitability score against helicopters.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `job_fighttank = 6` — AI suitability score against ground vehicles (tank and car).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `job_conquer = 6` — AI suitability score for conquering sectors.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `job_reconnoitre = 6` — AI suitability score for scouting and reconnaissance.
------------------------------------------------------------------------------------------------------------------------

AI fight affinities extend the broad flyer/tank targeting families to the newer model classes. When omitted, the legacy affinity is used.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `job_fightplane = 6` — AI fight affinity against model = plane. Absent = job_fightflyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `job_fightglider = 6` — AI fight affinity against model = glider. Absent = job_fightflyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `job_fightzeppelin = 6` — AI fight affinity against model = zeppelin. Absent = job_fightflyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `job_fightcruiser = 6` — AI fight affinity against model = cruiser. Absent = job_fightflyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `job_fightufo = 6` — AI fight affinity against model = ufo. Absent = job_fightflyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `job_fightcar = 6` — AI fight affinity against model = car. Absent = job_fighttank.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `job_fightgun = 6` — AI fight affinity against model = gun/module. Absent = legacy AI-path behavior.
------------------------------------------------------------------------------------------------------------------------

Three-stage attack loop replacing the old sector/bact pairs: attack, retreat when too close, re-engage once enough separation is recovered. For a Weapon with an `ai_fire_trigger_*` volume these values only steer maneuvering; the volume authorizes fire.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `ai_attack_range = 2100` — Replaces adist_sector and adist_bact. Distance within which the AI enters its attack maneuver.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ai_retreat_start_range = 140` — Replaces sdist_sector and sdist_bact. Below this distance, the AI starts retreating and moves away from the target.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ai_retreat_stop_range = 550` — Distance the AI must reach while retreating before it stops retreating and resumes its attack maneuver. Must be greater than ai_retreat_start_range and not greater than ai_attack_range.
------------------------------------------------------------------------------------------------------------------------

### Guns, flaks and radar

------------------------------------------------------------------------------------------------------------------------
- 🔵 `gun_type = flak` — Turret type of a `model = gun` unit. The values are listed below.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gun_side_angle = 60` — Flak turret: maximum sideways rotation in degrees.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gun_up_angle = 60` — Flak turret: maximum upward elevation in degrees.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gun_down_angle = 10` — Flak turret: maximum downward angle in degrees.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gun_type = flak` — Vanilla flak behavior; no OpenNeoUA role change.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `gun_type = mg` — Restored cut-content behavior: the flak uses MGUN as its primary weapon. Effective range follows game.mgun_range.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gun_type = dummy` — Passive module. Attached Unit Guns and dummy guns configured as vehicle spawners do not aim or fire. A dummy spawner needs no radar or power role.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gun_type = radar` — Rotates continuously through 360 degrees around its mount without pitching; it does not search or attack. Speed is `maxrot` in radians per second (a turn takes about 6.283 / maxrot seconds); zero, negative or non-finite stops it. `gun_*_angle` limits do not restrict the scan. `radar` still sets the visibility range.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gun_type = power` — Compatible with power and power_radius. The gun does not attack but supplies energy to nearby units.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gun_type = spawner` — Passive gun that uses the vehicle spawn settings. It is immobile unless `maxrot` is explicitly authored; only then does it rotate continuously around its mount. It does not aim or fire. With a valid spawn setup, it can show the world cooldown bar. When the spawner dies, every unit it spawned and that is still alive dies with it, with no exception (invulnerable, spawn-protected, or still spawning); their at-death spawns are cancelled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gun_levitate = 0` — gun/flak classes only: 1 prevents falling without a supporting surface. 0, absent or invalid = normal falling behavior.
------------------------------------------------------------------------------------------------------------------------

### Host Stations and mounted guns

------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_num_guns = 4` — Number of gun (flak) attachments on the Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_act_gun = 0` — Selects the gun slot being configured, starting at 0. The `robo_gun_*` lines below apply to that slot.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_gun_pos_x = 0` — X position of the gun relative to the Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_gun_pos_y = -100` — Y position of the gun (negative is higher).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_gun_pos_z = 0` — Z position of the gun (forward / backward).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_gun_dir_x = 0` — Initial aim direction X of the gun.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_gun_dir_y = 0` — Initial aim direction Y of the gun.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_gun_dir_z = 1` — Initial aim direction Z of the gun.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_gun_type = 95` — ID of the Vehicle (model = gun) used as this gun.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_gun_name = Main_Gun` — Name of the gun.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_dock_x = 0` — X position where the Host Station releases newly created units.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_dock_y = 0` — Y position where units are released.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_dock_z = 0` — Z position where units are released.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_coll_num = 3` — Number of extra collision spheres of the Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_coll_act = 0` — Selects the collision sphere being configured, starting at 0.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_coll_x = 0` — X position of the sphere relative to the center.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_coll_y = 0` — Y position of the sphere.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_coll_z = 0` — Z position of the sphere.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_coll_radius = 300` — Radius of the collision sphere.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_viewer_x = 0` — X position of the player camera while controlling the Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_viewer_y = -200` — Y position of the player camera.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_viewer_z = 0` — Z position of the player camera.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_viewer_max_up = 0.5` — Upward rotation limit of the player camera.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_viewer_max_down = 0.5` — Downward rotation limit of the player camera.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_viewer_max_side = 1.0` — Sideways rotation limit of the player camera.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_does_twist` — When present, the Host Station slowly rotates while idle.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_does_flux` — When present, the Host Station sways up and down while idle.
------------------------------------------------------------------------------------------------------------------------

Universal Unit Guns extend the Host Station gun system so guns and functional modules can be mounted on ordinary vehicles too. `unit_gun_protect` turns a mounted gun into a damage layer: it must be destroyed before the parent can be damaged directly.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_num_guns = 2` — Evolution of robo_num_guns: number of guns/modules that can be attached to any supported unit, including Host Stations.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_act_gun = 0` — Selects the mounted gun being defined (0, 1, ...). Repeat the `unit_gun_*` lines for each gun.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_icon = Graphics/StatusIcons/mobile_flak.svg` — Optional Data-relative icon assigned to gun 0 and shown on the parent unit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_pos_xyz = 0.0|-200.0|55.0` — Mounted gun local X|Y|Z position. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_dir_xyz = 0.0|0.0|1.0` — Mounted gun initial X|Y|Z direction component. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_type = 95` — Gun/module type ID.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_name = FLAK` — Display/internal name assigned to this mounted gun.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_protect = 1` — 1 makes this gun/module absorb damage and require destruction before direct damage can reach the parent vehicle.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_no_switch = 1` — 1 prevents the player from cycling from the parent unit into this gun with F6 (or the next-unit button). 0, absent or invalid = the gun remains reachable as before. Weapons fired by a mounted gun never hit the parent unit or the other guns mounted on it.
------------------------------------------------------------------------------------------------------------------------

### Appearance (models, VP, 3DS)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_normal = 10` — ID of the visual model (VP) used in the normal state.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_fire = 11` — ID of the model used while the unit is firing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_wait = 12` — ID of the model used while the unit is idle.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_dead = 13` — ID of the model used when the unit is destroyed.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_megadeth = 14` — ID of the model used for a heavy death (crash or hard impact).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_genesis = 15` — ID of the model used for the creation (beam-in / Genesis) effect.
------------------------------------------------------------------------------------------------------------------------

Physical Bruch fragments use the normal, goingdown and explode sound events. See [Bruch](#bruch-bruchcfg).

**Kamikaze decals.** When a `model = kamikaze` Weapon detonates with a positive `trigger_radius`:

- The carrier's destruction decal is created at the nearest world surface (terrain or building) touched by that sphere, tangency included. No contact = no decal, also for the later wreck impact. `size` sets the decal size, not the contact radius.
- The payload's decal and its crater or dent use the same point; the deformation also works without a decal.
- Absent, zero or invalid `trigger_radius` keeps the normal Vehicle decal. For Tank and Car, a death that begins in the air creates no decal, also on the later crash path. Decal limits apply.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `ground_decal_id = 1` — ID of the reusable new_decal profile. Vehicles spawn on grounded destruction; Weapons spawn on real world impact. -1, absent or unknown ID = no decal.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ground_decal_size = 80~120` — Manual diameter range in world units, 0..12000. Absent, zero or invalid disables the decal.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `crater_radius = 120~160` — Vehicle: radius of the crater dug under the wreck when it ends on the ground (same moment as the destruction decal), in world units; fixed value or `min~max` (0..12000). Needs `crater_depth`: if either is missing, zero or invalid there is no crater (vanilla). A vehicle still in the air, or a multiplayer game, digs nothing.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `crater_depth = 40~60` — Vehicle: depth of the crater centre, `min~max` (0..12000). Never more than half the radius.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `crater_rim = 20` — Vehicle: raised rim in % of the depth (0..100). Absent = 20; invalid = no crater.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cockpit_camera_offset_xyz = 0|0|0` — Cockpit camera X|Y|Z offset. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cockpit_gun_camera_recoil = 2.0` — 0..5 multiplier for cockpit camera recoil from the gun. Does not alter physics, aiming or projectile origin.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cockpit_camera_hide_vehicle = 0` — 1 hides the vehicle's own silhouette in the cockpit camera for an unobstructed forward view. 0 or absent = visible (vanilla); defaults to 1 for `model = ufo`. The Hide Vehicle key (default C, remappable in Input Settings) inverts it at runtime; the change is not written to the file and this value returns on restart.
------------------------------------------------------------------------------------------------------------------------

Each lifecycle state can have its own external visual. External assets extend, never remove, the original VP states.

- Keep related BASE states and their skeleton and animation files in one model folder. Name each BASE after its state.
- Add a `Textures/SetN/` subfolder beside the BASE to enable set-specific ILBM textures. The BASE and its animations can use the same texture name across sets.
- ILBM texture lookup (same order in the game and in OpenNeoUA Studio), stopping at the first file found:
  - `Textures/SetN/` of the active set.
  - The BASE folder, then its `Textures/` folder.
  - Any subfolder of the model folder, even another set's. If several files share the name, the active set's folder wins, then alphabetical order. The game logs a warning when it uses another set's folder or must choose among several files.
  - `Models/Sector/SetN/Textures/` (shared textures of the active set).
  - A PNG with the same name in the model folders.
  - The active `SET.BAS`.
  - The `SET.BAS` of Set1 as the last resort (logged as a warning).
- The lookup never leaves the model folder, except for the shared set textures and the two `SET.BAS` files.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_<state> = Data/Models/Base/VEHICLE.BASE` — External BASE model for a lifecycle state (normal, fire, wait, dead, megadeth, genesis); falls back to `vp_<state>`. If the resolved dead visual has no persistent geometry, the normal visual is reused during DEATH1 so the wreck stays visible while it falls. A BASE shared by Genesis and a regular state only receives faction plasma tint during Genesis or beam playback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_<state> = Data/Models/3ds/vehicle.3ds` — External 3DS model for a lifecycle state (same states as `base_<state>`); takes priority over `base_<state>` and `vp_<state>`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `scale_xyz = 1.0|1.0|3.0` — X|Y|Z visual scale of all VP/BASE/3DS states, including dead and megadeth. Does not change collision geometry. Default: 1.0. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spin_xyz = 0|0|10` — X|Y|Z spin intensity of the active main VP/BASE/3DS model: 0-10, decimals allowed; 0 = no spin. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `rotation_xyz = 0|0|0` — Fixed visual rotation in degrees around the X|Y|Z axes of the active main VP/BASE/3DS model. 0 or absent = no rotation. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tint = 40|120|255|255` — RGBA tint applied to the active main VP/BASE/3DS visual. It never tints the Genesis plasma: the Genesis uses the faction colour, and the Resistance keeps the original cyan.
------------------------------------------------------------------------------------------------------------------------

Vehicle and Weapon decorations use the same periodic/persistent system as building decorations, with positions relative to the parent object.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_mode = periodic` — Periodic mode: generates decorations intermittently using the parameters below.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp = 99` — Legacy VP decoration visual; final fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_base = Data/Models/Base/FX/DECORATION.BASE` — BASE decoration family, relative to Data; falls back to decoration_fx_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_3ds = Data/Models/3ds/FX/decoration.3ds` — 3DS decoration model; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_scale_xyz = 1.0|1.0|1.0` — X|Y|Z scale of the active decoration visual. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_spin_xyz = 0|0|0` — X|Y|Z spin intensity of the decoration VP/3DS visual: 0-10, decimals allowed; 0 = no spin. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_tint = 255|95|70|200` — RGBA tint applied to the main decoration visual.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_fade_inout = 500|500` — Main VP/3DS decoration fade-in and fade-out in milliseconds (format in|out). 0 or absent = immediate appearance/disappearance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_tint = 255|255|255|255` — RGBA tint of VP decoration particles/trails. Alpha 0 = invisible.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_scale_xyz = 1.0|1.0|1.0` — X|Y scale of VP decoration particles/trails, plus a Z multiplier for particle/trail duration and therefore apparent trail length (format x|y|z).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_fade_inout = 500|500` — Time in milliseconds (format in|out) for VP decoration particles/trails to fade in to their configured opacity (in) and to fade out to invisible (out). 0 or absent = immediate.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_duration = 1000` — Total duration in milliseconds of each periodic decoration.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_interval = 500~1500` — Random interval in milliseconds between decoration spawns. A single value gives a fixed interval.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_count = 2~7` — Random number of decorations generated per spawn. A single value gives a fixed count.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_random_pos = 90` — XYZ position randomization around the parent Vehicle/Weapon center.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_offset_xyz = 0|0|-40` — X|Y|Z offset from the parent Vehicle/Weapon center. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_mode = persistent` — Persistent mode: the decoration stays active while the parent Vehicle or Weapon is active. It uses `decoration_fx_vp`, `base`, `3ds`, `scale_xyz`, `spin_xyz`, `tint`, `fade_inout`, `vp_trail_*` and `offset_xyz`; `duration`, `interval`, `count` and `random_pos` are not used.
------------------------------------------------------------------------------------------------------------------------

Damaged-state visuals are coupled to optional gameplay penalties. This means the damaged state can be purely cosmetic, or it can also reduce movement, turning and weapon cadence once the configured HP threshold is crossed.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_fx_vp = 138|138|138` — VP visuals generated while the unit is damaged. Multiple VP IDs may be separated by |. Repeated damaged_fx_3ds lines can add 3DS visuals.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_fx_3ds = Data/Models/3ds/FX/damaged.3ds` — Optional damaged-state 3DS model. Repeat the line to add multiple 3DS candidates to the random visual selection alongside VP entries.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_fx_scale = 1.0~1.7|1.0~1.7|0.2~0.3` — Scale of the damaged FX per VP, in the order of `damaged_fx_vp`; separate entries with `|`, each a single value or `min~max`. A single value or range applies to all VPs and 3DS visuals. Missing, non-positive or invalid entries = 1.0. With several entries, 3DS visuals use 1.0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_fx_threshold = 30%` — HP threshold below which damaged FX activate. With %, uses maximum HP; without %, uses a fixed HP amount.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_fx_count = 1~3` — Random number of damaged FX generated per trigger. A single value gives a fixed count. Runtime maximum: 32.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_fx_interval = 500~1500` — Random interval in milliseconds between damaged-FX triggers. A single value gives a fixed interval.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_fx_random_max_offset = 25%` — Maximum random offset from the unit center. With %, effects spawn inside the unit silhouette (its `collision_shape` when present, otherwise the visual model; rotor blades and thin parts excluded): 100% = whole body, 25% = central core. Without %, a fixed distance in world units.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_fx_trail_only = 1` — 1 hides the main VP model while keeping its trails, particles and child effects active.
------------------------------------------------------------------------------------------------------------------------

### Destruction and effects

------------------------------------------------------------------------------------------------------------------------
- 🔵 `dest_fx = death_46_0_0_0` — Effect spawned on destruction. Format `type_ID_X_Y_Z`: `death` (normal destruction) or `megadeth` (heavy crash or surface hit), the effect ID (Bruch first, then a legacy Vehicle), then the X, Y, Z offset from the unit center. A fifth number can enable acceleration. `|` is also accepted as separator.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `ext_dest_fx = death_46_0_0_0` — Extended variant of dest_fx, with the same format. Rarely used. Alias: extended_dest_fx.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `scale_fx = 1_5_2.5_1500_292_291_290_289` — Scaled animation effect, usually referenced by dest_fx. Legacy format `start_speed_accel_duration_vp1_vp2`. The first three values set initial scale, scale velocity and scale acceleration. Duration is in milliseconds. Use a Bruch profile for new effects.
------------------------------------------------------------------------------------------------------------------------

A Vehicle `begin_fx` block references a [Bruch](#bruch-bruchcfg) profile and controls when and where it spawns.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `begin_fx` — Starts an effect reference. Close the block with `end`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `trigger = destroyed|air_destroyed` — Runs on ground or air destruction. Join multiple events with `|`. Any unknown trigger rejects the block.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `bruch_id = 1000` — Looks up a profile in Bruch first. If the same ID has no Bruch entry, uses the legacy Vehicle if it exists. Valid IDs are 1–65535. An invalid or missing ID disables a reference block.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `count = 2~4` — Number emitted per event. Use a fixed value or `min~max`. Maximum 32. Missing or invalid = 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `offset_xyz = -20|0|20` — Local X/Y/Z spawn offset. Each axis accepts a fixed value or `min~max`. It rolls independently for each instance. Legacy physical effects retain their original radius-based offset.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `launch_xyz = -30~30|-20|-30~30` — Local initial velocity for physical effects. Each axis accepts a fixed value or `min~max`. Omitted = zero initial velocity. Legacy Vehicle-style effects keep their original launch from the offset.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `random_rotation = 0` — 1 rolls a random orientation per instance. 0, absent or invalid preserves the normal orientation. Spawn position and launch direction stay independent.
------------------------------------------------------------------------------------------------------------------------

```
begin_fx
    trigger = air_destroyed
    bruch_id = 1000
    count = 1~4
    launch_xyz = -15~15|-30~-15|-15~15
end
```

### Buffs

------------------------------------------------------------------------------------------------------------------------
- 🟢 `buff_id = 1|3` — IDs of the new_buff profiles the unit is born with, separated by `|` (up to 16, repeated IDs count once). Born Buffs are permanent and never lost when something else buffs the unit. Invalid items and unknown IDs are skipped (unknown IDs also log a warning); -1, absent or nothing valid = no Buff.
------------------------------------------------------------------------------------------------------------------------

### Sounds, palette flashes and shakes

------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_<event>_sample = sounds/engine.wav` — Sound file played for the event. Events: normal (engine loop), fire, wait, genesis, explode, airexplode (OpenNeoUA: explosion in flight), crashland (hits the ground), crashvhcl (hits a vehicle), goingdown (falling), cockpit, beamin, beamout, build, spawn (OpenNeoUA: loops while the spawn state of `vp_spawn` is active, see below).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_<event>_volume = 120` — Volume of the event sound.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `snd_<event>_pitch = 0` — Pitch of the event sound. Accepts `min~max`.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_<event>_ext = 1_100_22050_0_0_sounds/extra.wav` — Additional sample for the event. Format loop_volume_sampleRate_offset_sampleCount_file.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_<event>_fade_inout = 150|300` — Volume fade-in and fade-out of the event sound in milliseconds (format in|out). A one-shot sample fades out over its own length; a looping sound only fades in. If both fades are longer than the sound they shrink together. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pal_<event>_slot = 1` — Screen color flash: effect type (1 red, 2 blue, 3 green, 4 inverse, 5 dark inverse, 6 black and white, 7 turquoise inverse). Events are the same as for snd_.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pal_<event>_time = 500` — Duration of the color flash in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pal_<event>_mag0 = 1` — Starting intensity of the flash.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pal_<event>_mag1 = 0` — Final intensity of the flash. The intensity fades linearly from mag0 to mag1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_<event>_fade_inout = 150|300` — Fade-in and fade-out of the color flash in milliseconds (format in|out). The fade-out ends together with `pal_<event>_time`; a looping effect only fades in. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<event>_slot = 1` — Camera shake: shake slot / type.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<event>_time = 300` — Duration of the shake in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<event>_mag0 = 1` — Starting intensity of the shake.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<event>_mag1 = 0.5` — Final intensity of the shake.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<event>_x = 1` — Shake strength on the X axis.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<event>_y = 1` — Shake strength on the Y axis.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<event>_z = 1` — Shake strength on the Z axis.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<event>_mute = 0` — How much the audio is attenuated while the shake plays.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_<event>_fade_inout = 150|300` — Fade-in and fade-out of the camera shake in milliseconds (format in|out). The fade-out ends together with `shk_<event>_time`; a looping effect only fades in. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `snd_genesis_pitch = auto` — `auto` = automatic Genesis sound pitch: the higher the unit's HP, the lower the pitch (opt-in, exact value `auto`). A number or `min~max` overrides it; absent = normal default. An explicit `building_time` overrides this setting and caps the automatic pitch at the 10-second reference.
------------------------------------------------------------------------------------------------------------------------

### OpenNeoUA special features

------------------------------------------------------------------------------------------------------------------------
- 🟢 `spy_ui_radius = 6000` — UFO classes only, player side: radius in which Spy Mode reveals unit interfaces. Toggle with the remappable UFO Spy Mode On/Off action (Spacebar by default) or the fixed middle-mouse shortcut. 0 or absent = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spy_ui_radius_sound = sounds/new/spy_ui.wav` — UFO classes only: optional sound played every time Spy Mode is enabled or disabled. Empty/absent/unloadable = silent.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `zoom_steps = 4` — UFO classes only, player side: maximum number of 1.25x optical zoom-in steps. 0 locks zoom to 1x; absent/invalid keeps the OpenNeoUA limit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `zoom_step_sound = sounds/new/spy_zoom.wav` — UFO classes only: optional one-shot played whenever an optical zoom step is actually applied. No sound is played when input is received at an already reached zoom limit. Empty/absent/unloadable = silent.
------------------------------------------------------------------------------------------------------------------------

Vehicle spawners position spawned units relative to the parent unit. Spawned units inherit the source's primary command, waypoint route and aggression, and reuse Genesis overlap separation, including instant spawns.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_vehicle = 188|21|75` — Vehicle ID spawned by this unit. With several IDs separated by `|`, each spawned unit picks one at random; repeat an ID to make it more likely. Invalid or non-positive entries are skipped; if none is valid, nothing spawns. A single ID works as before.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_count = 3` — Number of units spawned simultaneously per activation. Default: 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_interval = 6000` — Time in milliseconds between spawn activations.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_trigger_radius = 1800` — Enemy-presence radius that activates spawning.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_random_pos = 200` — XZ spawn-position randomization around the parent vehicle.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_offset_xyz = 0|0|0` — Spawn-position X|Y|Z offset from the parent vehicle. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_max_active = 9` — Maximum number of children generated by this unit that may be active simultaneously.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_instant = 1` — 1 spawns children immediately without the classic Genesis sequence.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_spawn = 42` — Optional VP used while at least one direct child created by periodic spawning is in Genesis. 0, absent or invalid = no VP fallback. Instant spawns do not activate this visual.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_spawn = Data/Models/Base/Effects/SPAWN.BASE` — External BASE for the same spawn state. Used when no valid 3DS is available. Missing or invalid falls back to `vp_spawn`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_spawn = Data/Models/3ds/SPAWN.3ds` — External 3DS for the same spawn state. Takes priority over BASE and VP. If no source resolves, the regular visual state is used.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_spawn_sample = sounds/new/spawn.wav` — Sound, palette flash and camera shake of the spawn state: all `snd_spawn_*`, `pal_spawn_*` and `shk_spawn_*` keys of "Sounds, palette flashes and shakes" are accepted. They start when the spawn state begins and loop until it ends (last Genesis child done, death); palette and shake hold `mag0` while looping. They follow the spawn state even when no spawn model is set. Absent = silent, no flash, no shake.
------------------------------------------------------------------------------------------------------------------------

Radial Shot is an autonomous secondary Weapon system that fires projectiles in all directions. "classic" reacts to nearby enemies repeatedly; "at_death" fires once on unit death.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `radial_shot_mode = classic` — System mode: classic = activates around nearby enemies; at_death = activates once at unit death and fires all projectiles simultaneously.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `radial_shot_weapon = 2` — Weapon ID used by Radial Shot.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `radial_shot_trigger_radius = 1200` — Enemy-presence distance that activates classic mode.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `radial_shot_interval = 5000` — Time in milliseconds between projectile bursts in classic mode.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `radial_shot_shots = 20` — Number of projectiles created per system activation.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `radial_shot_fire_mode = all_at_once` — Classic firing mode: all_at_once = all projectiles together; sequential = one after another. at_death always uses all_at_once.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `radial_shot_vp_launch = 70` — Legacy VP spawned when Radial Shot projectiles launch; final fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `radial_shot_base_launch = Data/Models/Base/FX/RADIAL_SHOT_LAUNCH.BASE` — BASE launch family, relative to Data; falls back to VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `radial_shot_3ds_launch = Data/Models/3ds/FX/radial_shot_launch.3ds` — 3DS launch model; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `radial_shot_sequence_delay = 100` — Delay in milliseconds between projectiles when fire_mode = sequential; ignored for all_at_once and at_death.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `radial_shot_vertical_angle = -90~90` — Projectile vertical angle. Single value = fixed angle; `min~max` = random angle in range. Absent = 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `radial_shot_horizontal_angle = 0~360` — Projectile horizontal angle. Single value = fixed angle; `min~max` = random angle in range. If absent, projectiles are distributed uniformly across 360 degrees.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `orbiter_weapon = 11` — Orbiter carrier link: Weapon ID of a `model = orbiter` Weapon. The unit keeps its guardian orbiters around itself and sends them against an enemy unit. Independent of weapon/weapon_2..4/mgun (the primary Weapon is unchanged). 0, absent or a Weapon that is not an orbiter = off (vanilla). Local games only: no orbiters in network games.
------------------------------------------------------------------------------------------------------------------------

## Weapons (Weapons.cfg)

### Identity and class

------------------------------------------------------------------------------------------------------------------------
- 🔵 `new_weapon = 64` — Opens the definition of a new Weapon with the given numeric ID. If the ID already exists, the prototype is rebuilt from scratch.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `modify_weapon = 64` — Reopens an existing Weapon so that only the listed values change. Typical use is level files and tech-upgrade blocks.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `end` — Closes the current new_weapon / modify_weapon block.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `enable = 1` — Makes the Weapon available to the given faction (1 Resistance, 2 Sulgogar, 3 Mykonian, 4 Taerkasten, 5 Black Sect, 6 Ghorkov, 7 neutral). One line per faction.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `disable = 1` — Removes the Weapon from the given faction.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `model = rocket` — Projectile class: rocket (powered rocket), missile (steers toward its target; homing_time limits how long), grenade, and bomb or special (an alias of bomb that drops under gravity). OpenNeoUA adds more classes, described below.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `name = Example_Missile` — Name of the Weapon. Underscores are shown as spaces.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `wireframe = wireless/example.skl` — Wireframe outline of the Weapon shown by the interface.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `type_icon = M` — Icon glyph key of the Weapon, mapped through TYPE_NS.FON into the H_FINDER graphic. Same syntax as the Vehicle `type_icon`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_use_vehicle_fire_state = 1` — When this Weapon fires, its carrier Vehicle briefly shows its own fire-state visual (VP, BASE or 3DS); for a mounted Unit Gun the parent Vehicle shows it. 0, absent or invalid = vanilla.
------------------------------------------------------------------------------------------------------------------------

### Special projectile classes

OpenNeoUA adds projectile classes through the existing `model` field.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `model = homing_bomb` — Homing bomb Weapon class that pursues enemy targets.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `model = arc_grenade` — True ballistic arc grenade. Flight uses `start_speed`, `grenade_arc_angle` and `grenade_arc_gravity`; `force` does not matter. The player aims mortar-style: the shell lands where the crosshair meets the terrain, so raising or lowering it changes the distance; aiming at the sky or beyond the maximum range fires at 45 degrees for maximum distance. Dispersion is the Vehicle's `weapon_spread_xyz` (X left/right, Y shortens or lengthens the shot).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `grenade_arc_angle = 60` — Initial vertical launch angle in degrees. With player aiming it is the preferred angle when choosing between the high and the low trajectory.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `grenade_arc_gravity = 35` — Gravity applied during projectile flight, controlling how quickly it falls. 0/absent/invalid = engine-standard gravity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `model = kamikaze` — Converts the firing unit into a kamikaze that closes on its target and detonates. It keeps obstacle avoidance and pending maneuvers, ignores `ai_attack_range`, `ai_retreat_start_range` and `ai_retreat_stop_range`, and aggression still drives target and sector choice (no sector target = no sector fuse). The blast also hits building collision surfaces inside `Blast_radius`, roofs included: each building slot is checked separately, with shields, damage rules and `Blast_falloff` active.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `trigger_radius = 360` — Kamikaze only: XYZ fuse radius. Detonates when this sphere touches an enemy's collision surface or the active sector target's geometry, including contact during movement.
------------------------------------------------------------------------------------------------------------------------

Artillery Shell supports both a real ballistic trajectory and an emulated vertical-barrage mode.

- Automatic targeting checks range, elevation limits and any radar requirement. It prefers enemy Host Stations, then enemy functional buildings, then enemy units. It picks the nearest valid target within each category, using horizontal distance. A building target defaults to the center of a surviving building slot.
- Manual Tactical Map orders use the selected position. A barrage keeps the position chosen when it was aimed.
- A shell's direct and blast damage both spare functional buildings owned by the firing faction, including power stations, radars and other special buildings. Ordinary sector structures can still take damage regardless of ownership.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `model = artillery_shell` — Artillery/mortar Weapon class with barrage fire, manually controllable from the Tactical Map or managed automatically.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_mode = vertical_barrage` — ballistic = projectile reaches the target area along a real arc; vertical_barrage = emulated barrage where rounds climb out of view and descend vertically.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_fall_delay = 2500` — vertical_barrage only: delay before projectiles begin descending from the sky toward targets.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_transfer_fade_inout = 900|1200` — `vertical_barrage` only. Milliseconds in `in|out` order: fade in from the start of descent over the target, fade out at the end of ascent over the launcher. A shell that fades out remains hidden while waiting at the top. Each fade is limited to its own flight phase. Each value accepts 0–32767; 0 disables that side. Missing or invalid = no fades. Visual only.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_vertical_spread_xz = 2|2` — vertical_barrage only: X|Z spread while projectiles rise vertically. Format: x|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_speed = 1800` — Projectile speed used by both modes.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_range_minmax = 1600|8500` — XZ distance from the source within which a barrage area may be selected manually or acquired automatically (format min|max).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_requires_radar = 1` — 1 requires the target area to be visible on the faction radar; 0 permits targeting unrevealed areas.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_barrage_radius = 900` — Radius of the bombarded area.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_barrage_shots = 5` — Total number of projectiles available per barrage.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_barrage_shot_time = 550` — Time in milliseconds between successive shots in a barrage. Default: 250. Zero or negative adds no delay.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_barrage_cooldown = 14000` — Mortar cooldown in milliseconds, starting after the last projectile of the barrage is launched.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_arc_height = 3300` — `ballistic` only. Maximum arc height above the line from launch to impact, in game units. Default: 2500. Zero, negative or invalid values give no extra height.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_vertical_height = 3300` — `vertical_barrage` only. Height climbed above the launcher and starting height above the target for descent, in game units. Default: 2500. Zero, negative or invalid values give no vertical travel. The engine logs a warning and ignores either height key when it does not match the selected mode.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_airburst = 1` — 1 (default) detonates rounds at the selected target's world Y, even after terminal unit contact. For automatic fire at a functional building, an eligible defending turret supplies the aim point and height. Barrage spread affects X and Z. 0 uses the world surface at each round's own landing point.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_manual_mode_only = 0` — 1 leaves only player manual targeting available; 0 also permits automatic acquisition.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_marker_path = Data/Graphics/Map/Markers/artillery_ring_classic.svg` — SVG or PNG attack-area marker for the map and minimap. Use `Data/` followed by the file path inside Data. Bare filenames still resolve in `Data/Graphics/Map/Markers`. Empty/missing/invalid falls back to artillery_ring_classic.svg.
------------------------------------------------------------------------------------------------------------------------

Continuous beam instead of a physical projectile. Damage is tick-based, can ramp against the same target and can chain to nearby units. The beam is built from the normal-state visual (`vp_normal`, `base_normal` or `3ds_normal`) repeated every `laser_spacing`. A laser needs exactly one valid AI fire volume (`ai_fire_trigger_box_forward_xyz`, `ai_fire_trigger_cone_forward_xyz`, `ai_fire_trigger_box_below_xyz` or `ai_fire_trigger_cone_below_xyz`): a missing, invalid or duplicate volume is a script error at startup. Forward volumes use normal vehicle combat; downward volumes give aircraft bomber behavior and world-down firing.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `model = laser` — Continuous laser Weapon instead of a physical projectile.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_energy_tick_time = 150` — Damage-tick interval in milliseconds when the Laser is used by AI.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_energy_tick_time_user = 150` — Damage-tick interval in milliseconds when the Laser is used by the player.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_energy_increment_rate = 500` — Amount by which Laser damage increases on each successive tick against the same target.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_spacing = 20` — Spacing between normal-state VP/BASE/3DS beam visuals. Lower values produce a denser beam. When `coll_*` spheres are configured, their repeated positions follow the same spacing.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_max_energy = 50000` — Maximum damage the Laser can reach through the progressive increase.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_chain_max_jumps = 6` — Maximum number of additional unit-to-unit jumps after the first target.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_chain_radius = 1200` — Radius around the last hit unit used to search for the next chain target.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_chain_energy_mult = 0.8` — Damage multiplier applied at each chain jump. 1.0 = no loss; 0.8 = each jump retains 80% of the previous damage.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_fade_inout = 90|90` — Spatial fades in world units: fade-in distance from the muzzle | fade-out distance toward a real impact. 0 disables that side. A missing or malformed group, or a negative or non-finite part, disables both; values are capped at 50000. The fade dims both the opacity and the brightness of additive (glowing) beam surfaces, so it is visible on glowing tubes too. Appearance only: damage, collision and impact position are unchanged.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_burn_decal_frequency = 5` — Continuous burn: how many decals are laid per 100 world units of path while the beam's ground contact point moves (5 = one every 20 units; fractions such as 0.5 are allowed). With it, the weapon's `ground_decal_id` / `ground_decal_size` (impact_world decal) is painted as a strip instead of one decal every 160 ms: each decal is laid along the travel direction, with a smooth edge, the profile's width and a length that overlaps the next one. The classic `crater_radius` / `crater_depth` / `crater_rim` keys are unchanged: with the burn on they dig along the same path, each crater scaled so the trench is as deep as `crater_depth`. Missing, zero, negative or invalid = off: a laser without it never digs and keeps its old single decals. Positive values are kept between 0.05 and 10. A unit hit or no contact ends the strip. Strip decals have a constant width (the minimum of `ground_decal_size` and of the profile's `stretch_xz` Z range) and no smoke. Ready-made profile: `new_decal 5` (`laser_burn`, `Laser_Burn.png`: charred soot with a glowing seam, long side along the beam). Every decal / crater counts against `gfx.ground_decal_limit` and `game.deformation_max_craters`: raise them for long burns.
------------------------------------------------------------------------------------------------------------------------

Laser rules:

- Weapon `coll_*` spheres repeat at direct-beam visual positions and hit units, terrain and buildings. An explicitly defined positive `radius` also repeats as a central sphere. Zero-radius spheres do not collide. With no `coll_*` spheres, the original continuous collision trace is unchanged. Chain jumps keep their target-based behavior.
- `pal_normal_*` and `shk_normal_*` stay active until the beam stops (their `*_normal_time` is ignored, so only the fade-in applies); launch and hit effects keep their timing.
- In `begin_fx`, `impact_world` runs on world contact and `detonate` on unit or world contact, both at the real hit point.
- AI beams cannot damage or debuff allies or remove their Deflect Buff; player beams can still damage allies.
- The beam count is the Vehicle's `num_weapons` (or `num_weapons_2/3/4` of the slot). Every beam starts at its own `fire_x/y/z` muzzle using the vanilla distribution and `fire_x` modes, and a ranged count is rolled once when continuous fire starts. Every direct beam applies its own damage ticks, so concentrating beams raises the total damage.
- `num_targets` limits distinct direct targets, not chain jumps. A chain multiplier above 1 amplifies damage cumulatively after the `laser_max_energy` clamp.
- A direct `debuff_id` applies on contact ticks even when energy is 0 or armor blocks damage. AI lasers still cannot debuff allies.
- `shot_time` does not control beam density: `laser_spacing` and the model scale do. A ready-made animated tube is `Data/Models/Base/New/Shared/Laser_Tube/VP_LASER_TUBE.BASE`.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `model = orbiter` — Guardian projectile carried by a Vehicle through `orbiter_weapon`, never fired from a weapon slot. Orbiters appear one at a time around the carrier and circle it slowly. When the carrier attacks an enemy unit (AI: weapon or MGUN; player: weapon, laser or MGUN) they leave it, dance around the enemy and dive through it. Each strike goes through the enemy's collision surface (collision shape, or collision radius without one) like a normal penetrating weapon: when it enters and again when it leaves, it deals its full `energy` and triggers every hit effect (hit sound, detonate effects, shake, palette) at that surface point. The last strike explodes where it leaves the surface, with the normal impact and blast. Allies, buildings and terrain never trigger them. Target lost = back to guard; carrier destroyed = they explode in place. Speed comes from `start_speed`, damage from `energy` (and `blast_*` on the last strike), visuals and sounds from the usual `vp_*`/`snd_*` keys; `life_time` is the safety limit of one siege. Every `orbiter_*` key is optional.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `orbiter_count = 2~3` — Maximum orbiters alive around the carrier, 1..8 (`min~max` rolled once per unit). Default 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `orbiter_respawn_time = 4000~6000` — Milliseconds before a missing orbiter is replaced, 100..600000 (`min~max` rolled for each orbiter). Default 4000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `orbiter_fade_in = 800` — Fade-in time in milliseconds when an orbiter appears, 0..10000 (single value). An orbiter attacks only after its fade-in. Default 800.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `orbiter_guard_radius = 60~90` — Distance of the guard orbit from the carrier centre (`min~max` rolled for each orbiter). 0 or absent = automatic from the carrier size. Never closer than the carrier body.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `orbiter_siege_radius = 100~160` — Size of the dance around the target (`min~max` rolled for each siege). 0 or absent = automatic from the target size. Never smaller than the target body.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `orbiter_first_strike_delay = 1200~2000` — Milliseconds of dance around the target before the first strike, 0..60000 (`min~max` rolled for each siege). Default 1500.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `orbiter_strikes = 3` — Strikes through the target, 1..20 (`min~max` rolled for each siege). The last strike detonates the orbiter. Default 3.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `orbiter_strike_interval = 700~1100` — Milliseconds of dance between two strikes, 100..60000 (`min~max` rolled for each pause). Default 900.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `orbiter_energy_mult = 1.25` — Damage cascade between consecutive strikes, 0.05..10 (single value). Strike N (its entry and its exit hit) deals energy × mult^N: 1 = always the same; 0.4 = each strike deals 40% of the previous one; 1.25 = each strike deals 25% more than the previous one. Default 1.
------------------------------------------------------------------------------------------------------------------------

### Damage and targets

------------------------------------------------------------------------------------------------------------------------
- 🔵 `energy = 10000` — Base damage of the projectile. Final damage is energy multiplied by the multiplier of the target class (energy_tank, energy_heli, energy_flyer, energy_robo).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `energy_tank = 1.0` — Damage multiplier against ground vehicles (tank and car). 1 is normal, 0.5 half, 2 double.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `energy_heli = 1.0` — Damage multiplier against helicopters.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `energy_flyer = 1.0` — Damage multiplier against aircraft (plane, glider, zeppelin and ufo).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `energy_robo = 1.0` — Damage multiplier against Host Stations.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_energy = 2000` — Upgrade-only: adds the value to the base damage.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_energy_tank = 0.2` — Upgrade-only: adds the value to the tank/car multiplier.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_energy_heli = 0.2` — Upgrade-only: adds the value to the helicopter multiplier.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_energy_flyer = 0.2` — Upgrade-only: adds the value to the aircraft multiplier.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_energy_robo = 0.2` — Upgrade-only: adds the value to the Host Station multiplier. The spelling add_energy_Robo is also accepted.
------------------------------------------------------------------------------------------------------------------------

Damage multipliers can distinguish newer/specialized model classes while preserving the older flyer/tank families as fallbacks.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `energy_plane = 1.0` — Damage multiplier against model = plane. Absent = energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `energy_glider = 1.0` — Damage multiplier against model = glider. Absent = energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `energy_zeppelin = 1.0` — Damage multiplier against model = zeppelin. Absent = energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `energy_cruiser = 1.0` — Damage multiplier against model = cruiser. Absent = energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `energy_ufo = 1.0` — Damage multiplier against model = ufo. Absent = energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `energy_car = 1.0` — Damage multiplier against model = car. Absent = energy_tank.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `energy_gun = 1.0` — Damage multiplier against model = gun/module. Absent = generic vanilla 1.0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `add_energy_plane = 1.0` — In modify_weapon, adds to energy_plane. If undefined, starts from energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `add_energy_glider = 1.0` — In modify_weapon, adds to energy_glider. If undefined, starts from energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `add_energy_zeppelin = 1.0` — In modify_weapon, adds to energy_zeppelin. If undefined, starts from energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `add_energy_cruiser = 1.0` — In modify_weapon, adds to energy_cruiser. If undefined, starts from energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `add_energy_ufo = 1.0` — In modify_weapon, adds to energy_ufo. If undefined, starts from energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `add_energy_car = 1.0` — In modify_weapon, adds to energy_car. If undefined, starts from energy_tank.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `add_energy_gun = 1.0` — In modify_weapon, adds to energy_gun. If undefined, starts from generic vanilla 1.0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `delay_time_debuff_id = 12` — Debuff profile ID applied as soon as the projectile begins its delay_time on the struck target.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `push = 4` — Target push intensity 0-10. Standalone GUNs remain excluded; when an attached GUN is hit, push is transferred once to its valid parent and uses the parent's push_resistance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `recoil = 0.5` — Primary Weapon recoil, 0-10. Also works on artillery shots. On `model = gun` flaks the model slides back along its barrel axis without tilting or moving its collision. 0 or absent = off.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `armor_penetration_targets = 2` — Number of units the Weapon may penetrate before stopping.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `armor_penetration_icon = Data/Graphics/StatusIcons/armor_penetration.svg` — Optional Data-relative icon shown next to the Weapon damage while armor_penetration_targets is active.
------------------------------------------------------------------------------------------------------------------------

Weapon Blast uses one shared radius for units, buildings and sector structures.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `Blast_radius = 400` — Shared 3D radius for units, buildings and sector structures. Missing or 0 disables the blast. Range: 0..12000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Blast_energy = 4000` — Energy removed from units, buildings and sector structures inside the blast. When `energy` is above 0 the directly struck target takes only `energy`; with `energy = 0` it takes Blast_energy too. Range 0..100000000. The damage shown in the interface (DMG row, briefing, Database) is the higher of `energy` and `Blast_energy`; Blast_energy counts only when `Blast_radius` is active and the weapon is not a laser.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Blast_Push = 1.05` — Radial push intensity 0..10, using the usual target filters and `push_resistance`; GUN actors are excluded. When `push` is above 0 the directly struck unit takes only `push`; with `push = 0` it takes Blast_Push too. Displacement = 100 x (intensity x (1 - push_resistance))^2 world units, so halving the intensity quarters the displacement.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `blast_debuff_id = 2` — ID or name of the new_debuff profile applied by the explosion to every enemy inside `Blast_radius` (for example `shock`), even with `Blast_energy = 0`. 0, absent or unknown = none. The directly struck unit takes only `debuff_id` when it is set. Allies are not affected; Deflect blocks it; if mind control cannot apply, the other Debuff effects still do; Host Station and child rules apply.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Blast_falloff = 1` — 1 reduces blast energy and push with distance; 0 keeps them uniform inside the radius. With 1 the energy decays as exp(-distance x 2.8 / 1200) with a hard cut at `Blast_radius` (about 70% is left at 150 units), while the push decays linearly to 0 at the radius.
------------------------------------------------------------------------------------------------------------------------

Balance reference used by the modernized weapons (`max_active_at_once = 5` units). Rules of thumb, not engine limits:

- Direct push: displacement on a light tank (push_resistance 0.2) = 9 x (damage / 1000)^0.75 x min(1, shot_time / 1000)^0.8 world units; `push` is then sqrt(displacement / 100) / 0.8, rounded to 0.05. Fast-firing weapons therefore push very little per hit.
- AOE damage: `Blast_energy` = 50% of `energy`. Exceptions: kamikaze and the 20 Tons Taerbomb keep 100%; cluster bomblets keep 25% because many blasts overlap.
- AOE radius: `Blast_radius` = 0.36 x average ground_decal_size x (1.4 + 0.6 x t), where t grows from 0 to 1 with the damage between 5000 and 100000 (cluster and salvo weapons use 80% of it, maximum 650). A weapon whose result is below 45 units is precise: it has no AOE at all.
- Precise weapons without AOE (visible explosion too small): Foxxer Dart, Small Choomba and the cluster Bomblets.
- `Blast_Push` is about 70% of `push` (half the displacement), except bombs and kamikaze where it equals `push`.
- `Blast_radius` is about 1.4-2 times the visible explosion, so the sphere drawn by F9 stays close to what the player sees.

Chain Weapon propagation continues a hit from one enemy unit to another nearby target. Damage, search radius and delay are independently configurable.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `chain_max_jumps = 4` — Maximum number of additional jumps/bounces after the first impact.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `chain_radius = 1400` — Search radius for the next enemy target after each impact.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `chain_energy_mult = 0.75` — Damage cascade at each jump. 1.0 = no change; 0.75 = each jump deals 75% of the previous jump's damage; values above 1 increase it (1.5 = each jump deals 50% more).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `chain_jump_delay = 120` — Delay in milliseconds between an impact and the next chain jump. 0 = immediate propagation.
------------------------------------------------------------------------------------------------------------------------

### Projectile flight and physics

------------------------------------------------------------------------------------------------------------------------
- 🔵 `mass = 50` — Mass of the projectile. With force and airconst it decides acceleration and inertia.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `force = 15000` — Thrust of the projectile. For missiles it is also what pulls the projectile toward its target, so together with airconst and mass it sets how well a missile tracks.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `maxrot = 1.5` — How fast the model rotates to align with its flight direction once the engine has burned out (bomb phase, after drive_time). It does not make a missile track better; use force, airconst and homing_force for that.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `airconst = 40` — Air resistance. Higher values slow the projectile down and limit its top speed.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `start_speed = 270` — Initial speed of the projectile when it leaves the weapon.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `drive_time = 1200` — Duration of the engine thrust in milliseconds. Afterwards a missile enters its falling phase and a homing_bomb stops using `force` to pursue; both can still turn with `homing_force` while `homing_time` runs. A homing_bomb applies it only when given a positive value. Other weapons default to 7000; arc grenades do not use this phase.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `life_time = 1300` — Maximum lifetime in milliseconds of a projectile fired at a target; it detonates when it expires. Accepts `min~max`, rolled independently for each projectile. For `model = laser` it also limits the beam duration and length, and it is the player's lock reach: the primary lock and every multi-lock target appear at the same distance (other weapons lock up to 2000).
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `life_time_nt = 1300` — Same as `life_time`, used when the projectile is fired without a unit target (free fire, or a num_targets missile or homing_bomb slot with no target). Accepts `min~max`. 0 or absent = `life_time` is used.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `delay_time = 500` — Delay in milliseconds between impact and detonation (time-fused bombs); 0 = explodes on contact. Against units the projectile attaches to the target and explodes when the timer ends.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `homing_time = 3000` — Missile and homing_bomb only: guidance duration in ms. On expiry, a powered missile keeps thrusting along the direction it was flying when guidance ended until `drive_time` expires. A homing bomb stops steering and applying thrust, and continues under gravity and drag. 0, absent or invalid = no guidance time limit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `homing_force = 4` — missile and homing_bomb only: extra turn speed toward the target in radians per second (about 1 gentle, 4 firm, 10 almost locked), added to the normal tracking. Active while the projectile has a live target and `homing_time` has not expired, also after `drive_time` ends. When the player fires, it applies only with a locked (red) crosshair; AI units are unchanged. Range 0-10; 0, absent or invalid = vanilla.
------------------------------------------------------------------------------------------------------------------------

Spiral and Chaos are alternative projectile-motion modifiers. If both are configured and valid, Chaos takes priority.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `chaos_factor = 2.5` — Visual chaotic-motion strength: increases the frequency of smooth random direction changes. Range 0-10, decimals allowed; 0 = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `chaos_radius = 30` — Maximum random-deviation radius around the central projectile trajectory. Runtime range 0-1000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `boomerang_return_mode = lifetime` — lifetime returns after life_time; lifetime_or_impact also returns after normal impact damage. Return follows the current muzzle, can hit each unit once, and ends on arrival, launcher loss or after 30 seconds. Missing/invalid = off. Excludes laser, artillery_shell, kamikaze, delay_time and cluster children.
------------------------------------------------------------------------------------------------------------------------

### Rate of fire and bursts

------------------------------------------------------------------------------------------------------------------------
- 🔵 `shot_time = 500` — Time in milliseconds between two shots (used by AI-controlled units).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shot_time_user = 400` — Same as shot_time, used when the player controls the unit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `salve_shots = 3` — Number of shots in one burst before the salve_delay pause starts.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `salve_delay = 2000` — Pause in milliseconds between bursts. It replaces shot_time once a burst is complete.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_shot_time = -50` — Upgrade-only: adds the value to shot_time (negative values fire faster).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_shot_time_user = -50` — Upgrade-only: adds the value to shot_time_user.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fire_time_scale = 0.35` — Kamikaze, player only: time-scale intensity applied while FIRE is pressed.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fire_time_scale_hp_drain = 12%` — Kamikaze, player only: HP drained per second during slow motion. With %, uses maximum HP; without %, uses fixed HP. 0 or absent = disabled.
------------------------------------------------------------------------------------------------------------------------

### AI fire volumes

- **Alternatives:** the four `ai_fire_trigger_*` shapes exclude each other: configure one positive volume per Weapon. Several volumes or an incompatible model give a warning and keep the legacy fire control, except lasers, which need exactly one valid volume. A single `0` disables a volume; missing, negative, non-finite or incomplete values do not enable it. When changing shape in `modify_weapon`, clear the previous volume explicitly.
- **Authorization:** the volume only authorizes a firing event. Other salvo projectiles and extra targets keep their weapon rules and may fall outside it, and acquisition does not guarantee a hit after spread and patterns.
- **Target choice:** the current unit or sector-point target has priority; otherwise the nearest valid enemy inside the volume is used (units overlap by broad collision radius, sector points by radius 0). The target center must be ahead of the muzzle for forward shapes, or below it for downward shapes.
- **Muzzles:** volumes span the whole `fire_x` interval (vanilla, sequence, random, salve modes and racks) without advancing slots or consuming random values.
- **Distance:** the depth sets the acquisition distance independently of `ai_attack_range` and the retreat ranges, which keep steering approach, retreat and re-engagement.
- **Downward shapes:** finite depth; acquisition, secondary-target choice and continuous vertical-laser retention share the volume (actual beam reach still applies).
- **Ground units:** tanks and cars use forward volumes within the vanilla reticle travel, narrowed by `scope_angle_min_max`; a broad volume cannot authorize a vertical shot. Guns keep their own mechanical stops.
- **Scope:** volumes apply to AI attacks, including guns firing normal Weapons. Manual fire, MGUN, kamikaze and artillery_shell are unchanged. Debug: F9 draws the volume of the selected Weapon slot in orange.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `ai_fire_trigger_box_below_xyz = 200|1200|400` — AI only: finite downward box for bomb, homing_bomb and downward laser, `total_width|vertical_depth|total_length` (X|Y|Z). It starts at the muzzle, follows the heading and stays vertical despite pitch or roll. Width covers the whole `fire_x` interval plus padding; Y is a finite depth independent of terrain height. Units are acquired by sphere contact at the sides and end cap; sector targets by point containment. A single 0, absent or invalid value keeps the legacy fire control.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ai_fire_trigger_cone_below_xyz = 30|20|1200` — AI only: downward elliptical cone for bomb, homing_bomb and downward laser, `lateral_angle|fore_aft_angle|vertical_depth`. Both full angles are in degrees, strictly between 0 and 180; the third value is the finite depth straight down. The cone stays vertical and follows the heading, with its origin at the muzzle, and covers the whole `fire_x` interval.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ai_fire_trigger_box_forward_xyz = 100|80|1200` — AI only: muzzle-local forward box, `total_width|total_height|axial_depth`. Width and height are centered on the firing axis and the volume follows pitch and roll. Models: rocket, missile, grenade, arc_grenade and non-downward laser.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ai_fire_trigger_cone_forward_xyz = 30|20|1200` — AI only: forward elliptical cone, `horizontal_angle|vertical_angle|axial_depth`. Both full angles are in degrees, strictly between 0 and 180; 30|20 means +/-15 horizontally and +/-10 vertically. Depth ends at a flat cap. Same models as the forward box.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ai_fire_trigger_forward_max_inclination = 25` — Per Weapon with a forward box or cone: maximum downward tilt toward ground targets, in degrees, 0 to below 90. Helicopters, fixed-wing aircraft and UFOs only: when attacking a ground target ahead of the nose, the volume tilts downward (never upward) toward the aim point up to this angle, so a plane passing over its target does not fire straight down. Tanks, cars and guns ignore it and downward volumes do not tilt. 0 or absent = vanilla (the volume stays on the hull axes).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ai_fire_trigger_dead_zone = 150` — Per Weapon with any box or cone: minimum axial distance from the muzzle. Targets inside this near dead zone do not trigger fire; Z remains the far limit. A large unit can trigger when its collision sphere touches the near limit. Zero or absent disables the dead zone.
------------------------------------------------------------------------------------------------------------------------

### Energy cost and progressive fire rate

------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_energy_cost = 0.5%` — Per-Weapon energy cost charged once per successful firing event, regardless of projectile count. 0% disables the legacy drain; absent/invalid preserves vanilla per-projectile drain.
------------------------------------------------------------------------------------------------------------------------

Progressive fire rate accelerates a continuously firing Weapon from its normal shot interval toward a minimum. Releasing FIRE or breaking the sequence resets the ramp.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `ramp_up_time = 4000` — Time in milliseconds to reduce shot interval progressively from normal shot_time/shot_time_user to ramp_up_max_shot_time. 0/absent/invalid = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ramp_up_max_shot_time = 150` — Minimum shot interval reached at the end of ramp_up_time. Ramp progress resets when FIRE is released or the sequence is interrupted.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ramp_up_max_weapon_spread_xy = 1|1` — Final random spread of this Weapon at the end of ramp_up_time (format x|y, horizontal_vertical). Decreases the X and Y of the Vehicle's weapon_spread_xyz without increasing an already lower value. Requires active ramp-up; 0/absent/invalid = disabled for that axis.
------------------------------------------------------------------------------------------------------------------------

Optional overheat behavior starts only after the Weapon has reached and then maintained ramp_up_max_shot_time for the configured delay.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `ramp_up_overheat_hp_drain = 4%` — Percentage of overheat damage applied after the Weapon reaches and maintains ramp_up_max_shot_time.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ramp_up_overheat_time = 5000` — Time in milliseconds spent at ramp_up_max_shot_time before overheat damage begins.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ramp_up_overheat_icon = Graphics/StatusIcons/overheat.svg` — Data-relative icon shown while the unit is overheating.
------------------------------------------------------------------------------------------------------------------------

### Collisions

------------------------------------------------------------------------------------------------------------------------
- 🔵 `radius = 50` — Collision radius of the projectile against units. Used for every unit class whose `radius_*` value is 0.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `radius_tank = 10` — Collision radius against ground vehicles (tank and car).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `radius_heli = 20` — Collision radius against helicopters.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `radius_flyer = 20` — Collision radius against aircraft (plane, glider, zeppelin and ufo).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `radius_robo = 30` — Collision radius against Host Stations.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `overeof = 0` — Vertical offset used by the ground collision. Rarely needs changing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vwr_radius = 0` — Radius used by the viewer camera when following the projectile. Rarely needs changing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vwr_overeof = 0` — Vertical viewer offset for the projectile. Rarely needs changing.
------------------------------------------------------------------------------------------------------------------------

### Appearance (models, VP, 3DS, trails)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_normal = 124` — ID of the visual model (VP) of the projectile in flight. This is the one that matters most.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_dead = 125` — ID of the model shown when the projectile explodes on a unit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_megadeth = 125` — ID of the model shown when the projectile explodes on terrain or buildings.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_wait = 22` — ID of the idle model. Rarely visible on vanilla weapons.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_fire = 22` — ID of the launch model. Usually has no visible effect.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_genesis = 22` — ID of the creation model. Usually has no visible effect.
------------------------------------------------------------------------------------------------------------------------

Physical Bruch fragments use the same sound, palette and shake families as Vehicles. See [Bruch](#bruch-bruchcfg).

Weapon Ground Decal profiles paint a persistent or timed footprint on the world around the impact: terrain and buildings, never units. The decal is created only after a real projectile/world collision.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `ground_decal_id = 1` — ID of the reusable new_decal profile. Vehicles spawn on grounded destruction; Weapons spawn on real world impact. -1, absent or unknown ID = no decal.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ground_decal_size = 80~120` — Manual diameter range in world units, 0..12000. Absent, zero or invalid disables the decal.
------------------------------------------------------------------------------------------------------------------------

**Craters and dents (Weapons only).** A real impact on the world deforms what it hits, visually and in collision.

- **Ground:** on sector ground and on the seams between sectors (slopes up to about 50 degrees) it digs a crater: units drive into it and the lower parts of nearby buildings bend with the ground.
- **Buildings:** on roofs, walls and steeper slopes it makes a dent of the same radius and depth, pushed into the surface. This works on every building type.
- **Never deformed:** Host Stations and units. A direct hit on a ground unit marks nothing on it: the crater and the decal go on the ground under the hit point. AOE does not count.
- **Stacking:** overlapping hits add up to at most 1.75 times the strongest.
- **Lifetime:** deformations are saved with the game. Destroying a building removes the dents from hits on that piece; terrain craters last for the mission, subject to the limit or to new construction in that sector. Disabled in multiplayer.
- **Decal:** the weapon decal follows the crater or dent and stays whole on the pit floor and steep walls. This needs float render targets (OpenGL 3, or ARB_texture_float and ARB_color_buffer_float); otherwise the decal is simply projected on the dug surface.
- **Map border:** the crater fades out near the border so the border stays whole. `game.deformation_max_craters` sets how many craters are kept.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `crater_radius = 260~320` — Radius of the crater (and of the dent on buildings) in world units; fixed value or `min~max` (0..12000). Needs `crater_depth`: if either is absent, zero or invalid there is no crater (vanilla).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `crater_depth = 60~80` — Depth of the crater centre (or how far the dent goes in) in world units, fixed value or `min~max` (0..12000). Never more than half the radius: walls stay below about 37 degrees, so tanks can drive in and out. The dug ground is also darkened by baked light; craters shallower than 20 darken proportionally less, so a small crater leaves no dark patch.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `crater_rim = 20` — Raised rim around the crater (ground only), in % of the depth, fixed value or `min~max` (0..100). Absent = 20; 0 = no rim; invalid = no crater.
------------------------------------------------------------------------------------------------------------------------

Weapons use the same state visuals as Vehicles: 3DS has priority over BASE, and BASE falls back to the corresponding VP.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_<state> = Data/Models/Base/PROJECTILE.BASE` — External BASE model for a state (normal, fire, wait, dead, megadeth, genesis); falls back to `vp_<state>`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_<state> = Data/Models/3ds/projectile.3ds` — External 3DS model for a state (same states as `base_<state>`); takes priority over `base_<state>` and `vp_<state>`.
------------------------------------------------------------------------------------------------------------------------

Mesh tracers are an extra visual layer that follows the real projectile trajectory next to the main VP/BASE/3DS visual. They never change trajectory, speed, collision or damage.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `tracer_3ds = Data/Models/3ds/mgun_tracer_cross.3ds` — External 3DS model used by the Weapon tracer. Only a 3DS is accepted: the tracer is bent and shaded vertex by vertex, which VP and BASE models cannot do.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tracer_tint = 255|80|80|180` — General R|G|B|A tracer tint. Alpha 0 disables the effect.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tracer_head_tint = 255|80|80|180` — R|G|B|A tint of the tracer head. Absent = tracer_tint.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tracer_tail_tint = 255|80|80|180` — R|G|B|A tint of the tracer tail. Absent = tracer_tint.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tracer_size_xyz = 4|4|100` — Tracer size (format x|y|z): X base width (range 0..100), Y size (range 0..100; invalid inherits X), Z maximum visible length along the Weapon's real trajectory (range 0..6000). A 0/invalid X or Z disables the tracer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tracer_pos_xyz = 0|0|0` — Local X|Y|Z position relative to Weapon launch point. Clamped to -6000..6000. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tracer_head_size = 4` — Uniform X/Y head size. Range 0..100; 0 creates a point; absent/invalid uses size_x/size_y. Z length is controlled only by size_z to keep the tracer continuous.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tracer_tail_size = 4` — Uniform X/Y tail size. Range 0..100; 0 closes the tail; absent/invalid uses size_x/size_y. Z length is controlled only by size_z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tracer_glow_rate = 0` — Glow intensity, range 0..10. Changes brightness only.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tracer_noise_rate = 0` — Stable random trail distortion/noise, range 0..10. 0 = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tracer_pulse_rate = 0` — Width-pulsation intensity, range 0..10. 0 = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tracer_pulse_speed = 0` — Pulsation speed in cycles per second, range 0..10. 0 = disabled; requires tracer_pulse_rate > 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tint = 40|120|255|255` — RGBA tint applied to the main VP/BASE/3DS visual. It never tints the Genesis plasma: the Genesis uses the faction colour, and the Resistance keeps the original cyan.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spin_xyz = 0|0|10` — X|Y|Z spin intensity of the main VP/BASE/3DS visual: 0-10, decimals allowed; 0 = no spin. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `scale_xyz = 1.0|1.0|3.0` — X|Y|Z visual scale of all VP/BASE/3DS states, including dead and megadeth. Does not change collision geometry. Default: 1.0. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `rotation_xyz = 0|0|0` — Fixed X|Y|Z visual rotation in degrees. 0 or absent = no rotation. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------

VP trail controls affect only embedded VP trails, particles and children; they never tint or transform an external 3DS model.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_trail_scale_xyz = 1.0|1.0|1.0` — X|Y|Z scale of VP trails, particles and embedded children only. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_trail_spin_xyz = 0|0|10` — X|Y|Z spin intensity of VP trails, particles and embedded children only: 0-10. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_trail_tint = 255|255|255|255` — RGBA tint applied only to VP trails, particles and embedded children; does not modify a 3DS model.
------------------------------------------------------------------------------------------------------------------------

Weapon decorations use the same periodic/persistent system as Vehicles, with offsets relative to the parent Weapon.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_mode = periodic` — Periodic decoration mode.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp = 99` — Legacy VP decoration visual; final fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_base = Data/Models/Base/FX/DECORATION.BASE` — Data-relative BASE family; falls back to decoration_fx_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_3ds = Data/Models/3ds/FX/decoration.3ds` — 3DS decoration model; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_scale_xyz = 1.0|1.0|1.0` — X|Y|Z scale of active decoration. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_spin_xyz = 0|0|0` — X|Y|Z spin intensity: 0-10, decimals allowed; 0 = no spin. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_tint = 255|95|70|200` — RGBA tint applied to the main decoration visual.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_fade_inout = 500|500` — Main VP/3DS decoration fade-in and fade-out in milliseconds (format in|out). 0 or absent = immediate appearance/disappearance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_tint = 255|255|255|255` — RGBA tint of VP decoration particles/trails. Alpha 0 = invisible.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_scale_xyz = 1.0|1.0|1.0` — X|Y scale of VP decoration particles/trails, plus a Z multiplier for particle/trail duration and therefore apparent trail length (format x|y|z).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_fade_inout = 500|500` — Time in milliseconds (format in|out) for VP decoration particles/trails to fade in to their configured opacity (in) and to fade out to invisible (out). 0 or absent = immediate.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_duration = 1000` — Total periodic-decoration duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_interval = 500~1500` — Random interval in milliseconds between spawns. A single value gives a fixed interval.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_count = 2~7` — Random decoration count per spawn. A single value gives a fixed count.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_random_pos = 90` — XYZ position randomization around the parent Vehicle/Weapon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_offset_xyz = 0|0|-40` — X|Y|Z offset from the parent Vehicle/Weapon center. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_mode = persistent` — Persistent mode: the decoration stays active while the parent Weapon is active. It uses `decoration_fx_vp`, `base`, `3ds`, `scale_xyz`, `spin_xyz`, `tint`, `fade_inout`, `vp_trail_*` and `offset_xyz`; `duration`, `interval`, `count` and `random_pos` are not used.
------------------------------------------------------------------------------------------------------------------------

### Impact effects

------------------------------------------------------------------------------------------------------------------------
- 🔵 `dest_fx = death_46_0_0_0` — Effect spawned on impact. Format type_ID_X_Y_Z: death when it hits a unit, megadeth when it hits terrain or buildings, then the effect ID (Bruch first, then a legacy Vehicle) and its X, Y and Z offset. The | separator is also accepted.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `ext_dest_fx = megadeth_17_10_-20_0` — Extended variant of dest_fx with the same format. Rarely used. Alias: extended_dest_fx.
------------------------------------------------------------------------------------------------------------------------

Weapon `begin_fx` uses the same Bruch reference and emission fields as [Vehicle effects](#destruction-and-effects).

------------------------------------------------------------------------------------------------------------------------
- 🟢 `begin_fx` — Starts an effect reference. Close the block with `end`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `trigger = impact_world|detonate|launch` — Runs on terrain or building impact, detonation or launch. Laser detonation on world contact uses the real beam endpoint. Launch starts at the projectile or beam origin. Join events with `|`. Any unknown trigger rejects the block.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `bruch_id = 1001` — Uses the same [Bruch lookup](#bruch-bruchcfg) as Vehicle effects.
------------------------------------------------------------------------------------------------------------------------

### Debuffs

------------------------------------------------------------------------------------------------------------------------
- 🟢 `debuff_id = 1` — ID or name of the new_debuff profile applied to the target struck directly by this Weapon (for example `debuff_id = shock`). It does not reach the blast area: use blast_debuff_id for that. Names must start with a letter and ignore case; if two profiles share a name, the lowest ID is used. -1, absent, or an unknown ID or name = no Debuff.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ai_avoid_debuffed_target_change = 70%` — AI only: chance, 0%-100%, that a unit carrying this Weapon ignores a "taken" enemy when it picks a target on its own and attacks another one instead. An enemy is taken while it carries a Debuff that one of this unit's Weapons applies (Debuffs of other profiles stack and do not count), or as soon as another allied unit with a Debuff Weapon has chosen it as target (known at launch, not at impact). Works for `debuff_id` and `delay_time_debuff_id`. Rolled at each target search: in the remaining chance the unit picks as usual and may join the others. If no other enemy is in range, the taken one is still attacked. The unit's main and extra Weapons are checked and the highest value is used. It only affects the AI's own target search: attack orders from the player, the unit under direct control and fire volumes are unchanged, and below aggression 50 (no automatic search) it has no effect. Disabled in multiplayer. Missing, 0% or invalid (a number without `%`) = vanilla.
------------------------------------------------------------------------------------------------------------------------

### Sounds, palette flashes and shakes

------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_<event>_sample = sounds/launch.wav` — Sound file played for the event. Weapon events: normal (while flying), launch (when fired) and hit (on impact).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_<event>_volume = 150` — Volume of the event sound.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `snd_<event>_pitch = 0` — Pitch of the event sound. Accepts `min~max`.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_<event>_ext = 1_100_22050_0_0_sounds/extra.wav` — Additional sample. Format loop_volume_sampleRate_offset_sampleCount_file.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_<event>_fade_inout = 150|300` — Volume fade-in and fade-out of the event sound in milliseconds (format in|out). A one-shot sample fades out over its own length; a looping sound only fades in. If both fades are longer than the sound they shrink together. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pal_<event>_slot = 2` — Screen color flash: effect type (1 red, 2 blue, 3 green, 4 inverse, 5 dark inverse, 6 black and white, 7 turquoise inverse).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pal_<event>_time = 500` — Duration of the flash in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pal_<event>_mag0 = 2` — Starting intensity of the flash.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pal_<event>_mag1 = 1` — Final intensity of the flash.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_<event>_fade_inout = 150|300` — Fade-in and fade-out of the color flash in milliseconds (format in|out). The fade-out ends together with `pal_<event>_time`; a looping effect only fades in. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<event>_slot = 1` — Camera shake: shake type.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<event>_time = 300` — Duration of the shake in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<event>_mag0 = 1` — Starting intensity of the shake.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<event>_mag1 = 0.5` — Final intensity of the shake.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<event>_x = 1` — Shake strength on the X axis.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<event>_y = 1` — Shake strength on the Y axis.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<event>_z = 1` — Shake strength on the Z axis.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<event>_mute = 0` — How much the audio is attenuated during the shake.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_<event>_fade_inout = 150|300` — Fade-in and fade-out of the camera shake in milliseconds (format in|out). The fade-out ends together with `shk_<event>_time`; a looping effect only fades in. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `snd_launch_sample = sounds/Laser.WAV` — Laser-specific behavior: non-looping launch sound played when FIRE begins/is held.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_launch_volume = 100` — Launch sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `snd_launch_pitch = 900~1100` — Launch sound pitch. Single value = fixed; `min~max` = random pitch selected on each playback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_launch_at_muzzle = 1` — Plays the launch sound from a fixed position at the shooter (like the MGUN) instead of from the moving projectile, avoiding Doppler/pan drift on fast bullets. Physical projectiles only, not lasers; launch shake/palette FX are not played. 0/absent = vanilla behavior.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `snd_normal_sample = sounds/Laser.WAV` — Laser-specific behavior: continuous looping sound used while the Laser is firing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_normal_volume = 100` — Normal/loop sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `snd_normal_pitch = 900~1100` — Normal/loop sound pitch. Single value = fixed; `min~max` = random pitch selected on each playback.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `snd_hit_sample = sounds/LaserHit.WAV` — Laser-specific behavior: serialized one-shot hit sound while the Laser contacts a surface. Continuous contact repeats it without overlapping instances.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_hit_volume = 100` — Hit sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `snd_hit_pitch = 900~1100` — Hit sound pitch. Single value = fixed; `min~max` = random pitch selected on each playback.
------------------------------------------------------------------------------------------------------------------------

Player-only launch shake is separate from world-space SHK effects: it shakes only the player's view, not nearby AI or other observers.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_launch_player_slot = 4` — Shake slot generated when this Weapon launches, for the player only.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_launch_player_time = 500` — Player-only launch-shake duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_launch_player_mag0 = 1.5` — Initial player-only launch-shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_launch_player_mag1 = 0.5` — Final player-only launch-shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_launch_player_fade_inout = 150|300` — Fade-in and fade-out of the player-only launch shake in milliseconds (format in|out). The fade-out ends together with `shk_launch_player_time`; a looping effect only fades in. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_cluster_slot = 4` — Palette slot used at split. 0/absent = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_cluster_mag0 = 5` — Initial split palette intensity. Default: 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_cluster_mag1 = 0` — Final split palette intensity. Default: 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_cluster_time = 1500` — Split palette duration in milliseconds. Default: 1000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_cluster_radius = 4000` — Maximum cluster palette-effect radius. Intensity decreases linearly to 0 at the limit. 0 or absent = legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_cluster_fade_inout = 150|300` — Fade-in and fade-out of the cluster split palette effect in milliseconds (format in|out). The fade-out ends together with `pal_cluster_time`; a looping effect only fades in. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_cluster_slot = 1` — Shake slot used at split. 0/absent = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_cluster_mag0 = 2` — Initial split-shake intensity. Default: 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_cluster_mag1 = 0` — Final split-shake intensity. Default: 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_cluster_time = 5000` — Split-shake duration in milliseconds. Default: 1000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_cluster_radius = 4000` — Maximum cluster-shake radius. Intensity decreases linearly to 0 at the limit. 0 or absent = legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_cluster_fade_inout = 150|300` — Fade-in and fade-out of the cluster split shake in milliseconds (format in|out). The fade-out ends together with `shk_cluster_time`; a looping effect only fades in. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_cluster_mute = 0.02` — Legacy cluster-shake distance attenuation factor used only when shk_cluster_radius = 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_cluster_xyz = 0.2|0.2|0.2` — Random cluster-shake strength multiplier on the X|Y|Z axis. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_cluster_sample = sounds/Smex02.wav` — Sound played when clustering activates.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_cluster_pitch = 0~500` — Cluster split sound pitch. Single value = fixed; `min~max` = random pitch selected on each playback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_cluster_volume = 120` — Split sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_cluster_radius = 4000` — Maximum cluster-sound radius. Volume decreases linearly to 0 at the limit. 0 or absent = legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_cluster_fade_inout = 150|300` — Volume fade-in and fade-out of the cluster split sound in milliseconds (format in|out). A one-shot sample fades out over its own length; a looping sound only fades in. If both fades are longer than the sound they shrink together. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------

### OpenNeoUA special features

Cluster Weapons split into child Weapons after a timer or on impact. Child count, direction, spread and generations are data-driven; keep generations and counts small, because they multiply quickly.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_count = 2~8` — Number of child Weapons generated per split. Single value = fixed count; `min~max` = inclusive random count. Safe range 1-64; invalid values disable clustering.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_weapon_id = 3` — Weapon ID generated as the cluster child.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_trigger_time = 1000` — > 0 = timed cluster trigger in milliseconds. 0 or absent = cluster splits on impact.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_horizontal_angle = 0~360` — Horizontal cluster direction. Single value = fixed angle; `min~max` = range. `0~360` distributes clusters uniformly.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_vertical_angle = -45~45` — Vertical cluster direction. Single value = fixed angle; `min~max` = random angle in range.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_spread_xy = 60|60` — Random X|Y deviation applied after the base angle direction; may be combined with angle parameters. Format: x|y.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_ai_group_radius = 600` — AI Smart only (weapon_ai_switch_mode = smart). Enemies usually enter a fire volume one at a time, so a lone enemy counts as a group when another enemy stands within this radius of the unit the shot will go to. The AI then prefers this cluster Weapon. 0 or absent = only enemies inside the volume count (two are required). Maximum 5000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_vp = 112` — Legacy VP generated when the Weapon splits; final fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_base = Data/Models/Base/FX/CLUSTER_SPLIT.BASE` — BASE family generated on split, relative to Data; falls back to cluster_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_3ds = Data/Models/3ds/FX/cluster_split.3ds` — 3DS split model; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_generations = 3` — Allows generated cluster Weapons to trigger their own clustering for the configured number of generations.
------------------------------------------------------------------------------------------------------------------------

## Buildings (Buildings.cfg)

### Identity and cost

------------------------------------------------------------------------------------------------------------------------
- 🔵 `new_building = 10` — Opens the definition of a new Building with the given numeric ID. If the ID already exists, the prototype is rebuilt from scratch.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `modify_building = 10` — Reopens an existing Building so that only the listed values change. Typical use is level files and tech-upgrade blocks.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `end` — Closes the current new_building / modify_building block.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `enable = 1` — Makes the Building available to the given faction (1 Resistance, 2 Sulgogar, 3 Mykonian, 4 Taerkasten, 5 Black Sect, 6 Ghorkov, 7 neutral/tutorial). One line per faction.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `disable = 1` — Removes the Building from the given faction.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `model = kraftwerk` — Building class: building (generic), kraftwerk (power station), radar and defcenter (defense center). Unknown values make the block invalid.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `name = Power_Station` — Name of the Building. Underscores are shown as spaces.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `sec_type = 20` — Sector of the Building: a sector number of the map set (vanilla), or a `Data/....sector` file, optionally followed by a set sector used when that file cannot be loaded (`sec_type = Data/Models/Sector/Sulg_Power/Sulg_Power.sector|50`). See [External sector files (.sector)](#external-sector-files-sector). An invalid value is ignored and the previous sector is kept; a file without fallback falls back to sector 0.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `type_icon = P` — Icon glyph key of the Building, mapped through TYPE_NS.FON into the H_FINDER graphic. Same syntax as the Vehicle `type_icon`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `production_cost = 1400` — Displayed Genesis price for this building. Overrides the vanilla Energy-derived price without changing hit points. 0 or absent = vanilla behavior (Energy / 100).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `building_time = 4.4` — Construction time in seconds (0-100) for the building to emerge from the terrain and become operational. Same rules as for Vehicles; buildings have no Genesis pitch.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_genesis_sample = sounds/lowapp.wav` — Construction sound of this building, emitted at its position and looped from placement until completion. Overrides the Host Station `snd_build` only when a `snd_genesis_*` key is authored; 0 disables it; without Genesis keys the legacy sound is kept. Independent of `game.building_visual_genesis`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_genesis_volume = 120` — Volume of this building’s construction sound.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_genesis_pitch = 0` — Fixed pitch, `min~max` range or `auto`. With `building_time` the time sets the pitch (4.4 = cost 440, 10 = cost 1000; above 10 unchanged); without it a number or range is used as written and `auto` follows the energy.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_genesis_fade_inout = 150|300` — Volume fade-in and fade-out of the construction sound in milliseconds (format in|out). A one-shot sample fades out over its own length; a looping sound only fades in. If both fades are longer than the sound they shrink together. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_genesis_slot = 1` — Construction palette slot of this building. 0 or absent = off. The effect lasts the whole construction (`building_time`, or the vanilla time), going from `mag0` to `mag1`; there is no time key. Independent of audio and visual plasma.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_genesis_mag0 = 0.25` — Initial palette effect intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_genesis_mag1 = 0` — Final palette effect intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_genesis_fade_inout = 150|300` — Fade-in and fade-out of the construction palette effect in milliseconds (format in|out). The fade-out ends when the construction ends. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_genesis_slot = 1` — Enables the construction camera shake of this building. 0 or absent = off. Like the palette it lasts the whole construction, from `mag0` to `mag1`, with no time key; axes and distance fade use the same defaults as vehicle sound events.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_genesis_mag0 = 0.5` — Initial shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_genesis_mag1 = 0` — Final shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_genesis_fade_inout = 150|300` — Fade-in and fade-out of the construction shake in milliseconds (format in|out). The fade-out ends when the construction ends. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `building_description = Defense outpost` — Optional building description shown in the Database. If absent, the Description row is not shown.
------------------------------------------------------------------------------------------------------------------------

### Stats

------------------------------------------------------------------------------------------------------------------------
- 🔵 `energy = 50000` — Hit points of the Building. In vanilla it is also its cost; production_cost overrides the price only.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `power = 100` — Energy produced or supplied by the Building. Mainly used by power stations.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shield = 20` — Armor as a percentage from 0 to 100, reducing incoming damage. Values outside the range are ignored.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `radar = 2` — Radius in sectors the Building reveals around itself (radar). Negative values are clamped to 0.
------------------------------------------------------------------------------------------------------------------------

### Briefing model settings for buildings

------------------------------------------------------------------------------------------------------------------------
- 🟢 `briefing_size = 100` — Size of the building shown by its prototype in the briefing and debriefing (a gate or tech building, for example), 1-10000. 100 = classic framing; larger enlarges the model. Zero, missing or invalid = classic framing.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `briefing_xyz = 0|0|0` — Preview offset in the same screen axes as the Vehicle setting (X right, Y down, Z away). Zero keeps the original position.
------------------------------------------------------------------------------------------------------------------------

### Flaks mounted on the building

------------------------------------------------------------------------------------------------------------------------
- 🔵 `sbact_act = 0` — Selects which attached flak slot is being configured, starting at 0. The `sbact_*` lines that follow apply to it. Repeat the block for each turret.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sbact_vehicle = 95` — ID of the Vehicle used as the turret. It should be a model = gun unit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sbact_pos_x = 0` — X position of the flak relative to the Building center.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sbact_pos_y = -80` — Y position of the flak. Negative values place it above the Building.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sbact_pos_z = 0` — Z position of the flak (forward / backward). It does not set where the flak looks.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sbact_dir_x = 0` — Initial facing X of the flak when it spawns.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sbact_dir_y = 0` — Initial facing Y of the flak.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sbact_dir_z = 1` — Initial facing Z of the flak. These are not movement limits: those belong to the gun Vehicle (gun_side_angle, gun_up_angle, gun_down_angle).
------------------------------------------------------------------------------------------------------------------------

### Appearance

Building decorations can be periodic (spawned at intervals) or persistent (active while the building is active and alive). Positions are relative to the center of the building sector.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_mode = periodic` — Periodic decoration mode: decorations appear intermittently using the timing/count parameters below.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp = 99` — Legacy VP decoration visual; final fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_base = Data/Models/Base/FX/DECORATION.BASE` — BASE decoration family, relative to Data; falls back to decoration_fx_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_3ds = Data/Models/3ds/FX/decoration.3ds` — 3DS decoration model; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_scale_xyz = 1.0|1.0|1.0` — X|Y|Z scale of the active decoration visual. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_spin_xyz = 0|0|0` — X|Y|Z spin intensity for the decoration VP/3DS visual: 0-10, decimals allowed; 0 = no spin. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_tint = 255|95|70|200` — RGBA tint applied to the main decoration visual.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_fade_inout = 500|500` — Main VP/3DS decoration fade-in and fade-out in milliseconds (format in|out). 0 or absent = immediate appearance/disappearance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_tint = 255|255|255|255` — RGBA tint of VP decoration particles/trails. Under additive blending, fading also attenuates visual intensity correctly.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_scale_xyz = 1.0|1.0|1.0` — X|Y scale of VP decoration particles/trails, plus a Z multiplier for particle/trail duration and therefore apparent trail length (format x|y|z).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_fade_inout = 500|500` — Time in milliseconds (format in|out) for VP decoration particles/trails to fade in to their configured opacity (in) and to fade out to invisible (out). 0 or absent = immediate.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_duration = 1000` — Total duration in milliseconds of each periodic decoration instance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_interval = 500~1500` — Random interval in milliseconds between decoration spawns. A single value gives a fixed interval.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_count = 2~7` — Random number of decorations generated per spawn. A single value gives a fixed count.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_random_pos = 90` — XYZ position randomization around the center of the building sector.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_offset_xyz = 0|0|-40` — X|Y|Z offset from the center of the building sector. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_mode = persistent` — Persistent mode: the decoration stays active while the building is active and alive. It uses `decoration_fx_vp`, `base`, `3ds`, `scale_xyz`, `spin_xyz`, `tint`, `fade_inout`, `vp_trail_*` and `offset_xyz`; `duration`, `interval`, `count` and `random_pos` are not used.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tint = 60|255|60|255` — RGBA tint applied to the building's main visual.
------------------------------------------------------------------------------------------------------------------------

### External sector files (.sector)

Each sector has a folder under `Data/Models/Sector`. Put the four damage-state visuals and their collisions in the sector folder, or group them in subfolders by piece. Shared ILBM textures for the active set can still live in `Data/Models/Sector/SetN/Textures`.

A `.sector` file describes one complete sector outside SET.BAS: cells, damage states, models, collision, map symbols and explosions. A Building uses it through `sec_type`.

- The sector lives in its own folder, normally `Data/Models/Sector/<Name>/<Name>.sector`, with only the files made for it.
- A local BASE is resolved relative to the `.sector`, or by its explicit `Data/...` path. It uses the same ILBM lookup order as a vehicle BASE (see the model folder rules above), starting with `Textures/SetN/` beside that BASE. Collision files resolve relative to the `.sector` and fall back to the active set.
- On every set load the sector is installed in free model, piece and sector slots (highest free sector slot from 247 down); SET.BAS and SET.SDF are not changed. The slot can change when the set or the scripts change.
- Any error (syntax, unknown key, missing model, no free slot) skips the whole file, logs a warning and uses the fallback set sector.
- Cells of a map placed without an owner keep the sector written in typ_map; external sectors appear on owned buildings and on buildings constructed in game.
- Syntax: `;` starts a comment, one `key = value` per line, blocks can be in any order.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `begin_sector` — Opens the sector block (exactly one per file). Close it with `end`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cells = G|G|G|G|CORE|G|G|G|G` — Piece names of the cells: 9 for a 3x3 sector (same order as SET.SDF, far row first) or 1 for one large cell. Required.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ground = 0` — Connection family 0-5 (SET.SDF surface type): picks the transition strip to the neighbouring sectors. Required.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `map_symbol = 3` — Map symbol 0-255 of the sector. Default 0. Inside a `begin_model` block it is the symbol of that model, default inherited from the set model or 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `map_icon = Map_Icons/sector.svg` — Optional PNG or SVG map icon replacing `map_symbol`, beside the .sector or a `Data/...` path. In `begin_sector`: the symbol of the whole sector at map zoom 1 and 2 (4 and 8 px); 1 file for both or 2 files `zoom1|zoom2`. In `begin_model`: the symbol of a building cell at map zoom 3, 4 and 5 (4, 8 and 16 px, 3 x 3 cells for one large cell), following the damage state; 1 file for all or 3 files `zoom3|zoom4|zoom5`. SVG is rasterized at each target pixel size (`StatusIconLoad`), PNG is scaled; transparent pixels show the map. Absent or missing file = the `map_symbol` glyph (logged).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `begin_piece CORE` — Opens a piece: what fills one cell, with its four damage states. The name is unique among pieces and has no spaces. Close it with `end`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `states = ok|hit|heavy|flat` — Model names for intact, damaged, heavily damaged and destroyed. One name = the same model for all four states. Two or three names are invalid.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `destructible = 1` — 1 = the piece is a target with hit points, 0 = ground that cannot be destroyed. Default 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `begin_model ok` — Opens a model: one visual with its collision. The name is unique among models (a piece may use the same name). Close it with `end`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `visual = KRAFT_OK.BASE` — Visual of the model: a VP number of the set, a `.BASE` or `.3ds` file (beside the .sector, or a `Data/...` path), or the name of a set model (SET.SDF or override.lst name). A file that is not beside the .sector falls back to the set model with the same name; a set model also gives the defaults of collision, collision_mode, map_symbol, shield and explosion. Required.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `collision = KRAFT_OK.sklt` — Collision skeleton: a file beside the .sector, else a resource of the set. Default: the collision of the set model named in `visual`. Required when `visual` is a file or a VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `collision_mode = 0` — 0 = own collision skeleton, 1 = the set's simple sub-sector collision, 2 = the set's complex collision. Default inherited or 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shield = 60` — Damage reduction 0-100 % of the model. Default inherited or 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `begin_effects` — Optional block inside `begin_model` grouping its destruction `explosion = ...` lines (close with `end`). Opening it replaces inherited explosions (put `explosion = none` inside when no explosions are wanted). Legacy flat `explosion = ...` directly inside `begin_model` remains accepted.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `explosion = 49|0|0|100` — Destruction effect `ID|x|z|height` with the same numeric order as a SET.SDF effect group, placed inside `begin_effects` (or directly in `begin_model`). The ID resolves Bruch first, then the legacy Vehicle. Repeat the line for more explosions; the first line replaces the inherited ones and `explosion = none` removes them.
------------------------------------------------------------------------------------------------------------------------

```
begin_sector
    cells = G|G|G|G|CORE|G|G|G|G
    ground = 0
    map_symbol = 3
end
begin_piece G
    states = ground
end
begin_piece CORE
    states = core|core|core|ground
    destructible = 1
end
begin_model ground
    visual = ST_EMPTY.base
end
begin_model core
    visual = Kyt_Post.BASE
    collision = KYTPOST.sklt
    shield = 40
    begin_effects
        explosion = 49|0|0|100
    end
end
```

### Sounds

------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_normal_sample = sounds/power.wav` — Looping sound of the Building.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_normal_volume = 120` — Volume of the Building sound.
------------------------------------------------------------------------------------------------------------------------
- 🔵🟢 `snd_normal_pitch = 0` — Pitch of the Building sound. Ranges with `min~max` are accepted.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_normal_fade_inout = 150|300` — Volume fade-in and fade-out of the Building sound in milliseconds (format in|out). A one-shot sample fades out over its own length; a looping sound only fades in. If both fades are longer than the sound they shrink together. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------

## Bruch (Bruch.cfg)

Bruch profiles share the visuals and physics formerly defined inline in Vehicle and Weapon effects. The same profile can be used by many emitters. Legacy Bruch definitions keep the original Vehicle-style fields where they reproduce retail flight, state visuals, audio and chained effects. `scale_fx`, inline `mode = visual` and `mode = physical`, and `physical_vehicle` remain readable for older content.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `new_bruch 1000` — Defines an effect without a Vehicle slot. Profile IDs range from 1 to 65535. `SET.SDF` and external sector explosion groups keep their original tuple format and accept the same profile IDs. Numeric references resolve Bruch first, then the legacy Vehicle.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `begin_effect` — Starts the shared profile body. Duration and visual sources define a timed animation. A physical fragment needs positive mass and radius plus a visual source. If both are defined, the animation and the fragment spawn together. Close this block and the outer definition with separate `end` lines.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `duration = 1000` — Timed animation duration in milliseconds. Must be positive. Ordered visuals divide this duration evenly.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `start_size = 0.1` — Initial timed animation scale. Default = 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mid_size = 1` — Optional scale at half the animation duration. Without it, scale changes linearly from start to end.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end_size = 2` — Final animation scale. Default = 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fade_inout = 500|1000` — Timed animation fade in/out in milliseconds. Absent or 0 makes that edge immediate.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spin_xyz = 0|0|10` — Timed animation spin strength per local axis. Range 0–10. 0 disables spin on that axis.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_model = 138|193` — VP choices from the active set. For a timed animation, values play in order and repeated lines add frames. For a fragment, the last list supplies up to 32 random choices. Repeated entries increase their weight.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_model = Data/Models/Base/FX/FRAGMENT.BASE` — Optional BASE visual. In a timed sequence, put it after the matching VP to retain the VP fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_model = Data/Models/3ds/FX/fragment.3ds` — Optional 3DS visual. It takes precedence over BASE and VP alternatives for the same element.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tint = 255|255|255|255` — RGBA tint for timed visuals and fragment bodies. The last value applies to all elements. Omitted = original colours.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_impact = 140|69` — One impact VP per physical VP choice, in the same order. 0 disables that impact visual. An external-only model can use one impact VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `scale = 1.0` — Uniform physical model scale. Nonpositive or invalid = 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mass = 150` — Physical fragment mass. Must be positive.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `radius = 1.0` — Physical fragment collision radius. Must be positive.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `airconst = 40.0` — Physical fragment air resistance. Nonnegative, default 500.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `force = 6000` — Added impulse in the launch direction. Nonnegative, default 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `maxrot = 3.0` — Physical fragment rotation intensity. Nonnegative, default 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `follow_carcass = 1` — 1 lets each fragment follow the falling wreck until it lands. 0, absent or invalid gives the fragment independent flight.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `lifetime = 30000` — Maximum fragment flight time in milliseconds. Fixed or `min~max` per fragment. Missing, zero or invalid = 30000. Contact can end flight sooner.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `impact_energy = 200` — Damage on the first unit hit. Respects shields and invulnerability. 0 or absent disables damage. Single-player only.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `impact_energy_radius = 0` — Radius for additional impact damage. 0 restricts damage to the first direct target.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_normal_sample = Data/Sounds/shot7.wav` — Optional fragment spawn sample. The same sound, palette and shake families support `normal`, `goingdown` and `explode`.
------------------------------------------------------------------------------------------------------------------------

```
new_bruch 1000
    name = Landing_Debris
    begin_effect
        vp_model = 138
        vp_impact = 140
        mass = 60
        airconst = 1
        force = 1200
        maxrot = 3
        radius = 10
        lifetime = 30000
    end
end
```

## Buffs (Buffs.cfg)

Buffs are reusable profiles linked from a Vehicle with `buff_id`. `new_buff` may appear in any script under Data/Scripts (Buffs.cfg is the recommended file) and never inside `new_vehicle`.

A unit can carry any number of different Buffs at once and they never replace each other, the same rule as Debuffs. Each profile keeps its own icon and look (only Buffs that are currently working show their icon and glow). Several Deflect Buffs add their charges together and are spent in the order the unit received them. An overload breaks only the Deflect in use, while laser contact breaks every Deflect. Invisibility and invulnerability are active as long as any Buff provides them.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `new_buff 1` — Starts reusable Buff profile ID 1. Multiple Vehicles may reference the same ID through buff_id.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `name = armored` — Internal Buff name. This identifier does not change gameplay by itself; it labels the reusable Buff profile.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `icon = Graphics/StatusIcons/deflect.svg` — Data-relative Status Icon selected directly by this Vehicle's Buff. Empty/invalid = no Buff icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `glow_intensity = 0.20` — Full-model Buff glow intensity, range 0..1, while at least one Buff property remains active. 0 or absent = no glow.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `glow_tint = 100|180|255|255` — RGBA tint of the Buff glow. Visual only: does not modify materials or gameplay values.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `glow_pulse_seconds = 1.25` — Duration in seconds of one complete Buff-glow pulse. 0 or absent = steady glow. Runtime maximum: 60 seconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `invisible = 1` — Keeps the unit invisible until it performs its first real attack. On reveal, this Buff property ends permanently.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `invisible_reveal_vp = 229` — Legacy VP generated when invisible is revealed; final fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `invisible_reveal_base = Data/Models/Base/FX/REVEAL.BASE` — BASE reveal effect, relative to Data; falls back to invisible_reveal_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `invisible_reveal_3ds = Data/Models/3ds/FX/reveal.3ds` — 3DS reveal effect; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `invulnerable = 1` — Makes the unit immune to damage while this Buff property remains active.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `deflect_charges = 4` — Deflect charges available when the unit is created. Each normal projectile within the supported threshold consumes one charge and is physically deflected; Deflect ends at 0 charges. MGUN hits are blocked without consuming charges.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `deflect_max_energy = 5000` — Maximum attack energy Deflect can stop. If a projectile or AOE exceeds this threshold, all remaining Deflect charges are destroyed and the attack passes through normally. 0 or absent = no energy limit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `deflect_end_energy_reduction = 50%` — One-hit damage reduction applied when an attack exceeds deflect_max_energy. The hit still destroys all Deflect charges and passes through, but that single damage event is reduced by the configured percentage.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `deflect_vp = 229` — VP effect generated when Deflect physically redirects a projectile; final fallback after 3DS and BASE.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `deflect_base = Data/Models/Base/FX/DEFLECT.BASE` — BASE effect generated when Deflect redirects a projectile; falls back to deflect_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `deflect_3ds = Data/Models/3ds/FX/deflect.3ds` — 3DS effect generated when Deflect redirects a projectile; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end` — Ends the new_buff profile.
------------------------------------------------------------------------------------------------------------------------

## Debuffs (Debuffs.cfg)

Debuffs are reusable profiles linked from a Weapon with `debuff_id` (SuperItems have their own `debuff_id`). `new_debuff` may appear in any script under Data/Scripts (Debuffs.cfg is the recommended file) and never inside `new_weapon`. Host Stations stay protected from mind control and stun even when Debuffs may affect Host Stations.

A unit can carry any number of different Debuffs at once and they never cancel or replace each other. Each profile keeps its own duration, damage ticks, icon and visuals. A new hit by a profile the unit already carries only refreshes that one (and adds a layer with `stack_max`). The maluses of different Debuffs multiply, so they never pass 100% together. With several stuns the strongest `stun_motion_level` applies, and unit fire is blocked if any stun blocks it. The unit takes the tint of the newest Debuff that defines one. All Debuffs share one sound channel: the last one applied with a sound owns it.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `new_debuff 1` — Starts reusable Debuff profile ID 1. Multiple Weapons and SuperItems may reference the same ID through debuff_id.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `allow_on_host_station = 0` — Allows the debuff to affect Host Stations. Mind control and stun are still ignored on Host Stations.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `allow_on_children = 0` — Automatically propagates the active Debuff to child units generated directly by the affected unit. 0 or absent = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `stack_max = 5` — Maximum number of layers, 1-99. Each new hit by this same profile on a unit that already carries it adds one layer of every malus (`force_malus`, `maxrot_malus`, `shield_malus`, `mgun_shot_time_malus`, `shot_time_malus`, `damage_malus`, `regen_malus`): with `shield_malus = -15%` two layers give -30%, each malus capped at -100%. Every hit renews `duration`. The layer count is shown as a small number at the bottom-right of the status icon. Other Debuffs on the unit are not affected. 0, absent or invalid = no stacking and no number.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `icon = Graphics/StatusIcons/electrified.svg` — Data-relative icon shown while the unit is debuffed. Empty/invalid = no icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `name = electrified` — Name of the debuff. debuff_id and blast_debuff_id can use it in place of the ID, so it must start with a letter.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mindcontrol = 1` — Converts the affected unit to the source faction. Does not apply to Host Stations; an inapplicable mind-control effect does not cancel the other Debuff effects.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `stun = 1` — Enables unit stun during the debuff. Does not apply to Host Stations.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `stun_motion_level = 50%` — Strength of altered movement during stun: 0% = disabled, 100% = maximum.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `stun_unit_fire = 1` — 1 = AI and player fire continuously while stunned; 0 = they cannot fire while stunned.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `energy = 2%` — Energy removed at each tick. With %, uses a percentage of target maximum HP; without %, uses a fixed amount.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tick_time = 2000` — Time in milliseconds between damage ticks.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `duration = 12000` — Total debuff duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `force_malus = -50%` — Force modifier during debuff. Example: -50% reduces force by 50%.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `maxrot_malus = -50%` — maxrot modifier during debuff.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shield_malus = -50%` — Shield modifier during debuff.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_shot_time_malus = -40%` — Modifier to the affected unit's MGUN fire rate.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shot_time_malus = -40%` — Modifier to the affected unit's primary Weapon fire rate.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damage_malus = -20%` — Modifier to all damage the affected unit deals (weapons, missiles, lasers, MGUN, blast, buildings). Example: -20% reduces it by 20%. Every hit still deals at least 1. 0 or absent = unchanged.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `regen_malus = -50%` — Modifier to the energy the affected unit gains from friendly power stations and allied mobile generators (Host Stations only with `allow_on_host_station = 1`). Example: -50% halves the gain. The drain of enemy power stations is not changed. 0 or absent = unchanged.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_pitch_malus = -50%` — Pitch modifier for sounds emitted by the affected unit. Negative lowers pitch, positive raises it. 0 or absent = unchanged.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp = 123|123|123|123|123|123|123|123` — VP visuals generated by the debuff. Multiple VP IDs may be separated with |. If 3ds is configured, the 3DS visual is generated alongside the VPs.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds = Data/Models/3ds/electrified.3ds` — Optional external 3DS visual generated alongside the debuff VPs, relative to Data. If missing/unloadable, configured VP visuals remain active.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tint = 0|255|0|255` — RGBA tint applied to debuff VP and 3DS visuals.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `scale = 1.0` — Uniform scale applied to debuff VP and 3DS visuals. Visual only: no effect on debuff gameplay values.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `random_max_offset = 25%` — Maximum random visual offset from the unit center: % of the unit silhouette (see `damaged_fx_random_max_offset`) or, without %, a fixed distance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_trail_tint = 0|255|0|255` — RGBA tint applied only to VP trails, particles and child effects belonging to the debuff.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `target_tint = 140|255|140|150` — RGBA tint applied to the affected unit model; compatible with VP and 3DS mesh visuals.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_sample = sounds/zap.wav` — Debuff sound. Without tick_time, remains looped for the entire debuff duration.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_pitch = 0~500` — Debuff sound pitch. Single value = fixed; `min~max` = random pitch selected on each playback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_volume = 180` — Volume of snd_sample.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_radius = 3000` — Maximum debuff-sound radius. Volume decreases linearly to 0 at the limit. 0 or absent = legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_fade_inout = 150|300` — Volume fade-in and fade-out of the debuff sound in milliseconds (format in|out). A one-shot sample fades out over its own length; a looping sound only fades in. If both fades are longer than the sound they shrink together. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_slot = 4` — Palette-effect slot used by the debuff.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_mag0 = 5` — Initial debuff palette intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_mag1 = 0` — Final debuff palette intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_time = 1500` — Debuff palette-effect duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_radius = 3000` — Maximum debuff palette-effect radius. Intensity decreases linearly to 0 at the limit. 0 or absent = legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_fade_inout = 150|300` — Fade-in and fade-out of the debuff palette effect in milliseconds (format in|out). The fade-out ends together with `pal_time`; a looping effect only fades in. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_slot = 1` — Shake slot used by the debuff.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_mag0 = 2` — Initial debuff-shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_mag1 = 0` — Final debuff-shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_time = 1500` — Debuff-shake duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_radius = 3000` — Maximum debuff-shake radius. Intensity decreases linearly to 0 at the limit. 0 or absent = legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_fade_inout = 150|300` — Fade-in and fade-out of the debuff shake in milliseconds (format in|out). The fade-out ends together with `shk_time`; a looping effect only fades in. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_mute = 0.02` — Legacy debuff-shake distance attenuation factor used only when shk_radius = 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end` — Ends the new_debuff profile.
------------------------------------------------------------------------------------------------------------------------

## Clouds (Clouds.cfg)

Cloud profiles are reusable volumes of floating meshes defined with `new_cloud <id>` in scripts loaded by Startup.cfg (Clouds.cfg is the organizational file). Levels link them with `cloud_id`, SuperItems with `fallout_cloud_id` and mobile Power Vehicles with `power_cloud_id`. An absent or unknown link activates no Cloud.

At least one of `vp`, `3ds` or `base` is required and `count` must be positive, otherwise the profile is ignored. `count` is capped at 512 and `loop_sound_volume` is clamped to 0-127. Reversed lifetimes are swapped; if neither is positive both become 1000 ms. A duplicate ID replaces the earlier profile.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `new_cloud 1` — Reusable Cloud profile linked by its numeric ID.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `name = radiations` — Profile name; links use the numeric ID.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `loop_sound = sounds/ambient/spore.wav` — Optional sound loop played while the atmospheric profile is active on screen. Empty/absent = no loop sound.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `loop_sound_volume = 70` — Volume of the atmospheric profile loop sound. <= 0 = muted/disabled. Runtime default: 100.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds = Data/Models/3ds/Radiation.3ds` — 3DS model spawned by this atmospheric profile. `vp` (number) and `base` (BASE path) are accepted instead or together; the 3DS wins, then the BASE, then the VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp = 12` — VP number spawned by this atmospheric profile (0 or absent = none).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `base = Data/Models/Base/New/Example/Example.BASE` — BASE spawned by this atmospheric profile.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `count = 135` — Number of 3DS meshes maintained within the configured player-centered spawn volume. Values above 512 are clamped to 512.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_radius_xyz = 1950|720|1950` — Random X|Y|Z spawn radius around the player/viewer. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_offset_xyz = 0|260|0` — X|Y|Z offset of the spawn volume relative to the viewer. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `lifetime_minmax = 5000~9000` — Minimum and maximum mesh lifetime in milliseconds (format `min~max`).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `velocity_xyz = 8|-18|5` — Base X|Y|Z mesh velocity. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `velocity_random_xyz = 40|22|40` — Random X|Y|Z velocity variation. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `scale_xyz = 3.2|3.2|3.2` — X|Y|Z scale of spawned meshes. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spin_xyz = 0.20|0.55|0.25` — Continuous X|Y|Z rotation rate/intensity. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fade_inout = 300|700` — Mesh fade-in and fade-out durations in milliseconds (format in|out).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tint = 170|255|145|165` — Mesh tint in R|G|B|A format.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end` — Closes the block.
------------------------------------------------------------------------------------------------------------------------

## SuperItems (SuperItems.cfg)

SuperItem profiles are defined with `new_superitem <id>` in scripts loaded by Startup.cfg (SuperItems.cfg is the organizational file). A level links one through `superitem_id` in its `begin_item` block: placement stays in the level, while the profile owns wave, damage, push and extra FX.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `superitem_id = 1` — ID of the new_superitem profile linked by the level begin_item block. -1, absent or unknown ID = vanilla SuperItem.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `new_superitem 1` — Reusable SuperItem profile linked by its numeric ID.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `name = stoudson_nuclear` — Profile and bomb display name; underscores are displayed as spaces. Linking uses the numeric ID.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fallout_cloud_id = 1` — ID of the Cloud activated after the SuperItem wave passes. -1, absent or unknown ID = no fallout Cloud.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_vp = 283` — Legacy VP used for the expanding wave front; final fallback for wave_3ds/wave_base.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_base = Data/Models/Base/FX/NUCLEAR_WAVE.BASE` — External BASE family for the wave front, relative to Data; automatically falls back to wave_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_3ds = Data/Models/3ds/FX/nuclear_wave.3ds` — 3DS model for the wave front, relative to Data; takes priority over wave_base and wave_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_scale_xyz = 1.0|1.0|1.0` — Base X|Y|Z scale multipliers for the VP/BASE/3DS wave visual (format x|y|z). X and Z grow automatically with the wave radius; the vertical Y does not. <= 0 or invalid = 1.0 for that axis.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_tint = 255|255|255|255` — RGBA tint applied to the wave visual for VP, BASE and 3DS.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_start_speed = 0` — Initial wave-front speed in world units per second. 0 allows the wave to start from rest.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_speed_ramp_time = 10000` — Linear transition time in milliseconds from wave_start_speed to wave_end_speed. 0 = use the final speed immediately.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_end_speed = 2000` — Final wave-front speed in world units per second. Must be positive.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_push_force = 6` — One-shot push intensity 0..10 applied to every unit reached by the wave. Uses the shared push curve: 100 * intensity^2 after push_resistance. Also affects allies and invulnerable units; static GUNs are excluded. The per-target one-shot state is saved.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_unit_energy = 50000` — Base energy removed from valid enemy units reached by the wave. Passes through the target's shield. 0 = no unit energy removed.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_building_total_destruction = 15%` — Percentage of intact buildings reached by the wave that are destroyed completely. Remaining buildings are damaged when possible.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `push_force = 10` — Instantaneous push intensity 0..10 generated at the detonation center. Uses the same shared curve, 100 * intensity^2, after falloff and push_resistance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `push_radius = 2000` — XZ radius of the instantaneous detonation push. <= 0 disables push_force.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `push_falloff = 1` — Detonation-push falloff: 1 reduces intensity with distance; 0 keeps the same intensity throughout push_radius.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fade_inout = 500|1000` — Fade-in and fade-out in milliseconds for the VP/BASE/3DS wave visual (format in|out). The fade-out happens when the wave reaches the map's natural limit. 0 or absent = immediate.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `debuff_id = 2` — ID of the new_debuff profile applied when the wave reaches a unit. -1, absent or unknown = no Debuff.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_detonate_sample = sounds/new/detonate.wav` — One-shot sound played at the SuperItem center when detonation begins.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_detonate_volume = 800` — Detonation sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_detonate_pitch = 0~500` — Detonation sound pitch. Single value = fixed; `min~max` = random pitch selected on each playback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_detonate_radius = 10000` — Maximum detonation-sound hearing radius. 0 = legacy attenuation behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_detonate_fade_inout = 150|300` — Volume fade-in and fade-out of the detonation sound in milliseconds (format in|out). A one-shot sample fades out over its own length; a looping sound only fades in. If both fades are longer than the sound they shrink together. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_detonate_slot = 1` — Palette-effect slot activated at detonation. 0 = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_detonate_mag0 = 20` — Initial palette intensity for detonation.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_detonate_mag1 = 10` — Final palette intensity for detonation.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_detonate_time = 6000` — Detonation palette-effect duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_detonate_radius = 10000` — Maximum radius of the detonation palette effect.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_detonate_fade_inout = 150|300` — Fade-in and fade-out of the detonation palette effect in milliseconds (format in|out). The fade-out ends together with `pal_detonate_time`; a looping effect only fades in. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_detonate_slot = 1` — Shake slot activated at detonation. 0 = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_detonate_mag0 = 1` — Initial detonation-shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_detonate_mag1 = 2` — Final detonation-shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_detonate_time = 6000` — Detonation-shake duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_detonate_radius = 10000` — Maximum radius of the detonation shake.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_detonate_mute = 0.02` — Legacy distance attenuation factor used only when shk_detonate_radius = 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_detonate_fade_inout = 150|300` — Fade-in and fade-out of the detonation shake in milliseconds (format in|out). The fade-out ends together with `shk_detonate_time`; a looping effect only fades in. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_detonate_x = 0.2` — Random shake strength multiplier on the X axis.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_detonate_y = 0.2` — Random shake strength multiplier on the Y axis.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_detonate_z = 1.0` — Random shake strength multiplier on the Z axis.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_wave_sample = sounds/new/wave.wav` — Looping sound attached to the wave front and positioned at the point on the front nearest the listener.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_wave_volume = 800` — Wave sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_wave_pitch = 0~500` — Wave sound pitch. Single value = fixed; `min~max` = random pitch selected on each playback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_wave_radius = 2000` — Maximum hearing radius measured from the wave front. 0 = legacy attenuation behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_wave_fade_inout = 150|300` — Volume fade-in and fade-out of the wave sound in milliseconds (format in|out). A one-shot sample fades out over its own length; a looping sound only fades in. If both fades are longer than the sound they shrink together. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_wave_slot = 1` — Palette-effect slot activated when the wave front enters the listener radius. 0 = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_wave_mag0 = 10` — Initial palette intensity of the wave.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_wave_mag1 = 5` — Final palette intensity of the wave.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_wave_time = 12000` — Wave palette-effect duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_wave_radius = 2000` — Distance from the front within which the palette effect may activate. 0 or absent uses 2400 as the front activation threshold while preserving legacy radius behavior for the effect.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_wave_fade_inout = 150|300` — Fade-in and fade-out of the wave palette effect in milliseconds (format in|out). The fade-out ends together with `pal_wave_time`; a looping effect only fades in. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_wave_slot = 1` — Shake slot activated when the wave front enters the listener radius. 0 = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_wave_mag0 = 0.1` — Initial wave-shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_wave_mag1 = 0` — Final wave-shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_wave_time = 12000` — Wave-shake duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_wave_radius = 2000` — Distance from the front within which wave shake may activate. 0 or absent uses 2400 as the front activation threshold while preserving legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_wave_mute = 0.02` — Legacy distance attenuation factor used only when shk_wave_radius = 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_wave_fade_inout = 150|300` — Fade-in and fade-out of the wave shake in milliseconds (format in|out). The fade-out ends together with `shk_wave_time`; a looping effect only fades in. 0 or absent = no fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_wave_x = 0.2` — Random wave-shake strength multiplier on the X axis.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_wave_y = 0.2` — Random wave-shake strength multiplier on the Y axis.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_wave_z = 1.0` — Random wave-shake strength multiplier on the Z axis.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `begin_fx` — Starts an additional visual effect reference at the SuperItem detonation.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `trigger = detonate` — Runs when detonation begins.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `bruch_id = 1002` — Plays the timed visual portion of a [Bruch](#bruch-bruchcfg). Uses offset and random rotation from the caller and emits one instance.
------------------------------------------------------------------------------------------------------------------------

## Ground Decals (Ground_Decals.cfg)

A decal is projected per pixel onto what is really drawn, so it looks the same on terrain, slopes, buildings and every LEGO piece, and never lands on units. It keeps its real proportions, continues from the ground up a nearby wall and over edges, and follows the geometry as a building changes state. Destroying a building removes the decals from hits on that piece, with their smoke and pulses. A large decal on a roof also marks the ground around the building when it is within reach. An invisible collision surface receives nothing: the decal lands on the real surface behind it, and a thin wall does not stop a decal on flat ground.

Profiles are defined with `new_decal <id>` in scripts loaded by Startup.cfg (the file name is organizational). Vehicles and Weapons link a profile with `ground_decal_id`, MGUN with `mgun_ground_decal_id`.

`effect_mode` is mandatory: without a valid mode the effect emits nothing and a warning is logged. The engine has no built-in effect model, so an authored model is always required.

- `ground_pulse`: short intermittent flashes around the decal (sparks, electric discharges). Keys: `effect_3ds`, `effect_base`, `effect_vp` (model priority in that order), `effect_pulse_interval`, `effect_pulse_time`, `effect_scale`.
- `rise`: camera-facing puffs that climb away from the decal (smoke, radioactive particles). Only `effect_3ds` is used. Keys: `effect_fade_inout`, `effect_count`, `effect_size`, `effect_growth`, `effect_rise_speed`, `effect_puff_time`, `effect_drift`, `effect_intensity`.
- Both modes: `effect_active_time`, `effect_retrigger_interval`, `effect_chance`, `effect_spread`, `effect_tint`.

A key written for the other mode is ignored and logged. Invalid `effect_*` values use their defaults, runtime bounds are applied, and reversed `min~max` ranges (for example `30~10`) are reordered by the engine.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `new_decal 1` — Starts Ground Decal profile ID 1, linked by `ground_decal_id`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `name = destroyed` — Profile name; links use the numeric ID.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_mode = rise` — Mandatory: rise = climbing puffs; ground_pulse = flashes around the decal.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_3ds = Data/Models/3ds/Smoke.3ds` — Effect model (3DS). `rise` uses only this key: a missing or unloadable 3DS emits nothing. For `ground_pulse` it is tried first, then `effect_base`, then `effect_vp`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_base = Data/Models/Base/FX/Spark.BASE` — ground_pulse only. BASE model of the flash, used when `effect_3ds` is absent or unloadable.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_vp = 2` — ground_pulse only. Legacy VP of the flash, the last fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_pulse_interval = 700~1500` — ground_pulse only. Pause between flashes in ms, 200..60000; single value or `min~max` (a new pause is rolled for every flash).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_pulse_time = 180` — ground_pulse only. How long each flash stays lit, 20..1000 ms, at most the interval of that flash.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_scale = 0.08` — ground_pulse only. Flash size at decal radius 50. Range 0.001-10; default 0.1. `rise` has no scale key: its size is set by `effect_size`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `texture = Graphics/GroundDecals/Impact.png` — Optional Data-relative texture; empty = procedural shape, unloadable file = decal discarded.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `stretch_xz = 0.8~1.2|0.8~1.2` — Local X|Z stretch (format x|z). Each axis takes a single value (fixed) or a random `min~max`. Default: 1|1; stretched axes rotate with the decal.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tint = 255|255|255|180` — RGBA tint; alpha 0 rejects the profile.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `random_rotation = 1` — Only exact 1 enables random rotation; other values or absent disable it.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `edge_fade = 5~10` — Edge fade in range 0-10; single value or random whole-number `min~max`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `duration = permanent` — Required: permanent or positive milliseconds. Missing/invalid rejects the profile. Still subject to gfx.ground_decal_limit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fade_inout = 0|1000` — Decal fade-in and fade-out in milliseconds (format in|out); in 0 = immediate, out is ignored for permanent duration.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_active_time = 20000~25000` — How long the effect keeps emitting over the decal, in ms; single value or `min~max`. Default `20000~25000`; maximum 3600000. The decal itself lasts `duration`. Valid in both modes.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_retrigger_interval = 30000~45000` — How often the whole effect restarts, in ms; single value or `min~max`. 0 = one activation only (default). With an interval the effect repeats for the whole decal life and each activation lasts at most the interval. Maximum 3600000. Valid in both modes.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_fade_inout = 3000|20000` — Rise only. Smoke fade-in and fade-out in milliseconds (format in|out). Default: 3000|20000; in 0 = immediate appearance, out 0 = abrupt stop.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_count = 10~30` — Rise only. Concurrent puffs for radius 50, scaled with decal size and capped at 50. Single value or `min~max`; 0 = no smoke. Default: `10~30`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_size = 35~50` — Rise only. Initial puff width as a percentage of decal radius (the model is normalised by the engine, whatever the 3DS units); single value or `min~max`. Range: 0.5-200; default: `35~50`. Use `4~7` for tiny fragments (Speck.3ds).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_growth = 3.3` — Rise only. Final puff size divided by initial size. Range: 1-10; default: 3.3.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_rise_speed = 25` — Rise only. Rise as a percentage of radius per second. Range: 0-200; default: 25.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_puff_time = 4000~7500` — Rise only. Individual puff lifetime in milliseconds; single value or `min~max`. Range: 200-60000; default: `4000~7500`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_drift = 35` — Rise only. Sideways drift as a percentage of rise. Range: 0-300; default: 35.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_spread = 35` — Initial emission area as a percentage of radius. Range: 0-100; default: 35. Also used by ground_pulse.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_intensity = 120` — Rise only. Opacity percentage. Range: 0-300; default: 120.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_tint = 70|70|70|150` — Effect RGBA tint. Default: 70|70|70|150. Also used by ground_pulse.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_chance = 100` — Probability that a decal emits the effect, in percent; the roll is fixed per decal. Range: 0-100; default: 100; 0 = never.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end` — Ends the profile.
------------------------------------------------------------------------------------------------------------------------

Examples:

```text
new_decal 2
name = mykonian_electric
texture = Graphics/GroundDecals/Mykonian_Electric.png
duration = permanent
effect_mode = ground_pulse
effect_vp = 2
effect_pulse_interval = 700~1500
effect_pulse_time = 180
effect_scale = 0.08
effect_active_time = 7000~11000
effect_spread = 30
effect_tint = 255|255|255|230
end

new_decal 4
name = mgun_hit
texture = Graphics/GroundDecals/Impact.png
random_rotation = 1
edge_fade = 5~10
duration = 60000
fade_inout = 0|2000
end
```

## Startup and Game Config (Startup.cfg)

OpenNeoUA uses .cfg files as the modern script/Game Config layout while retaining the original .scr entrypoints only where vanilla levels or data reference them.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `Data/Scripts/Startup.cfg` — Canonical OpenNeoUA script entrypoint. The engine prefers this file and it loads Buffs, Debuffs, Ground_Decals, Clouds, SuperItems, Weapons, Buildings, Bruch and Vehicles. If it is absent, the engine falls back to the real vanilla Data/Scripts/Startup.scr.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Data/Scripts/Multiplayer.cfg` — Standard multiplayer Game Config. Loads Startup.cfg and directly contains the definitions/overrides formerly split across Net_bldg.scr, Net_robo.scr and Netrflak.scr.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Data/Scripts/Multiplayer_Internet.cfg` — Internet multiplayer Game Config. Loads Startup.cfg and directly contains the definitions/overrides formerly split across Net_bldg.scr, Inetrobo.scr and Netrflak.scr.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Data/Scripts/Startup.scr` — Vanilla compatibility entrypoint that forwards to Startup.cfg in the modern Data layout.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Data/Scripts/Startup2.scr` — Vanilla level compatibility entrypoint. Retains its narrower per-level reset behavior instead of loading the complete Startup.cfg.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Data/Scripts/Net_ypa.scr` — Vanilla multiplayer compatibility entrypoint that forwards to Multiplayer.cfg.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Data/Scripts/Inet_ypa.scr` — Vanilla Internet multiplayer compatibility entrypoint that forwards to Multiplayer_Internet.cfg.
------------------------------------------------------------------------------------------------------------------------

## OpenNeoUA.ini

OpenNeoUA.ini is loaded after Nucleus.ini: when a key exists in both files, OpenNeoUA.ini wins. Settings handled by Advanced Graphics Settings are documented under Graphic Profiles.

### Game and debug

**Crash diagnostics** (`game.crash_diagnostics`):

- Each launch creates `Data/Env/Diagnostics/<date_time_pid>/` with SessionInfo.txt, copies of the two INI files, Events.log and LatestState.txt (updated about once per second).
- The event journal holds engine logs, mission transitions, unit creation, deletion and deaths. Events.log and Events.previous.log keep about 16 MiB each; overruns are reported.
- LatestState.txt records the engine phase, level, frame progress, unit count, active unit and prototype, position and memory use.
- On Windows, unhandled exceptions, abort and `std::terminate` write CrashReport.txt and try CrashDump.dmp from a separate process (15-second timeout). A gameplay stall above 2.5 seconds writes `HangReport_*.txt`; `Checkpoint_*.txt` records requested snapshots; SessionEnd.txt marks an orderly shutdown.
- These are observations, not a save or a replay. A forced kill or power loss can prevent a report, logging costs disk space and session folders are never removed automatically. Keep the matching executable for source-line analysis.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.building_visual_genesis = yes` — yes shows the animated Genesis plasma of the vehicles on every building under construction, in the constructor's faction color, keeping the existing emergence. The set's plain ground is drawn under the transparent plasma so the footprint never shows a black void. Terrain and gameplay are unchanged; classic and VBO rendering are supported. no, absent or invalid = off. It does not toggle the per-building sound, palette or shake.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.new.debug = yes` — yes enables the new debug controls F1-F12 (see Debug controls). no or absent keeps normal key behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.briefing_text_render = no` — Briefing unit presentation: yes = show text instead of the unit visual; no/0 or absent = render the available VP/BASE/3DS model.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.spectator_mode = yes` — Enables spectator mode, starting without a faction and in free camera. The spectator vehicle is read from Data/Scripts/spectator_vehicle.cfg and should define a hidden, invulnerable UFO.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.crash_diagnostics = yes` — yes enables persistent crash diagnostics from startup, independent of `game.new.debug` (restart required). no, absent or invalid = off. Each launch creates a session folder under `Data/Env/Diagnostics/`; see the note above.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.play_as_other_factions = yes` — Also exposed in the game options. Allows other factions to be selected and played from the briefing screen.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.cockpit_camera = no` — Enables OpenNeoUA cockpit camera mode in gameplay. yes = enabled; no or absent = disabled.
------------------------------------------------------------------------------------------------------------------------

### AI

------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.ai_robo_player_behavior = yes` — Enables manual movement behavior for the player's Host Station instead of teleport-style movement. Also enables the optional flux and twist parameters for the player's Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.ai_target_acquisition_range = 2160` — Maximum automatic target-acquisition distance for AI. 0 = vanilla behavior. Positive values are also applied to the player-side acquisition path. Maximum: 2160.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.ai_mgun_fire_alignment = 0.80` — Global forward-alignment requirement for AI MGUN fire. Lower values are more permissive. Default: 0.85.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.ai_max_altitude_above_ground = 1800` — Maximum altitude above ground for AI-controlled airborne units. 0, absent or invalid = vanilla behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.ai_air_vertical_force = 1.5` — AI aircraft climb and descent force. 1 = vanilla; above 1 strengthens vertical movement and lets AI aircraft pursue airborne targets vertically. Applies to all AI aircraft. Maximum 5; absent, zero or invalid = 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.ai_bomber_extra_altitude = 100` — AI aircraft carrying bomb, homing_bomb or laser with a downward fire volume try to fly this far above an airborne target or building roof (ground-unit targets are excluded). Maximum 3000; absent, zero or invalid = vanilla.
------------------------------------------------------------------------------------------------------------------------

### Combat and physics

------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.weapon_vs_weapon_collision = yes` — Enables weapon-versus-weapon collisions only: a projectile can hit another hostile or neutral projectile (it does not affect hits on units, buildings or terrain). Projectiles owned by the same faction do not collide with one another.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.recoil_kick_time = 0` — Global time in milliseconds for recoil to reach maximum displacement. 0 = immediate kick. Range: 0-5000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.recoil_hold_time = 0` — Global time in milliseconds to remain at maximum recoil before returning. 0 = no hold. Range: 0-5000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.recoil_return_time = 420` — Global time in milliseconds for recoil to return to neutral. Runtime default: 420. Range: 0-5000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.powerstation_energy_multiplier = 2.0` — Multiplies energy recharge and drain at Power Stations, Host Stations and mobile power sources. 1.0 = vanilla behavior. GUN actors (Host Station Robo-Guns, building turrets, flak and radar modules) never regenerate: no sector or mobile power heals them and they never show the Regen icon or particles; enemy drain still applies.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.fall_energy = 2%` — Energy removed by real fall/crash damage. With %, uses a percentage of maximum HP; without %, uses a fixed amount. Shield reduces the energy loss. Absent = vanilla behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.push_at_death_mult = 2.0` — Multiplies push and Blast_Push only when the hit kills the target. Range: 0..10; final effective intensity remains capped at 10. Absent or 1.0 = no extra push. Does not increase damage.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.gun_fall_tolerance = 3` — Building turrets (flaks, radars and other GUN actors on a building): how far the first surface under the turret may drop below its normal mount height before the turret loses its base and is destroyed. Range: 0..10, decimals allowed; each step adds 60 world units of reach (3 = 180, 10 = 600). 0 means no tolerance. Absent or invalid = 0 (vanilla). Host Station Robo-Guns and levitating guns are never affected.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.mgun_range = 1500` — Range of MGUN hit-scan fire in world units; also the range of `gun_type = mg` flaks. Default 1000; values above 6000 are capped.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.unit_enemy_collision_energy = 5%` — Energy removed from enemy units in a collision. With %, uses a percentage of target maximum HP; without %, uses a fixed amount. Shield reduces the energy loss. 0 or absent = no energy removed.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.unit_friendly_collision_energy = 1%` — Energy removed from allied units in a collision. With %, uses a percentage of target maximum HP; without %, uses a fixed amount. Shield reduces the energy loss. 0 or absent = no energy removed.
------------------------------------------------------------------------------------------------------------------------

### Deformation and craters

------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.deformation_max_craters = 128` — How many craters and dents (Weapons with `crater_radius` and `crater_depth`) are kept at once. Past the limit the oldest goes and the ground or building there returns to its original shape. 0 disables craters; negative = 128; maximum 2000; `no_limit` = no limit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.deformation_ground_limit = 10` — Maximum terrain displacement: value × 100 world units. Decimal 0..10; 0 stops terrain movement. Missing/invalid retains the old behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.deformation_building_limit = 0.4` — Maximum total displacement of buildings and their foundations: 0.4 = 40 world units. Decimal 0..10; 0 stops building movement. Nearby ground joins the foundation limit over 150 units. Graphics and collision share the limit, including overlapping hits and loaded saves. Missing/invalid retains the old behavior.
------------------------------------------------------------------------------------------------------------------------

### Player controls

------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.sprint_energy_cost = 1%` — Continuous Sprint energy cost per second. With %, uses maximum energy; without %, uses a fixed cost. 0 or absent = no cost.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.sprint_energy_drain_interval_ms = 1000` — Interval in milliseconds between continuous Sprint drain applications. 0 = continuous/accumulated application.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.fixed_tick_tank_ground_pose_mult = 5.5` — Adjusts tank response to slopes using the always-active fixed timing path. Range: 2.0-30.0. Suggested value: 5.5.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.handbrake_power = 1.0` — Handbrake strength multiplier. Affects braking behavior and reduces recoil/spread while the handbrake system is engaged. Runtime default: 1.0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.handbrake_sound = sounds/new/handbrake.wav` — Sound played when the handbrake is activated.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.player_tank_brake_time = 1500` — Time in milliseconds for a player tank to come to a natural stop after releasing acceleration. <= 0 or absent = vanilla behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.sprint_force_up = 60%` — Maximum force increase during Sprint. With %, a proportion of the unit's original force; without %, a fixed amount.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.sprint_pitch_up = 20%` — Maximum engine-pitch increase during Sprint. This value must be written as a percentage.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.sprint_ramp_time = 3000` — Ramp-up and ramp-down duration of Sprint in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.player_max_altitude_above_ground = 1600` — Maximum altitude above ground for player-controlled airborne units. 1600.0 = vanilla behavior.
------------------------------------------------------------------------------------------------------------------------

### Host Station

------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.robo_building_collision_energy = 5%` — Energy removed from each building the Host Station destroys in a collision. With %, uses a percentage of maximum HP; without %, uses a fixed amount. Shield reduces the energy loss. 0 or absent = no energy removed.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.robo_mobile_move_energy_cost_multiplier = 1.0` — Multiplier for the mobile Host Station movement battery cost. Absent/invalid = 1.0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.robo_mobile_main_energy_cost_multiplier = 0.33` — Multiplier for the mobile Host Station main-energy battery cost. Absent/invalid = 0.33.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.robo_mobile_build_energy_cost_multiplier = 0.33` — Multiplier for the mobile Host Station build/life battery cost. Absent/invalid = 0.33.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.robo_death_time_scale = 0.3` — Time scale applied when a Host Station is destroyed. 1.0 (runtime default) disables the slow motion; suggested range 0.05-1.0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.robo_death_time_scale_duration = 2000` — Duration in milliseconds of the Host Station destruction slow-motion effect. 0 or absent = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.robo_death_time_scale_max_distance = 0` — Maximum XZ distance from the player at which a Host Station death can trigger slow motion. 0 = unlimited distance.
------------------------------------------------------------------------------------------------------------------------

### GEM popup

------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.gem_unlock_duration = 2000` — GEM popup duration in milliseconds. Default: 10000 (vanilla). 0 = hide the popup. Does not slow game time.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.gem_unlock_sound` — Optional sound played when a GEM is unlocked. Set a licensed sound path starting with Data/. Empty = no sound (as in the original game).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.gem_unlock_slow_motion = yes` — When yes, gameplay time slows down (same scale as the debug slow motion, 80% slower) while the GEM popup is on screen; the on-screen time stays game.gem_unlock_duration. Default: no = no slowdown (as in the original game). Ignored in network games.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.gem_unlock_popup_y = -0.04` — Vertical anchor of the GEM popup: -1 = top edge, 0 = screen center, 1 = bottom edge (the vehicle panel extends above and below this point). Default: -0.5 = original game position; -0.04 = almost centered. Invalid values keep the default.
------------------------------------------------------------------------------------------------------------------------

### Elite units

------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.elite_unit_kills_per_medal = 5` — Eligible kills required for each medal. A Host Station kill awards the maximum at once. Range 1..100000; absent or invalid = 5. Medal progress is transient and single-player only.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.elite_unit_max_stat_bonus = 40%` — Maximum Elite stat bonus reached at full medals. Applied to force, maxrot, shield, outgoing damage and rate of fire, distributed linearly across the configured medal count. Requires an explicit %.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.elite_unit_max_medals = 4` — Maximum Elite medals a unit may earn during the session. Each medal requires the configured number of eligible kills; destroying a Host Station immediately awards the maximum. Runtime range 1-255; absent/0/invalid = 4. UFO and Host Station classes do not use Elite medals.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.elite_unit_max_glow_intensity = 1.0` — Maximum Elite glow intensity at full medals. Scales linearly with medal progress and reuses the glow_intensity rendering path. Range 0-1; 0/absent disables Elite glow.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.elite_unit_glow_tint = 0|150|255|255` — R|G|B|A tint used by the Elite glow, reusing buff_glow_tint rendering semantics. Components are clamped to 0-255.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.elite_unit_glow_pulse_seconds = 2.0` — Seconds per complete Elite glow pulse, reusing glow_pulse_seconds timing. 0/absent = steady glow; maximum 60 seconds.
------------------------------------------------------------------------------------------------------------------------

### Plasma

------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.plasma_currency_enable = yes` — Enables the global Plasma reserve in single-player. Absent or no = disabled and vanilla behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.plasma_currency_gain = 0.1%` — Plasma added to the global reserve when collected. With %, grants a fraction of the collected Plasma; without %, adds a fixed amount.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.plasma_death_duration_mult = 10` — Multiplies how long Plasma dropped by dead units remains in single-player. 1.0/absent/0/negative/invalid = vanilla duration. Netgame always uses vanilla behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.plasma_death_magnet_radius = 900` — 3D attraction radius. The nearest eligible collector wins, including physical pickup. The controlled unit can heal; player-owned AI collects enemy residue only for currency, even at full health. Friendly residue heals only the wounded controlled unit and never awards currency. Single-player only; 0/absent/invalid disables attraction.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.plasma_death_magnet_speed = 600` — Speed at which dropped Plasma moves toward its nearest eligible collector. Applies to the player-controlled unit and friendly AI units. 0/absent/invalid = disabled; also requires a positive magnet radius.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.plasma_snd_pickup_sample = sounds/pickup.wav` — Sound played by the unit that collects Plasma, including the player.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.plasma_snd_pickup_volume = 90` — Plasma pickup sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.plasma_snd_pickup_pitch = 0` — Plasma pickup sound pitch.
------------------------------------------------------------------------------------------------------------------------

### Audio and effect falloff

------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.global_shk_falloff_power = 1` — Curve of the distance attenuation of SHK shake effects that author a radius (`shk_*_radius`). 1 = linear, same as before; below 1 the shake stays stronger at a distance and still reaches 0 at the radius; above 1 it fades sooner. Range 0.1-4; missing or invalid = 1. Effects without a radius keep the legacy attenuation.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.global_pal_falloff_power = 1` — Same curve for PAL palette effects that author a radius (`pal_*_radius`). Range 0.1-4; missing or invalid = 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.global_snd_falloff_power = 1` — Same curve for sounds that author a radius (`snd_*_radius`). Range 0.1-4; missing or invalid = 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.ambient_sound = sounds/ambient/wind.wav` — Configurable global level ambience. If the path identifies a folder rather than a specific file, OpenNeoUA can select a sound from that folder for the level and loop it.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.ambient_sound_volume = 50` — Global ambient volume. Runtime default: 100. Slider range: 0-127; 0 = muted.
------------------------------------------------------------------------------------------------------------------------

### Graphics

> **Graphics chip.** On a computer with two graphics chips the game asks the NVIDIA or AMD driver for the fast one by itself, so no setting is needed. `Data/Env/ypa_log.txt` names the chip that got the window ("Graphics chip in use") and warns when it is a built-in one, which can cost most of the frame rate. On Windows pick the card for OpenNeoUA.exe in Settings > System > Display > Graphics. On Linux the game requests the NVIDIA card like `prime-run` (falling back to the default one); with an AMD or Intel card start the game with `DRI_PRIME=1`.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.black_sect_units_tint = 140|140|140|125` — Black Sect unit tint in R|G|B|A format. Each component ranges from 0 to 255.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.blending = 1` — Graphics blending mode: 0 = default, 1 = additive, 2 = sharp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.vsync = 2` — Vertical synchronization: 0 = disabled, 1 = enabled, 2 = adaptive with fallback to mode 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.color_effects = 1` — Enables the framebuffer used by post-processing color effects. 0 disables post-processing effects.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.maxfps = 60` — FPS limit. Safe default: 60. The FPS/Hz menu can expose values up to 240.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.ground_decal_limit = 256` — Maximum number of active Ground Decals. 0 disables them; negative/invalid = 256; values above 10000 are clamped to 10000; `no_limit` = no limit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.ground_decal_effect_limit = 256` — Maximum number of simultaneous decal effects (the `effect_*` profile keys); the oldest stops first. 0 disables them. The effective maximum never exceeds `gfx.ground_decal_limit`, also when either key uses `no_limit`. Default 256.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.sky_render = yes` — Enables or disables sky rendering. yes = enabled, no = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_fog_enable = yes` — Enables transparent horizon fog independently from horizon darkening.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_fog_color = 150|155|160` — Transparent horizon-fog color in R|G|B format. 150|155|160 is a light gray.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_dark_enable = yes` — Enables horizon darkening independently from transparent fog.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_dark_color = 0|0|0` — Horizon-darkening color in R|G|B format. 0|0|0 is pure black.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.vhs_filter_shader = res/ua_cinematic_1998_vhs.ps` — Shader path for the VHS filter. Missing/unloadable = filter disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.vhs_filter_shader_vbo = res/ua_cinematic_1998_vhs_vbo.ps` — VBO shader path for the VHS filter. Missing/unloadable = filter disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.atmosphere_fx = yes` — Enables the world Atmosphere pass. no = vanilla rendering; missing shader = vanilla fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.hide_map_border_walls = yes` — Allows the blue map-border walls to be hidden.
------------------------------------------------------------------------------------------------------------------------

### Regen and Drain effects

Regen and Drain feedback can use external VP/BASE/3DS visuals; without one, OpenNeoUA falls back to procedural + and - symbols. Either way each spawn is an independent, short-lived world-space mark: it rises away from its spawn point and fades in/out, and does not follow or rotate with the unit afterward. These effects are presentation only: gameplay comes from the Regen and Drain systems of the unit. They are not spawned on the unit you drive while you look through it (first-person or cockpit camera), where they would sit in front of the lens; other units and other views show them normally.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_vp = 0` — Legacy VP for the Regen effect; final fallback before the procedural + symbol.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_base = Data/Models/Base/FX/REGEN.BASE` — BASE family for the Regen effect, relative to Data; automatically falls back to gfx.regen_fx_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_3ds = Data/Models/3ds/FX/regen.3ds` — 3DS model for the Regen effect, relative to Data; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_scale = 1.0` — Uniform scale of the Regen effect: the VP/BASE/3DS visual, or the generated + symbol (30 units at 1.0, max 500). <= 0 or invalid = 1.0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_spin_xyz = 0|0|0` — X|Y|Z spin intensity for the Regen VP/BASE/3DS visual: 0-10, decimals allowed; 0 = no spin. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_tint = 80|255|120|230` — RGBA tint of the Regen effect, applied to VP/BASE/3DS or to the procedural + symbol.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_duration = 1000` — Lifetime in milliseconds of each Regen visual effect.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_interval = 300~600` — Random interval range in milliseconds between Regen-effect spawns. A single value gives a fixed interval.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_count = 2~4` — Random number of Regen effects created per trigger. A single value gives a fixed count. Runtime maximum: 32.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_random_max_offset = 80%` — Maximum random offset from the unit center: % of the unit silhouette (see `damaged_fx_random_max_offset`) or, without %, a fixed distance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_symbol_thickness = 5` — Bar thickness of the generated + symbols at scale 1.0 (multiplied by gfx.regen_fx_scale, never above the symbol size), used only when none of gfx.regen_fx_3ds, gfx.regen_fx_base or gfx.regen_fx_vp is set.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_symbol_rise_speed = 100` — Upward movement speed of the Regen effect: the generated + symbol, or a configured gfx.regen_fx_3ds/base/vp. 0 = does not rise.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_symbol_fade_inout = 150|300` — Fade-in and fade-out durations in milliseconds for the Regen effect: the generated + symbol, or a configured gfx.regen_fx_3ds/base/vp (format in|out). 0 = immediate appearance/disappearance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_vp = 0` — Legacy VP for the Drain effect; final fallback before the procedural - symbol.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_base = Data/Models/Base/FX/DRAIN.BASE` — BASE family for the Drain effect, relative to Data; automatically falls back to gfx.drain_fx_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_3ds = Data/Models/3ds/FX/drain.3ds` — 3DS model for the Drain effect, relative to Data; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_scale = 1.0` — Uniform scale of the Drain effect: the VP/BASE/3DS visual, or the generated - symbol (30 units at 1.0, max 500). <= 0 or invalid = 1.0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_spin_xyz = 0|0|0` — X|Y|Z spin intensity for the Drain VP/BASE/3DS visual: 0-10, decimals allowed; 0 = no spin. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_tint = 255|80|80|230` — RGBA tint of the Drain effect, applied to VP/BASE/3DS or to the procedural - symbol.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_duration = 1000` — Lifetime in milliseconds of each Drain visual effect.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_interval = 300~600` — Random interval range in milliseconds between Drain-effect spawns. A single value gives a fixed interval.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_count = 2~4` — Random number of Drain effects created per trigger. A single value gives a fixed count. Runtime maximum: 32.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_random_max_offset = 80%` — Maximum random offset from the unit center: % of the unit silhouette (see `damaged_fx_random_max_offset`) or, without %, a fixed distance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_symbol_thickness = 5` — Bar thickness of the generated - symbols at scale 1.0 (multiplied by gfx.drain_fx_scale, never above the symbol size), used only when none of gfx.drain_fx_3ds, gfx.drain_fx_base or gfx.drain_fx_vp is set.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_symbol_rise_speed = 100` — Upward movement speed of the Drain effect: the generated - symbol, or a configured gfx.drain_fx_3ds/base/vp. 0 = does not rise.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_symbol_fade_inout = 150|300` — Fade-in and fade-out durations in milliseconds for the Drain effect: the generated - symbol, or a configured gfx.drain_fx_3ds/base/vp (format in|out). 0 = immediate appearance/disappearance.
------------------------------------------------------------------------------------------------------------------------

### World HP and shield bars

------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.world_new_hp_bar_enable = no` — yes enables the configurable mesh-based world HP bar.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.world_new_hp_bar_full_tint = 0|217|81|255` — RGBA tint and opacity of the full-health portion of the bar.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.world_new_hp_bar_low_tint = 255|0|0|255` — RGBA tint toward which the HP bar gradually shifts as the vehicle takes damage.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.world_new_hp_bar_empty_tint = 255|0|0|0` — RGBA tint and opacity of the empty portion of the HP bar.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.world_new_shield_bar_enable = no` — yes enables the world shield bar drawn under the unit HP bar (thinner and shorter than it). While active it replaces the classic world shield row; the cockpit shield display is not affected.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.world_new_shield_bar_tint = 0|137|170|255` — RGBA tint and opacity of the shield bar fill. Defaults to the same teal as the cockpit shield squares. The missing portion is drawn as a semi-transparent track of the same color.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.world_new_cooldown_bar_enable = no` — Set to yes to show a cooldown bar above the energy bar for visible active spawners and mortars of any faction. With no or when omitted, mortars keep their classic bar.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.world_new_cooldown_bar_tint = 255|255|255|255` — Cooldown bar tint and opacity in `R|G|B|A` (0–255). The track uses the same tint at reduced opacity. Missing or invalid values use white.
------------------------------------------------------------------------------------------------------------------------

### User interface

------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.menu_font = Xolonium_Regular` — Selects the in-game UI font.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.retro_interface = yes` — Enables the rougher retro-style interface presentation.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.retro_cursors = yes` — yes = the alpha/beta cursor set, no = the release set (default, vanilla). The Windows executable uses embedded cursors when built with local cursor sources; loose Data/Res/Retro or Data/Res files remain valid fallbacks. Also the Retro Cursors checkbox of the Options menu, which saves this key.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.status_icon_blink_count = 3` — Number of complete on/off blink cycles used when dynamic Status Icons appear/disappear. 0 = immediate appearance; runtime range 0..10. Plasma popup behavior is handled separately.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.wireframe_tints_owner_<0..7> = 50|150|255|255` — RGBA tint of the in-game wireframe silhouettes (vehicle, projectile, MGUN) in the info panel, per owner 0-7. 0 or absent = original colors. Reticles, lock markers, visor overlays, text and notifications are not tinted. `ui.wireframe_tints_owner_01` equals `ui.wireframe_tints_owner_1`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.status_icon_regen = Graphics/StatusIcons/regen.svg` — Data-relative Regen icon path. Empty/invalid = disables only this icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.status_icon_drain = Graphics/StatusIcons/drain.svg` — Data-relative Drain icon path. Empty/invalid = disables only this icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.status_icon_damaged = Graphics/StatusIcons/damaged.svg` — Data-relative damaged-state icon path. Empty/invalid = disables only this icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.status_icon_spawn = Graphics/StatusIcons/mobile_spawner.svg` — Data-relative Mobile Spawner icon path. Empty/invalid = disables only this icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.status_icon_radar = Graphics/StatusIcons/mobile_radar.svg` — Data-relative Mobile Radar icon path. Empty/invalid = disables only this icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.status_icon_power = Graphics/StatusIcons/mobile_power.svg` — Data-relative Mobile Power icon path. Empty/invalid = disables only this icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.status_icon_sprint = Graphics/StatusIcons/sprint.svg` — Data-relative Sprint icon path. Empty/invalid = disables only this icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.status_icon_handbrake = Graphics/StatusIcons/handbrake.svg` — Data-relative Handbrake icon path. Empty/invalid = disables only this icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.map_marker_sound = sounds/alarm.wav` — Sound played once after a map marker is created successfully. Empty/unloadable = no sound.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.genesis_list_x = 0` — X position of the Genesis/Creation list. 0 = attached to the left edge; absent/invalid = legacy position.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.genesis_list_width = 180` — Width of the Genesis/Creation list. Absent/invalid/<= 0 = legacy width.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.genesis_list_height = 220` — Height of the Genesis/Creation list. Absent/invalid/<= 0 = legacy height.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.genesis_list_opacity = 220` — Genesis/Creation list background opacity. 0 = fully transparent, 255 = fully opaque. Icons, text and numbers are unaffected. Absent/invalid = 220.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.genesis_list_order = energy` — Genesis/Creation list sort order: classic = vanilla type groups followed by final cost, energy = sort only by final Genesis Energy cost. The Options menu exposes Classic/Energy and saves this value in OpenNeoUA.ini. price is not supported; missing/unknown = classic.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.map_opacity = 70` — Map background opacity. 0 = fully transparent, 255 = fully opaque. The top bar, icons, grid and markers are unaffected. Absent/invalid = 70.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.squadron_manager_opacity = 180` — Squadron Manager background opacity. 0 = fully transparent, 255 = fully opaque. The top bar, icons, text and numbers are unaffected. Absent/invalid = 180.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.weapon_name = no` — Shows the Weapon name in the vehicle Wireless interface. yes/1 = visible; no/0 or absent = hidden, preserving the vanilla HUD.
------------------------------------------------------------------------------------------------------------------------

## Nucleus.ini

Base configuration file of the engine. OpenNeoUA.ini is loaded after it and wins on conflicts. The `input.slider`, `input.button` and `input.hotkey` keys work exactly as in USER.TXT.

### Graphics

------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.dither = on` — Texture dithering.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.filter = on` — Linear texture filtering (smooth).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.antialias = on` — Antialiasing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.zbuf_when_tracy = no` — Also use the depth buffer for transparent objects.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.colorkey = no` — Color-key transparency for textures.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.movie_player = yes` — yes plays the movies. no skips them.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.disable_lowres = no` — yes hides the low resolutions.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.export_window_mode = no` — Window-mode export option.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.solidfont = off` — off draws text with soft edges; on uses a solid font.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.custommodes = 1920x1080,2560x1440` — Extra resolutions to add to the list, separated by commas.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.palette = standard.pal` — Color palette loaded at startup.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.vbo = on` — on renders with vertex buffers (faster).
------------------------------------------------------------------------------------------------------------------------

### Drivers and leftovers of the original engine

------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.timer = wintimer` — Name of the timer driver. Keep the original value.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.wimp = winp` — Name of the windowing / mouse driver. Keep the original value.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.keyboard = winp` — Name of the keyboard driver. Keep the original value.
------------------------------------------------------------------------------------------------------------------------

### Audio

------------------------------------------------------------------------------------------------------------------------
- 🔵 `audio.channels = 64` — Number of sound channels mixed together (minimum 1).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `audio.volume = 127` — Initial master volume (0 to 127).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `audio.num_palfx = 4` — Maximum number of palette flash effects active together, 8 at most.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `audio.rev_stereo = no` — yes swaps the left and right stereo channels.
------------------------------------------------------------------------------------------------------------------------

### Network and multiplayer

------------------------------------------------------------------------------------------------------------------------
- 🔵 `net.versioncheck = on` — on requires network players to run the same version.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `net.waitstart = 150000` — Multiplayer: maximum wait in milliseconds before the match starts.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `net.kickoff = 20000` — Multiplayer: milliseconds of silence after which a player is kicked.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `netgame.exclusivegem = on` — Multiplayer: when a player takes a gem, the upgrade is broadcast to everyone.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `net.gmode = 0` — Delivery mode for guaranteed network messages.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `game.debug = off` — on writes network debug information to the log.
------------------------------------------------------------------------------------------------------------------------

### Game and menu

------------------------------------------------------------------------------------------------------------------------
- 🔵 `game.newai = on` — on enables the newer Host Station AI for construction timing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `game.timeline = 600000` — Time scale of the Host Station AI in milliseconds: higher values make it build more slowly.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `menu.windowed = off` — on opens the menu in a window.
------------------------------------------------------------------------------------------------------------------------

### Palette (color effects)

Palette slots are reusable color-effect channels. Original slots retain their vanilla behavior; OpenNeoUA additionally allows custom RGB slots to be defined in Nucleus.ini and referenced by vehicle/Weapon PAL events.

Classic slots:

------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.color_eff_pwr[1] = 100` — Red
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.color_eff_pwr[2] = 100` — Blue
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.color_eff_pwr[3] = 100` — Green
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.color_eff_pwr[4] = 100` — Inverse
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.color_eff_pwr[5] = 100` — Inverse Dark
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.color_eff_pwr[6] = 100` — Black & White
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.color_eff_pwr[7] = 100` — Inverse Turquoise
------------------------------------------------------------------------------------------------------------------------

Custom slots:

------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.color_eff[9] = 255|102|7` — Defines a custom palette slot (8..16) with R|G|B parts 0-255; PAL parameters reference it through `pal_*_slot`. Can also be declared in OpenNeoUA.ini. Vanilla slots are unchanged. Example: `gfx.color_eff[9] = 255|102|7` is orange.
------------------------------------------------------------------------------------------------------------------------

Palette balance rules for the events explode, airexplode, goingdown, launch, hit and normal of Vehicles and Weapons (applied to every script entry that defines them; cluster, beam, build, genesis, detonate and wave events are not covered). The screen is the scene times the sum of (magnitude x slot colour) plus the normal image weighted by 1 minus the total magnitude, so effects active together add up (up to `audio.num_palfx`, 8 at most) and a total above 1 hides the normal image.

- Range: `mag0` and `mag1` are 0 (effect off) or go from 0.5 to 5.0 in steps of 0.5 (0.2 or 0.75 are not allowed). Most effects fade to 0, so the battlefield does not light up like a Christmas tree. `time` goes from 250 to 700 ms; only host station explosions (1500) and the 20 Tons Taerbomb (1000) are longer.
- One colour per faction, for vehicle and weapon events alike: Resistance slot 11 (light blue), Ghorkov slot 1 (red), Taerkasten slot 8 (yellow), Mykonians slot 2 (blue), Sulgogar slot 16 (green), Black Sect flak vehicles slot 5. Classic inverse slots (4, 5, 6, 7) are not used for faction units, except the Black Sect one.
- Vehicle explode / airexplode peak (`mag0`) grows with the vehicle's `energy`: up to 6500 = 1.0, 10000 = 1.5, 15000 = 2.0, 22000 = 2.5, 30000 = 3.0, 45000 = 3.5, more = 4.0. A `vwr_radius` of 60 or more adds 0.5 (maximum 4.5). Host stations explode at 5.0. `mag1` is 0. `time` = 300 + (peak - 1) x 100 ms; airexplode lasts 50 ms more (maximum 700).
- Vehicle goingdown: 0 (off) for explosion peaks up to 2.0, 0.5 up to 3.5, 1.0 above; it ends at 0 and lasts as long as the explode event.
- Weapon hit peak: score s = sqrt(energy / 2000) x min(1, shot_time / 600)^0.7, then `mag0` = 0.55 x s rounded down to 0.5 (maximum 5.0; under 0.5 it becomes 0). Weapons with `shot_time` of 300 ms or less have no hit flash (0). `mag1` is 0. `time` = 250 + (peak - 0.5) x 100 ms (250 when the peak is 0). Weapons 12, 17, 36, 38 and 39 (kamikaze and very large weapons) use the maximum 5.0.
- Weapon launch follows the hit peak: 0 (off) under 2.0, 0.5 from 2.0, 1.0 from 3.5, 1.5 at 5.0; it ends at 0 and lasts 250 ms (300 ms from 1.0). Weapon normal (glow in flight) is 0 under 1.5, 0.5 from 1.5, 1.0 from 4.0, so only strong projectiles tint the screen, lightly. Host station normal effects (Mykonian ramp) are kept.

## World.ini

### General settings (begin_misc)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_misc` — Opens the general settings block of World.ini. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `one_game_res = no` — yes makes the menu and the game use the same resolution.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shell_default_res = 640_480` — Default resolution of the menu shell. For high resolutions also change size_x and size_y in begin_bg.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `game_default_res = 640_480` — Default resolution of the game.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `max_impulse = 10000` — Upper limit of the impulse (push) that an explosion can give to a unit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `beam_energy_start = 500` — Beam energy capacity at the start of a level.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `beam_energy_add = 50` — Beam energy capacity gained when the player destroys an enemy Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `unit_limit = 800` — Default unit limit for every level; a level can override it.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `unit_limit_type = 1` — Limit type: 0 hard block, 1 power-station penalty, 2 more expensive units.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `unit_limit_arg = 5` — Extra argument of the limit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `easy_cheat_keys = 0` — 1 lets the cheat keys (keypad * and /) work without holding the right mouse button.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `multi_building = yes` — yes allows several buildings in the same sector.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hidden_fractions = 0` — Factions hidden by default, as a bit mask.
------------------------------------------------------------------------------------------------------------------------

### Interface colors (begin_colors)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_colors` — Opens the interface color block. Every color is four numbers R_G_B_A (alpha is the intensity). Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner_0 = 000_000_000_6` — Color of faction 0 (nobody).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner_1 = 000_110_194_15` — Color of faction 1 (Resistance).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner_2 = 000_179_066_37` — Color of faction 2 (Sulgogar).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner_3 = 232_232_232_207` — Color of faction 3 (Mykonian).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner_4 = 255_171_028_11` — Color of faction 4 (Taerkasten).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner_5 = 073_073_073_5` — Color of faction 5 (Black Sect).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner_6 = 255_000_000_10` — Color of faction 6 (Ghorkov).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner_7 = 000_000_000_0` — Color of faction 7 (undefined).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `map_direction = 255_255_255_1` — Map: color of the direction arrow.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `map_primtarget = 255_171_0_11` — Map: color of the primary target.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `map_sectarget = 255_000_000_10` — Map: color of the secondary target.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `map_commander = 000_167_200_15` — Map: color of the commander marker.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `map_dragbox = 000_150_250_15` — Map: color of the drag selection box.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `map_viewer = 255_255_255_1` — Map: color of the viewer cone.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_weapon = 080_160_160_1` — HUD: color of the weapon indicator.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_weapon_1 = 235_245_255_1` — HUD: weapon indicator color, alternate state (_1).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_vehicle = 080_160_160_1` — HUD: color of the vehicle indicator.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_vehicle_1 = 235_245_255_1` — HUD: vehicle indicator, alternate state.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_compass_commandvec = 000_137_170_15` — HUD compass: command vector.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_compass_commandvec_1 = 000_147_180_15` — HUD compass: command vector, alternate state.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_compass_primtarget = 255_171_000_11` — HUD compass: primary target.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_compass_primtarget_1 = 245_161_000_11` — HUD compass: primary target, alternate state.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_compass_locktarget = 255_000_000_10` — HUD compass: locked target.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_compass_locktarget_1 = 245_000_000_10` — HUD compass: locked target, alternate state.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_compass_compass = 060_130_140_1` — HUD compass: the compass itself.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_compass_compass_1 = 235_245_255_1` — HUD compass: the compass, alternate state.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_visor_mg = 255_255_255_1` — HUD visor: machine gun.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_visor_mg_1 = 100_180_180_1` — HUD visor: machine gun, alternate state.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_visor_locked = 255_000_000_10` — HUD visor: locked target.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_visor_locked_1 = 255_100_000_10` — HUD visor: locked, alternate state.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_visor_autonom = 100_180_180_1` — HUD visor: autonomous mode.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_visor_autonom_1 = 255_255_255_1` — HUD visor: autonomous, alternate state.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `brief_lines = 125_200_255_103` — Briefing: color of the lines.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `text_default = 010_150_150_1` — Text: default color.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `text_list = 020_160_160_1` — Text: lists.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `text_list_sel = 255_255_255_1` — Text: selected list entry.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `text_tooltip = 150_200_200_1` — Text: tooltips.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `text_message = 150_200_200_1` — Text: messages.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `text_hud = 070_150_150_1` — Text: HUD.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `text_briefing = 065_123_134_103` — Text: briefing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `text_debriefing = 065_123_134_103` — Text: debriefing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `text_button = 180_221_221_50` — Text: buttons.
------------------------------------------------------------------------------------------------------------------------

### Menu and map backgrounds (begin_bg)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_bg` — Opens the menu and map background images. Closed by end. One block can be written per set.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `size_x = 640` — Width of the images. For high resolutions use the real size, for example 1920. All images must share the same size.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `size_y = 480` — Height of the images.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `background_map = bg/normal.png` — Background of the mission selection map.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `rollover_map = bg/white.png` — Image shown while the mouse hovers over a mission.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `finished_map = bg/done.png` — Image of completed missions.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `enabled_map = bg/en.png` — Image of selectable missions.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `mask_map = bg/maske.png` — Mask image: the color index of every area selects which level it opens (index 1 opens L0101). The background must be index 0.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `tut_background_map = bg/tut1.png` — Same as background_map, for the tutorial map.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `tut_rollover_map = bg/tut2.png` — Same as rollover_map, for the tutorial.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `tut_mask_map = bg/tutmask.png` — Same as mask_map, for the tutorial.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `menu_map = bg/startup.png` — Background of the main menu.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input_map = bg/settings.png` — Background of the input menu.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `settings_map = bg/settings.png` — Background of the settings menu.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `network_map = bg/settings.png` — Background of the network menu.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `locale_map = bg/settings.png` — Background of the language menu.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `save_map = bg/settings.png` — Background of the save menu.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `about_map = bg/settings.png` — Background of the about screen.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `help_map = bg/settings.png` — Background of the help screen.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `brief_map = bg/brief.png` — Background of the briefing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `debrief_map = bg/debrief.png` — Background of the debriefing.
------------------------------------------------------------------------------------------------------------------------

### Movies (begin_movies)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_movies` — Opens the movie list. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `game_intro = mov:intro.mpg` — Intro movie of the game.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `win_extro = mov:win.mpg` — Ending movie when the game is won.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `lose_extro = mov:lose.mpg` — Movie shown when the game is lost.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `user_intro = mov:user.mpg` — Intro movie of the Resistance campaign.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `kyt_intro = mov:kyt.mpg` — Intro movie of the Ghorkov campaign.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `taer_intro = mov:taer.mpg` — Intro movie of the Taerkasten campaign.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `myk_intro = mov:myk.mpg` — Intro movie of the Mykonian campaign.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sulg_intro = mov:sulg.mpg` — Intro movie of the Sulgogar campaign.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `black_intro = mov:black.mpg` — Intro movie of the Black Sect campaign.
------------------------------------------------------------------------------------------------------------------------

### Vanilla SuperItems (begin_superitem)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_superitem` — Opens the vanilla SuperItem settings. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `superbomb_wall_vproto = 135` — Model ID of the super bomb shock wall.
------------------------------------------------------------------------------------------------------------------------

### Menu sounds and music (begin_shellsounds, begin_shelltracks)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_shellsounds` — Opens the menu sounds. Closed by end. Each sound has _sample, _volume and _pitch.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `<sound>_sample = sounds/stahlbr.wav` — Menu sound file. Sounds: volume, left, right, button, quit, slider, welcome, menuopen, overlevel, levelselect, textappear, objectappear, sectorconquered, vhcldestroyed, bldgconquered, timercount, select, error, attention, secret, plasma.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `<sound>_volume = 60` — Volume of the menu sound.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `<sound>_pitch = 0` — Pitch of the menu sound.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_shelltracks` — Opens the menu music tracks. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shelltrack = 3` — Menu music track. Optionally followed by _minPause_maxPause in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `missiontrack = 2` — Music track of the mission selection screen.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `loadingtrack = 0` — Music track of the loading screen.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `debriefingtrack = 0` — Music track of the debriefing.
------------------------------------------------------------------------------------------------------------------------

## USER.TXT (player profile)

### Player data (new_user)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `new_user` — Opens the player data block. It is written by the game and closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `plasma_bank = 1500` — Plasma currency collected by the player (OpenNeoUA currency).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `workshop_owned = 56` — ID of a vehicle already unlocked in the Workshop. One line per vehicle.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `workshop_host = 56` — ID of the Host Station selected in the Workshop.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `maxroboenergy = 0` — Highest Host Station energy reached so far, kept as a record.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `maxreloadconst = 0` — Highest reload constant reached so far, kept as a record.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `beamenergy = 500` — Current beam energy capacity of the player.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `playerstatus = 1_0_0_0_0_0_0_0` — Statistics of one faction. Format faction_destroyedUnits_destroyedByUser_elapsedTime_sectorsTaken_score_power_upgrades. One line per faction (0 to 7).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `jodiefoster = 0_0_0_0_0_0_0_0` — Contact flags for the 8 factions (the game's own label is 'the contact flags').
------------------------------------------------------------------------------------------------------------------------

### Controls and keys (new_input)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `new_input` — Opens the input block: it clears the assigned keys and loads new ones. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `modify_input` — Like new_input, but changes only the listed controls and keeps the others.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `joystick = yes` — yes enables the joystick.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `altjoystick = yes` — yes selects the alternative joystick model.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `forcefeedback = yes` — yes enables force feedback.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.slider[0] = ~#$a_#$d` — Aircraft: left / right. The value is a pair of keys: the first moves negative, the second positive. $ stands for winp: and _ separates the two keys.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.slider[1] = ~#$s_#$w` — Aircraft: nose down / up.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.slider[2] = ~#$z_#$q` — Aircraft: decelerate / accelerate.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.slider[3] = ~#$a_#$d` — Ground unit: left / right.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.slider[4] = ~#$s_#$w` — Ground unit: back / forward.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.slider[5] = ~#$1_#$2` — Ground unit: gun down / up.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.button[0] = $return` — Fire the primary weapon.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.button[1] = $tab` — Weapon switch (in vanilla: launch a missile with its camera).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.button[2] = $x` — Fire the machine gun.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.button[3] = $space` — Brake / auto-level the helicopter.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.button[4] = $rshift` — Map: set waypoints.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `input.button[5] = $grave` — Missile camera (OpenNeoUA).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `input.button[6] = $tab` — Cycle target (OpenNeoUA).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `input.button[7] = $f` — Alternative view (OpenNeoUA).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[0] = o` — Give orders to units.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[1] = space` — Jump into gun turrets.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[2] = c` — Create a vehicle or building.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[3] = q` — Add a vehicle to the current squad.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[4] = j` — Jump into a vehicle.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[7] = b` — Beam the Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[8] = m` — Map on / off.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[9] = e` — Squadron Manager on / off.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[10] = num1` — Map: sector detail.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[11] = num2` — Map: sector ownership.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[12] = num3` — Map: vehicle status.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[14] = num5` — Map: lock on the occupied unit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[16] = numplus` — Map: zoom in.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[17] = numminus` — Map: zoom out.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[18] = num4` — Map: resize.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[20] = f7` — Cycle through squad leaders.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[21] = f4` — Jump to the Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[22] = f6` — Cycle through the units of the squad.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[23] = f5` — Jump to the current squad leader.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[24] = esc` — Abort / quit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[25] = h` — HUD on / off.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[27] = i` — Information log on / off.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[31] = f8` — Jump to the last message sender.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[32] = p` — Pause.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[37] = rshift` — Send a chat message (multiplayer).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[38] = 1` — Aggression level 1.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[39] = 2` — Aggression level 2.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[40] = 3` — Aggression level 3.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[41] = 4` — Aggression level 4.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[42] = 5` — Aggression level 5.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[43] = f1` — Help (not available in multiplayer).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[44] = bs` — Jump to the last occupied unit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[45] = l` — Become the current squad leader.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[46] = f2` — Situation analyzer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `input.hotkey[47] = k` — Toggle the cockpit camera (OpenNeoUA).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `input.hotkey[48] = r` — Sprint (OpenNeoUA).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `input.hotkey[49]` — Place a map marker (OpenNeoUA). Markers are disabled in Spectator Mode, including the right-click shortcut.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `input.hotkey[52] = space` — UFO: toggle Spy mode (OpenNeoUA).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `input.hotkey[53] = e` — Map focus (OpenNeoUA).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `input.hotkey[54]` — Hide the player's vehicle (OpenNeoUA).
------------------------------------------------------------------------------------------------------------------------

### Audio (new_sound)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `new_sound` — Opens the audio block. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `volume = 127` — Sound effect volume (0 to 127).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `cdvolume = 127` — Music volume (0 to 127).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `invertlr = no` — yes swaps the left and right stereo channels.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `cdsound = yes` — CD music switch: stores the flag that makes the game ignore its music commands.
------------------------------------------------------------------------------------------------------------------------

### Video (new_video)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `new_video` — Opens the video block. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `videomode = 2621920` — Game resolution as one number: width x 4096 + height (2621920 is 640 x 480).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfxmode = 800_600_0` — Modern resolution: width_height_windowed (0 fullscreen, 1 windowed).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `farview = yes` — yes enables the far view, which renders more sectors.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `drawprimitive = no` — Flag of the original graphics driver (draw primitive).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `16bittexture = yes` — Flag of the original graphics driver (16-bit textures).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `softmouse = yes` — yes draws the mouse cursor in software.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `heaven = yes` — yes renders the sky.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `enemyindicator = yes` — yes shows the enemy indicator.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `fxnumber = 16` — Maximum number of visual effects (explosions, sparks) active at once.
------------------------------------------------------------------------------------------------------------------------

These values are saved per user profile rather than used as global mod settings.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `interface_style = retro` — Saved virtual UI style: retro = nearest/pixelated presentation, smooth = linear/smoother presentation. If absent, the profile starts from ui.retro_interface.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `palette_theme = Original` — Saved palette-theme name. Original selects the standard palette set; another value selects that named theme when available.
------------------------------------------------------------------------------------------------------------------------

### Completed levels and buddies

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_levelstatus = 1` — Opens the status of one level (the level number). Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `status = 2` — Level status: 1 locked, 2 available, 3 completed.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_buddy` — Opens a saved buddy. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `commandid = 1` — Command ID of the buddy.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `type = 2` — Vehicle ID of the buddy.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `energy = 1000` — Energy of the buddy.
------------------------------------------------------------------------------------------------------------------------

### Menu interface (new_shell)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `new_shell` — Opens the menu interface block. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `language = english` — Selected language, among those available in Locale.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_map_status = 1_1_0_0_200_200` — State of the Host Station map window: valid_open_x_y_width_height.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_finder_status = 1_1_0_0_200_200` — State of the Host Station finder window (same format).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vhcl_map_status = 1_1_0_0_200_200` — State of the vehicle map window (same format).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vhcl_finder_status = 1_1_0_0_200_200` — State of the vehicle finder window (same format).
------------------------------------------------------------------------------------------------------------------------

## Graphic Profiles

Graphic Profiles are editable .cfg files in Data/Scripts/Graphic_Profiles/ (legacy .txt profiles are still accepted). The file name is the preset name shown in Advanced Graphics Settings. The parameters below belong in Graphic Profile files, not in OpenNeoUA.ini; changing one of them manually in the menu switches the active preset to Custom. `gfx.render_sectors` is retired: the sector window is derived automatically from `gfx.skydistance`.

Original Video (`gfx.video_filter_mode = 1`) reduces the final composed frame, UI included, keeping the display resolution and aspect ratio: a 4:3 frame uses 320x240 and other aspect ratios adjust the width. Menu saves go to OpenNeoUA.ini while the editable preset values stay in the .cfg files. An older preset without `video_filter_mode` selects VHS mode.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.palette_filter = Black_Sect_Anvil.pal` — Full-screen palette filter loaded from Data/Filters. Standard disables the filter.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.palette_filter_strength = 0.50` — Palette-filter strength. 0.00 = no effect; 1.00 = full strength.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.atmosphere_strength = 0.50` — Overall strength of the atmosphere post-process.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.atmosphere_exposure = 1.70` — Atmosphere exposure / overall brightness response.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.atmosphere_contrast = 0.95` — Image contrast used by the atmosphere pass.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.atmosphere_saturation = 0.80` — Color saturation used by the atmosphere pass.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.atmosphere_vignette = 0.60` — Dark-edge vignette strength.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.skydistance = 60000` — Horizontal radius of the player-centered sky horizon. Lower values bring the dome closer and automatically prepare fewer world sectors; higher values move it farther away and prepare more. Menu range: 1200-60000. Sector coverage is derived automatically.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.sky_height = -400` — Vertical sky-dome offset relative to the player. More negative values lower the dome; higher values raise it. Menu range: -3000..3000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_fade_mode = 0` — Distance-fade curve: 0 = smooth radial horizon fade, 1 = classic linear camera-depth fade. Missing/invalid values use 0. Classic uses 1 with full black darkening from 1000 to 3100 units.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_fog_start = 4000` — Distance at which transparent horizon fog begins.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_fog_length = 2000` — Length of the horizon-fog transition. Higher values produce a softer fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_fog_strength = 0.80` — Horizon-fog strength. 0.00 disables it; 1.00 is maximum strength.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_dark_start = 2000` — Distance at which horizon darkening begins.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_dark_length = 2000` — Length of the horizon-darkening transition.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_dark_strength = 0.65` — Horizon-darkening strength. 0.00 disables it; 1.00 is maximum strength.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.world_ui_max_distance = 5700` — Maximum distance for supported world-space unit UI. Menu range: 100-20000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.video_filter_mode = 0` — Final-frame filter: 0 = existing VHS, 1 = Original Video (240-line reduction with bilinear enlargement, without VHS noise or distortion). Missing/invalid values use 0. Classic selects 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.vhs_filter_strength = 0.60` — Selected video-filter strength: 0.00 bypasses it; 1.00 applies it fully. Classic uses 1.00. The legacy key name is retained for compatibility.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.particles.limit = 9000` — Maximum active-particle limit stored by the profile. The menu supports values up to 20000.
------------------------------------------------------------------------------------------------------------------------

## Levels (LDF files)

### begin_level block

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_level` — Opens the general level block. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `title_default = My_Level` — Level title. A language line such as `title_italian` wins when that language is active; `title_default` is the fallback.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `set = 1` — Graphic set number (sectors and textures). Write it before anything else; changing it after building the sectors breaks the map.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sky = objects/sky1.base` — Sky object of the level (a .base file).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `typ = typ1.iff` — Original format: file name of the image holding the sector types. Used instead of the begin_maps block.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `own = own1.iff` — Original format: file name of the image holding the sector owners.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hgt = hgt1.iff` — Original format: file name of the image holding the heights.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `blg = blg1.iff` — Original format: file name of the image holding the functional buildings.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `palette = palette/standard.pal` — Color palette of the level. Equivalent to slot0.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `slot0 = palette/standard.pal` — Palette of slot 0. Slots 0 to 7 hold the palettes used by color effects (`pal_*_slot`).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `slot1 = palette/red.pal` — Palette of slot 1 (red).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `slot2 = palette/blue.pal` — Palette of slot 2 (blue).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `slot3 = palette/green.pal` — Palette of slot 3 (green).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `slot4 = palette/inverse1.pal` — Palette of slot 4 (inverse).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `slot5 = palette/inverse2.pal` — Palette of slot 5 (dark inverse).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `slot6 = palette/white.pal` — Palette of slot 6 (black and white).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `slot7 = palette/inverse3.pal` — Palette of slot 7 (turquoise inverse).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `script = data:scripts/startup2.scr` — Prototype script loaded for the level (weapons, vehicles, buildings), normally startup2.scr or net_ypa.scr. The level fails to load if the script cannot be read.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `ambiencetrack = 3_30000_60000` — Ambient music: track number, then optionally the minimum and maximum pause in milliseconds between repeats.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `movie = mov:intro.mpg` — Movie shown when the level starts.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `win_movie = mov:win.mpg` — Movie shown when the level is won.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `lose_movie = mov:lose.mpg` — Movie shown when the level is lost.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `event_loop = 1` — Tutorial lesson hook: the values 1, 2 and 3 load lesson1.lua, lesson2.lua and lesson3.lua. Other values do nothing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `slow_connection = yes` — Multiplayer only: mode intended for slow connections.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `unit_limit = 800` — Maximum number of units in the level. Overrides the unit_limit of World.ini.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `unit_limit_type = 1` — What happens beyond the limit: 0 is a hard block, 1 applies a penalty to power stations, 2 makes units more expensive.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `unit_limit_arg = 5` — Extra argument of the limit, used by types 1 and 2.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `multi_building = yes` — Allows several buildings to be built in the same sector for this level.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hidden_fractions = 0` — Factions hidden from the enemy, as a bit mask over the faction number.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `end` — Closes the block.
------------------------------------------------------------------------------------------------------------------------

Level-side settings override broader defaults where explicitly documented. This lets a map define its own ambience, SuperItem profile and atmospheric presentation without forcing the same setup on every level.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `ambient_sound = sounds/ambient/wind.wav` — High-priority level ambience. A folder path instead of a file lets OpenNeoUA pick a sound from that folder and loop it. In a level file it overrides the Nucleus.ini setting. The path is relative to Data.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `superitem_id = 1` — ID of the new_superitem profile linked by the level begin_item block. -1, absent or unknown ID = vanilla SuperItem.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cloud_id = 1` — ID of the new_cloud profile active in the level. -1, absent or unknown = no Cloud.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `lua_script = level_intro.lua` — Optional per-level Lua script, loaded from Data/res/. If absent or unloadable, the level runs with vanilla behavior and no errors. Tutorial levels (EventLoopID 1..3) use lessonN.lua instead. The engine calls init(ts) on level start, update(ts, delta) on every game tick, and makesave() to persist a custom string into saves.
------------------------------------------------------------------------------------------------------------------------

### Host Stations (begin_robo)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_robo` — Opens a Host Station of the level (7 at most). Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner = 1` — Owning faction (1 Resistance, 2 Sulgogar, 3 Mykonian, 4 Taerkasten, 5 Black Sect, 6 Ghorkov). The first Host Station of the file is the player's.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vehicle = 56` — ID of the Vehicle (model = robo) used as the Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sec_x = 8` — Sector column. Places the Host Station at the center of that sector.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sec_y = 5` — Sector row.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pos_x = 9960` — X position in world units (alternative to sec_x). A sector is 1200 x 1200 units. Avoid coordinates ending in 0 or 5, which sit on sector edges.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pos_y = -300` — Height in world units (negative is above ground).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pos_z = -6600` — Z position in world units (rows go negative).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `energy = 400000` — Starting energy of the Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `reload_const = 500000` — Reload constant. Required when the energy exceeds 500000 (enemies) or 550000 (player); otherwise the station can never be depleted.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `viewangle = 0` — Initial view / facing angle.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `con_budget = 50` — AI budget for conquest.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `def_budget = 20` — AI budget for defense.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `rob_budget = 20` — AI budget for attacks against enemy Host Stations.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `rec_budget = 10` — AI budget for reconnaissance.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pow_budget = 10` — AI budget for building power stations.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `rad_budget = 10` — AI budget for building radar stations.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `saf_budget = 10` — AI budget for building flak defenses.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `cpl_budget = 10` — AI budget for relocating the Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `con_delay = 0` — Delay in milliseconds before the AI starts spending its conquest budget.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `def_delay = 0` — Delay before the defense budget.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `rob_delay = 0` — Delay before the attack budget.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `rec_delay = 0` — Delay before the reconnaissance budget.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pow_delay = 0` — Delay before the power-station budget.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `rad_delay = 0` — Delay before the radar budget.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `saf_delay = 0` — Delay before the flak budget.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `cpl_delay = 0` — Delay before the Host Station relocation budget.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `mb_status = known` — Briefing visibility: known (visible), unknown (marked but unidentified) or hidden.
------------------------------------------------------------------------------------------------------------------------

### Squads (begin_squad)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_squad` — Opens a squad of pre-placed units (32 units at most). Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner = 1` — Owning faction of the squad.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vehicle = 2` — ID of the Vehicle in the squad. Required.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `num = 4` — Number of vehicles in the squad. Required.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pos_x = 5000` — X position in world units. Required.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pos_z = -3000` — Z position in world units. Required.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `useable` — When present, the player can command the squad.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `mb_status = known` — Briefing visibility (known, unknown, hidden).
------------------------------------------------------------------------------------------------------------------------

### Beam gates (begin_gate)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_gate` — Opens a beam gate (a passage to other levels). Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sec_x = 10` — Sector column of the gate.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sec_y = 7` — Sector row of the gate.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `closed_bp = 5` — Building ID used while the gate is closed.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `opened_bp = 6` — Building ID used while the gate is open.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `target_level = 12` — Level the gate leads to. Repeat the line for several destinations.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `keysec_x = 3` — Column of a key sector needed to open the gate (up to 16). Always followed by keysec_y.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `keysec_y = 4` — Row of the key sector, written right after keysec_x.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `mb_status = known` — Briefing visibility (known, unknown, hidden).
------------------------------------------------------------------------------------------------------------------------

### Gems and upgrades (begin_gem)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_gem` — Opens a technology-upgrade sector (gem). Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sec_x = 8` — Sector column of the gem.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sec_y = 1` — Sector row of the gem.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `building = 4` — ID of the Building shown on the gem sector.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `type = 3` — Sound played by the upgrade, from 0 to 6.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `script = data:scripts/gem1.scr` — Optional script run by the gem. The file must exist.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `msg_default = New_technology` — Message displayed. A language line `msg_<language>` wins when that language is active.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `nw_vproto_num = 11_12` — Vehicle numbers (up to 4) listed as newly available in the briefing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `nw_bproto_num = 5` — Building numbers (up to 4) listed as newly available.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_action` — Opens the list of changes applied when the gem is collected (modify_vehicle, modify_weapon, modify_building). Closed by end_action.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `mb_status = known` — Briefing visibility (known, unknown, hidden).
------------------------------------------------------------------------------------------------------------------------

### Enables (begin_enable)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_enable = 1` — Opens a list of units and buildings enabled for a faction (the faction number). Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vehicle = 6` — ID of a Vehicle to enable for the faction of the block.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `building = 4` — ID of a Building to enable for the faction of the block.
------------------------------------------------------------------------------------------------------------------------

### Level SuperItems (begin_item)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_item` — Opens a SuperItem (super bomb or wave) in the level. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sec_x = 1` — Sector column of the SuperItem.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sec_y = 12` — Sector row of the SuperItem.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `inactive_bp = 35` — Building ID while the SuperItem is inactive.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `active_bp = 36` — Building ID while the SuperItem is active.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `trigger_bp = 37` — Building ID in the trigger phase.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `type = 1` — Type: 1 is a super bomb, 2 is a wave.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `countdown = 998000` — Countdown in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `keysec_x = 3` — Column of a key sector that arms the SuperItem; always followed by keysec_y.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `keysec_y = 4` — Row of the key sector.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `mb_status = known` — Briefing visibility (known, unknown, hidden).
------------------------------------------------------------------------------------------------------------------------

### Lua API (lua_script)

- `World`: ShowMessageID(msgId), StartIterateUnits() returns an iterator (first unit or nil), GetUnits() returns a 0-based list, GetUserRobo(), GetUserUnit(), IsInRoboGun() returns bool, GetUserSquadsCount(), GetSector(x, y), GetSectorInfo(sector) returns {owner, w_type, x, y, type_id}, GetPowerStations() returns a list of {sector, power, effpwr}, GetGates() returns a list of {sector, mbstatus, keysectors}, ExitTutorialWindow(), IsRoboMapOpen() returns bool.
- `Bact`: GetStatus() returns a `Bact.STATUS_*` constant (NOPE, NORMAL, DEAD, IDLE, CREATE, BEAM), GetVehicleId(), GetOwner(), GetType() returns a `Bact.TYPE_*` constant (NOPE, BACT, TANK, ROBO, MISSLE, ZEPP, FLYER, UFO, CAR, GUN), StartIterateKids() returns an iterator, GetKids() returns a 0-based list, GetPSector().
- `IterUnits`: Next(iterator) and Get(iterator) return the next unit or nil; Free(iterator) releases the iterator.
- `System.RunScript(filename)` loads another script. All standard Lua libraries are available.

### Briefing and maps

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_mbmap` — Opens the mission-briefing background definition. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_dbmap` — Opens the debriefing background definition. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `name = DEFAULT.IFF` — Image file name (under levels/mbpix/). Used by both mbmap and dbmap.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `size_x = 640` — Image width.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `size_y = 480` — Image height.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_maps` — Opens the four maps of the level. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `typ_map` — Sector type map (hexadecimal): the visual type of every sector. It is followed by the width and height line and then the grid.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `own_map` — Owner map: 0 nobody, 1 Resistance, 2 Sulgogar, 3 Mykonian, 4 Taerkasten, 5 Black Sect, 6 Ghorkov, 7 neutral (power stations, radars, beam gates).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hgt_map` — Height map, from 0 to 255.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `blg_map` — Map of functional buildings (power, radar, flak and others), in hexadecimal.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `begin_briefing_text` — Level-local mission briefing (OpenNeoUA only): an optional block in the LDF after `begin_mbmap` / `begin_dbmap`, closed by `end`. Non-blank text overrides the language-file briefing; a missing or blank block keeps the locale briefing. Line breaks are kept and semicolons are literal; a line that is only `end` must be escaped with a leading backslash. Remove the block when targeting the original engine.
------------------------------------------------------------------------------------------------------------------------

```text
begin_briefing_text
Secure the power station.
Keep your Host Station alive.
end
```

### Level locations (OpenNeoUA only, in LDF files)

Location blocks belong in an LDF, never in OpenNeoUA.ini or Nucleus.ini. Without `begin_location` blocks, location notifications are off. A level that uses them requires OpenNeoUA and is not compatible with the vanilla engine.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `begin_location 1` — Unique positive ID. A level supports up to 256 locations.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `name = Dairopoli` — Display name, 1-96 characters. UTF-8 and Windows-1252 accents are supported. No semicolons/newlines.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `color = white` — Area colour. Used for in-game title text, perimeter lines, translucent map fill, and HUD name badges in briefing and debriefing screens. Default white; allowed values: white, yellow, orange, red, pink, purple, blue, cyan, green, lime, gray.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `sound = Data/Sounds/Locations/City.wav` — Optional entry sound. A missing/invalid sound does not prevent the title from appearing.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fade_in_ms = 450` — Fade-in duration, 0-10000 ms; default 450. Zero means immediate appearance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `hold_ms = 2800` — Fully visible duration, 0-60000 ms; default 2800.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fade_out_ms = 750` — Fade-out duration, 0-10000 ms; default 750. Zero means immediate disappearance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `point = 4800|-6000` — Polygon vertex in world X|Z coordinates. Repeat for every vertex (3-256).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end` — Implicitly joins the last vertex to the first.
------------------------------------------------------------------------------------------------------------------------

> **Location rules.** Use 3-256 distinct vertices forming a simple, non-zero-area polygon (concave is allowed). Coordinates accept `|` or `_` as separator. Only the directly controlled unit triggers in-game entry, at any altitude; leaving and re-entering repeats the notice, switching units inside the same location does not. The last listed area wins overlaps. A mission that starts inside an area announces it; loading a save inside does not. In briefing and debriefing, location areas are rendered immediately on the map with a color-tinted translucent fill, perimeter outlines, and a tactical HUD name badge with a dark backing. The in-game title uses Xolonium Bold and the color applies to the location name, briefing/debriefing perimeter, fill tint, and map badge. New saves keep the definitions; older saves without them use the original LDF. Sound paths are game-root-relative `Data/...` with `/` separators (no absolute paths, virtual prefixes or traversal). Invalid geometry, IDs or timing are logged and skipped; an invalid sound is ignored on its own; an unknown color falls back to white.

## Debug controls (F1-F12)

------------------------------------------------------------------------------------------------------------------------
- 🟢 `F1` — New debug mode: toggles a Minecraft F3-style information panel at the top left. It shows: build, FPS (measured, average and 1% low), frame time, display refresh rate (Hz), VSync, FPS cap, resolution, the graphics chip in use (flagged as integrated, iGPU, or dedicated), OpenGL version, position and sector, level and time, draw distance, sectors, polygons, unit and effect counts. The old DPS meter has been removed.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F2` — New debug mode: slows the world to 20% of normal speed; the controlled unit, its mounted parts, actions and player-fired projectiles remain at normal speed. Other allies stay slowed. Toggles on/off; F4 still freezes everything. Single-player only.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F3` — New debug mode: speeds game time up to 200% of normal speed (+100%); toggles on/off.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F4` — New debug mode: toggles game-time freeze; useful for screenshots and video capture. It can freeze time while slow or fast debug speed is active.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F5` — New debug mode: instantly destroys the vehicle or gun under the mouse.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F6` — New debug mode: toggles invulnerability for the allied vehicle under the mouse, or the player-controlled vehicle when the mouse is not over a unit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F7` — New debug mode: toggles whether enemy AI ignores the allied vehicle under the mouse, or the player-controlled vehicle when the mouse is not over a unit. This runtime-only setting is not saved; the vehicle stays visible and can still attack, and its attacks do not cancel the exclusion. Projectiles already fired, crossfire and area damage can still hit it.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F8` — New debug mode: toggles invulnerability for all units.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F9` — New debug mode: displays legacy collisions, main compound spheres, projectile blast spheres on impact and below/forward box and cone AI fire volumes. The selected Weapon slot is drawn in orange without a dimension label. Invisible bridges are not shown.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F10` — New debug mode: toggles the in-game HUD visibility; useful for screenshots and video capture.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F11` — New debug mode: fills the three Host Station bars (Genesis, Energy and Move), makes all Host-issued in-game actions free and bypasses max_active_at_once for player production. Disabling the cheat restores the limit without removing existing units or changing prototype/GEM values.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F12` — New debug mode: switches the player faction and lets the player control the enemy vehicle or gun under the mouse. Requires a living Host Station of that faction; does not change the unit owner. Single-player only.
------------------------------------------------------------------------------------------------------------------------

F2 and F3 exclude each other: enabling one disables the other. F4 can freeze time while slow or fast mode is active. Built-in slowdown effects take priority over debug acceleration.

## Retired and ignored keys

The engine still reads these keys so old scripts, profiles and saves load, but they have no effect. Do not use them in new content.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `<status>_icon = ...` — Vehicles. `regen_icon`, `drain_icon`, `damaged_icon`, `spawn_icon`, `radar_icon` and `power_icon`: status icons are assigned automatically; set their images with `ui.status_icon_*`.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_data_slot = 0` — Vehicles (Host Station). Read and ignored.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_push_radius = 2000` — SuperItems. Deprecated; the wave push now uses `wave_push_force` alone. A warning is logged.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_push_falloff = 1` — SuperItems. Deprecated, same as `wave_push_radius`.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `superbomb_center_vproto = 134` — World.ini `begin_superitem`. Read and ignored; only `superbomb_wall_vproto` is used.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vehicle_sector_ratio = 1.0` — Levels (`begin_level`). Read and ignored.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `default_view = 0` — USER.TXT. Retired; the cockpit camera comes from `game.cockpit_camera`.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `username = Player` — USER.TXT. Read and ignored.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `netname = Player` — USER.TXT. Read and ignored.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `numbuddies = 0` — USER.TXT. Read and ignored.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `palettefx = 0` — USER.TXT. Read and ignored.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `qualmode = 0` — USER.TXT. Read and ignored.
------------------------------------------------------------------------------------------------------------------------

Nucleus.ini also keeps a few keys of the original graphics driver that nothing reads any more: `gfx.all_modes`, `gfx.alpha`, `gfx.display`, `gfx.display2`, `gfx.force_alpha_textures`, `gfx.force_emul`, `gfx.force_soft_cursor`, `gfx.mode`, `gfx.newsky`, `gfx.skylength`, `gfx.use_draw_primitive`, `gfx.xres`, `gfx.yres`, `tform.backplane`, `tform.frontplane`, `tform.zoomx`, `tform.zoomy` and `input.debug`.

## OpenNeoUA Studio notes

The vehicle Model row can edit the optional Spawn state. The asset browser lists its resolved 3DS, BASE or VP source.

How the parameters above are handled in OpenNeoUA Studio. They do not change the engine.

### Models

- **Missing texture fallback:** Import Model and the Studio preview try the first readable same-named file in the model folder and its subfolders after a normal lookup reports a missing texture. The orange Loaded (fallback) status warns that it is outside the active set lookup and may display different colors. Choose Source can override it. Resources > Model Resources reopens the importer with the open external model, its current archive and manual choices.
- **Models without copies of set textures:** in Import Model, select the installed set's `Set.BAS`. Existing loose files win and missing resources, including textures referenced by `.ANM` frames, are resolved from the archive. The palette and rendering tables (`PALETTE`, `REMAP`) are read from the installed set's folders and must stay in the installation. The Export Model button writes a complete package with textures and rendering profile: remove the original resources you do not intend to distribute. A custom texture absent from the set needs its own file.
- **Apply to States** (Model Editor toolbar, or File > Apply Changes to Other States): finds the other states of the open model through Entity List, folder names, Bas Manager and similar shape, shows each one with the changes applied and writes only the ticked states. Faces are paired by position; animated faces and faces of one state only are left alone. Nothing is ticked automatically. A Genesis state has only animated faces, so it cannot be paired: with "Rebuild the shape of Genesis states" on, it takes the whole shape of the open model and every face gets the Genesis material again (emitters keep their place).
- **Viewport context menu:** Select All Emitters and Deselect All Emitters operate on the current model's emitter anchor vertices. With one vertex selected, Add Emitter attaches the chosen emitter to that vertex. With the selection cleared, Add Emitter lets you pick the anchor after choosing the emitter. Clear the selection to use Add Texture or Add ANM. Add Texture places a static, rectangular face with the chosen bitmap and its aspect ratio. Replace Texture and Replace ANM follow the selected faces' material bindings.
- **Generate Model Genesis:** converts every polygon surface of the current model, regardless of selection.
- **Texture mapping copy:** previews and copies the selected faces' mappings to chosen loose BASE states. Faces must match uniquely and the destination must resolve the same texture.
- **Replace Texture, Import External Texture:** the Choose Texture dialog can load a PNG or a loose `.ILBM` file from outside the set. An ILBM keeps its own palette. A PNG is accepted only if every pixel already matches a color of the replaced texture's palette (or the family's shared external palette); the shared SET palette is never changed, so a PNG with unmatched colors is refused rather than quantized. On an animated (`bmpanim`) binding the import replaces a single-image animation's picture only; an animation with more than one distinct frame image is refused.
- **Emitter and ANM previews:** the Add Emitter dialog, the simulated emitter preview and the ANM/Emitter preview panel approximate additive (flat-tracy) particle stages with the engine's own HI/ALPHA effect PNG override when one exists, or otherwise with "luma" alpha (brightness-normalized color, alpha from the brightest channel) instead of the raw indexed palette colors - closer to the engine's additive glow while staying transparent. True additive compositing (`CompositionMode_Plus`) is not used for the preview: with many overlapping particles in one frame it saturates to an opaque black square instead of staying transparent, which "luma" avoids by design.
- **MCP texture coordinates:** `studio_set_uvs` edits ordinary AMESH faces and static ILBM AREA terrain faces. Animated AREA faces retain their animation coordinates. Explicit UV lists are supplied per selected polygon, or in a dictionary keyed by polygon ID.
- **MCP emitter settings:** `set_emitter` changes any particle emitter setting (acceleration and magnify vectors, start speed, birth rate, lifetime, sizes, noise) of a loose model; `use_set_animation` points a loose `.ANM` at the same animation of the installed `Set.BAS`, so no copy of the original file is needed.
- **Sulgogar terrain textures:** the infected ground, the station skin and the strips between ground families are local assets. From Studio, run `tools/build_sulg_terrain_textures.py --data "<Data folder>"` with your installed Set 3. It writes the textures to `Models/Sector/Set3/Textures`. The models reference them by name.
- **Emitters:** deleting geometry keeps particle emitter source points and updates their references; it is refused if a source point is already invalid. Copy Emitter duplicates and Cut Emitter moves an emitter: both start an animated gray preview, click a point to place it, Esc or right-click cancels. During Cut the original circle disappears unless other emitters share that point.
- **Ground Alignment tab:** a schematic 2D profile of the model surfaces against flat ground. The profile and Auto exclude particles and, when other surfaces exist, additive effects (an additive-only estimate is marked). Scroll over the profile to adjust `overeof`, then verify the contact in-game.
- **Origin marker:** Edit Mode always shows the model's local origin (0, 0, 0) as the exact vanilla faction arrow (`Wireless/TRIANGLE.SKL`), tinted with the active theme's accent color. It is the point the game anchors the unit to (world position, faction marker, cockpit 000) regardless of mesh shape. It is a visual indicator only, not interactive or selectable; move the origin with **Set Origin to Model Center** (formerly "Center Model at Origin") or **Set Selection as Origin** (formerly "Center Selection at Origin"). Both apply the same offset to every local vertex, preserving the shape. Both commands keep the model in place on screen while moving the marker to the model bounds center, the selected point or the selection bounds center. Particle previews follow their anchor vertices. Overwrite automatically saves root origin changes to matching loose states in the model folder and its subfolders, including their emitter anchors and sensors. Undo before saving cancels the pending change. Both commands are in the Origin section and in the viewport context menu, including when an emitter anchor is selected.

### Entities and effects

OpenNeoUA Studio resolves Bruch IDs for entity and sector effect previews using the installed script catalog. The entity effect editor preserves the `bruch_id` reference when saving.

- **Entity Designer, Add:** accepts supported effects and routes model, decal and crater keys to their rows. Unknown keys inside `begin_fx` are preserved in the saved text. Several rows (for example `crater_*` with `ground_decal_id`) can be written in one Add. Model, decal and crater keys cannot share an effect with `begin_fx`, `dest_fx` or sound keys; an unknown key, a crater key on a vehicle or an unknown sound event gets a message that names the key.
- **Visuals & FX:** lists existing items or drafts; craters, decals and Death behavior are marked red because vanilla does not support them. Each `begin_fx` row shows its header comment, the number and IDs of its VPs and any BASE or 3DS names. Click an effect in the 3D view to select its row (white border; the smallest wins when they overlap); double-click away to deselect; dragging orbits; the unit and hidden effects cannot be clicked. The timeline starts at 0.25x, with a speed menu. The flight altitude starts at 300 for flying units and the weapon preview starts at 300 (an artillery shell uses its arc height).
- **Crater and Ground decal rows:** values typed in a new effect are transferred to the Crater row. Delete removes the Crater and Ground decal settings from the definition's scripts, `modify_*` blocks included. For cluster weapons put craters on the child that hits the ground: the cluster preview shows them read-only, and a parent's crater applies to its own impact only. When a cluster weapon spawns itself, both crater rows share the same settings.
- **Death behavior row (vehicles only):** edits `disable_genesis` and `air_dead` as free text, like the Crater and Ground decal rows; values typed in a new effect are routed here. The death-sequence preview follows them: `air_dead = 1` on a Flyer or UFO model skips the falling wreck and plays the terminal state in place, in mid-air; `disable_genesis = 1` or `at_death` removes the death plasma from the preview (the row's tooltip explains both keys, and a note under the timeline confirms which rule fired). Weapons never show this row; saving either key on a weapon block is refused.
- **Yes/no values:** Studio reads every yes/no form the engine accepts (`1`, `yes`, `true`, `on`, any non-zero number, and their opposites) in effect and Death behavior definitions, and saves scripts in the vanilla `0`/`1` form.
- **Kamikaze payload:** Visuals & FX opens Payload effects to edit the charge effects in place; the menu switches to the carrier model and its destruction sequence.
- **Wireframe Editor:** previews components from other entities or files and saves them as separate files linked to the vehicle or weapon. Right-click a file and choose Copy Path to copy its path.
- **Fire volumes:** for a downward cone, opposite handles adjust the same symmetric angles.
- **Icon Selector:** the Page list switches between the original H_FINDER page and the OpenNeoUA page (`Graphics/UnitIcons`). New `type_icon` keys are written only to the OpenNeoUA page; the original page and TYPE_NS.FON are never changed. Clicking a cell with no symbol gives it a random free one; Random or the Symbol field change it. The Font Page box shows the path of the page on show. On Original it is the H_FINDER found in the game, fixed. On OpenNeoUA it is the datasheet (`Graphics/UnitIcons/H_FINDER_OpenNeoUA.PNG` and `.FON`, the only files the game reads): Browse checks that a PNG has the same grid as the original icons and copies it there; Create makes an empty one. The OpenNeoUA page can be opened even when the datasheet is missing. A `.FON` next to the chosen PNG is copied too, otherwise the current keys stay. Replaced files keep a `.bak` copy. Cockpit View: with the camera offset disabled, the preview shows no unit.

### Sectors

- **External sectors (Sector Editor, File menu):** Export Sector writes the current sector as a folder (`Data/Models/Sector/<Name>/<Name>.sector`) or as a `.onua` archive of that folder. Models, skeletons and textures that exist in `Set.BAS` are written by name only; only your own loose files are copied. Import Sector opens a `.sector` file or a `.onua` archive (unpacked into `Data/Models/Sector/<Name>` first) in a free slot for editing. Its own models are never saved into the set: export it again, then clear or wipe the slot before Overwrite Set (the save is refused while a sector still uses them).
- **Map Editor and Entity Designer:** building previews of an external `sec_type` show that sector; map tiles, icons and typ_map still use the fallback set sector.
- **Map icons tab:** the current sector at the five map zooms (own icons where set, vanilla symbols elsewhere). Generate icons draws them from the geometry of the sector's own models (every zoom and damage state); set models keep their vanilla symbol. Load cell icon / Load sector icon take your PNG or SVG; Generate missing icons on export is on by default. Icons go into the `map/` folder of the exported sector.
- **Play:** The Sector Editor opens with playback enabled. Play/Pause keeps the same preview, camera, zoom and selection. All editors share the same Play/Pause button. Play, >> and Speed animate the entire sector in the main preview, including every cell's animated textures and emitters. The explicitly selected model preview still plays that model alone. Changing damage state while playing shows each cell's outgoing destruction effects at its sector position; an effect whose FX vehicle is not defined in the scripts is reported in the status bar.
- **Destruction effects:** Definitions shows named effect choices and separate Side X, Forward Z and Height fields. Forward Z is displayed in model-space coordinates; the editor converts it back to the original file axes on save. Preview selected plays that row in the main sector view. Unknown effect IDs are preserved and identified as missing from the scripts.
- **Definitions** follows the model of the selected cell in the current damage state.
- **MCP:** `export_external_sector` (with `map_icons`, default true) and `import_external_sector` operations of `studio_set_edit` (see `studio_help('sets')`).

### Gun points

- **Host Stations:** gun points accept only vehicles with `model = gun`, the `unit_*` family included. In Gun Points the Move strength starts at 50.
- **Buildings:** choose Buildings in Entity List; buildings with mounts expose Gun Points. Show Mounted Flak toggles the 3D models while the direction markers stay; Restore Loaded Position restores the script coordinates of the selected mount; Move to 0 / 0 / 0 places it at the model origin. Edits write the vanilla `sbact_*` fields. The Vehicle menu lists only Flak vehicles of the catalog (`model = gun`), so adding a gun point needs at least one Flak. A zero `sbact_vehicle` ends the active list, inherited slots in `modify_building` included.
- **Numeric script output:** Generated numeric fields use at most two decimal places. The Fire Area dead zone uses whole units. The Fire Area preview and script export share the same output.
- **Briefing Model Settings:** turrets mounted on Host Stations and buildings rotate with the model, in Debriefing mode too.

### Levels

- **Level Info:** Ambient sound, Clouds (with an animated map preview) and Briefing text edit `ambient_sound`, `cloud_id` and `begin_briefing_text`. Studio escapes a briefing line that is only `end`.
- **Locations tab:** red OpenNeoUA-only marker, polygon editing with the chosen map color, text color, Undo/Redo and an animated/audio preview.
