# OpenNeoUA - Parameter Guide

> Public reference for modders. It explains **every** parameter understood by the engine: the vanilla Urban Assault ones and the OpenNeoUA extensions, grouped by the file and block they belong to.

## Legend

Each parameter is one line: marker, `name = value`, em dash and description.

- 🔵 **Vanilla** parameters: they already exist in the original Urban Assault.
- 🟢 **OpenNeoUA** parameters: new, added by the engine.

## Contents

- [Introduction and general rules](#introduction-and-general-rules)
  - [Sound pitch and effect radius](#sound-pitch-and-effect-radius)
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
  - [Collisions](#collisions)
  - [Appearance (models, VP, 3DS, trails)](#appearance-models-vp-3ds-trails)
  - [Impact effects](#impact-effects)
  - [Debuffs](#debuffs)
  - [Sounds, palette flashes and shakes](#sounds-palette-flashes-and-shakes-1)
  - [OpenNeoUA special features](#openneoua-special-features-1)
- [Buildings (Buildings.cfg)](#buildings-buildingscfg)
  - [Identity and cost](#identity-and-cost)
  - [Stats](#stats)
  - [Flaks mounted on the building](#flaks-mounted-on-the-building)
  - [Appearance](#appearance)
  - [Sounds](#sounds)
  - [OpenNeoUA special features](#openneoua-special-features-2)
- [Buffs (Buffs.cfg)](#buffs-buffscfg)
- [Debuffs (Debuffs.cfg)](#debuffs-debuffscfg)
- [Clouds (Clouds.cfg)](#clouds-cloudscfg)
- [SuperItems (SuperItems.cfg)](#superitems-superitemscfg)
- [Ground Decals (Ground_Decals.cfg)](#ground-decals-ground_decalscfg)
- [Startup and Game Config (Startup.cfg)](#startup-and-game-config-startupcfg)
- [OpenNeoUA.ini](#openneouaini)
- [Nucleus.ini](#nucleusini)
  - [Graphics](#graphics)
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
  - [Briefing and maps](#briefing-and-maps)
  - [Level locations (OpenNeoUA only, in LDF files)](#level-locations-openneoua-only-in-ldf-files)
- [Debug controls (F1-F12)](#debug-controls-f1-f12)

## Introduction and general rules

```text
 OpenNeoUA Parameter Guide
 Public configuration and scripting reference for modders

 OpenNeoUA is an independent, open-source evolution of the Urban Assault
 UA_source/OpenUA engine. Its goal is to extend real engine capabilities while
 preserving compatibility with vanilla data, levels, scripts, saves and the
 original game feel wherever possible.

 Overlapping collision shapes of two units in Genesis separate in a consistent
 direction with bounded movement. Initial overlap may remain visible briefly.

 This file is a REFERENCE AND EXAMPLE CATALOG, not a configuration that should
 be copied wholesale into a game installation. Add only the parameters needed
 by your mod or project. Example values demonstrate syntax and typical usage;
 they are not necessarily recommended balance values.
 New damage-bearing parameters use energy-based names for Weapons, waves, impacts, collisions and
 falls; former damage-based names are not aliases. Existing `damaged_*` settings and vanilla save
 formats remain unchanged.

 IMPORTANT COMPATIBILITY NOTES
 - When an OpenNeoUA parameter is omitted, the engine uses the documented
   fallback or legacy behavior for that feature.
 - Many extensions are intentionally opt-in. Read the comment for each
   parameter before enabling it.
 - Percent values such as 5% are interpreted as percentages where explicitly
   documented. The same parameter may sometimes also accept a fixed numeric
   value without the % symbol.
 - Where supported by a parameter, the following value symbols apply:
     ~  means "random between". `min~max` picks a random value between
        the minimum and the maximum, for example snd_launch_pitch = `900~1100`
        or count = `2~4`. A single value keeps the result fixed.
     |  separates the fields of a list or of a group of values, in the
        order written in the parameter name: offset_xyz = x|y|z,
        fade_inout = in|out, tint = R|G|B|A, vp_model = 138|193,
        trigger = detonate|impact_world.
   Some parameters combine both symbols. For example:
   launch_xyz = `-15~15|-20|-15~15`. Check each entry for supported syntax.
 - The underscore no longer separates values. It only appears inside names
   (impact_world, air_destroyed). The original Urban Assault parameters
   (dest_fx, scale_fx, ...) keep their underscore syntax and also accept |.
 - Parameters that used to have one entry per axis or per endpoint are now a
   single compact parameter: spin_xyz = x|y|z, fade_inout = in|out,
   lifetime_minmax = `min~max`. All values are required: a value with the wrong
   number of parts is ignored and the log reports it. The former separate
   names (spin_x, spin_y, spin_z, fade_in, fade_out, ...) and the former
   underscore syntax no longer exist and have no fallback.
 - Original Urban Assault parameters (robo_gun_pos_x, sbact_pos_x, shk_hit_x,
   ...) are unchanged.
 - Unless stated otherwise, time values are in milliseconds.
 - "World units" refers to the coordinate/distance scale used by the engine.
 - Paths documented as Data-relative are resolved from the game's Data folder.

 CANONICAL EXTERNAL PATH RULE
 For external file paths, use the form documented for each parameter. Many
 model and sound paths use an explicit Data/... prefix; some settings accept
 paths relative to Data or to a more specific folder.
 Forward slash (/) is the canonical separator used by this public guide and
 by modern OpenNeoUA scripts. Shared Data-path loaders normalize supported
 legacy backslashes when needed, but new content should prefer /.
 Do not add a second Data/ prefix to an example that already contains it.
 Older relative paths may remain accepted only as a compatibility fallback.
 Example: Data/Models/3ds/Tracer.3ds

 VISUAL MODEL FALLBACKS
 OpenNeoUA can extend legacy VP visuals with external BASE and 3DS assets.
 Where all three forms are supported, the usual priority is:

     3DS -> BASE -> VP

 A `3ds_*` asset therefore overrides the matching `base_*` and `vp_*` visual when it
 loads successfully. A `base_*` asset falls back to the corresponding VP.
 BASE parameters point to a .BASE file as the visual family's single entry
 point. The native loader resolves supported BASE/KIDS/SKLT relationships,
 ILBM/texture resources, ANM/VANM animation data and supported embedded assets.
 .BASE dependencies are resolved from the file's folder and its subfolders as
 supported by the loader.
 In OpenNeoUA Studio, Model Editor > Apply to States (toolbar, or File > Apply Changes to Other States) finds the other states of the open model by itself, through Entity List, folder names, Bas Manager and similar shape, shows each one with the changes applied and writes only the ticked states. Faces are paired by position, animated faces and faces of one state only are left alone, and nothing is ticked automatically, Genesis included.

 In Model Editor, deleting geometry preserves particle emitter source points and updates their references. Deletion is refused if a source point is already invalid.
 In Entity Designer, Add accepts supported effects and routes model, decal and crater keys to their respective rows while retaining other existing values. Unknown keys inside `begin_fx` are preserved in the saved text, even when Studio cannot interpret them yet. Keys of several rows (for example `crater_*` with `ground_decal_id`) can be written in one Add: the editor keeps the first row, the others are filled and saved together with it. Model, decal and crater keys cannot share an effect with `begin_fx`, `dest_fx` or sound keys; a key Studio does not know, a crater key on a vehicle or an unknown sound event gets a message that names the key.
 The Visuals & FX list shows only existing items or drafts being created; craters and decals are marked red because vanilla does not support them. The weapon preview starts at Projectile altitude 300; an artillery shell uses its configured arc height.
 For cluster weapons, configure craters on the child weapon that hits the ground. The cluster preview shows child craters and a read-only row with their IDs when configured; open the child weapon to edit them. A parent projectile's crater applies to its own impact, if any, and is not inherited by its children.
 previews and copies selected faces' texture mappings to chosen loose BASE
 states. Faces must match uniquely and the destination must resolve the same
 texture. No state, including Genesis, is selected automatically.

 The exact runtime behavior is authoritative. This guide documents the public
 parameters and their intended use, but modders should test combinations that
 substantially change vanilla systems.
```

### Studio: models without copies of set textures

A `.BASE` model can keep its own skeletons and animations without local copies of the original textures. In **Import Model**, select the installed set's `Set.BAS`: existing loose files take precedence, and missing resources are resolved from the archive. This includes textures referenced by `.ANM` animation frames. If local copies are removed while Studio is open, deleted paths are ignored the next time the model is imported or reopened.

The palette and rendering tables are read from the installed set's folders (`PALETTE` and `REMAP`), rather than necessarily from inside `Set.BAS`. They must remain available in the installation. Resolving these resources does not copy them into the model folder. **Export Asset Family** creates a complete package, including textures and the rendering profile: before publishing it, exclude original resources you do not intend to distribute. A custom texture absent from the set still needs its own file.

### Sound pitch and effect radius

Many sound pitch parameters accept `min~max`, allowing one event to choose a different pitch each time and reducing repetition without duplicating assets.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_...._pitch = -4000~500` — Example pitch range supported by pitch parameters; useful for randomized sound variation.
------------------------------------------------------------------------------------------------------------------------

Radius parameters localize sound, palette and shake events in world space. Setting 0 or omitting the radius preserves the documented legacy behavior.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_***_radius = 4000` — Maximum radius for snd events. Volume decreases linearly from the source to 0 at the limit. 0 or absent = legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_***_radius = 4000` — Maximum radius for pal events. Intensity decreases linearly from the source to 0 at the limit. 0 or absent = legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_***_radius = 4000` — Maximum radius for shk events. Intensity decreases linearly from the source to 0 at the limit. 0 or absent = legacy behavior based on the corresponding `shk_***_mute` behavior.
------------------------------------------------------------------------------------------------------------------------

## Vehicles (Vehicles.cfg)

### Identity, class and production

------------------------------------------------------------------------------------------------------------------------
- 🔵 `new_vehicle = 21`                      — Opens the definition of a new Vehicle with the given numeric ID. If the ID already exists, the prototype is rebuilt from scratch, so every value must be written again.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `modify_vehicle = 21`                   — Reopens an existing Vehicle so that only the listed values change. Typical use is inside level files and tech-upgrade (begin_gem / begin_action) blocks. Values not mentioned stay untouched.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `end`                                   — Closes the current new_vehicle / modify_vehicle block.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `enable = 1`                            — Makes the Vehicle available to the given faction (1 Resistance, 2 Sulgogar, 3 Mykonian, 4 Taerkasten, 5 Black Sect, 6 Ghorkov, 7 neutral/tutorial). Use one line per faction. In a level file the same effect is reached with begin_enable.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `disable = 1`                           — Removes the Vehicle from the given faction (same numbers as enable).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `model = tank`                          — Vehicle class, which decides the movement model and the engine code that drives the unit: tank and car (ground), heli (helicopter), plane, glider and zeppelin (aircraft with different pitch/bank behavior), ufo (hovering craft), gun (fixed flak turret) and robo (Host Station). Unknown values make the whole block invalid.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `name = Tiger_Tank`                     — Display name of the Vehicle. Underscores are shown as spaces.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `type_icon = A`                         — Single character that selects the unit icon used in lists and on the map. The character must be mapped to coordinates inside TYPE_NS.FON, which points into the H_FINDER graphic. See the OpenNeoUA entry further below for the Unicode form.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vo_type = 1`                           — Selects the voice set used for the unit's spoken messages. It is read as a hexadecimal number.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `wireframe = wireless/tiger.skl`        — Wireframe outline of the unit used by the in-game interface (unit list and wireless panel). Path to a skeleton file.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_wireframe = wireless/tigerhud.skl` — Wireframe used for the cockpit / HUD overlay while the player controls the unit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `mg_wireframe = wireless/mg.skl`        — Wireframe of the machine gun shown in the interface.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `wpn_wireframe_1 = wireless/wpn1.skl`   — Wireframe of weapon 1 shown in the interface.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `wpn_wireframe_2 = wireless/wpn2.skl`   — Wireframe of weapon 2 shown in the interface.
------------------------------------------------------------------------------------------------------------------------

OpenNeoUA adds new vehicle models/aliases and extends existing vehicle definitions with AI affinities, per-event speech, custom visuals, collision behavior, status systems and other optional capabilities.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `model = cruiser`                                                         — Airborne vehicle model with smooth up/down response and comparatively rigid left/right response.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `model = module`                                                          — Alternate alias for model = gun; behavior is the same.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `model = mimic`                                                           — Mimic vehicle type. Creates a copy of itself using another eligible unit already present on the map as the visual/template source; excludes itself, robo and gun classes. Add at_death_spawn_vehicle to generate units when it dies.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mimic_tint = 60|255|90|255`                                              — RGBA multiplier applied to the visual copied by the Mimic, for both VP and 3DS visuals.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mimic_energy_cost = 20000~30000`                                         — Energy required to create the mimic. Single value = fixed cost; `min~max` = random cost.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_mimic_sample = sounds/sulgi.wav`                                     — Mimic-specific sound used during disguise.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_mimic_volume = 120`                                                  — Mimic sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_mimic_pitch = 4500~5500`                                             — Mimic sound pitch. Single value = fixed; `min~max` = random pitch selected on each playback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `speech_voicepack = Data/Sounds/Speech/Voicepacks/resistance-tank-medium` — Unit event voices inside new_vehicle: point directly to the final pack directory. Direct pack path under Data. Missing or 0 uses vanilla voices.
------------------------------------------------------------------------------------------------------------------------

The engine recognizes these fixed WAV stems: unit_ready, unit_idle, order_ack_friendly, order_ack_enemy, unit_control_entered, enemy_sector_entered, unit_recovered, enemy_unit_spotted, enemy_host_found, request_support, unit_retreating, unit_lost, enemy_unit_engaged, enemy_unit_destroyed, enemy_host_destroyed, powerstation_captured. Numbered variants such as unit_ready_01.wav and unit_ready_02.wav are selected randomly without an immediate repeat. Missing event files use the existing vanilla voice handling. The vanilla vo_type field remains supported; speech_voicepack is the only custom voice path. Mimic uses the disguised vehicle prototype’s pack. WAV files may be mono or stereo, 8 or 16 bit PCM; voice filters are not applied. Restart after changing scripts or audio. Only the 16 engine events listed above are supported.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `max_active_at_once = 15`                  — Maximum active count for this specific unit type, excluding instances already placed by level definitions. The campaign may raise this limit through GEM unlocks. Undefined (0) = no limit, as in the original game. The classic GEM popup shows a MAX row under the vehicle name only when the limit is defined (not in network games).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `add_max_active_at_once = 3`               — GEM upgrade value used inside modify_vehicle to increase and persist max_active_at_once.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `production_cost = 150`                    — Displayed Genesis price for this vehicle. Overrides the vanilla energy-derived price (energy x 2 / 100) without changing hit points. The Genesis list is sorted by this final price. 0 or absent = vanilla behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vehicle_description = Heavy assault unit` — Optional vehicle description shown in the Database. If absent, the Description row is not shown.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `purchase_cost = 1000`                     — Plasma cost to unlock the vehicle in the Workshop. If absent, the vehicle is not offered. Zero allows a free unlock. Purchases and the equipped Host Station are stored in the player profile and carry over to later levels.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `type_icon = K`                            — Vehicle icon (a .FON glyph key). A Unicode symbol is also accepted, e.g. ∆ or U+2206, linked in the .FON to the icon coordinates. Any value other than I lists the vehicle in the Database. Vanilla compatibility: vanilla reads only the first byte of the value, so use a plain ASCII character (letters, digits or symbols such as ] or `). U+XXXX values and Unicode symbols are OpenNeoUA-only; vanilla reads U+2206 as the letter U (the Radar Station icon). When a non-ASCII symbol is needed, write it as U+XXXX: a pasted symbol can be corrupted by the editor's text encoding, and an unreadable icon falls back to the default A icon. In the .FON, write new keys as a single byte; a U+XXXX line must stay above the U line, otherwise vanilla replaces the Radar Station icon. The replacement character U+FFFD is rejected.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `briefing_size = 100`                      — Size of the vehicle in the briefing and debriefing model preview. Larger values make the model larger; smaller values make it smaller. Valid range 1-10000; 0, absent or any other value uses the legacy radius framing. Independent of collision_shape and `coll_*`; gameplay geometry, collisions and save format are unchanged.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `briefing_xyz = 0|0|0`                     — Vehicle-only briefing/debriefing model offset in camera-axis model units: X positive right, Y positive down, Z positive away from the camera. Position remains independent of briefing_size; units refer to the default vehicle framing.
------------------------------------------------------------------------------------------------------------------------

### Stats and physics

------------------------------------------------------------------------------------------------------------------------
- 🔵 `energy = 1000`         — Hit points (energy) of the Vehicle. In vanilla this value is also the Genesis cost of the unit; production_cost overrides the price without touching the hit points.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shield = 20`           — Armor as a percentage from 0 to 100: it reduces incoming damage. 100 makes the unit immune to the regular damage formula.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `mass = 400`            — Mass of the Vehicle. Together with force and airconst it decides acceleration and how hard the unit is to push around.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `force = 20000`         — Engine thrust. Higher values accelerate the unit faster and raise its top speed together with airconst.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `maxrot = 1.0`          — Maximum turn rate of the unit. Higher values let it steer and rotate faster. For `gun_type = radar`, it sets the continuous rotation in radians per second.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `airconst = 500`        — Air resistance / drag. Higher values brake the unit harder and lower its top speed; top speed is roughly where drag balances force.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `height = 200`          — Preferred flight height above the ground for flying classes (heli, plane, glider, zeppelin, ufo).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `radius = 20`           — Physical collision radius of the unit. It decides collisions, hit detection and how much space the unit occupies.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `overeof = 10`          — Vertical offset used by ground collision: the distance between the unit origin and the ground. Studio's Ground Alignment tab shows a schematic 2D profile of the model surfaces against flat ground. The profile and Auto exclude particles and, when other surfaces exist, additive effects; an additive-only estimate is marked. Scroll over the profile to adjust `overeof`. Verify contact in-game.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vwr_radius = 20`       — Collision radius used by the viewer / player camera while the unit is controlled or observed.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vwr_overeof = 30`      — Vertical offset used by the viewer. In practice it sets the camera height when the player enters the unit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `max_pitch = 1.2`       — Scales the engine sound pitch with speed. Despite the name it does not tilt the model; it is used by the movement-sound code.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `radar = 2`             — Radius, in sectors, of the area the unit reveals around itself on the map (fog of war). 0 means no radar.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `unhide_radar = 1`      — Radius, in sectors, in which the unit reveals enemy units that use hidden. Sectors uncovered this way make hidden enemies visible to this faction.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hidden = yes`          — Makes the unit invisible to other factions. It is revealed only inside the area covered by another unit's unhide_radar.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_energy = 200`      — Upgrade-only (gem / script): adds the value to the current energy instead of replacing it.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_shield = 10`       — Upgrade-only: adds the value to the current shield.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_radar = 1`         — Upgrade-only: adds the value to the current radar radius.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_unhide_radar = 1`  — Upgrade-only: adds the value to unhide_radar.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `disable_collision = 0` — Vehicle and Weapon: 1 disables physical contacts with units, projectiles, terrain and buildings, including radius, compound spheres and collision_shape. 0, absent or invalid = normal collisions.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `push_resistance = 0.5` — Resistance level 0-10 against weapon/event push. Also applies to Host Stations; not useful for static gun classes.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `disable_genesis = 1`   — Vehicle only: 1 removes everything Genesis-related (the plasma) from this Vehicle. On creation it appears instantly, fully formed: no plasma, no scale-in, no genesis sound (its dest_fx create effects still play). On death the wreck falls and explodes as usual, but no death plasma is released. Works on every Vehicle class, host stations included. 0, absent or invalid keeps vanilla behavior.
------------------------------------------------------------------------------------------------------------------------

For Weapons using an ai_fire_trigger volume, these three values control maneuvering rather than firing distance. The configured volume authorizes fire, subject to actual weapon constraints.

Mobile Power turns a vehicle into an energy source using the same broader energy ecosystem as stationary power sources, while allowing its feedback to be defined through a reusable new_cloud profile.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `power = 130`                         — Amount of mobile energy generated for this vehicle and nearby allied units.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `power_radius = 2400`                 — Radius within which the vehicle supplies energy to itself and nearby allies.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `power_falloff = 1`                   — 1 reduces Power effect strength with distance; 0 keeps it uniform within power_radius.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `power_cloud_id = 3`                  — ID of the visual/audio Cloud associated with this Vehicle mobile Power. -1, absent or unknown ID = no Power Cloud.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_force_malus = -50%`          — Force modifier while damaged. Example: -50% reduces force by 50%.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_maxrot_malus = -50%`         — maxrot modifier while damaged. Example: -50% reduces maxrot by 50%.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_mgun_shot_time_malus = -50%` — MGUN fire-rate modifier while damaged. Example: -50% reduces MGUN rate of fire by 50%; 0 or absent = unchanged.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_shot_time_malus = -50%`      — Weapon fire-rate modifier while damaged. Example: -50% reduces Weapon rate of fire by 50%; 0 or absent = unchanged.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_snd_pitch = -50%`            — Pitch modifier for sounds emitted by the damaged unit. Negative values lower pitch; positive values raise it. 0 or absent = unchanged.
------------------------------------------------------------------------------------------------------------------------

Applied after screen placement, so the offset does not rotate with the model. Independent of Database/Workshop, cockpit, physics and collisions. Omitted or 0|0|0 preserves classic placement. Each axis must be finite and within -100000..100000; an invalid tuple leaves the previous valid value unchanged.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `collision_shape = Data/Collisions/Tiger_Set1.collision`  — Model-derived vehicle collision generated by OpenNeoUA Studio. A valid profile replaces physical spheres for movement, vehicle contacts and direct projectile/MGUN/laser hits. Empty/0/absent or invalid/missing profile = existing radius/`coll_*` fallback. Supported: heli, tank, car, flyer and UFO. Maximum 64 convex hulls, 128 vertices per hull; visual scale/rotation must match the bake. The same profile applies to every texture SET; asset_set records only the source SET. Rotors/FX are excluded explicitly; turrets remain in their authored pose. Source/model changes require regeneration. AoE and proximity keep separate existing logic.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `collision_shape = Data/Collisions/Rocket_Set1.collision` — Weapon (new_weapon) projectile body generated by OpenNeoUA Studio. A valid profile replaces the weapon's physical spheres for direct hits against units: its hulls are swept against the target's shape, or against the target's spheres when the target has none. Terrain, building and weapon-vs-weapon impacts keep their existing logic. Empty/0/absent or invalid/missing profile = existing radius/`coll_*` fallback. Same limits, path rules and visual scale/rotation match as for vehicles.
------------------------------------------------------------------------------------------------------------------------

Compound collision spheres replace the limitations of a single legacy radius with up to 256 manually placed spheres. This is useful for long, asymmetric or multi-part units while keeping the collision representation simple and data-driven.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `coll_num = 3`       — Number of manually defined `coll_*` spheres. Range 0-256; negative values become 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `coll_act = 0`       — Selects collision sphere index 0. Index is clamped to 0-255.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `coll_xyz = 50|0|0`  — Sphere X|Y|Z position relative to unit center. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `coll_radius = 200`  — Sphere radius.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `coll_act = 1`       — Selects collision sphere index 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `coll_xyz = 0|-50|0` — Sphere X|Y|Z position relative to unit center. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `coll_radius = 250`  — Sphere radius.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `coll_act = 2`       — Selects collision sphere index 2.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `coll_xyz = 0|50|0`  — Sphere X|Y|Z position relative to unit center. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `coll_radius = 300`  — Sphere radius.
------------------------------------------------------------------------------------------------------------------------

### Weapons and firing

------------------------------------------------------------------------------------------------------------------------
- 🔵 `weapon = 5`                  — ID of the primary Weapon, taken from the weapon list (new_weapon).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `mgun = 3`                    — ID of the secondary machine-gun Weapon.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `num_weapons = 2`             — Number of copies of the primary Weapon fired together by each shot (side-by-side salvo). It is not a list of different weapons.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `fire_x = 0`                  — Muzzle position of the projectile on the X axis (right positive), relative to the unit center.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `fire_y = 0`                  — Muzzle position on the Y axis. The engine Y axis points down, so negative values are higher.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `fire_z = 50`                 — Muzzle position on the Z axis (forward positive).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gun_radius = 5`              — Machine gun: radius of the hit test against targets.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gun_power = 4000`            — Machine gun: power (damage) of each hit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gun_angle = 0.1`             — Machine gun: width of the aiming cone. Higher values make the gun more permissive about firing off-axis.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `kill_after_shot = 1`         — The unit dies right after firing. Useful for one-shot or suicide units.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `kamikaze = 5000`             — Car class only: the unit explodes on contact. The value is the energy of the blast. It is not the same as kill_after_shot.
------------------------------------------------------------------------------------------------------------------------

MGUN is OpenNeoUA's shared hitscan machine-gun path. The same system is used Restored cut-content behavior: the flak uses MGUN as its primary weapon. Effective range follows game.mgun_range. recoil timing, tracer rendering and decal behavior.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_name = MGUN`                                            — Restored cut-content behavior: the flak uses MGUN as its primary weapon. Effective range follows game.mgun_range.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_wireframe = wireless/otlw38.sklt`                       — Restored cut-content behavior: the flak uses MGUN as its primary weapon. Effective range follows game.mgun_range.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_spread_xy = 20|20`                                      — MGUN spread (format x|y): X horizontal left/right, Y vertical up/down.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_cockpit_camera_recoil = 1.5`                            — 0-10 first-person MGUN shake intensity. One shake per shot, so it follows mgun_shot_time. Only active in the player's cockpit camera and never moves the vehicle. 0, absent or invalid = no shake.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_energy = 900`                                           — Energy removed by each MGUN shot. When present, the MGUN fires one shot every mgun_shot_time: sound, FIRE state, spread hit scan and energy hit all happen together. Absent, 0 or invalid = vanilla MGUN (continuous damage from gun_power).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_angle = 0`                                              — MGUN angle. If absent, falls back to legacy gun_angle.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_vp_dead = 99`                                           — Legacy VP for the MGUN dead impact; final fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_base_dead = Data/Models/Base/FX/MGUN_DEAD.BASE`         — BASE family for the MGUN dead impact; falls back to mgun_vp_dead.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_3ds_dead = Data/Models/3ds/FX/mgun_dead.3ds`            — 3DS model for the dead impact; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_vp_megadeth = 99`                                       — Legacy VP for the MGUN megadeth impact; final fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_base_megadeth = Data/Models/Base/FX/MGUN_MEGADETH.BASE` — BASE family for the megadeth impact; falls back to mgun_vp_megadeth.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_3ds_megadeth = Data/Models/3ds/FX/mgun_megadeth.3ds`    — 3DS model for the megadeth impact; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_shot_time = 100`                                        — Time between two MGUN shots, shared by player and AI. With mgun_energy it sets the real fire rate (sound, FIRE state, hit scan and energy hit); without mgun_energy it only paces sound, animation and cost.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_shot_time_user = 100`                                   — Player-specific time between two MGUN shots; absent or zero uses mgun_shot_time.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_fire_energy_cost = 0.05%`                               — Energy cost per MGUN shot. 0 or absent = no cost.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_ground_decal_id = 4`                                    — MGUN impact decals are optional and independent from other MGUN visuals. Profile for MGUN world impacts; set its diameter with mgun_ground_decal_size.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_ground_decal_size = 3~7`                                — Manual diameter range in world units, 0..12000. Absent, zero or invalid disables the decal.
------------------------------------------------------------------------------------------------------------------------

Multi-projectile aiming can combine independent random spread, fixed arc and uniform cone distribution. These values affect player fire and AI fire using the same weapon definition.

Which spread parameter applies depends on the weapon class being fired: Bullets, rockets, missiles, grenades, arc grenades (every non-bomb class): the X and Y values of weapon_spread_xyz rotate the aim direction. The Z value is ignored. Bombs and homing bombs ("model = bomb" / "model = homing_bomb"): the X (left/right) and Z (forward/back) values of weapon_spread_xyz push each bomb on the ground plane, relative to the carrier's heading and independent of its pitch. the Y value is ignored. Bombs leave the carrier at only 20% of its speed, so the push uses a fixed reference launch speed instead of the carrier's speed: it works the same for a hovering or a fast carrier, and the scatter grows with the height of the drop. Lasers ("model = laser", including vertical lasers) do not use any spread parameter. Absent, 0 or invalid (negative, non-numeric) values mean no spread on that axis. The Hand Brake reduction applies to all of them; ramp_up_max_weapon_spread_xy is used by non-bomb weapons (its X also applies to the X of a bomb's weapon_spread_xyz, its Y has no effect on bombs).

------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_spread_xyz = 20|20|20`   — Independent random spread for each projectile, in degrees (format x|y|z). X: horizontal (bombs: random left/right push on the ground plane). Y: vertical, not used by bombs and homing bombs. Z: bombs and homing bombs only, random forward/back spread on the ground plane along the carrier's heading; ignored by every other weapon class; 0, absent or invalid = none. Applied the same way for the player.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_arc_xy = 0|0`            — Fixed arc offset for each projectile (format x|y): X horizontal, Y vertical. Applied the same way for the player.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_cone_xy = 0`             — XY cone width for each generated projectile, using a uniform conical distribution.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `scope_angle_min_max = -100|100` — Tank/car only: inclusive min|max limits of vanilla aim travel (-100 down, 0 parallel, 100 up). Absent = vanilla; outside range, red reticle and firing/lock blocked; MG unchanged.
------------------------------------------------------------------------------------------------------------------------

`fire_x_*` controls a virtual firing rack. It is useful for weapons that should visibly walk across multiple muzzle positions or alternate entire salvos.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `fire_x_mode = sequence` — Projectile X mode: sequence = orderly X cycle; random = random X slot; salve_sequence = each whole salvo starts from one side and the next from the other; salve_mirror = splits one salvo and fires half simultaneously from each side.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fire_x_start = -45`     — Initial X position of the firing rack used by sequence/random.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fire_x_step = 10`       — X distance between adjacent firing-rack slots.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fire_x_slots = 10`      — Total number of X slots before the rack cycle resets.
------------------------------------------------------------------------------------------------------------------------

A vehicle can expose up to four Weapon slots. Player and AI switching policies are independent, so the same vehicle may be manual for the player and smart or sequenced for AI control.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_2 = 2`                         — Optional second Weapon slot used by the unit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_3 = 8`                         — Optional third Weapon slot used by the unit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_4 = 7`                         — Optional fourth Weapon slot used by the unit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `num_weapons = 2~4`                    — Projectiles generated per shot. Single value = fixed; `min~max` = inclusive random count in the 1-255 range. 0 preserves the vanilla single-projectile behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `num_weapons_snd_events = 2`           — Maximum number of complete weapon sound events generated for a multi-projectile shot, preventing excessive overlap when num_weapons is greater than one.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `num_weapons_2 = 0`                    — Projectile count used by weapon_2. Single value = fixed; `min~max` = random 1-255; 0 or absent = inherits num_weapons.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `num_weapons_3 = 0`                    — Projectile count used by weapon_3. Single value = fixed; `min~max` = random 1-255; 0 or absent = inherits num_weapons.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `num_weapons_4 = 0`                    — Projectile count used by weapon_4. Single value = fixed; `min~max` = random 1-255; 0 or absent = inherits num_weapons.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_player_switch_mode = manual`   — Player changes Weapon manually using the mapped dedicated control.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_player_switch_mode = sequence` — Player Weapon automatically cycles through weapon, weapon_2, weapon_3, ...
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_player_switch_mode = random`   — Player Weapon automatically changes to a random configured slot.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_ai_switch_mode = smart`        — Compares base energy times the target's effective `energy_*` multiplier, including each shot's normal defaults. Selects the largest result; ties are random. Cluster priority: a cluster Weapon whose own `ai_fire_trigger_*` volume currently holds more than one enemy unit (or one enemy plus a neighbour, see cluster_ai_group_radius) is always chosen, regardless of damage. If several qualify, the one with the largest damage result wins. Weapons without a cluster or without a fire volume are not affected.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_ai_switch_mode = sequence`     — AI uses its configured Weapons in sequence.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_ai_switch_mode = random`       — AI selects a random configured Weapon.
------------------------------------------------------------------------------------------------------------------------

### AI behavior

------------------------------------------------------------------------------------------------------------------------
- 🔵 `adist_sector = 1200`     — Maximum distance at which the AI wants to attack or interact with a sector.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sdist_sector = 300`      — Minimum safe distance the AI keeps from a sector target.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `adist_bact = 1200`       — Maximum distance at which the AI wants to engage another unit (a bact is any actor: vehicles, Host Stations, guns).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sdist_bact = 300`        — Minimum safe distance from another unit. The ideal band is sdist <= target distance <= adist; too far and the AI approaches, too close and it repositions.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `job_fightrobo = 6`       — AI suitability score against Host Stations. Job values do not change damage or speed; they only tell the AI and the Host Station how well the unit fits a task.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `job_fightflyer = 6`      — AI suitability score against aircraft (plane, glider, zeppelin, ufo).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `job_fighthelicopter = 6` — AI suitability score against helicopters.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `job_fighttank = 6`       — AI suitability score against ground vehicles (tank and car).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `job_conquer = 6`         — AI suitability score for conquering sectors.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `job_reconnoitre = 6`     — AI suitability score for scouting and reconnaissance.
------------------------------------------------------------------------------------------------------------------------

disable_collision does not disable gravity, lifetime expiry, timed explosions or area damage, and does not grant invulnerability. Map bounds remain enforced.

AI fight affinities extend the original broad flyer/tank targeting families to the newer model classes. When omitted, the documented legacy affinity is used so older scripts continue to behave as expected.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `job_fightplane = 6`    — AI fight affinity against model = plane. Absent = job_fightflyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `job_fightglider = 6`   — AI fight affinity against model = glider. Absent = job_fightflyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `job_fightzeppelin = 6` — AI fight affinity against model = zeppelin. Absent = job_fightflyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `job_fightcruiser = 6`  — AI fight affinity against model = cruiser. Absent = job_fightflyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `job_fightufo = 6`      — AI fight affinity against model = ufo. Absent = job_fightflyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `job_fightcar = 6`      — AI fight affinity against model = car. Absent = job_fighttank.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `job_fightgun = 6`      — AI fight affinity against model = gun/module. Absent = legacy AI-path behavior.
------------------------------------------------------------------------------------------------------------------------

These AI distance parameters replace the old paired sector/bact values with a clearer three-stage attack loop: acquire/attack, retreat when too close, then re-engage after enough separation has been recovered.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `ai_attack_range = 2100`       — Replaces adist_sector and adist_bact. Distance within which the AI enters its attack maneuver.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ai_retreat_start_range = 140` — Replaces sdist_sector and sdist_bact. Below this distance, the AI starts retreating and moves away from the target.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ai_retreat_stop_range = 550`  — Distance the AI must reach while retreating before it stops retreating and resumes its attack maneuver. Must be greater than ai_retreat_start_range and not greater than ai_attack_range.
------------------------------------------------------------------------------------------------------------------------

### Guns, flaks and radar

------------------------------------------------------------------------------------------------------------------------
- 🔵 `gun_type = flak`     — Turret type for model = gun units. Vanilla values are flak and mg.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gun_side_angle = 60` — Flak turret: maximum sideways rotation in degrees.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gun_up_angle = 60`   — Flak turret: maximum upward elevation in degrees.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gun_down_angle = 10` — Flak turret: maximum downward angle in degrees.
------------------------------------------------------------------------------------------------------------------------

gun_type changes the role of vehicle/building definitions using the gun class. This is more than a visual switch: each type selects a different interaction path while retaining the existing gun data structure where possible.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `gun_type = flak`  — Vanilla flak behavior; no OpenNeoUA role change.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gun_type = mg`    — Restored cut-content behavior: the flak uses MGUN as its primary weapon. Effective range follows game.mgun_range.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gun_type = dummy` — Decorative/defensive role: does not interact and its interfaces are disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gun_type = radar` — The module rotates continuously through 360° around its mounting axis without pitching, searching for targets or attacking. Speed is `maxrot` in radians per second: one turn takes approximately `6.283 / maxrot` seconds; zero, negative or non-finite values stop rotation. `gun_*_angle` limits do not restrict the scan. The `radar` parameter still controls visibility range, and other `gun_type` values keep their own behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gun_type = power` — Compatible with power and power_radius. The gun does not attack but supplies energy to nearby units.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gun_levitate = 0` — gun/flak classes only: 1 prevents falling without a supporting surface. 0, absent or invalid = normal falling behavior.
------------------------------------------------------------------------------------------------------------------------

### Host Stations and mounted guns

------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_num_guns = 4`          — Number of gun (flak) attachments on the Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_act_gun = 0`           — Selects the gun slot being configured, starting at 0. The `robo_gun_*` lines below apply to that slot.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_gun_pos_x = 0`         — X position of the gun relative to the Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_gun_pos_y = -100`      — Y position of the gun (negative is higher).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_gun_pos_z = 0`         — Z position of the gun (forward / backward).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_gun_dir_x = 0`         — Initial aim direction X of the gun.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_gun_dir_y = 0`         — Initial aim direction Y of the gun.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_gun_dir_z = 1`         — Initial aim direction Z of the gun.
------------------------------------------------------------------------------------------------------------------------
In OpenNeoUA Studio, Host Station gun points accept only vehicles with `model = gun`, including the `unit_*` family. In **Gun Points**, Move strength starts at 50.

- 🔵 `robo_gun_type = 95`         — ID of the Vehicle (model = gun) used as this gun.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_gun_name = Main_Gun`   — Name of the gun.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_dock_x = 0`            — X position where the Host Station releases newly created units.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_dock_y = 0`            — Y position where units are released.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_dock_z = 0`            — Z position where units are released.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_coll_num = 3`          — Number of extra collision spheres of the Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_coll_act = 0`          — Selects the collision sphere being configured, starting at 0.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_coll_x = 0`            — X position of the sphere relative to the center.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_coll_y = 0`            — Y position of the sphere.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_coll_z = 0`            — Z position of the sphere.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_coll_radius = 300`     — Radius of the collision sphere.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_viewer_x = 0`          — X position of the player camera while controlling the Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_viewer_y = -200`       — Y position of the player camera.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_viewer_z = 0`          — Z position of the player camera.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_viewer_max_up = 0.5`   — Upward rotation limit of the player camera.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_viewer_max_down = 0.5` — Downward rotation limit of the player camera.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_viewer_max_side = 1.0` — Sideways rotation limit of the player camera.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_does_twist`            — When present, the Host Station slowly rotates while idle.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_does_flux`             — When present, the Host Station sways up and down while idle.
------------------------------------------------------------------------------------------------------------------------

Universal Unit Guns generalize the original robo-mounted-gun concept so guns and functional modules can be attached to ordinary vehicles as well as Host Stations. unit_gun_protect optionally turns a mounted gun into a real damage layer: the module must be destroyed before the parent can be damaged directly.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_num_guns = 2`                                      — Evolution of robo_num_guns: number of guns/modules that can be attached to any supported unit, including Host Stations.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_act_gun = 0`                                       — Selects mounted gun index 0 for the following `unit_gun_*` definitions.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_icon = Graphics/StatusIcons/mobile_flak.png`  — Optional Data-relative icon assigned to gun 0 and shown on the parent unit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_pos_xyz = 0.0|-200.0|55.0`                     — Mounted gun local X|Y|Z position. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_dir_xyz = 0.0|0.0|1.0`                         — Mounted gun initial X|Y|Z direction component. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_type = 95`                                     — Gun/module type ID.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_name = FLAK`                                   — Display/internal name assigned to this mounted gun.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_protect = 1`                                   — 1 makes this gun/module absorb damage and require destruction before direct damage can reach the parent vehicle.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_no_switch = 1`                                 — 1 prevents the player from cycling from the parent unit into this gun with F6 (or the next-unit button). 0, absent or invalid = the gun remains reachable as before. Weapons fired by a mounted gun never hit the parent unit or the other guns mounted on it.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_act_gun = 1`                                       — Selects mounted gun index 1 for the following `unit_gun_*` definitions.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_icon = Graphics/StatusIcons/mobile_radar.png` — Optional Data-relative icon assigned to gun 1 and shown on the parent unit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_pos_xyz = 0.0|-200.0|55.0`                     — Mounted gun local X|Y|Z position. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_dir_xyz = 0.0|0.0|1.0`                         — Mounted gun initial X|Y|Z direction component. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_type = 109`                                    — Gun/module type ID.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_name = RADAR`                                  — Display/internal name assigned to this mounted gun.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `unit_gun_protect = 1`                                   — 1 makes this gun/module absorb damage and require destruction before direct damage can reach the parent vehicle.
------------------------------------------------------------------------------------------------------------------------

### Appearance (models, VP, 3DS)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_normal = 10`   — ID of the visual model (VP) used in the normal state.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_fire = 11`     — ID of the model used while the unit is firing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_wait = 12`     — ID of the model used while the unit is idle.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_dead = 13`     — ID of the model used when the unit is destroyed.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_megadeth = 14` — ID of the model used for a heavy death (crash or hard impact).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_genesis = 15`  — ID of the model used for the creation (beam-in / Genesis) effect.
------------------------------------------------------------------------------------------------------------------------

Inline Physical FX support the normal, goingdown and explode event families. Their `snd_*`, `pal_*` and `shk_*` suffixes use the same semantics as the existing effect system. Vanilla compatibility: physical_vehicle with offset_xyz remains readable for legacy data only. Do not mix physical_vehicle/`offset_*` with inline physical keys in the same block.

Kamikaze exception: when a model = kamikaze Weapon detonates with a positive trigger_radius, the carrier's destroyed Ground Decals are checked immediately. Any contact between that XYZ sphere and a world surface (terrain or building), including tangency, creates the decal at the nearest surface point. No contact = no decal, including the later wreck impact. size controls decal dimensions, not the contact radius. The payload's ground decal and configured crater or dent use that same nearest surface point. The deformation also works with no decal configured. Absent/zero/invalid trigger_radius retains the ordinary Vehicle decal behavior. Decal limits apply. Vehicle Ground Decals use the same projected renderer as Weapon/MGUN decals. For Tank and Car, if death begins while the vehicle is airborne, no Vehicle Ground Decal is created for that death, including the later crash/death2 path.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `ground_decal_id = 1`               — ID of the reusable new_decal profile. Vehicles spawn on grounded destruction; Weapons spawn on real world impact. -1, absent or unknown ID = no decal.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ground_decal_size = 80~120`        — Manual diameter range in world units, 0..12000. Absent, zero or invalid disables the decal.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cockpit_camera_offset_xyz = 0|0|0` — Cockpit camera X|Y|Z offset. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cockpit_gun_camera_recoil = 2.0`   — 0..5 multiplier for cockpit camera recoil from the gun. Does not alter physics, aiming or projectile origin.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cockpit_camera_hide_vehicle = 0`   — Hides the vehicle's own silhouette in cockpit camera view, leaving an unobstructed forward view. 0 or absent = silhouette visible (vanilla behaviour); 1 = silhouette hidden. Defaults to 1 for model = ufo vehicles, which need maximum visibility. Can be inverted at runtime with the Hide Vehicle key (default C, remappable and persistent from Input Settings); the change is not written to this file and the value set here is restored on restart.
------------------------------------------------------------------------------------------------------------------------

Main vehicle visuals can be supplied independently for each lifecycle state. External assets extend, rather than remove, the original VP states.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_normal = Data/Models/Base/VEHICLE.BASE`            — External BASE model for normal state; automatically falls back to vp_normal.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_fire = Data/Models/Base/VEHICLE_FIRE.BASE`         — External BASE model for fire state; automatically falls back to vp_fire.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_wait = Data/Models/Base/VEHICLE_WAIT.BASE`         — External BASE model for wait state; automatically falls back to vp_wait.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_dead = Data/Models/Base/VEHICLE_DEAD.BASE`         — External BASE model for dead state; automatically falls back to vp_dead.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_megadeth = Data/Models/Base/VEHICLE_MEGADETH.BASE` — If the resolved dead visual has no persistent geometry, OpenNeoUA automatically reuses the normal visual during DEATH1 so the wreck remains visible while the existing fall physics carries it to the crash. External BASE model for megadeth state; automatically falls back to vp_megadeth.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_genesis = Data/Models/Base/VEHICLE_GENESIS.BASE`   — External BASE model for genesis state; automatically falls back to vp_genesis.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_normal = Data/Models/3ds/vehicle.3ds`               — External 3DS model for normal state; takes priority over base_normal and vp_normal.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_fire = Data/Models/3ds/vehicle_fire.3ds`            — External 3DS model for fire state; takes priority over base_fire and vp_fire.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_wait = Data/Models/3ds/vehicle_wait.3ds`            — External 3DS model for wait state; takes priority over base_wait and vp_wait.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_dead = Data/Models/3ds/vehicle_dead.3ds`            — External 3DS model for dead state; takes priority over base_dead and vp_dead.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_megadeth = Data/Models/3ds/vehicle_megadeth.3ds`    — External 3DS model for megadeth state; takes priority over base_megadeth and vp_megadeth.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_genesis = Data/Models/3ds/vehicle_genesis.3ds`      — External 3DS model for genesis state; takes priority over base_genesis and vp_genesis.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `scale_xyz = 1.0|1.0|3.0`                                — X|Y|Z visual scale of all VP/BASE/3DS states, including dead and megadeth. Does not change collision geometry. Default: 1.0. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spin_xyz = 0|0|10`                                      — X|Y|Z spin intensity of the active main VP/BASE/3DS model: 0-10, decimals allowed; 0 = no spin. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `rotation_xyz = 0|0|0`                                   — Fixed visual rotation in degrees around the X|Y|Z axes of the active main VP/BASE/3DS model. 0 or absent = no rotation. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tint = 40|120|255|255`                                  — RGBA tint applied to the active main VP/BASE/3DS visual.
------------------------------------------------------------------------------------------------------------------------

Vehicle/Weapon decoration FX use the same periodic/persistent concept as building decorations, but positions are relative to the parent object.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_mode = periodic`                            — Periodic mode: generates decorations intermittently using the parameters below.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp = 99`                                    — Legacy VP decoration visual; final fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_base = Data/Models/Base/FX/DECORATION.BASE` — BASE decoration family, relative to Data; falls back to decoration_fx_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_3ds = Data/Models/3ds/FX/decoration.3ds`    — 3DS decoration model; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_scale_xyz = 1.0|1.0|1.0`                    — X|Y|Z scale of the active decoration visual. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_spin_xyz = 0|0|0`                           — X|Y|Z spin intensity of the decoration VP/3DS visual: 0-10, decimals allowed; 0 = no spin. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_tint = 255|95|70|200`                       — RGBA tint applied to the main decoration visual.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_fade_inout = 500|500`                       — Main VP/3DS decoration fade-in and fade-out in milliseconds (format in|out). 0 or absent = immediate appearance/disappearance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_tint = 255|255|255|255`            — RGBA tint of VP decoration particles/trails. Alpha 0 = invisible.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_scale_xyz = 1.0|1.0|1.0`           — X|Y scale of VP decoration particles/trails, plus a Z multiplier for particle/trail duration and therefore apparent trail length (format x|y|z).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_fade_inout = 500|500`              — Time in milliseconds (format in|out) for VP decoration particles/trails to fade in to their configured opacity (in) and to fade out to invisible (out). 0 or absent = immediate.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_duration = 1000`                            — Total duration in milliseconds of each periodic decoration.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_interval = 500~1500`                        — Random interval in milliseconds between decoration spawns. A single value gives a fixed interval.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_count = 2~7`                                — Random number of decorations generated per spawn. A single value gives a fixed count.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_random_pos = 90`                            — XYZ position randomization around the parent Vehicle/Weapon center.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_offset_xyz = 0|0|-40`                       — X|Y|Z offset from the parent Vehicle/Weapon center. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_mode = persistent`                          — Persistent mode: decoration remains active while the parent Vehicle/Weapon remains active.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp = 99`                                    — Legacy VP decoration visual; final fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_base = Data/Models/Base/FX/DECORATION.BASE` — BASE decoration family, relative to Data; falls back to decoration_fx_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_3ds = Data/Models/3ds/FX/decoration.3ds`    — 3DS decoration model; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_scale_xyz = 1.0|1.0|1.0`                    — X|Y|Z scale of the active decoration visual. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_spin_xyz = 0|0|0`                           — X|Y|Z spin intensity of the decoration VP/3DS visual: 0-10, decimals allowed; 0 = no spin. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_tint = 255|95|70|200`                       — RGBA tint applied to the main decoration visual.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_fade_inout = 500|500`                       — Main VP/3DS decoration fade-in and fade-out in milliseconds (format in|out). 0 or absent = immediate appearance/disappearance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_tint = 255|255|255|255`            — RGBA tint of VP decoration particles/trails. Alpha 0 = invisible.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_scale_xyz = 1.0|1.0|1.0`           — X|Y scale of VP decoration particles/trails, plus a Z multiplier for particle/trail duration and therefore apparent trail length (format x|y|z).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_fade_inout = 500|500`              — Time in milliseconds (format in|out) for VP decoration particles/trails to fade in to their configured opacity (in) and to fade out to invisible (out). 0 or absent = immediate.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_offset_xyz = 0|0|-40`                       — X|Y|Z offset from the parent Vehicle/Weapon center. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------

Damaged-state visuals are coupled to optional gameplay penalties. This means the damaged state can be purely cosmetic, or it can also reduce movement, turning and weapon cadence once the configured HP threshold is crossed.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_fx_vp = 138|138|138`                     — VP visuals generated while the unit is damaged. Multiple VP IDs may be separated by |. Repeated damaged_fx_3ds lines can add 3DS visuals.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_fx_3ds = Data/Models/3ds/FX/damaged.3ds` — Optional damaged-state 3DS model. Repeat the line to add multiple 3DS candidates to the random visual selection alongside VP entries.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_fx_scale = 1.0~1.7|1.0~1.7|0.2~0.3`      — Damaged-FX scale per VP, in the order listed by damaged_fx_vp; separate entries with | and use `min~max` for a random range. A single value or range applies to all VPs and 3DS visuals. Missing, non-positive or invalid entries default to 1.0. With multiple entries, 3DS visuals use 1.0. Rewrite legacy `min~max` ranges as `min~max`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_fx_threshold = 30%`                      — HP threshold below which damaged FX activate. With %, uses maximum HP; without %, uses a fixed HP amount.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_fx_count = 1~3`                          — Random number of damaged FX generated per trigger. A single value gives a fixed count. Runtime maximum: 32.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_fx_interval = 500~1500`                  — Random interval in milliseconds between damaged-FX triggers. A single value gives a fixed interval.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_fx_random_max_offset = 25%`              — Maximum random offset from the unit center. With %, effects spawn inside the unit silhouette (its `collision_shape` profile when present, otherwise the visual model, vanilla scripts included, without rotor blades and thin parts): 100% = whole body, 25% = central core only; without %, uses a fixed distance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `damaged_fx_trail_only = 1`                       — 1 hides the main VP model while keeping its trails, particles and child effects active.
------------------------------------------------------------------------------------------------------------------------

### Destruction and effects

------------------------------------------------------------------------------------------------------------------------
- 🔵 `dest_fx = death_46_0_0_0`                — Effect spawned on destruction. Format type_ID_X_Y_Z: death (normal destruction) or megadeth (heavy crash / surface), the ID of the effect vehicle (effects are ordinary entries of Vehicles.cfg), then the X, Y and Z offset from the unit center. A fifth number can enable acceleration. The | separator is also accepted.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `ext_dest_fx = death_46_0_0_0`            — Extended variant of dest_fx, with the same format. Rarely used. Alias: extended_dest_fx.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `scale_fx = 1_5_2.5_1500_292_291_290_289` — Scaled animation effect, usually referenced by dest_fx. Format start_unused_speed_duration_modelIDs: the first value is where the animation starts (0 = from the beginning), the second is unused, then playback speed (negative plays backwards), the duration in ms and finally the IDs of the model sequence.
------------------------------------------------------------------------------------------------------------------------

Vehicle Chain FX can replace the old single-purpose dest_fx relationship with an explicit sequence. "visual" creates scalable visuals; modern "physical" creates an autonomous inline fragment with its own visual and physics data, so new content no longer needs to reserve a Vehicle ID for debris/effect objects.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `begin_fx`                                       — Visual vehicle Chain FX, replacing a dest_fx that points to a scale_fx.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `trigger = destroyed|air_destroyed`              — Trigger: destroyed = vehicle destroyed on the ground; air_destroyed = vehicle destroyed in flight. Several triggers can be joined with | (e.g. destroyed|air_destroyed): the block runs on each one, exactly as if it were written twice. Any unknown name rejects the whole block.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mode = visual`                                  — Visual mode uses start_size/end_size/duration and the VP/BASE/3DS visual definitions below.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `random_rotation = 0`                            — 1 assigns a random X/Y/Z orientation once at spawn, independently per instance; 0, omitted or invalid = normal orientation. Spawn position and launch direction are unchanged.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `count = 2~4`                                    — Number of effect instances generated per activation. Single value = fixed amount; `min~max` = random amount rolled per activation. Maximum 32; invalid = 1. With count > 1 each instance rolls its own `min~max` offset. Supported only with mode = visual and mode = physical; not available in SuperItem Chain FX blocks.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `offset_xyz = -20|0|20`                          — Effect X|Y|Z offset. Single value = fixed; `min~max` = random offset rolled at every activation. Format: x|y|z; each axis takes a single value or `min~max`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spin_xyz = 0|0|10`                              — X|Y|Z spin intensity of the Chain FX VP/BASE/3DS visuals: 0-10, decimals allowed; 0 = no spin. Applies to every model in the block. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `start_size = 0.1`                               — Initial effect size. 1 = original visual size.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end_size = 2`                                   — Final effect size.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mid_size = 1`                                   — Intermediate effect size.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `duration = 1000`                                — Effect duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fade_inout = 500|1000`                          — Visual fade-in and fade-out in milliseconds (format in|out). 0 or absent = immediate.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_model = 229`                                 — First legacy VP visual in the sequence.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_model = Data/Models/Base/FX/CHAIN_FX.BASE` — Optional BASE family for the same element. Place after vp_model to keep that VP as fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_model = Data/Models/3ds/FX/chain_fx.3ds`    — Optional 3DS model for the same element; takes priority over base_model and vp_model.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tint = 80|160|255|180`                          — RGBA tint for all models in the visual block, regardless of position. The last value wins; omitted tint preserves original colours.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_model = 230`                                 — Second visual in the sequence.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_model = 231`                                 — Third visual in the sequence.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end`
------------------------------------------------------------------------------------------------------------------------
- 🟢 `begin_fx`                                       — Autonomous inline Physical FX; defines the fragment directly without consuming a Vehicle ID.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `trigger = destroyed|air_destroyed`              — Vehicle trigger: destroyed = killed on the ground; air_destroyed = killed in flight.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mode = physical`                                — Enables the modern inline physical fragment path. physical_vehicle is not required.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `random_rotation = 0`                            — 1 assigns a random X/Y/Z orientation once at spawn, independently per instance; 0, omitted or invalid = normal orientation. Spawn position and launch direction are unchanged.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `follow_carcass = 1`                             — 1 = this fragment mirrors the position and rotation of the destroyed unit's falling wreck (the wreck keeps its own physics) until the wreck lands, then continues with its own physics. 0 or absent = the fragment uses its own physics. Applies to every fragment with follow_carcass = 1, including those from other blocks or with count > 1. Mainly meaningful with air_destroyed, where the wreck is still in flight.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `lifetime = 30000`                               — Maximum time in milliseconds that the Physical Fragment may remain in flight before it is removed. Single value = fixed lifetime; `min~max` = a lifetime rolled independently for each fragment. 0, absent or invalid = 30000. Ground contact still removes the fragment immediately.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `count = 1~4`                                    — Number of fragments created per activation. Single value = fixed; `min~max` = random. Runtime maximum: 32. Each fragment independently selects its own VP entry from vp_model.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `launch_xyz = -30~30|-20|-30~30`                 — Initial local X|Y|Z velocity (format x|y|z). Each axis takes a single value (fixed) or `min~max` (randomized independently for each fragment). Inline fragments spawn at the event position rather than using offset_xyz.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_model = 138|193`                             — One or more legacy moving-fragment VP IDs separated by |. Each spawned fragment randomly selects one entry; a single value preserves the previous behavior. VP remains the final fallback after 3DS/BASE.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_model = Data/Models/Base/FX/FRAGMENT.BASE` — Optional BASE family; falls back to vp_model.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_model = Data/Models/3ds/FX/fragment.3ds`    — Optional 3DS fragment model; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_impact = 140|69`                             — Positional impact-VP list paired with vp_model. The selected model index always uses the impact at the same index, so 138 -> 140 and 193 -> 69. When present, the list must contain the same number of entries as vp_model; 0 disables the impact VP for that position.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tint = 255|255|255|255`                         — Entries may be repeated to weight random selection. For example, vp_model = 138|138|193 makes 138 twice as likely; vp_impact must repeat the matching impacts in the same order. RGBA tint applied to the fragment visual.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `scale = 1.0`                                    — Uniform VP/BASE/3DS fragment scale. <= 0 or invalid = 1.0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mass = 150`                                     — Fragment mass. Must be positive.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `airconst = 40.0`                                — Air resistance used by fragment physics. 0 is valid.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `force = 6000`                                   — Additional impulse along the launch direction. 0 is valid.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `maxrot = 3.0`                                   — Fragment rotation intensity. 0 disables additional rotation.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `radius = 1.0`                                   — Physical/collision radius. Must be positive.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `impact_energy = 200`                            — Energy removed once from the first unit hit by this physical fragment. Respects shield and invulnerability. 0 or absent = disabled. Single-player only.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `impact_energy_radius = 0`                       — Additional 3D radius for energy removal on impact. 0 = direct target only; a positive value removes the same energy from nearby units. Each fragment can remove energy only once.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_normal_sample = Data/Sounds/shot7.wav`      — Optional sound started when the fragment is created.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_normal_volume = 90`                         — normal event sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_normal_pitch = 0~500`                       — normal event pitch. Single value = fixed; `min~max` = random.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_normal_radius = 0`                          — normal event sound radius. 0 = legacy attenuation behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_goingdown_sample = Data/Sounds/fall.wav`    — Optional sound for the goingdown phase.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_goingdown_volume = 90`                      — goingdown event sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_goingdown_pitch = 0~500`                    — goingdown event pitch. Single value = fixed; `min~max` = random.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_goingdown_radius = 0`                       — goingdown event sound radius. 0 = legacy attenuation behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_goingdown_slot = 1`                         — Palette slot for goingdown. 0 = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_goingdown_mag0 = 1`                         — Initial goingdown palette magnitude.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_goingdown_mag1 = 0`                         — Final goingdown palette magnitude.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_goingdown_time = 1000`                      — goingdown palette duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_goingdown_radius = 0`                       — goingdown palette radius. 0 = legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_explode_sample = Data/Sounds/impact.wav`    — Optional sound played when the fragment finishes/impacts.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_explode_volume = 90`                        — explode event sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_explode_pitch = 0~500`                      — explode event pitch. Single value = fixed; `min~max` = random.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_explode_radius = 0`                         — explode event sound radius. 0 = legacy attenuation behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_explode_slot = 1`                           — Palette slot for explode. 0 = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_explode_mag0 = 1`                           — Initial explode palette magnitude.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_explode_mag1 = 0`                           — Final explode palette magnitude.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_explode_time = 1000`                        — explode palette duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_explode_radius = 0`                         — explode palette radius. 0 = legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_explode_slot = 1`                           — Shake slot for explode. 0 = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_explode_mag0 = 1`                           — Initial explode shake magnitude.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_explode_mag1 = 0`                           — Final explode shake magnitude.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_explode_time = 1000`                        — explode shake duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_explode_radius = 0`                         — explode shake radius. 0 uses shk_explode_mute legacy attenuation.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_explode_mute = 0.02`                        — Legacy shake attenuation used when shk_explode_radius = 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_explode_x = 0.2`                            — X-axis shake strength.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_explode_y = 0.2`                            — Y-axis shake strength.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_explode_z = 1.0`                            — Z-axis shake strength.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end`
------------------------------------------------------------------------------------------------------------------------

`at_death_*` effects are independent death-triggered systems. They can be used separately or combined on the same Vehicle.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `at_death_push_force = 10`             — AOE push intensity 0-10 generated when the unit dies. 0 or absent = no death push.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `at_death_push_radius = 1200`          — 3D radius of the death AOE push. 0 or absent = radial death push disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `at_death_push_falloff = 1`            — 1 decreases death-push strength progressively with distance from the epicenter; 0 keeps the same strength throughout the radius.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `at_death_energy = 1000`               — Energy removed in an area when the unit dies, including from allies. Respects shield and invulnerability. 0 or absent = disabled. Lethal hits can trigger another unit's at_death_energy and therefore create chain reactions. Single-player only.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `at_death_energy_radius = 700`         — 3D radius used by at_death_energy. 0 or absent = death energy removal disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `at_death_energy_falloff = 1`          — 1 reduces energy removal linearly with distance from the epicenter; 0 keeps the same energy loss throughout the radius.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `at_death_spawn_vehicle = 5`           — Vehicle ID generated at death.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `at_death_spawn_count = 3`             — Number of child units generated simultaneously at death. Minimum/default: 1. Runtime maximum: 8.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `at_death_spawn_random_pos = 30`       — XZ position randomization applied to child units generated at death.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `at_death_spawn_instant = 1`           — 1 generates child units immediately without the classic Genesis sequence.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `at_death_spawn_immunity_time = 2000`  — Damage-immunity time in milliseconds granted to newly generated child units.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `at_death_spawn_units_eject_force = 4` — Push strength applied to units spawned at death, in a random direction (same 0-10 scale and push system as at_death_push_force; flying air units are pushed horizontally only). Range 0-10; decimals allowed. 0, missing or invalid = no ejection. Periodic spawning is unaffected.
------------------------------------------------------------------------------------------------------------------------

### Buffs

------------------------------------------------------------------------------------------------------------------------
- 🟢 `buff_id = 1` — ID of the new_buff profile linked to this Vehicle. -1, absent or an unknown ID = no Buff.
------------------------------------------------------------------------------------------------------------------------

### Sounds, palette flashes and shakes

------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_<evento>_sample = sounds/engine.wav`             — Sound file played for the event. Events: normal (engine loop), fire, wait, genesis, explode, crashland (hits the ground), crashvhcl (hits a vehicle), goingdown (falling), cockpit, beamin, beamout, build.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_<evento>_volume = 120`                           — Volume of the event sound.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_<evento>_pitch = 0`                              — Pitch of the event sound. Ranges with `min~max` are described further below.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_<evento>_ext = 1_100_22050_0_0_sounds/extra.wav` — Additional sample for the event. Format loop_volume_sampleRate_offset_sampleCount_file.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pal_<evento>_slot = 1`                               — Screen color flash: effect type (1 red, 2 blue, 3 green, 4 inverse, 5 dark inverse, 6 black and white, 7 turquoise inverse). Events are the same as for snd_.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pal_<evento>_time = 500`                             — Duration of the color flash in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pal_<evento>_mag0 = 1`                               — Starting intensity of the flash.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pal_<evento>_mag1 = 0`                               — Final intensity of the flash. The intensity fades linearly from mag0 to mag1.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<evento>_slot = 1`                               — Camera shake: shake slot / type.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<evento>_time = 300`                             — Duration of the shake in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<evento>_mag0 = 1`                               — Starting intensity of the shake.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<evento>_mag1 = 0.5`                             — Final intensity of the shake.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<evento>_x = 1`                                  — Shake strength on the X axis.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<evento>_y = 1`                                  — Shake strength on the Y axis.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<evento>_z = 1`                                  — Shake strength on the Z axis.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<evento>_mute = 0`                               — How much the audio is attenuated while the shake plays.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_airexplode_slot = 4`                             — Palette-effect slot used when an airborne vehicle explodes in the air.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_airexplode_time = 1000`                          — Air-explosion palette-effect duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_airexplode_mag0 = 1`                             — Initial air-explosion palette intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_airexplode_mag1 = 0`                             — Final air-explosion palette intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_airexplode_radius = 5000`                        — Maximum palette-effect radius. Intensity decreases linearly to 0 at the limit. 0 or absent = legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_airexplode_slot = 1`                             — Shake slot used when an airborne vehicle explodes in the air.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_airexplode_time = 1000`                          — Air-explosion shake duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_airexplode_mag0 = 2`                             — Initial air-explosion shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_airexplode_mag1 = 0`                             — Final air-explosion shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_airexplode_radius = 5000`                        — Maximum shake radius. Intensity decreases linearly to 0 at the limit. 0 or absent = legacy shk_airexplode_mute behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_airexplode_sample = sounds/Smex02.wav`           — Sound played when an airborne vehicle explodes in the air.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_airexplode_volume = 120`                         — Air-explosion sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_airexplode_pitch = 0~500`                        — Air-explosion sound pitch. Single value = fixed; `min~max` = random pitch selected on each playback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_airexplode_radius = 5000`                        — Maximum air-explosion sound radius. Volume decreases linearly to 0 at the limit. 0 or absent = legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_genesis_pitch = auto`                            — Automatic Genesis sound pitch: the higher the unit's HP, the lower the generated pitch. The automatic mode is opt-in and only active with the exact value "auto"; an explicit numeric value or `min~max` range overrides it, while a missing pitch keeps the normal default.
------------------------------------------------------------------------------------------------------------------------

### OpenNeoUA special features

------------------------------------------------------------------------------------------------------------------------
- 🟢 `spy_ui_radius = 6000`                        — UFO classes only, player side: radius in which Spy Mode reveals unit interfaces. Toggle with the remappable UFO Spy Mode On/Off action (Spacebar by default) or the fixed middle-mouse shortcut. 0 or absent = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spy_ui_radius_sound = sounds/new/spy_ui.wav` — UFO classes only: optional sound played every time Spy Mode is enabled or disabled. Empty/absent/unloadable = silent.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `zoom_steps = 4`                              — UFO classes only, player side: maximum number of 1.25x optical zoom-in steps. 0 locks zoom to 1x; absent/invalid keeps the OpenNeoUA limit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `zoom_step_sound = sounds/new/spy_zoom.wav`   — UFO classes only: optional one-shot played whenever an optical zoom step is actually applied. No sound is played when input is received at an already reached zoom limit. Empty/absent/unloadable = silent.
------------------------------------------------------------------------------------------------------------------------

Mobile unit spawners use the same core idea as building spawners, but their spawn positions are relative to the parent unit. Spawned units inherit the source primary command, waypoint route and aggression. Spawned units reuse Genesis overlap separation, including instant spawns.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_vehicle = 5`           — Vehicle ID spawned by this unit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_count = 3`             — Number of units spawned simultaneously per activation. Default: 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_interval = 6000`       — Time in milliseconds between spawn activations.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_trigger_radius = 1800` — Enemy-presence radius that activates spawning.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_random_pos = 200`      — XZ spawn-position randomization around the parent vehicle.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_offset_xyz = 0|0|0`    — Spawn-position X|Y|Z offset from the parent vehicle. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_max_active = 9`        — Maximum number of children generated by this unit that may be active simultaneously.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_instant = 1`           — 1 spawns children immediately without the classic Genesis sequence.
------------------------------------------------------------------------------------------------------------------------

Proximity Defense is an autonomous secondary Weapon system. "classic" reacts to nearby enemies repeatedly; "at_death" fires once on unit death.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `proximity_defense_mode = classic`                                          — System mode: classic = activates around nearby enemies; at_death = activates once at unit death and fires all projectiles simultaneously.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `proximity_defense_weapon = 2`                                              — Weapon ID used by Proximity Defense.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `proximity_defense_trigger_radius = 1200`                                   — Enemy-presence distance that activates classic mode.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `proximity_defense_interval = 5000`                                         — Time in milliseconds between projectile bursts in classic mode.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `proximity_defense_shots = 20`                                              — Number of projectiles created per system activation.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `proximity_defense_fire_mode = all_at_once`                                 — Classic firing mode: all_at_once = all projectiles together; sequential = one after another. at_death always uses all_at_once.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `proximity_defense_vp_launch = 70`                                          — Legacy VP spawned when Proximity Defense projectiles launch; final fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `proximity_defense_base_launch = Data/Models/Base/FX/PROXIMITY_LAUNCH.BASE` — BASE launch family, relative to Data; falls back to VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `proximity_defense_3ds_launch = Data/Models/3ds/FX/proximity_launch.3ds`    — 3DS launch model; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `proximity_defense_sequence_delay = 100`                                    — Delay in milliseconds between projectiles when fire_mode = sequential; ignored for all_at_once and at_death.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `proximity_defense_vertical_angle = -90~90`                                 — Projectile vertical angle. Single value = fixed angle; `min~max` = random angle in range. Absent = 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `proximity_defense_horizontal_angle = 0~360`                                — Projectile horizontal angle. Single value = fixed angle; `min~max` = random angle in range. If absent, projectiles are distributed uniformly across 360 degrees.
------------------------------------------------------------------------------------------------------------------------

## Weapons (Weapons.cfg)

### Identity and class

------------------------------------------------------------------------------------------------------------------------
- 🔵 `new_weapon = 64`                  — Opens the definition of a new Weapon with the given numeric ID. If the ID already exists, the prototype is rebuilt from scratch.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `modify_weapon = 64`               — Reopens an existing Weapon so that only the listed values change. Typical use is level files and tech-upgrade blocks.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `end`                              — Closes the current new_weapon / modify_weapon block.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `enable = 1`                       — Makes the Weapon available to the given faction (1 Resistance, 2 Sulgogar, 3 Mykonian, 4 Taerkasten, 5 Black Sect, 6 Ghorkov, 7 neutral). One line per faction.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `disable = 1`                      — Removes the Weapon from the given faction.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `model = rocket`                   — Projectile class: rocket (powered rocket), missile (steers toward its target; homing_time limits how long), grenade, and bomb or special (an alias of bomb that drops under gravity). OpenNeoUA adds more classes, described below.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `name = Example_Missile`           — Name of the Weapon. Underscores are shown as spaces.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `wireframe = wireless/example.skl` — Wireframe outline of the Weapon shown by the interface.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `type_icon = M`                    — Character selecting the Weapon icon, mapped through TYPE_NS.FON into the H_FINDER graphic.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_use_vehicle_fire_state = 1` — When this Weapon fires, its carrier Vehicle temporarily shows its own fire-state visual (VP, BASE or 3DS). For an attached Unit Gun, the parent Vehicle shows it. 0, absent or invalid keeps vanilla behavior. The former Vehicle key `weapon_use_vehicle_fire` has been removed, with no fallback.
------------------------------------------------------------------------------------------------------------------------

### Special projectile classes

New Weapon models are selected with the existing model field, keeping the system data-driven instead of introducing parallel weapon-definition formats.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `model = homing_bomb`      — Homing bomb Weapon class that pursues enemy targets.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `model = arc_grenade`      — True ballistic arc-grenade Weapon. Flight uses start_speed, grenade_arc_angle and grenade_arc_gravity; force does not control the trajectory. The player aims mortar-style: the shell lands on the terrain point the crosshair meets, so raising or lowering the crosshair changes the firing distance. Aiming at the sky or beyond the maximum range fires at 45 degrees for maximum distance along the same heading. Dispersion is the Vehicle's weapon_spread_xyz (X left/right, Y shortens/lengthens the shot).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `grenade_arc_angle = 60`   — Initial vertical launch angle in degrees. With player aiming it is the preferred angle when choosing between the high and the low trajectory.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `grenade_arc_gravity = 35` — Gravity applied during projectile flight, controlling how quickly it falls. 0/absent/invalid = engine-standard gravity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `model = kamikaze`         — Kamikaze Weapon class: converts the firing unit into a kamikaze that approaches enemies and then detonates.

    In OpenNeoUA Studio, **Visuals & FX** opens **Payload effects** to preview and edit the charge effects in place. Use the menu to switch to the carrier model and its destruction sequence.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `trigger_radius = 360`     — Kamikaze only: XYZ fuse radius. Detonates when this sphere touches an enemy's collision surface or the active sector target's geometry, including contact during movement.
------------------------------------------------------------------------------------------------------------------------

Artillery Shell supports both a real ballistic trajectory and an emulated vertical-barrage mode. Manual targeting is integrated with the Tactical Map; automatic targeting remains available unless explicitly disabled.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `model = artillery_shell`                                  — Artillery/mortar Weapon class with barrage fire, manually controllable from the Tactical Map or managed automatically.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_mode = vertical_barrage`                  — ballistic = projectile reaches the target area along a real arc; vertical_barrage = emulated barrage where rounds climb out of view and descend vertically.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_fall_delay = 2500`                        — vertical_barrage only: delay before projectiles begin descending from the sky toward targets.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_vertical_spread_xz = 2|2`                 — vertical_barrage only: X|Z spread while projectiles rise vertically. Format: x|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_speed = 1800`                             — Projectile speed used by both modes.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_range_minmax = 1600|8500`                 — XZ distance from the source within which a barrage area may be selected manually or acquired automatically (format min|max).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_requires_radar = 1`                       — 1 requires the target area to be visible on the faction radar; 0 permits targeting unrevealed areas.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_barrage_radius = 900`                     — Radius of the bombarded area.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_barrage_shots = 5`                        — Total number of projectiles available per barrage.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_barrage_shot_delay = 550`                 — Time in milliseconds between successive shots in a barrage.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_barrage_cooldown = 14000`                 — Mortar cooldown in milliseconds, starting after the last projectile of the barrage is launched.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_arc_height = 3300`                        — ballistic: maximum projectile arc height; vertical_barrage: climb/descent altitude used above the target.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_airburst = 0`                             — 1 detonates rounds at target altitude (airburst); 0 waits for the real terrain impact point.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_manual_mode_only = 0`                     — 1 leaves only player manual targeting available; 0 also permits automatic acquisition.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `artillery_shell_marker_path = artillery_ring_classic.svg` — SVG marker path relative to Data/Graphics/Map/Markers. Empty/missing/invalid falls back to artillery_ring_classic.svg.
------------------------------------------------------------------------------------------------------------------------

Laser replaces a physical projectile with a continuous beam. Damage is tick- based, can ramp against the same target, can chain to nearby units and can be rendered either with the legacy repeated visual or an external mesh.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `model = laser`                               — Continuous laser Weapon instead of a physical projectile.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `model = vertical_laser`                      — Downward-firing variant of the continuous laser. Other laser parameters remain available.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vertical_laser_ai_trigger_radius = 30`       — Vertical mode, AI only: XZ distance from an enemy at which AI activates vertical fire. Does not change beam range or thickness.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_energy_tick_time = 150`                — Damage-tick interval in milliseconds when the Laser is used by AI.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_energy_tick_time_user = 150`           — Damage-tick interval in milliseconds when the Laser is used by the player.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_energy_increment_rate = 500`           — Amount by which Laser damage increases on each successive tick against the same target.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_spacing = 20`                          — Spacing between normal-state visual instances used to build the legacy VP/BASE/3DS beam. Lower values produce a denser beam.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_max_energy = 50000`                    — model=laser: `pal_normal_*` and `shk_normal_*` stay active until the beam stops. Their `*_normal_time` values are ignored; launch and hit effects retain their existing timing. model=laser: begin_fx impact_world runs on world contact, detonate on unit or world contact. Both use the real hit point and the laser's existing impact-FX cadence. model=laser: AI beams cannot damage or debuff allied units or remove their Deflect Buff. Contact visuals remain; player-controlled beams can still damage allies. Maximum damage the Laser may reach through progressive increase.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_chain_max_jumps = 6`                   — Maximum number of additional unit-to-unit jumps after the first target.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_chain_radius = 1200`                   — Radius around the last hit unit used to search for the next chain target.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_chain_energy_mult = 0.8`               — Damage multiplier applied at each chain jump. 1.0 = no loss; 0.8 = each jump retains 80% of the previous damage.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_beam_count = 5`                        — Number of direct Laser beams the source may generate. 0 or 1 keeps a single beam.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_mesh_path = Data/Models/3ds/laser.3ds` — External 3DS mesh path used for the Laser beam.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_mesh_tint = 255|255|255|255`           — RGBA tint of the mesh Laser beam.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_mesh_size_xy = 5|5`                    — X|Y width of the mesh Laser beam. Format: x|y.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_mesh_glow_rate = 1.0`                  — Additive glow intensity, 0..10. Invalid disables only glow.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_mesh_pulse_rate = 0.0`                 — Light-pulse amplitude, 0..10. 0 or invalid disables only pulse.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_mesh_pulse_speed = 0.0`                — Pulse speed in cycles per second, 0..10. 0 or invalid disables only pulse.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_mesh_noise_rate = 0.0`                 — Deterministic lateral beam distortion, 0..10. 0 or invalid leaves the segment straight.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `laser_mesh_impact_fade_length = 150`         — Fade length near real contact with units, terrain or buildings. 0/invalid = no fade. Does not modify hits, damage or collision.
------------------------------------------------------------------------------------------------------------------------

### Damage and targets

------------------------------------------------------------------------------------------------------------------------
- 🔵 `energy = 10000`         — Base damage of the projectile. Final damage is energy multiplied by the multiplier of the target class (energy_tank, energy_heli, energy_flyer, energy_robo).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `energy_tank = 1.0`      — Damage multiplier against ground vehicles (tank and car). 1 is normal, 0.5 half, 2 double.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `energy_heli = 1.0`      — Damage multiplier against helicopters.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `energy_flyer = 1.0`     — Damage multiplier against aircraft (plane, glider, zeppelin and ufo).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `energy_robo = 1.0`      — Damage multiplier against Host Stations.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_energy = 2000`      — Upgrade-only: adds the value to the base damage.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_energy_tank = 0.2`  — Upgrade-only: adds the value to the tank/car multiplier.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_energy_heli = 0.2`  — Upgrade-only: adds the value to the helicopter multiplier.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_energy_flyer = 0.2` — Upgrade-only: adds the value to the aircraft multiplier.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_energy_robo = 0.2`  — Upgrade-only: adds the value to the Host Station multiplier. The spelling add_energy_Robo is also accepted.
------------------------------------------------------------------------------------------------------------------------

Damage multipliers can distinguish newer/specialized model classes while preserving the older flyer/tank families as fallbacks.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `energy_plane = 1.0`                                                        — Damage multiplier against model = plane. Absent = energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `energy_glider = 1.0`                                                       — Damage multiplier against model = glider. Absent = energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `energy_zeppelin = 1.0`                                                     — Damage multiplier against model = zeppelin. Absent = energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `energy_cruiser = 1.0`                                                      — Damage multiplier against model = cruiser. Absent = energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `energy_ufo = 1.0`                                                          — Damage multiplier against model = ufo. Absent = energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `energy_car = 1.0`                                                          — Damage multiplier against model = car. Absent = energy_tank.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `energy_gun = 1.0`                                                          — Damage multiplier against model = gun/module. Absent = generic vanilla 1.0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `add_energy_plane = 1.0`                                                    — In modify_weapon, adds to energy_plane. If undefined, starts from energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `add_energy_glider = 1.0`                                                   — In modify_weapon, adds to energy_glider. If undefined, starts from energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `add_energy_zeppelin = 1.0`                                                 — In modify_weapon, adds to energy_zeppelin. If undefined, starts from energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `add_energy_cruiser = 1.0`                                                  — In modify_weapon, adds to energy_cruiser. If undefined, starts from energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `add_energy_ufo = 1.0`                                                      — In modify_weapon, adds to energy_ufo. If undefined, starts from energy_flyer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `add_energy_car = 1.0`                                                      — In modify_weapon, adds to energy_car. If undefined, starts from energy_tank.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `add_energy_gun = 1.0`                                                      — In modify_weapon, adds to energy_gun. If undefined, starts from generic vanilla 1.0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `delay_time_debuff_id = 12`                                                 — Debuff profile ID applied as soon as the projectile begins its delay_time on the struck target.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `push = 4`                                                                  — Target push intensity 0-10. Standalone GUNs remain excluded; when an attached GUN is hit, push is transferred once to its valid parent and uses the parent's push_resistance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `recoil = 0.5`                                                              — Primary Weapon recoil, range 0-10. Compatible vehicles keep their existing recoil behavior; on gun/flak model = gun, only the firing gun model slides smoothly backwards along its current barrel axis, without recoil tilt or physical displacement of the gun or parent. 0 or absent disables the effect.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `armor_penetration_targets = 2`                                             — Number of units the Weapon may penetrate before stopping.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `armor_penetration_icon = Data/Graphics/StatusIcons/armor_penetration.png` — Optional Data-relative icon shown next to the Weapon damage while armor_penetration_targets is active.
------------------------------------------------------------------------------------------------------------------------

Weapon Blast uses one shared radius for units, buildings and sector structures.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `Blast_radius = 400`  — Shared 3D radius for units, buildings and sector structures. Missing or 0 disables the blast. Range: 0..12000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Blast_energy = 4000` — Energy removed from all three target categories. When energy is above 0, the unit, building or sector struck directly takes only energy and no Blast_energy; with energy = 0 it takes Blast_energy like the others. Range: 0..100000000. The damage shown in the interface (DMG row, briefing, Database) is always the higher of energy and Blast_energy; Blast_energy counts only when Blast_radius is active and the weapon is not a laser.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Blast_Push = 10`     — Radial push intensity 0..10. Keeps the existing target filters and push_resistance. When push is above 0, the unit struck directly takes only push and no Blast_Push; with push = 0 it takes Blast_Push like the others. GUN actors remain excluded.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `blast_debuff_id = 2` — ID or name of the new_debuff profile applied by the explosion to all enemies inside Blast_radius (for example `blast_debuff_id = shock`). 0, absent, invalid, or an unknown ID or name = no area Debuff. When debuff_id is set, the unit struck directly takes only debuff_id; without debuff_id it takes blast_debuff_id like the others.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Blast_falloff = 1`   — 1 reduces blast energy and push with distance; 0 keeps them uniform inside the radius.
------------------------------------------------------------------------------------------------------------------------

Chain Weapon propagation continues a hit from one enemy unit to another nearby target. Damage, search radius and delay are independently configurable.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `chain_max_jumps = 4`      — Maximum number of additional jumps/bounces after the first impact.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `chain_radius = 1400`      — Search radius for the next enemy target after each impact.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `chain_energy_mult = 0.75` — Damage retained at each jump. 1.0 = no loss; 0.75 = each jump deals 75% of the previous jump's damage.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `chain_jump_delay = 120`   — Delay in milliseconds between an impact and the next chain jump. 0 = immediate propagation.
------------------------------------------------------------------------------------------------------------------------

### Projectile flight and physics

------------------------------------------------------------------------------------------------------------------------
- 🔵 `mass = 50`                — Mass of the projectile. With force and airconst it decides acceleration and inertia.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `force = 15000`            — Thrust of the projectile. For missiles it is also what pulls the projectile toward its target, so together with airconst and mass it sets how well a missile tracks.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `maxrot = 1.5`             — How fast the model rotates to align with its flight direction once the engine has burned out (bomb phase, after drive_time). It does not make a missile track better; use force, airconst and homing_force for that.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `airconst = 40`            — Air resistance. Higher values slow the projectile down and limit its top speed.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `start_speed = 270`        — Initial speed of the projectile when it leaves the weapon.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `drive_time = 1200`        — Duration of the engine thrust in milliseconds. After it expires the projectile stops accelerating and falls like a bomb.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `life_time = 1300`         — Maximum lifetime of the projectile in milliseconds when it was fired at a target. When it runs out the projectile detonates. OpenNeoUA also accepts `min~max`.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `life_time_nt = 1300`      — Same as life_time, used when the Weapon is fired without a unit target (nt = no target).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `delay_time = 500`         — Delay in milliseconds between impact and detonation, which gives time-fused bombs. 0 means it explodes on contact.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `life_time = 4000~5000`    — Weapon lifetime. Single value = fixed; `min~max` = a random lifetime resolved independently for each physical projectile. For model = laser it also controls maximum beam duration/length.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `life_time_nt = 2200~3000` — Lifetime used when the projectile is fired without a unit target (free fire, or a multi_target missile or homing_bomb slot with no target). Replaces life_time. Single value = fixed; `min~max` = a random lifetime resolved independently for each projectile. 0 or absent = life_time is used.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `multi_target = 10`        — Maximum simultaneous targets/projectiles managed by missile and homing_bomb only. Effective limit = min(multi_target, projectile count of the active slot).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `homing_time = 3000` — Missile and homing_bomb only: guidance duration in ms. Once expired, tracking stops and movement continues under gravity and drag. 0, absent or invalid = no guidance time limit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `homing_force = 4`         — missile and homing_bomb only: how well the projectile turns toward its target and follows its path. Extra turn speed in radians per second (about 1 = gentle, 4 = firm, 10 = almost locked on), added to the normal tracking. On homing_bomb the base tracking comes from force, as for missiles. Active while the projectile has a live target and homing_time has not expired, including after drive_time ends, until it explodes. When the player fires, it applies only if the crosshair is locked onto an enemy at the moment of firing (red crosshair); without a lock the projectile steers as in vanilla, even if multi_target assigns it automatic targets. AI-controlled units are unchanged. Range 0-10, decimals allowed; 0, absent or invalid = vanilla behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `drive_time = 7000` — Thrust duration in ms. A missile enters its falling phase; a homing_bomb stops using force to pursue. Both can still turn using homing_force while homing_time remains active. A homing_bomb applies this limit only when explicitly given a positive value; absent, zero or invalid preserves previous behavior. Other weapons retain the 7000 default; arc grenades do not use this phase.
------------------------------------------------------------------------------------------------------------------------

AI ground units (tanks and derived wheeled vehicles) aim forward volumes within the existing vanilla reticle travel, narrowed by scope_angle_min_max when authored. These limits clamp debug orientation and gate primary fire, alternate targets, missile/laser secondary targets and continuous-laser retention. A broad volume cannot authorize a vertical shot. Guns reuse their native up/down/side mechanical stops independently of acquisition distance. Only valid configured forward AI volumes enable these checks; legacy AI without a volume, manual reticle travel, hull orientation, MGUN and downward payload volumes retain their existing behavior. When helicopters, fixed-wing aircraft or UFOs attack a ground target (a sector, a tank, a car or a turret placed on the map), forward volumes tilt vertically toward the aim point when that point lies ahead of the nose, only when the weapon has ai_fire_trigger_forward_max_inclination, and never beyond that angle, and only downward (never upward), so shots still leave roughly forward and an aircraft passing over its target does not fire straight down. Without it, the aircraft volume stays on the hull axes. The hull is not rotated and horizontal acquisition is not widened; the configured width and depth still apply. Weapons without a volume keep the vanilla check. New Debug and continuous-laser retention use the same elevation. Airborne targets and Host Station turrets are not tilted toward. Tanks and cars ignore the parameter: their volume follows the target within their native elevation limits. Guns ignore it too and keep their own rotation and elevation limits. Downward volumes are unchanged. The four AI trigger parameters are alternatives: configure one positive volume per Weapon. Multiple positive volumes or an incompatible model produce a warning and retain legacy fire control. A single 0 disables that volume. Missing, negative, non-finite, incomplete or invalid values do not enable it. When changing shape in modify_weapon, clear the previous volume explicitly. During an attack, the current unit or sector-point target has priority. If it cannot trigger fire, the nearest valid enemy unit within the volume is considered. Unit overlap uses its broad collision radius rather than the exact mesh; sector points use radius 0. The target centre must be in front of the muzzle for forward volumes, or below it for downward volumes. All shapes reuse the full fire_x spawn interval, including vanilla, sequence, random, salve_sequence, salve_mirror and advanced fire_x_start/step/slots racks. Boxes expand across this interval; cones are swept across it. Acquisition does not advance slots, consume random values or change fire_x mode semantics. Fixed/ranged num_weapons and weapon_2/3/4 retain their existing source-slot rules: acquisition uses the selected Weapon, and launch uses that slot's projectile count. Direction patterns (weapon_arc_xy and weapon_cone_xy), weapon_spread_xyz, multi_target, shot_time, shot_time_user, salve_delay, energy costs and recoil are unchanged. The volume authorizes a firing event; other salvo projectiles and additional targets retain their existing weapon rules and may fall outside it. Vertical laser retains its existing in-zone secondary-target filter. Forward volumes respect scope_angle_min_max and Arc Grenade reachability; Arc Grenade launch also validates each actual muzzle origin. Configured volume depth determines acquisition distance independently of ai_attack_range, ai_retreat_start_range and ai_retreat_stop_range. Those parameters continue to control approach, retreat and reengagement; the volume can authorize fire during these maneuvers while a valid combat state and in-volume target remain. Actual laser reach and normal projectile flight remain unchanged. Acquisition does not guarantee a hit after authored spread/patterns. Applies to AI attacks, including guns firing normal Weapons. Manual fire, MGUN, kamikaze and artillery_shell are unchanged. In New Debug, F9 draws the selected compatible Weapon slot in orange. Other slots are hidden. Trigger volumes have no text or dimension labels. Edges are clipped against the camera near plane and viewport so their visible portions remain drawn.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `delay_time = 8000` — Delayed detonation against units: projectile attaches to the target and explodes when the timer expires.
------------------------------------------------------------------------------------------------------------------------

Spiral and Chaos are alternative projectile-motion modifiers. If both are configured and valid, Chaos takes priority.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `chaos_factor = 2.5`               — Visual chaotic-motion strength: increases the frequency of smooth random direction changes. Range 0-10, decimals allowed; 0 = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `chaos_radius = 30`                — Maximum random-deviation radius around the central projectile trajectory. Runtime range 0-1000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `boomerang_return_mode = lifetime` — lifetime returns after life_time; lifetime_or_impact also returns after normal impact damage. Return follows the current muzzle, can hit each unit once, and ends on arrival, launcher loss or after 30 seconds. Missing/invalid = off. Excludes laser, artillery_shell, kamikaze, delay_time and cluster children.
------------------------------------------------------------------------------------------------------------------------

### Rate of fire and bursts

------------------------------------------------------------------------------------------------------------------------
- 🔵 `shot_time = 500`                               — Time in milliseconds between two shots (used by AI-controlled units).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shot_time_user = 400`                          — Same as shot_time, used when the player controls the unit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `salve_shots = 3`                               — Number of shots in one burst before the salve_delay pause starts.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `salve_delay = 2000`                            — Pause in milliseconds between bursts. It replaces shot_time once a burst is complete.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_shot_time = -50`                           — Upgrade-only: adds the value to shot_time (negative values fire faster).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `add_shot_time_user = -50`                      — Upgrade-only: adds the value to shot_time_user.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fire_time_scale = 0.35`                        — Once attacking, a kamikaze keeps closing on the live target while preserving native obstacle avoidance and pending maneuvers. It ignores combat distance: ai_attack_range, ai_retreat_start_range and ai_retreat_stop_range. Aggression still controls target selection and sector attacks. No sector target = no sector fuse. Kamikaze blast damage also reaches building collision surfaces inside Blast_radius, including roofs. Each building slot is checked separately; shields, damage rules and Blast_falloff remain active. Other Weapon classes retain their existing blast behavior. Kamikaze, player only: time-scale intensity activated while pressing FIRE.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fire_time_scale_hp_drain = 12%`                — Kamikaze, player only: HP drained per second during slow motion. With %, uses maximum HP; without %, uses fixed HP. 0 or absent = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ai_fire_trigger_box_below_xyz = 200|1200|400`  — AI only: finite downward box for bomb, homing_bomb and vertical laser. Format: total_width_vertical_depth_total_horizontal_length (X|Y|Z). The volume starts at the muzzle, follows heading and remains vertical despite pitch or roll. Width covers the complete fire_x interval plus the configured padding. Y is finite downward depth, independent of terrain height. Unit acquisition uses sphere contact at sides and end cap; sector targets use point containment. A single 0, absent or invalid values retain legacy fire control.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ai_fire_trigger_cone_below_xyz = 30|20|1200`   — AI only: downward elliptical cone for bomb, homing_bomb and vertical laser. Format: full_lateral_angle_full_fore_aft_angle_vertical_depth. Both full angles are in degrees, strictly between 0 and 180. The third value is finite axial depth vertically downward. The cone remains vertical and follows heading only, while its origin follows the muzzle. Reuses the complete fire_x interval and the shared finite-cone contact test. In Studio, opposite handles adjust the same symmetric angles.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ai_fire_trigger_box_forward_xyz = 100|80|1200` — Downward cone acquisition, secondary-target selection and continuous vertical-laser retention share the configured volume; actual beam reach still applies. It retains existing projectile counts, fire_x modes/racks, direction patterns, spread, multi_target and firing cadence. Both downward shapes have finite depth and share the same sphere-contact and beam-reach rules. AI only: muzzle-local forward box, specified as total_width_total_height_axial_depth. Width and height are centred on the firing axis; the volume follows pitch and roll. Supported models: rocket, missile, grenade, arc_grenade and non-vertical laser.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ai_fire_trigger_cone_forward_xyz = 30|20|1200` — AI only: forward elliptical cone, specified as full_horizontal_angle_full_vertical_angle_axial_depth. Both full angles are in degrees, strictly between 0 and 180; 30|20 means +/-15 horizontally and +/-10 vertically. Z is depth along the firing axis, ending at a flat cap. Supports the same models as the forward box.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ai_fire_trigger_forward_max_inclination = 25`  — Per Weapon with a forward box or cone: maximum downward tilt toward ground targets, in degrees. Used by helicopters, fixed-wing aircraft and UFOs only; ground units and guns ignore it. From 0 to below 90; zero or absent preserves vanilla behavior. Downward volumes do not tilt.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ai_fire_trigger_dead_zone = 150`               — Per Weapon with any box or cone: minimum axial distance from the muzzle. Targets inside this near dead zone do not trigger fire; Z remains the far limit. A large unit can trigger when its collision sphere touches the near limit. Zero or absent disables the dead zone.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `weapon_energy_cost = 0.5%`                     — Per-Weapon energy cost charged once per successful firing event, regardless of projectile count. 0% disables the legacy drain; absent/invalid preserves vanilla per-projectile drain.
------------------------------------------------------------------------------------------------------------------------

Progressive fire rate accelerates a continuously firing Weapon from its normal shot interval toward a configured minimum. Releasing FIRE or breaking the sequence resets the ramp.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `ramp_up_time = 4000`                — Time in milliseconds to reduce shot interval progressively from normal shot_time/shot_time_user to ramp_up_max_shot_time. 0/absent/invalid = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ramp_up_max_shot_time = 150`        — Minimum shot interval reached at the end of ramp_up_time. Ramp progress resets when FIRE is released or the sequence is interrupted.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ramp_up_max_weapon_spread_xy = 1|1` — Final random spread of this Weapon at the end of ramp_up_time (format x|y, horizontal_vertical). Decreases the X and Y of the Vehicle's weapon_spread_xyz without increasing an already lower value. Requires active ramp-up; 0/absent/invalid = disabled for that axis.
------------------------------------------------------------------------------------------------------------------------

Optional overheat behavior starts only after the Weapon has reached and then maintained ramp_up_max_shot_time for the configured delay.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `ramp_up_overheat_hp_drain = 4%`                             — Percentage of overheat damage applied after the Weapon reaches and maintains ramp_up_max_shot_time.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ramp_up_overheat_time = 5000`                               — Time in milliseconds spent at ramp_up_max_shot_time before overheat damage begins.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ramp_up_overheat_icon = Graphics/StatusIcons/overheat.png` — Data-relative icon shown while the unit is overheating.
------------------------------------------------------------------------------------------------------------------------

### Collisions

------------------------------------------------------------------------------------------------------------------------
- 🔵 `radius = 50`       — Collision radius of the projectile against units. Used for every unit class whose `radius_*` value is 0.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `radius_tank = 10`  — Collision radius against ground vehicles (tank and car).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `radius_heli = 20`  — Collision radius against helicopters.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `radius_flyer = 20` — Collision radius against aircraft (plane, glider, zeppelin and ufo).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `radius_robo = 30`  — Collision radius against Host Stations.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `overeof = 0`       — Vertical offset used by the ground collision. Rarely needs changing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vwr_radius = 0`    — Radius used by the viewer camera when following the projectile. Rarely needs changing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vwr_overeof = 0`   — Vertical viewer offset for the projectile. Rarely needs changing.
------------------------------------------------------------------------------------------------------------------------

### Appearance (models, VP, 3DS, trails)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_normal = 124`   — ID of the visual model (VP) of the projectile in flight. This is the one that matters most.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_dead = 125`     — ID of the model shown when the projectile explodes on a unit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_megadeth = 125` — ID of the model shown when the projectile explodes on terrain or buildings.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_wait = 22`      — ID of the idle model. Rarely visible on vanilla weapons.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_fire = 22`      — ID of the launch model. Usually has no visible effect.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vp_genesis = 22`   — ID of the creation model. Usually has no visible effect.
------------------------------------------------------------------------------------------------------------------------

normal, goingdown and explode use the same `snd_*`, `pal_*` and `shk_*` event families documented for Vehicle Physical FX above. Vanilla compatibility: physical_vehicle with offset_xyz remains readable for legacy scripts, but new content should use the inline format.

Weapon Ground Decal profiles paint a persistent or timed footprint on the world around the impact: terrain and buildings, never units. The decal is created only after a real projectile/world collision.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `ground_decal_id = 1`        — ID of the reusable new_decal profile. Vehicles spawn on grounded destruction; Weapons spawn on real world impact. -1, absent or unknown ID = no decal.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ground_decal_size = 80~120` — Manual diameter range in world units, 0..12000. Absent, zero or invalid disables the decal.
------------------------------------------------------------------------------------------------------------------------

Craters and dents (Weapons only): a real impact on the world really deforms what it hits, both visually and in the collision. On sector ground and on the seams between sectors (slopes up to about 50 degrees) it digs a crater: units drive down into it and the lower parts of nearby buildings bend with the ground. On roofs, walls and steeper slopes it makes a crumpled dent instead, pushed into the surface that was hit, with the same radius and depth: this works on every building, classic and special (power stations, flak, radar, gates and the others). Host Stations and units are never deformed. Overlapping hits add up, to at most 1.75 times the strongest one. They last for the whole mission, are stored in the saved game and are removed only when a building is built in that sector. They are disabled in multiplayer. The weapon decal follows the crater or dent as if it had been painted before the hit and then dug with the surface: it stays whole on the bottom and on the steep walls of the pit, on dented walls and under many overlapping hits. This needs a graphics card with float render targets (OpenGL 3, or the ARB_texture_float and ARB_color_buffer_float extensions); without them the decal is simply projected on the dug surface, like on any other. When a building next to a crater is damaged, destroyed or built, the dug surfaces around it are made again together, so walls and ground stay joined. When the shot hits a ground unit (tanks, cars and any future ground class) the unit is neither deformed nor marked: the crater and the decal go on the ground right under the hit point, as for a ground hit. Near the map border the crater fades out on the border pieces (wall and ground strip), so the border stays whole, with no cracks. game.deformation_max_craters sets how many are kept (or `no_limit`).

------------------------------------------------------------------------------------------------------------------------
- 🟢 `crater_radius = 260~320`    — Radius of the crater (and of the dent on buildings) in world units, fixed value or `min~max` (0..12000). Works together with crater_depth: if either is absent, zero or invalid there is no crater (vanilla behavior). In Studio use the Crater row; values typed in a new effect are transferred there while retaining the other existing values.

    **Delete** removes Crater and Ground decal settings from the definition's scripts, including `modify_*` blocks. When a cluster weapon spawns itself, both crater rows share the same settings.

    The **Visuals & FX** timeline starts at 0.25×. Use its speed menu to change playback speed.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `crater_depth = 60~80`       — Depth of the crater centre (or how far the dent goes in) in world units, fixed value or `min~max` (0..12000). Never more than half the radius: walls stay below about 37 degrees, so tanks can drive in and out.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `crater_rim = 20`            — Raised rim around the crater (ground only), in % of the depth, fixed value or `min~max` (0..100). Absent = 20; 0 = no rim; invalid = no crater.
------------------------------------------------------------------------------------------------------------------------

Weapons can also use state-based external visuals. As elsewhere, 3DS has priority over BASE, and BASE falls back to the corresponding VP.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_normal = Data/Models/Base/VEHICLE.BASE`            — External BASE model for normal state; falls back to vp_normal.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_fire = Data/Models/Base/VEHICLE_FIRE.BASE`         — External BASE model for fire state; falls back to vp_fire.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_wait = Data/Models/Base/VEHICLE_WAIT.BASE`         — External BASE model for wait state; falls back to vp_wait.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_dead = Data/Models/Base/VEHICLE_DEAD.BASE`         — External BASE model for dead state; falls back to vp_dead.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_megadeth = Data/Models/Base/VEHICLE_MEGADETH.BASE` — External BASE model for megadeth state; falls back to vp_megadeth.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_genesis = Data/Models/Base/VEHICLE_GENESIS.BASE`   — External BASE model for genesis state; falls back to vp_genesis.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_normal = Data/Models/3ds/vehicle.3ds`               — External 3DS model for normal state; takes priority over base_normal and vp_normal.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_fire = Data/Models/3ds/vehicle_fire.3ds`            — External 3DS model for fire state; takes priority over base_fire and vp_fire.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_wait = Data/Models/3ds/vehicle_wait.3ds`            — External 3DS model for wait state; takes priority over base_wait and vp_wait.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_dead = Data/Models/3ds/vehicle_dead.3ds`            — External 3DS model for dead state; takes priority over base_dead and vp_dead.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_megadeth = Data/Models/3ds/vehicle_megadeth.3ds`    — External 3DS model for megadeth state; takes priority over base_megadeth and vp_megadeth.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_genesis = Data/Models/3ds/vehicle_genesis.3ds`      — External 3DS model for genesis state; takes priority over base_genesis and vp_genesis.
------------------------------------------------------------------------------------------------------------------------

blast_debuff_id applies at explosion to every living enemy inside the radius, even with Blast_energy = 0, except the unit struck directly by a Weapon with debuff_id. Allies are not affected. Deflect blocks it; when mind control cannot apply, the other Debuff effects still can. Host Station and child rules also apply.

Physical Weapon mesh tracers are an additional visual layer. They follow the real projectile trajectory and are rendered alongside the main VP/BASE/3DS projectile visual. They do not change trajectory, speed, collision or damage.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `mesh_tracer_path = Data/Models/3ds/mgun_tracer_cross.3ds` — External 3DS mesh used by the Weapon tracer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mesh_tracer_tint = 255|80|80|180`                         — General R|G|B|A tracer tint. Alpha 0 disables the effect.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mesh_tracer_head_tint = 255|80|80|180`                    — R|G|B|A tint of the tracer head. Absent = mesh_tracer_tint.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mesh_tracer_tail_tint = 255|80|80|180`                    — R|G|B|A tint of the tracer tail. Absent = mesh_tracer_tint.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mesh_tracer_size_xyz = 4|4|100`                           — Tracer size (format x|y|z): X base width (range 0..100), Y size (range 0..100; invalid inherits X), Z maximum visible length along the Weapon's real trajectory (range 0..6000). A 0/invalid X or Z disables the tracer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mesh_tracer_pos_xyz = 0|0|0`                              — Local X|Y|Z position relative to Weapon launch point. Clamped to -6000..6000. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mesh_tracer_head_size = 4`                                — Uniform X/Y head size. Range 0..100; 0 creates a point; absent/invalid uses size_x/size_y. Z length is controlled only by size_z to keep the tracer continuous.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mesh_tracer_tail_size = 4`                                — Uniform X/Y tail size. Range 0..100; 0 closes the tail; absent/invalid uses size_x/size_y. Z length is controlled only by size_z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mesh_tracer_glow_rate = 0`                                — Glow intensity, range 0..10. Changes brightness only.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mesh_tracer_noise_rate = 0`                               — Stable random trail distortion/noise, range 0..10. 0 = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mesh_tracer_pulse_rate = 0`                               — Width-pulsation intensity, range 0..10. 0 = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mesh_tracer_pulse_speed = 0`                              — Pulsation speed in cycles per second, range 0..10. 0 = disabled; requires mesh_tracer_pulse_rate > 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tint = 40|120|255|255`                                    — RGBA tint applied to the main VP/BASE/3DS visual.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spin_xyz = 0|0|10`                                        — X|Y|Z spin intensity of the main VP/BASE/3DS visual: 0-10, decimals allowed; 0 = no spin. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `scale_xyz = 1.0|1.0|3.0`                                  — X|Y|Z visual scale of all VP/BASE/3DS states, including dead and megadeth. Does not change collision geometry. Default: 1.0. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `rotation_xyz = 0|0|0`                                     — Fixed X|Y|Z visual rotation in degrees. 0 or absent = no rotation. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------

VP trail controls affect only embedded VP trails, particles and children; they do not tint or transform an external 3DS model. This separation is useful when the projectile body and its legacy particle system need different treatment.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_trail_scale_xyz = 1.0|1.0|1.0` — X|Y|Z scale of VP trails, particles and embedded children only. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_trail_spin_xyz = 0|0|10`       — X|Y|Z spin intensity of VP trails, particles and embedded children only: 0-10. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_trail_tint = 255|255|255|255`  — RGBA tint applied only to VP trails, particles and embedded children; does not modify a 3DS model.
------------------------------------------------------------------------------------------------------------------------

Weapon decorations use the same reusable periodic/persistent visual subsystem documented above. Their offsets are relative to the parent Weapon.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_mode = periodic`                            — Periodic decoration mode.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp = 99`                                    — Legacy VP decoration visual; final fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_base = Data/Models/Base/FX/DECORATION.BASE` — Data-relative BASE family; falls back to decoration_fx_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_3ds = Data/Models/3ds/FX/decoration.3ds`    — 3DS decoration model; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_scale_xyz = 1.0|1.0|1.0`                    — X|Y|Z scale of active decoration. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_spin_xyz = 0|0|0`                           — X|Y|Z spin intensity: 0-10, decimals allowed; 0 = no spin. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_tint = 255|95|70|200`                       — RGBA tint applied to the main decoration visual.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_fade_inout = 500|500`                       — Main VP/3DS decoration fade-in and fade-out in milliseconds (format in|out). 0 or absent = immediate appearance/disappearance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_tint = 255|255|255|255`            — RGBA tint of VP decoration particles/trails. Alpha 0 = invisible.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_scale_xyz = 1.0|1.0|1.0`           — X|Y scale of VP decoration particles/trails, plus a Z multiplier for particle/trail duration and therefore apparent trail length (format x|y|z).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_fade_inout = 500|500`              — Time in milliseconds (format in|out) for VP decoration particles/trails to fade in to their configured opacity (in) and to fade out to invisible (out). 0 or absent = immediate.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_duration = 1000`                            — Total periodic-decoration duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_interval = 500~1500`                        — Random interval in milliseconds between spawns. A single value gives a fixed interval.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_count = 2~7`                                — Random decoration count per spawn. A single value gives a fixed count.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_random_pos = 90`                            — XYZ position randomization around the parent Vehicle/Weapon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_offset_xyz = 0|0|-40`                       — X|Y|Z offset from the parent Vehicle/Weapon center. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_mode = persistent`                          — Persistent decoration remains active while the parent Vehicle/Weapon remains active.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp = 99`                                    — Legacy VP decoration visual; final fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_base = Data/Models/Base/FX/DECORATION.BASE` — Data-relative BASE family; falls back to decoration_fx_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_3ds = Data/Models/3ds/FX/decoration.3ds`    — 3DS decoration model; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_scale_xyz = 1.0|1.0|1.0`                    — X|Y|Z scale of the active decoration. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_spin_xyz = 0|0|0`                           — X|Y|Z spin intensity: 0-10, decimals allowed; 0 = no spin. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_tint = 255|95|70|200`                       — RGBA tint applied to the main decoration visual.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_fade_inout = 500|500`                       — Main VP/3DS decoration fade-in and fade-out in milliseconds (format in|out). 0 or absent = immediate appearance/disappearance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_tint = 255|255|255|255`            — RGBA tint of VP decoration particles/trails. Alpha 0 = invisible.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_scale_xyz = 1.0|1.0|1.0`           — X|Y scale of VP decoration particles/trails, plus a Z multiplier for particle/trail duration and therefore apparent trail length (format x|y|z).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_fade_inout = 500|500`              — Time in milliseconds (format in|out) for VP decoration particles/trails to fade in to their configured opacity (in) and to fade out to invisible (out). 0 or absent = immediate.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_offset_xyz = 0|0|-40`                       — X|Y|Z offset from the parent Vehicle/Weapon center. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------

### Impact effects

------------------------------------------------------------------------------------------------------------------------
- 🔵 `dest_fx = death_46_0_0_0`           — Effect spawned on impact. Format type_ID_X_Y_Z: death when it hits a unit, megadeth when it hits terrain or buildings, then the ID of the effect vehicle (effects are entries of Vehicles.cfg) and its X, Y and Z offset. The | separator is also accepted.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `ext_dest_fx = megadeth_17_10_-20_0` — Extended variant of dest_fx with the same format. Rarely used. Alias: extended_dest_fx.
------------------------------------------------------------------------------------------------------------------------

Weapon Chain FX use the same visual/physical concepts as vehicle Chain FX, but their triggers are tied to projectile impact/detonation. This makes it possible to build richer explosions without requiring the old dest_fx -> scale_fx relationship.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `begin_fx`                                       — Visual Chain FX replacing a dest_fx that points to scale_fx.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `trigger = impact_world, detonate or launch`     — Trigger: impact_world = weapon hits terrain/buildings; detonate = weapon hits a unit or detonates; launch = weapon is fired (projectile launch point, or the start of the beam for model=laser; offset_xyz follows the projectile rotation). For model=laser, detonate also runs on world contact at the real beam endpoint. Several triggers can be joined with | (e.g. impact_world|detonate): the block runs on each one, exactly as if it were written twice (an event that fires both triggers plays the effect twice). Any unknown name rejects the whole block.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mode = visual`                                  — Uses start_size/end_size/duration and VP/BASE/3DS visuals.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `random_rotation = 0`                            — 1 assigns a random X/Y/Z orientation once at spawn, independently per instance; 0, omitted or invalid = normal orientation. Spawn position and launch direction are unchanged.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `count = 2~4`                                    — Number of effect instances generated per activation. Single value = fixed amount; `min~max` = random amount rolled per activation. Maximum 32; invalid = 1. With count > 1 each instance rolls its own `min~max` offset. Supported only with mode = visual and mode = physical; not available in SuperItem Chain FX blocks.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `offset_xyz = -20|0|20`                          — Effect X|Y|Z offset. Single value = fixed; `min~max` = random offset rolled at every activation. Format: x|y|z; each axis takes a single value or `min~max`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spin_xyz = 0|0|10`                              — X|Y|Z spin intensity of the Chain FX VP/BASE/3DS visuals: 0-10, decimals allowed; 0 = no spin. Applies to every model in the block. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `start_size = 0.1`                               — Initial effect size. 1 = original visual size.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mid_size = 1`                                   — Intermediate effect size.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end_size = 2`                                   — Final effect size.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `duration = 1000`                                — Effect duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fade_inout = 500|1000`                          — Visual fade-in and fade-out in milliseconds (format in|out). 0 or absent = immediate.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_model = 229`                                 — First legacy VP visual in the sequence.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_model = Data/Models/Base/FX/CHAIN_FX.BASE` — Optional BASE family for the same element. Place after vp_model to preserve VP fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_model = Data/Models/3ds/FX/chain_fx.3ds`    — Optional 3DS model for the same element; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tint = 80|160|255|180`                          — RGBA tint for all models in the visual block, regardless of position. The last value wins; omitted tint preserves original colours.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_model = 230`                                 — Second visual in the sequence.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_model = 231`                                 — Third visual in the sequence.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end`
------------------------------------------------------------------------------------------------------------------------
- 🟢 `begin_fx`                                       — Autonomous inline Physical FX generated by a Weapon event.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `trigger = impact_world, detonate or launch`     — Weapon trigger: world/building impact, unit/air detonation, or weapon launch.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mode = physical`                                — Uses the modern inline physical fragment path rather than a Vehicle effect ID.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `random_rotation = 0`                            — 1 assigns a random X/Y/Z orientation once at spawn, independently per instance; 0, omitted or invalid = normal orientation. Spawn position and launch direction are unchanged.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `lifetime = 30000`                               — Maximum time in milliseconds that the Physical Fragment may remain in flight before it is removed. Single value = fixed lifetime; `min~max` = a lifetime rolled independently for each fragment. 0, absent or invalid = 30000. Ground contact still removes the fragment immediately.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `count = 1~4`                                    — Number of fragments created. Single value = fixed; `min~max` = random. Runtime maximum: 32. Each fragment independently selects its own VP entry from vp_model.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `launch_xyz = -30~30|-20|-30~30`                 — Initial local X|Y|Z velocity (format x|y|z). Each axis takes a single value (fixed) or `min~max` (randomized independently for each fragment). Inline fragments spawn at the event position rather than using offset_xyz.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_model = 138|193`                             — One or more legacy fragment VP IDs separated by |. Each spawned fragment randomly selects one entry; a single value preserves the previous behavior. VP remains the final fallback after 3DS/BASE.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_model = Data/Models/Base/FX/FRAGMENT.BASE` — Optional BASE fragment family.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_model = Data/Models/3ds/FX/fragment.3ds`    — Optional 3DS fragment model; highest visual priority.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_impact = 140|69`                             — Positional impact-VP list paired with vp_model. The selected model index always uses the impact at the same index. When present, the list must contain the same number of entries; 0 disables the impact VP for that position.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tint = 255|255|255|255`                         — Entries may be repeated to weight random selection, with vp_impact repeated in the same positional order. Fragment RGBA tint.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `scale = 1.0`                                    — Uniform fragment scale. <= 0 or invalid = 1.0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mass = 150`                                     — Fragment mass. Must be positive.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `airconst = 40.0`                                — Air resistance. 0 is valid.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `force = 6000`                                   — Additional impulse along the launch direction. 0 is valid.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `maxrot = 3.0`                                   — Rotation intensity. 0 disables additional rotation.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `radius = 1.0`                                   — Physical/collision radius. Must be positive.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `impact_energy = 200`                            — Energy removed once from the first unit hit by this physical fragment. Respects shield and invulnerability. 0 or absent = disabled. Single-player only.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `impact_energy_radius = 0`                       — Additional 3D radius for energy removal on impact. 0 = direct target only; a positive value removes the same energy from nearby units. Each fragment can remove energy only once.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_normal_sample = Data/Sounds/shot7.wav`      — Optional fragment-spawn sound.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_goingdown_sample = Data/Sounds/fall.wav`    — Optional goingdown sound.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_explode_sample = Data/Sounds/impact.wav`    — Optional finish/impact sound.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_goingdown_slot = 1`                         — Example goingdown PAL event; mag0, mag1, time and radius are also supported.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_explode_slot = 1`                           — Example explode PAL event; mag0, mag1, time and radius are also supported.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_explode_slot = 1`                           — Example explode SHK event; mag0, mag1, time, radius/mute and x/y/z are also supported.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end`
------------------------------------------------------------------------------------------------------------------------

### Debuffs

------------------------------------------------------------------------------------------------------------------------
- 🟢 `debuff_id = 1` — ID or name of the new_debuff profile applied to the target struck directly by this Weapon (for example `debuff_id = shock`). It does not reach the blast area: use blast_debuff_id for that. Names must start with a letter and ignore case; if two profiles share a name, the lowest ID is used. -1, absent, or an unknown ID or name = no Debuff.
------------------------------------------------------------------------------------------------------------------------

### Sounds, palette flashes and shakes

------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_<evento>_sample = sounds/launch.wav`             — Sound file played for the event. Weapon events: normal (while flying), launch (when fired) and hit (on impact).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_<evento>_volume = 150`                           — Volume of the event sound.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_<evento>_pitch = 0`                              — Pitch of the event sound.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_<evento>_ext = 1_100_22050_0_0_sounds/extra.wav` — Additional sample. Format loop_volume_sampleRate_offset_sampleCount_file.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pal_<evento>_slot = 2`                               — Screen color flash: effect type (1 red, 2 blue, 3 green, 4 inverse, 5 dark inverse, 6 black and white, 7 turquoise inverse).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pal_<evento>_time = 500`                             — Duration of the flash in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pal_<evento>_mag0 = 2`                               — Starting intensity of the flash.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pal_<evento>_mag1 = 1`                               — Final intensity of the flash.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<evento>_slot = 1`                               — Camera shake: shake type.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<evento>_time = 300`                             — Duration of the shake in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<evento>_mag0 = 1`                               — Starting intensity of the shake.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<evento>_mag1 = 0.5`                             — Final intensity of the shake.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<evento>_x = 1`                                  — Shake strength on the X axis.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<evento>_y = 1`                                  — Shake strength on the Y axis.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<evento>_z = 1`                                  — Shake strength on the Z axis.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shk_<evento>_mute = 0`                               — How much the audio is attenuated during the shake.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_launch_sample = sounds/Laser.WAV`                — Laser-specific behavior: non-looping launch sound played when FIRE begins/is held.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_launch_volume = 100`                             — Launch sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_launch_pitch = 900~1100`                         — Launch sound pitch. Single value = fixed; `min~max` = random pitch selected on each playback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_launch_at_muzzle = 1`                            — Plays the launch sound from a fixed position at the shooter (like the MGUN) instead of from the moving projectile, avoiding Doppler/pan drift on fast bullets. Physical projectiles only, not lasers; launch shake/palette FX are not played. 0/absent = vanilla behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_normal_sample = sounds/Laser.WAV`                — Laser-specific behavior: continuous looping sound used while the Laser is firing.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_normal_volume = 100`                             — Normal/loop sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_normal_pitch = 900~1100`                         — Normal/loop sound pitch. Single value = fixed; `min~max` = random pitch selected on each playback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_hit_sample = sounds/LaserHit.WAV`                — Laser-specific behavior: serialized one-shot hit sound while the Laser contacts a surface. Continuous contact repeats it without overlapping instances.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_hit_volume = 100`                                — Hit sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_hit_pitch = 900~1100`                            — Hit sound pitch. Single value = fixed; `min~max` = random pitch selected on each playback.
------------------------------------------------------------------------------------------------------------------------

Player-only launch shake is separate from world-space SHK effects. It can be used to give the player stronger weapon feedback without shaking nearby AI or other observers.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_launch_player_slot = 4`             — Shake slot generated when this Weapon launches, for the player only.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_launch_player_time = 500`           — Player-only launch-shake duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_launch_player_mag0 = 1.5`           — Initial player-only launch-shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_launch_player_mag1 = 0.5`           — Final player-only launch-shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_cluster_slot = 4`                   — Palette slot used at split. 0/absent = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_cluster_mag0 = 5`                   — Initial split palette intensity. Default: 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_cluster_mag1 = 0`                   — Final split palette intensity. Default: 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_cluster_time = 1500`                — Split palette duration in milliseconds. Default: 1000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_cluster_radius = 4000`              — Maximum cluster palette-effect radius. Intensity decreases linearly to 0 at the limit. 0 or absent = legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_cluster_slot = 1`                   — Shake slot used at split. 0/absent = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_cluster_mag0 = 2`                   — Initial split-shake intensity. Default: 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_cluster_mag1 = 0`                   — Final split-shake intensity. Default: 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_cluster_time = 5000`                — Split-shake duration in milliseconds. Default: 1000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_cluster_radius = 4000`              — Maximum cluster-shake radius. Intensity decreases linearly to 0 at the limit. 0 or absent = legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_cluster_mute = 0.02`                — Legacy cluster-shake distance attenuation factor used only when shk_cluster_radius = 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_cluster_xyz = 0.2|0.2|0.2`          — Random cluster-shake strength multiplier on the X|Y|Z axis. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_cluster_sample = sounds/Smex02.wav` — Sound played when clustering activates.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_cluster_pitch = 0~500`              — Cluster split sound pitch. Single value = fixed; `min~max` = random pitch selected on each playback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_cluster_volume = 120`               — Split sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_cluster_radius = 4000`              — Maximum cluster-sound radius. Volume decreases linearly to 0 at the limit. 0 or absent = legacy behavior.
------------------------------------------------------------------------------------------------------------------------

### OpenNeoUA special features

Cluster Weapons split into child Weapons either after a timer or on impact. Child count, direction, spread and recursive generations are all data-driven. Keep recursive generation/count values reasonable: combinations multiply very quickly and can create large numbers of active projectiles.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_count = 2~8`                                   — Number of child Weapons generated per split. Single value = fixed count; `min~max` = inclusive random count. Safe range 1-64; invalid values disable clustering.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_weapon_id = 3`                                 — Weapon ID generated as the cluster child.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_trigger_time = 1000`                           — > 0 = timed cluster trigger in milliseconds. 0 or absent = cluster splits on impact.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_horizontal_angle = 0~360`                      — Horizontal cluster direction. Single value = fixed angle; `min~max` = range. `0~360` distributes clusters uniformly.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_vertical_angle = -45~45`                       — Vertical cluster direction. Single value = fixed angle; `min~max` = random angle in range.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_spread_xy = 60|60`                             — Random X|Y deviation applied after the base angle direction; may be combined with angle parameters. Format: x|y.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_ai_group_radius = 600`                         — AI Smart only (weapon_ai_switch_mode = smart). Enemies usually enter a fire volume one at a time, so a lone enemy counts as a group when another enemy stands within this radius of the unit the shot will go to. The AI then prefers this cluster Weapon. 0 or absent = only enemies inside the volume count (two are required). Maximum 5000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_vp = 112`                                      — Legacy VP generated when the Weapon splits; final fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_base = Data/Models/Base/FX/CLUSTER_SPLIT.BASE` — BASE family generated on split, relative to Data; falls back to cluster_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_3ds = Data/Models/3ds/FX/cluster_split.3ds`    — 3DS split model; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cluster_generations = 3`                               — Allows generated cluster Weapons to trigger their own clustering for the configured number of generations.
------------------------------------------------------------------------------------------------------------------------

## Buildings (Buildings.cfg)

### Identity and cost

------------------------------------------------------------------------------------------------------------------------
- 🔵 `new_building = 10`                      — Opens the definition of a new Building with the given numeric ID. If the ID already exists, the prototype is rebuilt from scratch.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `modify_building = 10`                   — Reopens an existing Building so that only the listed values change. Typical use is level files and tech-upgrade blocks.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `end`                                    — Closes the current new_building / modify_building block.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `enable = 1`                             — Makes the Building available to the given faction (1 Resistance, 2 Sulgogar, 3 Mykonian, 4 Taerkasten, 5 Black Sect, 6 Ghorkov, 7 neutral/tutorial). One line per faction.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `disable = 1`                            — Removes the Building from the given faction.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `model = kraftwerk`                      — Building class: building (generic), kraftwerk (power station), radar and defcenter (defense center). Unknown values make the block invalid.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `name = Power_Station`                   — Name of the Building. Underscores are shown as spaces.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sec_type = 20`                          — Links the Building to a sector type of the map set. typ_map defines the visual sector, blg_map places functional buildings on top, and sec_type ties the two together.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `type_icon = P`                          — Character selecting the Building icon, mapped through TYPE_NS.FON into the H_FINDER graphic. See the OpenNeoUA form below for Unicode symbols.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `production_cost = 1400`                 — Displayed Genesis price for this building. Overrides the vanilla Energy-derived price without changing hit points. 0 or absent = vanilla behavior (Energy / 100).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `building_description = Defense outpost` — Optional building description shown in the Database. If absent, the Description row is not shown.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `type_icon = V`                          — Building icon (a .FON glyph key). A Unicode symbol is also accepted, e.g. ∆ or U+2206, linked in the .FON to the icon coordinates. Any value other than I lists the building in the Database. Vanilla compatibility: vanilla reads only the first byte of the value, so use a plain ASCII character (letters, digits or symbols such as ] or `). U+XXXX values and Unicode symbols are OpenNeoUA-only; vanilla reads U+2206 as the letter U (the Radar Station icon). When a non-ASCII symbol is needed, write it as U+XXXX: a pasted symbol can be corrupted by the editor's text encoding, and an unreadable icon falls back to the default A icon. In the .FON, write new keys as a single byte; a U+XXXX line must stay above the U line, otherwise vanilla replaces the Radar Station icon. The replacement character U+FFFD is rejected.
------------------------------------------------------------------------------------------------------------------------

### Stats

------------------------------------------------------------------------------------------------------------------------
- 🔵 `energy = 50000` — Hit points of the Building. In vanilla it is also its cost; production_cost overrides the price only.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `power = 100`    — Energy produced or supplied by the Building. Mainly used by power stations.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shield = 20`    — Armor as a percentage from 0 to 100, reducing incoming damage. Values outside the range are ignored.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `radar = 2`      — Radius in sectors the Building reveals around itself (radar). Negative values are clamped to 0.
------------------------------------------------------------------------------------------------------------------------

### Briefing model settings for buildings

- 🟢 `briefing_size = 100` sets the size of a building shown by its prototype in the briefing or debriefing, for example a gate or tech building. 100 keeps the classic framing, higher values enlarge the model and lower values shrink it. Valid range is 1–10000. Zero, missing or invalid uses classic framing.
- 🟢 `briefing_xyz = 0|0|0` moves that preview in the same screen axes as the vehicle setting. Positive X moves right, positive Y down and positive Z away. Zero keeps the original position. These are optional OpenNeoUA parameters.

### Flaks mounted on the building

In **Briefing Model Settings**, turrets mounted on host stations and buildings rotate with the model, including in Debriefing mode.

In OpenNeoUA Studio, choose **Buildings** in Entity List. Buildings with mounts expose **Gun Points**. **Show Mounted Flak** toggles the 3D models while the direction markers remain available. **Restore Loaded Position** restores the selected mount's script coordinates. **Move to 0 / 0 / 0** places it at the model origin. Mount edits write the vanilla `sbact_*` fields. The **Vehicle** menu for sector gun points lists only Flak vehicles from the catalog (`model = gun`). Adding a gun point requires at least one Flak in the catalog. A zero `sbact_vehicle` ends the active list, including inherited slots in `modify_building`.

------------------------------------------------------------------------------------------------------------------------
- 🔵 `sbact_act = 0`      — Selects which attached flak slot is being configured, starting at 0. The `sbact_*` lines that follow apply to it. Repeat the block for each turret.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sbact_vehicle = 95` — ID of the Vehicle used as the turret. It should be a model = gun unit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sbact_pos_x = 0`    — X position of the flak relative to the Building center.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sbact_pos_y = -80`  — Y position of the flak. Negative values place it above the Building.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sbact_pos_z = 0`    — Z position of the flak (forward / backward). It does not set where the flak looks.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sbact_dir_x = 0`    — Initial facing X of the flak when it spawns.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sbact_dir_y = 0`    — Initial facing Y of the flak.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sbact_dir_z = 1`    — Initial facing Z of the flak. These are not movement limits: those belong to the gun Vehicle (gun_side_angle, gun_up_angle, gun_down_angle).
------------------------------------------------------------------------------------------------------------------------

### Appearance

Building decoration FX can be periodic or persistent. Both modes use the same VP/BASE/3DS visual family, but their lifetime rules differ: - periodic: spawn one or more decorations at configured intervals; - persistent: keep the decoration active while the building remains active and alive.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_mode = periodic`                            — Periodic decoration mode: decorations appear intermittently using the timing/count parameters below.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp = 99`                                    — Legacy VP decoration visual; final fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_base = Data/Models/Base/FX/DECORATION.BASE` — BASE decoration family, relative to Data; falls back to decoration_fx_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_3ds = Data/Models/3ds/FX/decoration.3ds`    — 3DS decoration model; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_scale_xyz = 1.0|1.0|1.0`                    — X|Y|Z scale of the active decoration visual. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_spin_xyz = 0|0|0`                           — X|Y|Z spin intensity for the decoration VP/3DS visual: 0-10, decimals allowed; 0 = no spin. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_tint = 255|95|70|200`                       — RGBA tint applied to the main decoration visual.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_fade_inout = 500|500`                       — Main VP/3DS decoration fade-in and fade-out in milliseconds (format in|out). 0 or absent = immediate appearance/disappearance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_tint = 255|255|255|255`            — RGBA tint of VP decoration particles/trails. Under additive blending, fading also attenuates visual intensity correctly.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_scale_xyz = 1.0|1.0|1.0`           — X|Y scale of VP decoration particles/trails, plus a Z multiplier for particle/trail duration and therefore apparent trail length (format x|y|z).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_fade_inout = 500|500`              — Time in milliseconds (format in|out) for VP decoration particles/trails to fade in to their configured opacity (in) and to fade out to invisible (out). 0 or absent = immediate.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_duration = 1000`                            — Total duration in milliseconds of each periodic decoration instance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_interval = 500~1500`                        — Random interval in milliseconds between decoration spawns. A single value gives a fixed interval.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_count = 2~7`                                — Random number of decorations generated per spawn. A single value gives a fixed count.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_random_pos = 90`                            — XYZ position randomization around the center of the building sector.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_offset_xyz = 0|0|-40`                       — X|Y|Z offset from the center of the building sector. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_mode = persistent`                          — Persistent decoration mode: the decoration remains active while the building remains active and alive.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp = 99`                                    — Legacy VP decoration visual; final fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_base = Data/Models/Base/FX/DECORATION.BASE` — BASE decoration family, relative to Data; falls back to decoration_fx_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_3ds = Data/Models/3ds/FX/decoration.3ds`    — 3DS decoration model; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_scale_xyz = 1.0|1.0|1.0`                    — X|Y|Z scale of the active decoration visual. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_spin_xyz = 0|0|0`                           — X|Y|Z spin intensity for the decoration VP/3DS visual: 0-10, decimals allowed; 0 = no spin. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_tint = 255|95|70|200`                       — RGBA tint applied to the main decoration visual.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_fade_inout = 500|500`                       — Main VP/3DS decoration fade-in and fade-out in milliseconds (format in|out). 0 or absent = immediate appearance/disappearance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_tint = 255|255|255|255`            — RGBA tint of VP decoration particles/trails. Under additive blending, fading also attenuates visual intensity correctly.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_scale_xyz = 1.0|1.0|1.0`           — X|Y scale of VP decoration particles/trails, plus a Z multiplier for particle/trail duration and therefore apparent trail length (format x|y|z).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_vp_trail_fade_inout = 500|500`              — Time in milliseconds (format in|out) for VP decoration particles/trails to fade in to their configured opacity (in) and to fade out to invisible (out). 0 or absent = immediate.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `decoration_fx_offset_xyz = 0|0|-40`                       — X|Y|Z offset from the center of the building sector. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tint = 60|255|60|255`                                     — RGBA tint applied to the building's main visual.
------------------------------------------------------------------------------------------------------------------------

### Sounds

------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_normal_sample = sounds/power.wav` — Looping sound of the Building.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_normal_volume = 120`              — Volume of the Building sound.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `snd_normal_pitch = 0`                 — Pitch of the Building sound. Ranges with `min~max` are accepted.
------------------------------------------------------------------------------------------------------------------------

### OpenNeoUA special features

Building spawners let a map object create vehicles dynamically instead of requiring every unit to be placed directly in the level data. OpenNeoUA spawns inherit the source primary command, waypoint route and aggression. Building spawns use their owning Host Station. Spawned units reuse Genesis overlap separation, including instant spawns. A spawner requires a valid Vehicle ID; other settings alone cannot create units.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_vehicle = 5`           — Vehicle ID generated by this building.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_count = 3`             — Number of units generated at each spawner activation.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_interval = 6000`       — Interval in milliseconds between spawner activations.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_trigger_radius = 1800` — XZ radius within which enemy presence activates the spawner.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_random_pos = 340`      — XZ position randomization around the building center.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_offset_xz = 37|-41`    — X|Z offset applied to the center of the building spawn area. Format: x|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_height = 650~900`      — Random additional height above the real surface at which the unit is created. A single value gives a fixed height.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_instant = 1`           — 1 creates units instantly without the classic Genesis sequence.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_max_active = 9`        — Maximum number of units generated by this building that may remain active at the same time.
------------------------------------------------------------------------------------------------------------------------

## Buffs (Buffs.cfg)

Buffs are reusable top-level profiles. A Vehicle links to one with buff_id; the Buff definition itself is declared separately with new_buff [number]. new_buff may appear in any supported script file under Data/Scripts; Buffs.cfg is only the recommended organizational file. Do not place the new_buff block inside new_vehicle.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `new_buff 1`                                              — Starts reusable Buff profile ID 1. Multiple Vehicles may reference the same ID through buff_id.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `name = armored`                                          — Internal Buff name. This identifier does not change gameplay by itself; it labels the reusable Buff profile.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `icon = Graphics/StatusIcons/deflect.png`                — Data-relative Status Icon selected directly by this Vehicle's Buff. Empty/invalid = no Buff icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `glow_intensity = 0.20`                                   — Full-model Buff glow intensity, range 0..1, while at least one Buff property remains active. 0 or absent = no glow.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `glow_tint = 100|180|255|255`                             — RGBA tint of the Buff glow. Visual only: does not modify materials or gameplay values.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `glow_pulse_seconds = 1.25`                               — Duration in seconds of one complete Buff-glow pulse. 0 or absent = steady glow. Runtime maximum: 60 seconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `invisible = 1`                                           — Keeps the unit invisible until it performs its first real attack. On reveal, this Buff property ends permanently.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `invisible_reveal_vp = 229`                               — Legacy VP generated when invisible is revealed; final fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `invisible_reveal_base = Data/Models/Base/FX/REVEAL.BASE` — BASE reveal effect, relative to Data; falls back to invisible_reveal_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `invisible_reveal_3ds = Data/Models/3ds/FX/reveal.3ds`    — 3DS reveal effect; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `invulnerable = 1`                                        — Makes the unit immune to damage while this Buff property remains active.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `deflect_charges = 4`                                     — Deflect charges available when the unit is created. Each normal projectile within the supported threshold consumes one charge and is physically deflected; Deflect ends at 0 charges. MGUN hits are blocked without consuming charges.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `deflect_max_energy = 5000`                               — Maximum attack energy Deflect can stop. If a projectile or AOE exceeds this threshold, all remaining Deflect charges are destroyed and the attack passes through normally. 0 or absent = no energy limit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `deflect_end_energy_reduction = 50%`                      — One-hit damage reduction applied when an attack exceeds deflect_max_energy. The hit still destroys all Deflect charges and passes through, but that single damage event is reduced by the configured percentage.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `deflect_vp = 229`                                        — VP effect generated when Deflect physically redirects a projectile; final fallback after 3DS and BASE.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `deflect_base = Data/Models/Base/FX/DEFLECT.BASE`         — BASE effect generated when Deflect redirects a projectile; falls back to deflect_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `deflect_3ds = Data/Models/3ds/FX/deflect.3ds`            — 3DS effect generated when Deflect redirects a projectile; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end`                                                     — Ends the new_buff profile.
------------------------------------------------------------------------------------------------------------------------

## Debuffs (Debuffs.cfg)

Debuffs are reusable top-level profiles. A Weapon links to one with debuff_id; SuperItems can reference the same profile family through their own debuff_id. The Debuff definition itself is declared separately with new_debuff [number]. new_debuff may appear in any supported script file under Data/Scripts; Debuffs.cfg is only the recommended organizational file. Do not place the new_debuff block inside new_weapon. Host Stations remain protected from mind control and stun even when general Host Station Debuff application is enabled.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `new_debuff 1`                                 — Starts reusable Debuff profile ID 1. Multiple Weapons and SuperItems may reference the same ID through debuff_id.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `allow_on_host_station = 0`                    — Allows the debuff to affect Host Stations. Mind control and stun are still ignored on Host Stations.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `allow_on_children = 0`                        — Automatically propagates the active Debuff to child units generated directly by the affected unit. 0 or absent = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `icon = Graphics/StatusIcons/electrified.png` — Data-relative icon shown while the unit is debuffed. Empty/invalid = no icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `name = electrified`                           — Name of the debuff. debuff_id and blast_debuff_id can use it in place of the ID, so it must start with a letter.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mindcontrol = 1`                              — Converts the affected unit to the source faction. Does not apply to Host Stations; an inapplicable mind-control effect does not cancel the other Debuff effects.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `stun = 1`                                     — Enables unit stun during the debuff. Does not apply to Host Stations.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `stun_motion_level = 50%`                      — Strength of altered movement during stun: 0% = disabled, 100% = maximum.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `stun_unit_fire = 1`                           — 1 = AI and player fire continuously while stunned; 0 = they cannot fire while stunned.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `energy = 2%`                                  — Energy removed at each tick. With %, uses a percentage of target maximum HP; without %, uses a fixed amount.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tick_time = 2000`                             — Time in milliseconds between damage ticks.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `duration = 12000`                             — Total debuff duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `force_malus = -50%`                           — Force modifier during debuff. Example: -50% reduces force by 50%.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `maxrot_malus = -50%`                          — maxrot modifier during debuff.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shield_malus = -50%`                          — Shield modifier during debuff.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mgun_shot_time_malus = -40%`                  — Modifier to the affected unit's MGUN fire rate.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shot_time_malus = -40%`                       — Modifier to the affected unit's primary Weapon fire rate.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_pitch_malus = -50%`                       — Pitch modifier for sounds emitted by the affected unit. Negative lowers pitch, positive raises it. 0 or absent = unchanged.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp = 123|123|123|123|123|123|123|123`         — VP visuals generated by the debuff. Multiple VP IDs may be separated with |. If 3ds is configured, the 3DS visual is generated alongside the VPs.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds = Data/Models/3ds/electrified.3ds`        — Optional external 3DS visual generated alongside the debuff VPs, relative to Data. If missing/unloadable, configured VP visuals remain active.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tint = 0|255|0|255`                           — RGBA tint applied to debuff VP and 3DS visuals.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `scale = 1.0`                                  — Uniform scale applied to debuff VP and 3DS visuals. Visual only: no effect on debuff gameplay values.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `random_max_offset = 25%`                      — Maximum random visual offset from unit center. With %, effects spawn inside the unit silhouette (its `collision_shape` profile when present, otherwise the visual model, vanilla scripts included, without rotor blades and thin parts): 100% = whole body, 25% = central core only; without %, uses a fixed distance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_trail_tint = 0|255|0|255`                  — RGBA tint applied only to VP trails, particles and child effects belonging to the debuff.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `target_tint = 140|255|140|150`                — RGBA tint applied to the affected unit model; compatible with VP and 3DS mesh visuals.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_sample = sounds/zap.wav`                  — Debuff sound. Without tick_time, remains looped for the entire debuff duration.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_pitch = 0~500`                            — Debuff sound pitch. Single value = fixed; `min~max` = random pitch selected on each playback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_volume = 180`                             — Volume of snd_sample.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_radius = 3000`                            — Maximum debuff-sound radius. Volume decreases linearly to 0 at the limit. 0 or absent = legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_slot = 4`                                 — Palette-effect slot used by the debuff.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_mag0 = 5`                                 — Initial debuff palette intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_mag1 = 0`                                 — Final debuff palette intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_time = 1500`                              — Debuff palette-effect duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_radius = 3000`                            — Maximum debuff palette-effect radius. Intensity decreases linearly to 0 at the limit. 0 or absent = legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_slot = 1`                                 — Shake slot used by the debuff.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_mag0 = 2`                                 — Initial debuff-shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_mag1 = 0`                                 — Final debuff-shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_time = 1500`                              — Debuff-shake duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_radius = 3000`                            — Maximum debuff-shake radius. Intensity decreases linearly to 0 at the limit. 0 or absent = legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_mute = 0.02`                              — Legacy debuff-shake distance attenuation factor used only when shk_radius = 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end`                                          — Ends the new_debuff profile.
------------------------------------------------------------------------------------------------------------------------

## Clouds (Clouds.cfg)

Cloud profiles are reusable volumes of floating meshes defined by new_cloud `<id>.` Declare them in scripts loaded by Startup.cfg; Scripts/Clouds.cfg is the organizational file. Levels use cloud_id, SuperItems use fallout_cloud_id, and mobile Power Vehicles use power_cloud_id. Absent or unknown links do not activate a Cloud.

mesh_3ds is required and count must be positive; otherwise the profile is ignored. count is capped at 512; loop_sound_volume is clamped to 0-127. Reversed lifetimes are swapped; if neither lifetime is positive, both use 1000 ms. Duplicate IDs replace the earlier profile.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `new_cloud 1`                              — Reusable Cloud profile linked by its numeric ID.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `name = radiations`                        — Profile name; links use the numeric ID.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `loop_sound = sounds/ambient/spore.wav`    — Optional sound loop played while the atmospheric profile is active on screen. Empty/absent = no loop sound.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `loop_sound_volume = 70`                   — Volume of the atmospheric profile loop sound. <= 0 = muted/disabled. Runtime default: 100.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mesh_3ds = Data/Models/3ds/Radiation.3ds` — 3DS mesh spawned by this atmospheric profile.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `count = 135`                              — Number of 3DS meshes maintained within the configured player-centered spawn volume. Values above 512 are clamped to 512.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_radius_xyz = 1950|720|1950`         — Random X|Y|Z spawn radius around the player/viewer. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spawn_offset_xyz = 0|260|0`               — X|Y|Z offset of the spawn volume relative to the viewer. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `lifetime_minmax = 5000~9000`              — Minimum and maximum mesh lifetime in milliseconds (format `min~max`).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `velocity_xyz = 8|-18|5`                   — Base X|Y|Z mesh velocity. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `velocity_random_xyz = 40|22|40`           — Random X|Y|Z velocity variation. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `scale_xyz = 3.2|3.2|3.2`                  — X|Y|Z scale of spawned meshes. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spin_xyz = 0.20|0.55|0.25`                — Continuous X|Y|Z rotation rate/intensity. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fade_inout = 300|700`                     — Mesh fade-in and fade-out durations in milliseconds (format in|out).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tint = 170|255|145|165`                   — Mesh tint in R|G|B|A format.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end`
------------------------------------------------------------------------------------------------------------------------

## SuperItems (SuperItems.cfg)

SuperItem profiles are defined by new_superitem `<id>` in scripts loaded by Startup.cfg. Scripts/SuperItems.cfg organizes the definitions; the level links an item with superitem_id. Placement remains in the level, while the profile owns wave, damage, push and additional FX. The filename does not determine the link.  Example level-side item:

------------------------------------------------------------------------------------------------------------------------
- 🟢 `begin_item`
------------------------------------------------------------------------------------------------------------------------
- 🟢 `superitem_id = 1` — ID of the new_superitem profile linked by the level begin_item block. -1, absent or unknown ID = vanilla SuperItem.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `sec_x = 1`
- 🟢 `sec_y = 12`
- 🟢 `inactive_bp = 35`
- 🟢 `active_bp = 36`
- 🟢 `trigger_bp = 37`
- 🟢 `type = 1`
- 🟢 `countdown = 998000`
- 🟢 `end`
------------------------------------------------------------------------------------------------------------------------
- 🟢 `new_superitem 1`                                   — Reusable SuperItem profile linked by its numeric ID.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `name = stoudson_nuclear`                           — Profile and bomb display name; underscores are displayed as spaces. Linking uses the numeric ID.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fallout_cloud_id = 1`                              — ID of the Cloud activated after the SuperItem wave passes. -1, absent or unknown ID = no fallout Cloud.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_vp = 283`                                     — Legacy VP used for the expanding wave front; final fallback for wave_3ds/wave_base.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_base = Data/Models/Base/FX/NUCLEAR_WAVE.BASE` — External BASE family for the wave front, relative to Data; automatically falls back to wave_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_3ds = Data/Models/3ds/FX/nuclear_wave.3ds`    — 3DS model for the wave front, relative to Data; takes priority over wave_base and wave_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_scale_xyz = 1.0|1.0|1.0`                      — Base X|Y|Z scale multipliers for the VP/BASE/3DS wave visual (format x|y|z). X and Z grow automatically with the wave radius; the vertical Y does not. <= 0 or invalid = 1.0 for that axis.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_tint = 255|255|255|255`                       — RGBA tint applied to the wave visual for VP, BASE and 3DS.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_start_speed = 0`                              — Initial wave-front speed in world units per second. 0 allows the wave to start from rest.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_speed_ramp_time = 10000`                      — Linear transition time in milliseconds from wave_start_speed to wave_end_speed. 0 = use the final speed immediately.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_end_speed = 2000`                             — Final wave-front speed in world units per second. Must be positive.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_push_force = 6`                               — One-shot push intensity 0..10 applied to every unit reached by the wave. Uses the shared push curve: 100 * intensity^2 after push_resistance. Also affects allies and invulnerable units; static GUNs are excluded. The per-target one-shot state is saved.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_unit_energy = 50000`                          — Base energy removed from valid enemy units reached by the wave. Passes through the target's shield. 0 = no unit energy removed.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `wave_building_total_destruction = 15%`             — Percentage of intact buildings reached by the wave that are destroyed completely. Remaining buildings are damaged when possible.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `push_force = 10`                                   — Instantaneous push intensity 0..10 generated at the detonation center. Uses the same shared curve, 100 * intensity^2, after falloff and push_resistance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `push_radius = 2000`                                — XZ radius of the instantaneous detonation push. <= 0 disables push_force.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `push_falloff = 1`                                  — Detonation-push falloff: 1 reduces intensity with distance; 0 keeps the same intensity throughout push_radius.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fade_inout = 500|1000`                             — Fade-in and fade-out in milliseconds for the VP/BASE/3DS wave visual (format in|out). The fade-out happens when the wave reaches the map's natural limit. 0 or absent = immediate.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `debuff_id = 2`                                     — ID of the reusable new_debuff profile applied when the wave reaches a unit. -1, absent or an unknown ID = no Debuff. The profile may be defined in any supported script file under Data/Scripts; Debuffs.cfg is only the recommended organizational file.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_detonate_sample = sounds/new/detonate.wav`     — One-shot sound played at the SuperItem center when detonation begins.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_detonate_volume = 800`                         — Detonation sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_detonate_pitch = 0~500`                        — Detonation sound pitch. Single value = fixed; `min~max` = random pitch selected on each playback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_detonate_radius = 10000`                       — Maximum detonation-sound hearing radius. 0 = legacy attenuation behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_detonate_slot = 1`                             — Palette-effect slot activated at detonation. 0 = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_detonate_mag0 = 20`                            — Initial palette intensity for detonation.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_detonate_mag1 = 10`                            — Final palette intensity for detonation.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_detonate_time = 6000`                          — Detonation palette-effect duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_detonate_radius = 10000`                       — Maximum radius of the detonation palette effect.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_detonate_slot = 1`                             — Shake slot activated at detonation. 0 = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_detonate_mag0 = 1`                             — Initial detonation-shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_detonate_mag1 = 2`                             — Final detonation-shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_detonate_time = 6000`                          — Detonation-shake duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_detonate_radius = 10000`                       — Maximum radius of the detonation shake.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_detonate_mute = 0.02`                          — Legacy distance attenuation factor used only when shk_detonate_radius = 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_detonate_x = 0.2`                              — Random shake strength multiplier on the X axis.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_detonate_y = 0.2`                              — Random shake strength multiplier on the Y axis.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_detonate_z = 1.0`                              — Random shake strength multiplier on the Z axis.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_wave_sample = sounds/new/wave.wav`             — Looping sound attached to the wave front and positioned at the point on the front nearest the listener.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_wave_volume = 800`                             — Wave sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_wave_pitch = 0~500`                            — Wave sound pitch. Single value = fixed; `min~max` = random pitch selected on each playback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `snd_wave_radius = 2000`                            — Maximum hearing radius measured from the wave front. 0 = legacy attenuation behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_wave_slot = 1`                                 — Palette-effect slot activated when the wave front enters the listener radius. 0 = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_wave_mag0 = 10`                                — Initial palette intensity of the wave.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_wave_mag1 = 5`                                 — Final palette intensity of the wave.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_wave_time = 12000`                             — Wave palette-effect duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `pal_wave_radius = 2000`                            — Distance from the front within which the palette effect may activate. 0 or absent uses 2400 as the front activation threshold while preserving legacy radius behavior for the effect.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_wave_slot = 1`                                 — Shake slot activated when the wave front enters the listener radius. 0 = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_wave_mag0 = 0.1`                               — Initial wave-shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_wave_mag1 = 0`                                 — Final wave-shake intensity.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_wave_time = 12000`                             — Wave-shake duration in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_wave_radius = 2000`                            — Distance from the front within which wave shake may activate. 0 or absent uses 2400 as the front activation threshold while preserving legacy behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_wave_mute = 0.02`                              — Legacy distance attenuation factor used only when shk_wave_radius = 0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_wave_x = 0.2`                                  — Random wave-shake strength multiplier on the X axis.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_wave_y = 0.2`                                  — Random wave-shake strength multiplier on the Y axis.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `shk_wave_z = 1.0`                                  — Random wave-shake strength multiplier on the Z axis.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `begin_fx`                                          — Additional visual Chain FX executed once when detonation starts.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `trigger = detonate`                                — SuperItems currently support detonate as the Chain FX trigger.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mode = visual`                                     — SuperItems currently support visual as the Chain FX mode.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `random_rotation = 0`                               — 1 assigns a random X/Y/Z orientation once at spawn, independently per instance; 0, omitted or invalid = normal orientation. Spawn position and launch direction are unchanged.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `offset_xyz = 0|0|0`                                — X|Y|Z offset from the bomb's native center. Single value = fixed; `min~max` = random offset rolled at every activation. Format: x|y|z; each axis takes a single value or `min~max`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `spin_xyz = 0|0|10`                                 — X|Y|Z spin intensity of the Chain FX VP/BASE/3DS visuals: 0-10, decimals allowed; 0 = no spin. Applies to every model in the block. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `start_size = 0`                                    — Initial Chain FX scale.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `mid_size = 10`                                     — Optional intermediate scale. If absent/invalid, direct start_size -> end_size interpolation is used.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end_size = 20`                                     — Final Chain FX scale.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `duration = 6000`                                   — Total Chain FX duration in milliseconds. Must be positive.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fade_inout = 500|1000`                             — Visual fade-in and fade-out in milliseconds (format in|out). 0 or absent = immediate.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `vp_model = 254`                                    — Legacy VP visual for this Chain FX element. The line may be repeated to build a sequence.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `base_model = Data/Models/Base/FX/CHAIN_FX.BASE`    — Optional BASE family for the same element. Place it after vp_model to keep that VP as fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `3ds_model = Data/Models/3ds/FX/chain_fx.3ds`       — Optional 3DS model for the same element; takes priority over base_model and vp_model.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tint = 255|255|255|255`                            — RGBA tint for all models in the visual block, regardless of position. The last value wins; omitted tint preserves original colours.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end`
------------------------------------------------------------------------------------------------------------------------

## Ground Decals (Ground_Decals.cfg)

How decals are drawn: a decal is projected per pixel onto what is really drawn on screen, so it works the same on terrain, slopes, buildings and every LEGO piece, and never on units. It keeps its real proportions on every surface, continues from the ground up a nearby wall and over edges, and adapts by itself when a building changes state: what was on a part that no longer exists disappears with it, and nothing is left floating. A large decal that lands on a roof also marks the ground around the building when the ground is within reach. An invisible collision surface receives nothing: the decal lands on the real surface behind it when one is in reach. A thin wall does not stop a decal on flat ground, which can continue on the other side.

Define profiles in scripts loaded by Startup.cfg; the filename is organizational. Vehicles/Weapons use ground_decal_id; MGUN uses mgun_ground_decal_id. effect_mode is mandatory. Without a valid mode the effect emits nothing and the engine logs a warning: ground_pulse = short intermittent flashes around the decal (sparks, electric discharges). rise         = camera-facing puffs that climb away from the decal (smoke, radioactive particles). Any other value, including the former pulse and plume, logs a warning and disables the effect. The engine has no built-in effect model: an authored model is always required, otherwise nothing is emitted. ground_pulse: effect_3ds, effect_base and effect_vp select the model in that order; unresolved sources fall back to the next. rise: only effect_3ds is used (effect_base and effect_vp are ignored); a missing or unloadable 3DS emits nothing. Keys valid in both modes: effect_mode, effect_3ds, effect_active_time, effect_retrigger_interval, effect_chance, effect_spread, effect_tint. ground_pulse only: effect_base, effect_vp, effect_pulse_interval, effect_pulse_time, effect_scale. effect_count has no effect in this mode. rise only: effect_fade_inout, effect_count, effect_size, effect_growth, effect_rise_speed, effect_puff_time, effect_drift, effect_intensity. A key written for the other mode is ignored and the engine logs a warning. Invalid `effect_*` values use defaults; runtime bounds are applied. Reversed `min~max` ranges (e.g. `30~10`) are reordered by the engine (count, active_time, retrigger_interval, puff_time, pulse_interval). effect_pulse_interval = 900 controls the pause between flashes (200..60000 ms); a single value is fixed, `min~max` (e.g. `700~1500`) rolls a new pause for every flash. effect_pulse_time = 120 is how long each flash stays lit (20..1000 ms, at most the interval of that flash). effect_retrigger_interval = 0 means one activation only; a positive value (single value or `min~max`) restarts the whole effect every given interval; each activation lasts at most the interval and the effect repeats for the whole decal life. effect_scale = 0.1 (ground_pulse only) sets the flash size at decal radius 50. Rise has no scale key: the engine measures the width of the 3DS model itself, so the puff size is set only by effect_size. effect_active_time is how long the effect keeps emitting over the decal; the decal itself lasts for duration.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `new_decal 1`
------------------------------------------------------------------------------------------------------------------------
- 🟢 `name = destroyed`                               — Profile name; links use the numeric ID.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_mode = rise`                             — Mandatory: rise = climbing puffs; ground_pulse = flashes around the decal.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_3ds = Data/Models/3ds/Smoke.3ds`         — Puff model (3DS). See the notes above for effect_base/effect_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_scale = 0.08`                            — ground_pulse only. Flash model scale. Range: 0.001-10; default: 0.1. Not used in rise: size is set by effect_size.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `texture = Graphics/GroundDecals/impact_01.png` — Optional Data-relative texture; empty = procedural shape, unloadable file = decal discarded.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `stretch_xz = 0.8~1.2|0.8~1.2`                   — Local X|Z stretch (format x|z). Each axis takes a single value (fixed) or a random `min~max`. Default: 1|1; stretched axes rotate with the decal.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `tint = 255|255|255|180`                         — RGBA tint; alpha 0 rejects the profile.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `random_rotation = 1`                            — Only exact 1 enables random rotation; other values or absent disable it.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `edge_fade = 5~10`                               — Edge fade in range 0-10; single value or random whole-number `min~max`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `duration = permanent`                           — Required: permanent or positive milliseconds. Missing/invalid rejects the profile. Still subject to gfx.ground_decal_limit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fade_inout = 0|1000`                            — Decal fade-in and fade-out in milliseconds (format in|out); in 0 = immediate, out is ignored for permanent duration.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_active_time = 20000~25000`               — How long the effect keeps emitting, in milliseconds; single value or `min~max`. Default: `20000~25000`; maximum 3600000. Valid in both modes.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_retrigger_interval = 30000~45000`        — How often the whole effect starts again, in milliseconds; single value or `min~max`. 0 = one activation only (default). With an interval the effect repeats for the whole decal life; each activation lasts at most the interval. Maximum 3600000. Valid in both modes.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_fade_inout = 3000|20000`                 — Rise only. Smoke fade-in and fade-out in milliseconds (format in|out). Default: 3000|20000; in 0 = immediate appearance, out 0 = abrupt stop.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_count = 10~30`                           — Rise only. Concurrent puffs for radius 50, scaled with decal size and capped at 50. Single value or `min~max`; 0 = no smoke. Default: `10~30`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_size = 35~50`                            — Rise only. Initial puff width as a percentage of decal radius (the model is normalised by the engine, whatever the 3DS units); single value or `min~max`. Range: 0.5-200; default: `35~50`. Use `4~7` for tiny fragments (Speck.3ds).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_growth = 3.3`                            — Rise only. Final puff size divided by initial size. Range: 1-10; default: 3.3.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_rise_speed = 25`                         — Rise only. Rise as a percentage of radius per second. Range: 0-200; default: 25.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_puff_time = 4000~7500`                   — Rise only. Individual puff lifetime in milliseconds; single value or `min~max`. Range: 200-60000; default: `4000~7500`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_drift = 35`                              — Rise only. Sideways drift as a percentage of rise. Range: 0-300; default: 35.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_spread = 35`                             — Initial emission area as a percentage of radius. Range: 0-100; default: 35. Also used by ground_pulse.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_intensity = 120`                         — Rise only. Opacity percentage. Range: 0-300; default: 120.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_tint = 70|70|70|150`                     — Effect RGBA tint. Default: 70|70|70|150. Also used by ground_pulse.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_chance = 100`                            — Probability that a decal emits the effect, in percent; the roll is fixed per decal. Range: 0-100; default: 100; 0 = never.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end`
------------------------------------------------------------------------------------------------------------------------
- 🟢 `new_decal 2` — ground_pulse example: intermittent electric discharges, with no rise-only keys.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `name = mykonian_electric`
- 🟢 `texture = Graphics/GroundDecals/Mykonian_Electric_02.png`
- 🟢 `duration = permanent`
- 🟢 `effect_mode = ground_pulse`
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_vp = 2`                    — Set VP. Alternatively effect_3ds or effect_base.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_pulse_interval = 700~1500` — Pause between flashes in ms, fixed or `min~max`.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_pulse_time = 180`          — How long each flash stays lit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `effect_scale = 0.08`
- 🟢 `effect_active_time = 7000~11000`
- 🟢 `effect_spread = 30`
- 🟢 `effect_tint = 255|255|255|230`
- 🟢 `end`
------------------------------------------------------------------------------------------------------------------------
- 🟢 `new_decal 4` — Smoke-free MGUN example referenced by mgun_ground_decal_id = 4.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `name = mgun_hit`
- 🟢 `texture = Graphics/GroundDecals/impact_01.png`
- 🟢 `random_rotation = 1`
- 🟢 `edge_fade = 5~10`
- 🟢 `duration = 60000`
- 🟢 `fade_inout = 0|2000`
- 🟢 `end`
------------------------------------------------------------------------------------------------------------------------

## Startup and Game Config (Startup.cfg)

OpenNeoUA uses .cfg files as the modern script/Game Config layout while retaining the original .scr entrypoints only where vanilla levels or data reference them.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `Data/Scripts/Startup.cfg`              — Canonical OpenNeoUA script entrypoint. The engine prefers this file and it loads Buffs, Debuffs, Ground_Decals, Clouds, SuperItems, Weapons, Buildings and Vehicles. If it is absent, the engine falls back to the real vanilla Data/Scripts/Startup.scr.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Data/Scripts/Effects.cfg`              — Separate compatibility catalog for legacy Vehicle definitions that are actually physical effects. Modern inline Physical FX do not require Vehicle IDs.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Data/Scripts/Multiplayer.cfg`          — Standard multiplayer Game Config. Loads Startup.cfg and directly contains the definitions/overrides formerly split across Net_bldg.scr, Net_robo.scr and Netrflak.scr.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Data/Scripts/Multiplayer_Internet.cfg` — Internet multiplayer Game Config. Loads Startup.cfg and directly contains the definitions/overrides formerly split across Net_bldg.scr, Inetrobo.scr and Netrflak.scr.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Data/Scripts/Startup.scr`              — Vanilla compatibility entrypoint that forwards to Startup.cfg in the modern Data layout.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Data/Scripts/Startup2.scr`             — Vanilla level compatibility entrypoint. Retains its narrower per-level reset behavior instead of loading the complete Startup.cfg.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Data/Scripts/Net_ypa.scr`              — Vanilla multiplayer compatibility entrypoint that forwards to Multiplayer.cfg.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Data/Scripts/Inet_ypa.scr`             — Vanilla Internet multiplayer compatibility entrypoint that forwards to Multiplayer_Internet.cfg.
------------------------------------------------------------------------------------------------------------------------

## OpenNeoUA.ini

------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.new.debug = yes` — OpenNeoUA.ini is loaded as a custom configuration layer after Nucleus.ini. When the same key exists in both files, the OpenNeoUA.ini value takes priority. Settings managed by Advanced Graphics Settings are documented separately under Graphic Profiles. Enables the new debug controls (F1 through F12). When disabled, normal key behavior is preserved.
------------------------------------------------------------------------------------------------------------------------

**AI**

------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.ai_robo_player_behavior = yes`                           — Enables manual movement behavior for the player's Host Station instead of teleport-style movement. Also enables the optional flux and twist parameters for the player's Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.ai_target_acquisition_range = 2160`                      — Maximum automatic target-acquisition distance for AI. 0 = vanilla behavior. Positive values are also applied to the player-side acquisition path. Maximum: 2160.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.ai_mgun_fire_alignment = 0.80`                           — Global forward-alignment requirement for AI MGUN fire. Lower values are more permissive. Default: 0.85.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.ai_max_altitude_above_ground = 1800`                     — Maximum altitude above ground for AI-controlled airborne units. 0, absent or invalid = vanilla behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.ai_air_vertical_force = 1.5`                             — Global OpenNeoUA.ini setting for AI aircraft climb and descent force. 1 = vanilla; values above 1 strengthen vertical movement and let AI aircraft pursue airborne targets vertically. Applies to all AI aircraft without Vehicle entries. Maximum 5; absent, zero or invalid = 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.ai_bomber_extra_altitude = 100`                          — Global OpenNeoUA.ini setting for AI aircraft carrying bomb, homing_bomb or vertical_laser. They try to fly this far above an airborne target or building roof. Ground-unit targets are excluded. Applies to every eligible aircraft; maximum 3000. Absent, zero or invalid preserves vanilla behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.deformation_max_craters = 128`                                     — OpenNeoUA.ini: how many craters and dents (Weapons with crater_radius and crater_depth) are kept at once. Past the limit the oldest one goes and the ground or building there returns to its original shape. 0 disables craters; negative = 128; maximum 2000; `no_limit` = no limit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.deformation_ground_limit = 10` — Maximum terrain displacement: value × 100 world units. Decimal 0..10; 0 stops terrain movement. Missing/invalid retains the old behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.deformation_building_limit = 0.4` — Maximum total displacement of buildings and their foundations: 0.4 = 40 world units. Decimal 0..10; 0 stops building movement. Nearby ground joins the foundation limit over 150 units. Graphics and collision share the limit, including overlapping hits and loaded saves. Missing/invalid retains the old behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.briefing_text_render = no`                               — Briefing unit presentation: yes = show text instead of the unit visual; no/0 or absent = render the available VP/BASE/3DS model.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.robo_building_collision_energy = 5%`                     — Energy removed from each building the Host Station destroys in a collision. With %, uses a percentage of maximum HP; without %, uses a fixed amount. Shield reduces the energy loss. 0 or absent = no energy removed.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.robo_mobile_move_energy_cost_multiplier = 1.0`           — Multiplier for the mobile Host Station movement battery cost. Absent/invalid = 1.0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.robo_mobile_main_energy_cost_multiplier = 0.33`          — Multiplier for the mobile Host Station main-energy battery cost. Absent/invalid = 0.33.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.robo_mobile_build_energy_cost_multiplier = 0.33`         — Multiplier for the mobile Host Station build/life battery cost. Absent/invalid = 0.33.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.spectator_mode = yes`                                    — Enables spectator mode, starting without a faction and in free camera. The spectator vehicle is read from Data/Scripts/spectator_vehicle.cfg and should define a hidden, invulnerable UFO.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.weapon_weapon_collision = yes`                           — Enables collisions between hostile or neutral weapons/projectiles. Projectiles owned by the same faction do not collide with one another.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.recoil_kick_time = 0`                                    — Global time in milliseconds for recoil to reach maximum displacement. 0 = immediate kick. Range: 0-5000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.recoil_hold_time = 0`                                    — Global time in milliseconds to remain at maximum recoil before returning. 0 = no hold. Range: 0-5000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.recoil_return_time = 420`                                — Global time in milliseconds for recoil to return to neutral. Runtime default: 420. Range: 0-5000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.black_sect_units_tint = 140|140|140|125`                 — Black Sect unit tint in R|G|B|A format. Each component ranges from 0 to 255.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.powerstation_energy_multiplier = 2.0`                    — Multiplies energy recharge and drain at Power Stations, Host Stations and mobile power sources. 1.0 = vanilla behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.sprint_energy_cost = 1%`                                 — Continuous Sprint energy cost per second. With %, uses maximum energy; without %, uses a fixed cost. 0 or absent = no cost.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.sprint_energy_drain_interval_ms = 1000`                  — Interval in milliseconds between continuous Sprint drain applications. 0 = continuous/accumulated application.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.fixed_tick_tank_ground_pose_mult = 5.5`                  — Adjusts tank response to slopes using the always-active fixed timing path. Range: 2.0-30.0. Suggested value: 5.5.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.fall_energy = 2%`                                        — Energy removed by real fall/crash damage. With %, uses a percentage of maximum HP; without %, uses a fixed amount. Shield reduces the energy loss. Absent = vanilla behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.push_at_death_mult = 2.0`                                — Multiplies push and Blast_Push only when the hit kills the target. Range: 0..10; final effective intensity remains capped at 10. Absent or 1.0 = no extra push. Does not increase damage.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.handbrake_power = 1.0`                                   — Handbrake strength multiplier. Affects braking behavior and reduces recoil/spread while the handbrake system is engaged. Runtime default: 1.0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.handbrake_sound = sounds/new/handbrake.wav`              — Sound played when the handbrake is activated.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.player_tank_brake_time = 1500`                           — Time in milliseconds for a player tank to come to a natural stop after releasing acceleration. <= 0 or absent = vanilla behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.gem_unlock_duration = 2000`                              — GEM popup duration in milliseconds. Default: 10000 (vanilla). 0 = hide the popup. Does not slow game time.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.gem_unlock_sound`                                        — Optional sound played when a GEM is unlocked. Set a licensed sound path starting with Data/. Empty = no sound (as in the original game).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.gem_unlock_slow_motion = yes`                            — When yes, gameplay time slows down (same scale as the debug slow motion, 80% slower) while the GEM popup is on screen; the on-screen time stays game.gem_unlock_duration. Default: no = no slowdown (as in the original game). Ignored in network games.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.gem_unlock_popup_y = -0.04`                              — Vertical anchor of the GEM popup: -1 = top edge, 0 = screen center, 1 = bottom edge (the vehicle panel extends above and below this point). Default: -0.5 = original game position; -0.04 = almost centered. Invalid values keep the default.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.mgun_range = 1500`                                       — Restored cut-content behavior: the flak uses MGUN as its primary weapon. Effective range follows game.mgun_range.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.sprint_force_up = 60%`                                   — MGUNs retain vanilla sector behavior: AI sector attack/capture orders use the primary weapon and stop MGUN fire. MGUN impacts do not damage buildings or sectors, including manual fire; unit damage remains active. Maximum force increase during Sprint. With %, increases the unit's original force proportionally; without %, adds a fixed amount.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.sprint_pitch_up = 20%`                                   — Maximum engine-pitch increase during Sprint. This value must be written as a percentage.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.sprint_ramp_time = 3000`                                 — Ramp-up and ramp-down duration of Sprint in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.player_max_altitude_above_ground = 1600`                 — Maximum altitude above ground for player-controlled airborne units. 1600.0 = vanilla behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.unit_enemy_collision_energy = 5%`                        — Energy removed from enemy units in a collision. With %, uses a percentage of target maximum HP; without %, uses a fixed amount. Shield reduces the energy loss. 0 or absent = no energy removed.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.unit_friendly_collision_energy = 1%`                     — Energy removed from allied units in a collision. With %, uses a percentage of target maximum HP; without %, uses a fixed amount. Shield reduces the energy loss. 0 or absent = no energy removed.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.crash_diagnostics = yes`                                 — Enables persistent crash diagnostics from startup, independently of game.new.debug. yes = enabled, no = disabled; restart required. Missing or invalid values default to no, including builds made with diagnostics locally enabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.elite_unit_kills_per_medal = 5`                          — Each launch creates Data/Env/Diagnostics/`<date_time_pid>`/ with SessionInfo.txt, copies of the two INI files, Events.log and LatestState.txt (updated approximately once per second). The event journal includes engine logs, mission transitions, unit creation/deletion and deaths. Events.log and Events.previous.log retain up to approximately 16 MiB each; buffer overruns are reported explicitly. LatestState.txt includes the engine phase, level, frame progress, unit count, active unit/prototype/position and process memory usage. These are observations, not a full save or a deterministic replay. On Windows, unhandled exceptions, abort and std::terminate produce CrashReport.txt and attempt CrashDump.dmp through a separate process, with a 15-second timeout and the writer result in the report. A gameplay heartbeat stall exceeding 2.5 seconds produces `HangReport_*.txt.` `Checkpoint_*.txt` records requested snapshots; SessionEnd.txt records an orderly diagnostic shutdown. Forced process termination, power loss or severe process corruption may prevent a crash report/dump. Periodic files retain only information already written; absence of SessionEnd.txt does not establish the cause. No diagnostic mode detects every bug. Logging and dumps add overhead and consume disk space; session folders are retained until manually removed. Keep the matching executable for later source-line analysis. Additional kills required per medal. A Host Station kill immediately awards the maximum. Range 1..100000; missing or invalid = 5. Medal progress is transient and single-player only.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.elite_unit_max_stat_bonus = 40%`                         — Maximum Elite stat bonus reached at full medals. Applied to force, maxrot, shield, outgoing damage and rate of fire, distributed linearly across the configured medal count. Requires an explicit %.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.elite_unit_max_medals = 4`                               — Maximum Elite medals a unit may earn during the session. Each medal requires the configured number of eligible kills; destroying a Host Station immediately awards the maximum. Runtime range 1-255; absent/0/invalid = 4. UFO and Host Station classes do not use Elite medals.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.elite_unit_max_glow_intensity = 1.0`                     — Maximum Elite glow intensity at full medals. Scales linearly with medal progress and reuses the glow_intensity rendering path. Range 0-1; 0/absent disables Elite glow.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.elite_unit_glow_tint = 0|150|255|255`                    — R|G|B|A tint used by the Elite glow, reusing buff_glow_tint rendering semantics. Components are clamped to 0-255.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.elite_unit_glow_pulse_seconds = 2.0`                     — Seconds per complete Elite glow pulse, reusing glow_pulse_seconds timing. 0/absent = steady glow; maximum 60 seconds.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.robo_death_time_scale = 0.3`                             — Example time-scale value used when a Host Station is destroyed. Runtime default: 1.0. Suggested range: 0.05-1.0; 1.0 disables the slow-motion effect.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.robo_death_time_scale_duration = 2000`                   — Duration in milliseconds of the Host Station destruction slow-motion effect. 0 or absent = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.robo_death_time_scale_max_distance = 0`                  — Maximum XZ distance from the player at which a Host Station death can trigger slow motion. 0 = unlimited distance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.play_as_other_factions = yes`                            — Also exposed in the game options. Allows other factions to be selected and played from the briefing screen.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.plasma_currency_enable = 1`                              — Enables the global Plasma reserve in single-player. Absent/no/0 = disabled and vanilla behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.plasma_currency_gain = 0.1%`                             — Plasma added to the global reserve when collected. With %, grants a fraction of the collected Plasma; without %, adds a fixed amount.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.plasma_death_duration_mult = 10`                         — Multiplies how long Plasma dropped by dead units remains in single-player. 1.0/absent/0/negative/invalid = vanilla duration. Netgame always uses vanilla behavior.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.plasma_death_magnet_radius = 900`                        — 3D attraction radius. The nearest eligible collector wins, including physical pickup. The controlled unit can heal; player-owned AI collects enemy residue only for currency, even at full health. Friendly residue heals only the wounded controlled unit and never awards currency. Single-player only; 0/absent/invalid disables attraction.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.plasma_death_magnet_speed = 600`                         — Speed at which dropped Plasma moves toward its nearest eligible collector. Applies to the player-controlled unit and friendly AI units. 0/absent/invalid = disabled; also requires a positive magnet radius.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.plasma_snd_pickup_sample = sounds/pickup.wav`            — Sound played by the unit that collects Plasma, including the player.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.plasma_snd_pickup_volume = 90`                           — Plasma pickup sound volume.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.plasma_snd_pickup_pitch = 0`                             — Plasma pickup sound pitch.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.global_shk_fade_inout = 50|500`                          — Global fade-in and fade-out in milliseconds for SHK shake effects (format in|out). The fade-out applies only to finite-duration SHK effects.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.global_pal_fade_inout = 50|500`                          — Global fade-in and fade-out in milliseconds for PAL palette effects (format in|out). The fade-out applies only to finite-duration PAL effects.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.global_shk_falloff_power = 1`                            — Curve of the distance attenuation of SHK shake effects that author a radius (`shk_*_radius`). 1 = linear, same as before; below 1 the shake stays stronger at a distance and still reaches 0 at the radius; above 1 it fades sooner. Range 0.1-4; missing or invalid = 1. Effects without a radius keep the legacy attenuation.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.global_pal_falloff_power = 1`                            — Same curve for PAL palette effects that author a radius (`pal_*_radius`). Range 0.1-4; missing or invalid = 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.global_snd_falloff_power = 1`                            — Same curve for sounds that author a radius (`snd_*_radius`). Range 0.1-4; missing or invalid = 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.ambient_sound = sounds/ambient/wind.wav`                 — Configurable global level ambience. If the path identifies a folder rather than a specific file, OpenNeoUA can select a sound from that folder for the level and loop it.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.ambient_sound_volume = 50`                               — Global ambient volume. Runtime default: 100. Slider range: 0-127; 0 = muted.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.cockpit_camera = no`                                     — Enables OpenNeoUA cockpit camera mode in gameplay. yes = enabled; no or absent = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.blending = 1`                                             — Graphics blending mode: 0 = default, 1 = additive, 2 = sharp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.vsync = 2`                                                — Vertical synchronization: 0 = disabled, 1 = enabled, 2 = adaptive with fallback to mode 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.color_effects = 1`                                        — Enables the framebuffer used by post-processing color effects. 0 disables post-processing effects.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.maxfps = 60`                                              — FPS limit. Safe default: 60. The FPS/Hz menu can expose values up to 240.
------------------------------------------------------------------------------------------------------------------------
Graphics chip: on a computer with two of them the game asks the NVIDIA or AMD driver for the fast one by itself, so no setting is needed. The log file Data/Env/ypa_log.txt names the chip that really got the window ("Graphics chip in use"), and warns when it is a built-in one, which can cost most of the frame rate. On Linux the game asks the NVIDIA library for the card by itself, in the same way the prime-run command does, and falls back to the default one if no window can be made with it. If the warning appears on Windows, pick the graphics card for OpenNeoUA.exe in Settings, System, Display, Graphics; on Linux with an AMD or Intel card, start the game with DRI_PRIME=1.

- 🟢 `gfx.ground_decal_limit = 256`                                 — Maximum number of active Ground Decals. 0 disables them; negative/invalid = 256; values above 10000 are clamped to 10000; `no_limit` = no limit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.ground_decal_effect_limit = 256`                          — Maximum number of simultaneous decal effects, configured with the `effect_*` profile keys. When exceeded, the oldest stops first. 0 disables the effects. The effective maximum is gfx.ground_decal_limit, also when either key uses `no_limit`; `no_limit` = no limit. Default: 256.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.sky_render = yes`                                         — Enables or disables sky rendering. yes = enabled, no = disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_fog_enable = yes`                                 — Enables transparent horizon fog independently from horizon darkening.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_fog_color = 150|155|160`                          — Transparent horizon-fog color in R|G|B format. 150|155|160 is a light gray.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_dark_enable = yes`                                — Enables horizon darkening independently from transparent fog.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_dark_color = 0|0|0`                               — Horizon-darkening color in R|G|B format. 0|0|0 is pure black.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.vhs_filter_shader = res/ua_cinematic_1998_vhs.ps`         — Shader path for the VHS filter. Missing/unloadable = filter disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.vhs_filter_shader_vbo = res/ua_cinematic_1998_vhs_vbo.ps` — VBO shader path for the VHS filter. Missing/unloadable = filter disabled.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.atmosphere_fx = yes`                                      — Enables the world Atmosphere pass. no = vanilla rendering; missing shader = vanilla fallback.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.hide_map_border_walls = yes`                              — Allows the blue map-border walls to be hidden.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.world_new_hp_bar_enable = no`                             — yes enables the configurable mesh-based world HP bar.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.world_new_hp_bar_full_tint = 0|217|81|255`                — RGBA tint and opacity of the full-health portion of the bar.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.world_new_hp_bar_low_tint = 255|0|0|255`                  — RGBA tint toward which the HP bar gradually shifts as the vehicle takes damage.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.world_new_hp_bar_empty_tint = 255|0|0|0`                  — RGBA tint and opacity of the empty portion of the HP bar.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.world_new_shield_bar_enable = no`                         — yes enables the world shield bar drawn under the unit HP bar (thinner and shorter than it). While active it replaces the classic world shield row; the cockpit shield display is not affected.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.world_new_shield_bar_tint = 0|137|170|255`                — RGBA tint and opacity of the shield bar fill. Defaults to the same teal as the cockpit shield squares. The missing portion is drawn as a semi-transparent track of the same color.
------------------------------------------------------------------------------------------------------------------------

Regen and Drain feedback can use external VP/BASE/3DS visuals. When no custom visual is available, OpenNeoUA can fall back to the procedural + / - symbols. These effects are presentation-only; the gameplay behavior is defined by the corresponding Regen/Drain system on the unit.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_vp = 0`                                             — Legacy VP for the Regen effect; final fallback before the procedural + symbol.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_base = Data/Models/Base/FX/REGEN.BASE`              — BASE family for the Regen effect, relative to Data; automatically falls back to gfx.regen_fx_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_3ds = Data/Models/3ds/FX/regen.3ds`                 — 3DS model for the Regen effect, relative to Data; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_scale = 1.0`                                        — Uniform X/Y/Z scale of the Regen VP/BASE/3DS visual. <= 0 or invalid = 1.0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_spin_xyz = 0|0|0`                                   — X|Y|Z spin intensity for the Regen VP/BASE/3DS visual: 0-10, decimals allowed; 0 = no spin. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_tint = 80|255|120|230`                              — RGBA tint of the Regen effect, applied to VP/BASE/3DS or to the procedural + symbol.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_duration = 1000`                                    — Lifetime in milliseconds of each Regen visual effect.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_interval = 300~600`                                 — Random interval range in milliseconds between Regen-effect spawns. A single value gives a fixed interval.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_count = 2~4`                                        — Random number of Regen effects created per trigger. A single value gives a fixed count. Runtime maximum: 32.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_fx_random_max_offset = 80%`                            — Maximum random offset from the unit center. With %, effects spawn inside the unit silhouette (its `collision_shape` profile when present, otherwise the visual model, vanilla scripts included, without rotor blades and thin parts): 100% = whole body, 25% = central core only; without %, uses a fixed world-space distance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_mesh_size = 30`                                        — Size of the procedural mesh-generated + symbols.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_mesh_thickness = 5`                                    — Bar thickness of the procedural + symbols.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_mesh_rise_speed = 100`                                 — Upward movement speed of the procedural + symbols.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.regen_mesh_fade_inout = 150|300`                             — Fade-in and fade-out durations in milliseconds for procedural + symbols (format in|out). 0 = immediate appearance/disappearance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_vp = 0`                                             — Legacy VP for the Drain effect; final fallback before the procedural - symbol.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_base = Data/Models/Base/FX/DRAIN.BASE`              — BASE family for the Drain effect, relative to Data; automatically falls back to gfx.drain_fx_vp.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_3ds = Data/Models/3ds/FX/drain.3ds`                 — 3DS model for the Drain effect, relative to Data; takes priority over BASE and VP.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_scale = 1.0`                                        — Uniform X/Y/Z scale of the Drain VP/BASE/3DS visual. <= 0 or invalid = 1.0.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_spin_xyz = 0|0|0`                                   — X|Y|Z spin intensity for the Drain VP/BASE/3DS visual: 0-10, decimals allowed; 0 = no spin. Format: x|y|z.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_tint = 255|80|80|230`                               — RGBA tint of the Drain effect, applied to VP/BASE/3DS or to the procedural - symbol.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_duration = 1000`                                    — Lifetime in milliseconds of each Drain visual effect.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_interval = 300~600`                                 — Random interval range in milliseconds between Drain-effect spawns. A single value gives a fixed interval.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_count = 2~4`                                        — Random number of Drain effects created per trigger. A single value gives a fixed count. Runtime maximum: 32.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_fx_random_max_offset = 80%`                            — Maximum random offset from the unit center. With %, effects spawn inside the unit silhouette (its `collision_shape` profile when present, otherwise the visual model, vanilla scripts included, without rotor blades and thin parts): 100% = whole body, 25% = central core only; without %, uses a fixed world-space distance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_mesh_size = 30`                                        — Size of the procedural mesh-generated - symbols.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_mesh_thickness = 5`                                    — Bar thickness of the procedural - symbols.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_mesh_rise_speed = 100`                                 — Upward movement speed of the procedural - symbols.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.drain_mesh_fade_inout = 150|300`                             — Fade-in and fade-out durations in milliseconds for procedural - symbols (format in|out). 0 = immediate appearance/disappearance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.menu_font = Xolonium_Regular`                                 — Selects the in-game UI font.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.retro_interface = yes`                                        — Enables the rougher retro-style interface presentation.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.retro_cursors = yes`                                          — Mouse cursors: yes = the alpha/beta set, no = the release set. The Windows executable uses embedded cursors when built with local cursor sources; loose Data/Res/Retro or Data/Res files remain compatible fallbacks. Default: no (vanilla). Also available as the Retro Cursors checkbox in the Options menu, which saves this key.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.status_icon_blink_count = 3`                                  — Number of complete on/off blink cycles used when dynamic Status Icons appear/disappear. 0 = immediate appearance; runtime range 0..10. Plasma popup behavior is handled separately.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.wireframe_tints_owner_0 = 0`                                  — Global vehicle, Weapon and MGUN wireframe colors. Configure in OpenNeoUA.ini. These global per-owner RGBA tints color only the in-game vehicle, projectile and MGUN wireframe silhouettes in the info panel; they do not color aiming reticles, lock markers, visor/aim overlays, text or notifications. Value 0 keeps the original colors; ui.wireframe_tints_owner_01 is equivalent to ui.wireframe_tints_owner_1. Owner 0 RGBA tint, e.g. 50|150|255|255. 0 = default.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.wireframe_tints_owner_1 = 0`                                  — Owner 1 RGBA tint, e.g. 50|150|255|255. 0 = default.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.wireframe_tints_owner_2 = 0`                                  — Owner 2 RGBA tint, e.g. 50|150|255|255. 0 = default.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.wireframe_tints_owner_3 = 0`                                  — Owner 3 RGBA tint, e.g. 50|150|255|255. 0 = default.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.wireframe_tints_owner_4 = 0`                                  — Owner 4 RGBA tint, e.g. 50|150|255|255. 0 = default.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.wireframe_tints_owner_5 = 0`                                  — Owner 5 RGBA tint, e.g. 50|150|255|255. 0 = default.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.wireframe_tints_owner_6 = 0`                                  — Owner 6 RGBA tint, e.g. 50|150|255|255. 0 = default.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.wireframe_tints_owner_7 = 0`                                  — Owner 7 RGBA tint, e.g. 50|150|255|255. 0 = default.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.status_icon_regen = Graphics/StatusIcons/regen.png`          — Data-relative Regen icon path. Empty/invalid = disables only this icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.status_icon_drain = Graphics/StatusIcons/drain.png`          — Data-relative Drain icon path. Empty/invalid = disables only this icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.status_icon_damaged = Graphics/StatusIcons/damaged.png`      — Data-relative damaged-state icon path. Empty/invalid = disables only this icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.status_icon_spawn = Graphics/StatusIcons/mobile_spawner.png` — Data-relative Mobile Spawner icon path. Empty/invalid = disables only this icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.status_icon_radar = Graphics/StatusIcons/mobile_radar.png`   — Data-relative Mobile Radar icon path. Empty/invalid = disables only this icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.status_icon_power = Graphics/StatusIcons/mobile_power.png`   — Data-relative Mobile Power icon path. Empty/invalid = disables only this icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.status_icon_sprint = Graphics/StatusIcons/sprint.png`        — Data-relative Sprint icon path. Empty/invalid = disables only this icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.status_icon_handbrake = Graphics/StatusIcons/handbrake.png`  — Data-relative Handbrake icon path. Empty/invalid = disables only this icon.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.map_marker_sound = sounds/alarm.wav`                          — Sound played once after a map marker is created successfully. Empty/unloadable = no sound.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.genesis_list_x = 0`                                           — X position of the Genesis/Creation list. 0 = attached to the left edge; absent/invalid = legacy position.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.genesis_list_width = 180`                                     — Width of the Genesis/Creation list. Absent/invalid/<= 0 = legacy width.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.genesis_list_height = 220`                                    — Height of the Genesis/Creation list. Absent/invalid/<= 0 = legacy height.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.genesis_list_opacity = 220`                                   — Genesis/Creation list background opacity. 0 = fully transparent, 255 = fully opaque. Icons, text and numbers are unaffected. Absent/invalid = 220.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.genesis_list_order = energy`                                  — Genesis/Creation list sort order: classic = vanilla type groups followed by final cost, energy = sort only by final Genesis Energy cost. The Options menu exposes Classic/Energy and saves this value in OpenNeoUA.ini. price is not supported; missing/unknown = classic.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.map_opacity = 70`                                             — Map background opacity. 0 = fully transparent, 255 = fully opaque. The top bar, icons, grid and markers are unaffected. Absent/invalid = 70.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.squadron_manager_opacity = 180`                               — Squadron Manager background opacity. 0 = fully transparent, 255 = fully opaque. The top bar, icons, text and numbers are unaffected. Absent/invalid = 180.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `ui.weapon_name = no`                                             — Shows the Weapon name in the vehicle Wireless interface. yes/1 = visible; no/0 or absent = hidden, preserving the vanilla HUD.
------------------------------------------------------------------------------------------------------------------------

## Nucleus.ini

Base configuration file of the engine. OpenNeoUA.ini is loaded after it and wins on conflicts. The input.slider / input.button / input.hotkey keys work exactly like the ones in USER.TXT.

### Graphics

------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.dither = on`                       — Texture dithering.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.filter = on`                       — Linear texture filtering (smooth).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.antialias = on`                    — Antialiasing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.zbuf_when_tracy = no`              — Also use the depth buffer for transparent objects.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.colorkey = no`                     — Color-key transparency for textures.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.movie_player = yes`                — yes plays the movies. no skips them.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.disable_lowres = no`               — yes hides the low resolutions.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.export_window_mode = no`           — Window-mode export option.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.solidfont = off`                   — off draws text with soft edges; on uses a solid font.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.custommodes = 1920x1080,2560x1440` — Extra resolutions to add to the list, separated by commas.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.palette = standard.pal`            — Color palette loaded at startup.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfx.vbo = on`                          — on renders with vertex buffers (faster).
------------------------------------------------------------------------------------------------------------------------

### Drivers and leftovers of the original engine

------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.timer = wintimer` — Name of the timer driver. Keep the original value.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.wimp = winp`      — Name of the windowing / mouse driver. Keep the original value.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.keyboard = winp`  — Name of the keyboard driver. Keep the original value.
------------------------------------------------------------------------------------------------------------------------

### Audio

------------------------------------------------------------------------------------------------------------------------
- 🔵 `audio.channels = 64`   — Number of sound channels mixed together (minimum 1).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `audio.volume = 127`    — Initial master volume (0 to 127).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `audio.num_palfx = 4`   — Maximum number of palette flash effects active together, 8 at most.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `audio.rev_stereo = no` — yes swaps the left and right stereo channels.
------------------------------------------------------------------------------------------------------------------------

### Network and multiplayer

------------------------------------------------------------------------------------------------------------------------
- 🔵 `net.versioncheck = on`     — on requires network players to run the same version.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `net.waitstart = 150000`    — Multiplayer: maximum wait in milliseconds before the match starts.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `net.kickoff = 20000`       — Multiplayer: milliseconds of silence after which a player is kicked.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `netgame.exclusivegem = on` — Multiplayer: when a player takes a gem, the upgrade is broadcast to everyone.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `net.gmode = 0`             — Delivery mode for guaranteed network messages.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `game.debug = off`          — on writes network debug information to the log.
------------------------------------------------------------------------------------------------------------------------

### Game and menu

------------------------------------------------------------------------------------------------------------------------
- 🔵 `game.newai = on`        — on enables the newer Host Station AI for construction timing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `game.timeline = 600000` — Time scale of the Host Station AI in milliseconds: higher values make it build more slowly.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `menu.windowed = off`    — on opens the menu in a window.
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
- 🟢 `gfx.color_eff[New_Slot_Number] = R|G|B` — Defines a custom palette slot (slots 8..16). Multiple custom slots may be declared in OpenNeoUA.ini and referenced by vehicle/Weapon PAL parameters. R|G|B components use 0-255. Example: gfx.color_eff[9] = 255|102|7 ; Orange. Existing vanilla slots remain unchanged.
------------------------------------------------------------------------------------------------------------------------

## World.ini

### General settings (begin_misc)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_misc`                  — Opens the general settings block of World.ini. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `one_game_res = no`           — yes makes the menu and the game use the same resolution.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shell_default_res = 640_480` — Default resolution of the menu shell. For high resolutions also change size_x and size_y in begin_bg.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `game_default_res = 640_480`  — Default resolution of the game.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `max_impulse = 10000`         — Upper limit of the impulse (push) that an explosion can give to a unit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `beam_energy_start = 500`     — Beam energy capacity at the start of a level.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `beam_energy_add = 50`        — Beam energy capacity gained when the player destroys an enemy Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `unit_limit = 800`            — Default unit limit for every level; a level can override it.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `unit_limit_type = 1`         — Limit type: 0 hard block, 1 power-station penalty, 2 more expensive units.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `unit_limit_arg = 5`          — Extra argument of the limit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `easy_cheat_keys = 0`         — 1 lets the cheat keys (keypad * and /) work without holding the right mouse button.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `multi_building = yes`        — yes allows several buildings in the same sector.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hidden_fractions = 0`        — Factions hidden by default, as a bit mask.
------------------------------------------------------------------------------------------------------------------------

### Interface colors (begin_colors)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_colors`                              — Opens the interface color block. Every color is four numbers R_G_B_A (alpha is the intensity). Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner_0 = 000_000_000_6`                   — Color of faction 0 (nobody).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner_1 = 000_110_194_15`                  — Color of faction 1 (Resistance).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner_2 = 000_179_066_37`                  — Color of faction 2 (Sulgogar).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner_3 = 232_232_232_207`                 — Color of faction 3 (Mykonian).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner_4 = 255_171_028_11`                  — Color of faction 4 (Taerkasten).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner_5 = 073_073_073_5`                   — Color of faction 5 (Black Sect).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner_6 = 255_000_000_10`                  — Color of faction 6 (Ghorkov).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner_7 = 000_000_000_0`                   — Color of faction 7 (undefined).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `map_direction = 255_255_255_1`             — Map: color of the direction arrow.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `map_primtarget = 255_171_0_11`             — Map: color of the primary target.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `map_sectarget = 255_000_000_10`            — Map: color of the secondary target.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `map_commander = 000_167_200_15`            — Map: color of the commander marker.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `map_dragbox = 000_150_250_15`              — Map: color of the drag selection box.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `map_viewer = 255_255_255_1`                — Map: color of the viewer cone.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_weapon = 080_160_160_1`                — HUD: color of the weapon indicator.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_weapon_1 = 235_245_255_1`              — HUD: weapon indicator color, alternate state (_1).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_vehicle = 080_160_160_1`               — HUD: color of the vehicle indicator.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_vehicle_1 = 235_245_255_1`             — HUD: vehicle indicator, alternate state.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_compass_commandvec = 000_137_170_15`   — HUD compass: command vector.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_compass_commandvec_1 = 000_147_180_15` — HUD compass: command vector, alternate state.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_compass_primtarget = 255_171_000_11`   — HUD compass: primary target.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_compass_primtarget_1 = 245_161_000_11` — HUD compass: primary target, alternate state.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_compass_locktarget = 255_000_000_10`   — HUD compass: locked target.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_compass_locktarget_1 = 245_000_000_10` — HUD compass: locked target, alternate state.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_compass_compass = 060_130_140_1`       — HUD compass: the compass itself.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_compass_compass_1 = 235_245_255_1`     — HUD compass: the compass, alternate state.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_visor_mg = 255_255_255_1`              — HUD visor: machine gun.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_visor_mg_1 = 100_180_180_1`            — HUD visor: machine gun, alternate state.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_visor_locked = 255_000_000_10`         — HUD visor: locked target.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_visor_locked_1 = 255_100_000_10`       — HUD visor: locked, alternate state.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_visor_autonom = 100_180_180_1`         — HUD visor: autonomous mode.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hud_visor_autonom_1 = 255_255_255_1`       — HUD visor: autonomous, alternate state.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `brief_lines = 125_200_255_103`             — Briefing: color of the lines.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `text_default = 010_150_150_1`              — Text: default color.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `text_list = 020_160_160_1`                 — Text: lists.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `text_list_sel = 255_255_255_1`             — Text: selected list entry.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `text_tooltip = 150_200_200_1`              — Text: tooltips.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `text_message = 150_200_200_1`              — Text: messages.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `text_hud = 070_150_150_1`                  — Text: HUD.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `text_briefing = 065_123_134_103`           — Text: briefing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `text_debriefing = 065_123_134_103`         — Text: debriefing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `text_button = 180_221_221_50`              — Text: buttons.
------------------------------------------------------------------------------------------------------------------------

### Menu and map backgrounds (begin_bg)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_bg`                         — Opens the menu and map background images. Closed by end. One block can be written per set.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `size_x = 640`                     — Width of the images. For high resolutions use the real size, for example 1920. All images must share the same size.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `size_y = 480`                     — Height of the images.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `background_map = bg/normal.png`   — Background of the mission selection map.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `rollover_map = bg/white.png`      — Image shown while the mouse hovers over a mission.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `finished_map = bg/done.png`       — Image of completed missions.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `enabled_map = bg/en.png`          — Image of selectable missions.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `mask_map = bg/maske.png`          — Mask image: the color index of every area selects which level it opens (index 1 opens L0101). The background must be index 0.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `tut_background_map = bg/tut1.png` — Same as background_map, for the tutorial map.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `tut_rollover_map = bg/tut2.png`   — Same as rollover_map, for the tutorial.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `tut_mask_map = bg/tutmask.png`    — Same as mask_map, for the tutorial.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `menu_map = bg/startup.png`        — Background of the main menu.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input_map = bg/settings.png`      — Background of the input menu.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `settings_map = bg/settings.png`   — Background of the settings menu.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `network_map = bg/settings.png`    — Background of the network menu.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `locale_map = bg/settings.png`     — Background of the language menu.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `save_map = bg/settings.png`       — Background of the save menu.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `about_map = bg/settings.png`      — Background of the about screen.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `help_map = bg/settings.png`       — Background of the help screen.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `brief_map = bg/brief.png`         — Background of the briefing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `debrief_map = bg/debrief.png`     — Background of the debriefing.
------------------------------------------------------------------------------------------------------------------------

### Movies (begin_movies)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_movies`                — Opens the movie list. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `game_intro = mov:intro.mpg`  — Intro movie of the game.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `win_extro = mov:win.mpg`     — Ending movie when the game is won.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `lose_extro = mov:lose.mpg`   — Movie shown when the game is lost.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `user_intro = mov:user.mpg`   — Intro movie of the Resistance campaign.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `kyt_intro = mov:kyt.mpg`     — Intro movie of the Ghorkov campaign.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `taer_intro = mov:taer.mpg`   — Intro movie of the Taerkasten campaign.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `myk_intro = mov:myk.mpg`     — Intro movie of the Mykonian campaign.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sulg_intro = mov:sulg.mpg`   — Intro movie of the Sulgogar campaign.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `black_intro = mov:black.mpg` — Intro movie of the Black Sect campaign.
------------------------------------------------------------------------------------------------------------------------

### Vanilla SuperItems (begin_superitem)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_superitem`             — Opens the vanilla SuperItem settings. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `superbomb_wall_vproto = 135` — Model ID of the super bomb shock wall.
------------------------------------------------------------------------------------------------------------------------

### Menu sounds and music (begin_shellsounds, begin_shelltracks)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_shellsounds`                   — Opens the menu sounds. Closed by end. Each sound has _sample, _volume and _pitch.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `<suono>_sample = sounds/stahlbr.wav` — Menu sound file. Sounds: volume, left, right, button, quit, slider, welcome, menuopen, overlevel, levelselect, textappear, objectappear, sectorconquered, vhcldestroyed, bldgconquered, timercount, select, error, attention, secret, plasma.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `<suono>_volume = 60`                 — Volume of the menu sound.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `<suono>_pitch = 0`                   — Pitch of the menu sound.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_shelltracks`                   — Opens the menu music tracks. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `shelltrack = 3`                      — Menu music track. Optionally followed by _minPause_maxPause in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `missiontrack = 2`                    — Music track of the mission selection screen.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `loadingtrack = 0`                    — Music track of the loading screen.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `debriefingtrack = 0`                 — Music track of the debriefing.
------------------------------------------------------------------------------------------------------------------------

## USER.TXT (player profile)

### Player data (new_user)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `new_user`                       — Opens the player data block. It is written by the game and closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `plasma_bank = 1500`             — Plasma currency collected by the player (OpenNeoUA currency).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `workshop_owned = 56`            — ID of a vehicle already unlocked in the Workshop. One line per vehicle.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `workshop_host = 56`             — ID of the Host Station selected in the Workshop.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `maxroboenergy = 0`              — Highest Host Station energy reached so far, kept as a record.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `maxreloadconst = 0`             — Highest reload constant reached so far, kept as a record.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `beamenergy = 500`               — Current beam energy capacity of the player.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `playerstatus = 1_0_0_0_0_0_0_0` — Statistics of one faction. Format faction_destroyedUnits_destroyedByUser_elapsedTime_sectorsTaken_score_power_upgrades. One line per faction (0 to 7).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `jodiefoster = 0_0_0_0_0_0_0_0`  — Contact flags for the 8 factions (the game's own label is 'the contact flags').
------------------------------------------------------------------------------------------------------------------------

### Controls and keys (new_input)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `new_input`                   — Opens the input block: it clears the assigned keys and loads new ones. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `modify_input`                — Like new_input, but changes only the listed controls and keeps the others.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `joystick = yes`              — yes enables the joystick.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `altjoystick = yes`           — yes selects the alternative joystick model.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `forcefeedback = yes`         — yes enables force feedback.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.slider[0] = ~#$a_#$d`  — Aircraft: left / right. The value is a pair of keys: the first moves negative, the second positive. $ stands for winp: and _ separates the two keys.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.slider[1] = ~#$s_#$w`  — Aircraft: nose down / up.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.slider[2] = ~#$z_#$q`  — Aircraft: decelerate / accelerate.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.slider[3] = ~#$a_#$d`  — Ground unit: left / right.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.slider[4] = ~#$s_#$w`  — Ground unit: back / forward.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.slider[5] = ~#$1_#$2`  — Ground unit: gun down / up.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.button[0] = $return`   — Fire the primary weapon.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.button[1] = $tab`      — Weapon switch (in vanilla: launch a missile with its camera).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.button[2] = $x`        — Fire the machine gun.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.button[3] = $space`    — Brake / auto-level the helicopter.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.button[4] = $rshift`   — Map: set waypoints.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `input.button[5] = $grave`    — Missile camera (OpenNeoUA).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `input.button[6] = $tab`      — Cycle target (OpenNeoUA).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `input.button[7] = $f`        — Alternative view (OpenNeoUA).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[0] = o`         — Give orders to units.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[1] = space`     — Jump into gun turrets.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[2] = c`         — Create a vehicle or building.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[3] = q`         — Add a vehicle to the current squad.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[4] = j`         — Jump into a vehicle.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[7] = b`         — Beam the Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[8] = m`         — Map on / off.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[9] = e`         — Squadron Manager on / off.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[10] = num1`     — Map: sector detail.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[11] = num2`     — Map: sector ownership.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[12] = num3`     — Map: vehicle status.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[14] = num5`     — Map: lock on the occupied unit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[16] = numplus`  — Map: zoom in.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[17] = numminus` — Map: zoom out.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[18] = num4`     — Map: resize.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[20] = f7`       — Cycle through squad leaders.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[21] = f4`       — Jump to the Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[22] = f6`       — Cycle through the units of the squad.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[23] = f5`       — Jump to the current squad leader.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[24] = esc`      — Abort / quit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[25] = h`        — HUD on / off.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[27] = i`        — Information log on / off.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[31] = f8`       — Jump to the last message sender.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[32] = p`        — Pause.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[37] = rshift`   — Send a chat message (multiplayer).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[38] = 1`        — Aggression level 1.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[39] = 2`        — Aggression level 2.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[40] = 3`        — Aggression level 3.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[41] = 4`        — Aggression level 4.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[42] = 5`        — Aggression level 5.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[43] = f1`       — Help (not available in multiplayer).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[44] = bs`       — Jump to the last occupied unit.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[45] = l`        — Become the current squad leader.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `input.hotkey[46] = f2`       — Situation analyzer.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `input.hotkey[47] = k`        — Toggle the cockpit camera (OpenNeoUA).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `input.hotkey[48] = r`        — Sprint (OpenNeoUA).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `input.hotkey[49]`            — Place a map marker (OpenNeoUA). Markers are disabled in Spectator Mode, including the right-click shortcut.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `input.hotkey[52] = space`    — UFO: toggle Spy mode (OpenNeoUA).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `input.hotkey[53] = e`        — Map focus (OpenNeoUA).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `input.hotkey[54]`            — Hide the player's vehicle (OpenNeoUA).
------------------------------------------------------------------------------------------------------------------------

### Audio (new_sound)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `new_sound`      — Opens the audio block. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `volume = 127`   — Sound effect volume (0 to 127).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `cdvolume = 127` — Music volume (0 to 127).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `invertlr = no`  — yes swaps the left and right stereo channels.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `cdsound = yes`  — CD music switch: stores the flag that makes the game ignore its music commands.
------------------------------------------------------------------------------------------------------------------------

### Video (new_video)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `new_video`            — Opens the video block. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `videomode = 2621920`  — Game resolution as one number: width x 4096 + height (2621920 is 640 x 480).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `gfxmode = 800_600_0`  — Modern resolution: width_height_windowed (0 fullscreen, 1 windowed).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `farview = yes`        — yes enables the far view, which renders more sectors.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `drawprimitive = no`   — Flag of the original graphics driver (draw primitive).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `16bittexture = yes`   — Flag of the original graphics driver (16-bit textures).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `softmouse = yes`      — yes draws the mouse cursor in software.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `heaven = yes`         — yes renders the sky.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `enemyindicator = yes` — yes shows the enemy indicator.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `fxnumber = 16`        — Maximum number of visual effects (explosions, sparks) active at once.
------------------------------------------------------------------------------------------------------------------------

These values are saved per user profile rather than used as global mod settings.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `interface_style = retro`  — Saved virtual UI style: retro = nearest/pixelated presentation, smooth = linear/smoother presentation. If absent, the profile starts from ui.retro_interface.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `palette_theme = Original` — Saved palette-theme name. Original selects the standard palette set; another value selects that named theme when available.
------------------------------------------------------------------------------------------------------------------------

### Completed levels and buddies

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_levelstatus = 1` — Opens the status of one level (the level number). Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `status = 2`            — Level status: 1 locked, 2 available, 3 completed.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_buddy`           — Opens a saved buddy. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `commandid = 1`         — Command ID of the buddy.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `type = 2`              — Vehicle ID of the buddy.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `energy = 1000`         — Energy of the buddy.
------------------------------------------------------------------------------------------------------------------------

### Menu interface (new_shell)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `new_shell`                            — Opens the menu interface block. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `language = english`                   — Selected language, among those available in Locale.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_map_status = 1_1_0_0_200_200`    — State of the Host Station map window: valid_open_x_y_width_height.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `robo_finder_status = 1_1_0_0_200_200` — State of the Host Station finder window (same format).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vhcl_map_status = 1_1_0_0_200_200`    — State of the vehicle map window (same format).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vhcl_finder_status = 1_1_0_0_200_200` — State of the vehicle finder window (same format).
------------------------------------------------------------------------------------------------------------------------

## Graphic Profiles

Original Video reduces the final composed frame, including UI, while preserving the display resolution and aspect ratio. A 4:3 frame uses 320x240; other aspect ratios adjust the width. Menu saves are persisted in OpenNeoUA.ini; editable preset values remain in .cfg files. Applying an older preset without video_filter_mode selects VHS mode.

Graphic Profiles are editable .cfg files stored in Data/Scripts/Graphic_Profiles/. Legacy .txt profiles remain accepted for compatibility. The file name becomes the preset name shown in Advanced Graphics Settings. The parameters below belong in Graphic Profile files rather than OpenNeoUA.ini. Manually changing one of these settings in the menu changes the active preset to Custom. gfx.render_sectors is retired as a public setting: the engine derives its sector window automatically from gfx.skydistance.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.palette_filter = Black_Sect_Anvil.pal` — Full-screen palette filter loaded from Data/Filters. Standard disables the filter.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.palette_filter_strength = 0.50`        — Palette-filter strength. 0.00 = no effect; 1.00 = full strength.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.atmosphere_strength = 0.50`            — Overall strength of the atmosphere post-process.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.atmosphere_exposure = 1.70`            — Atmosphere exposure / overall brightness response.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.atmosphere_contrast = 0.95`            — Image contrast used by the atmosphere pass.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.atmosphere_saturation = 0.80`          — Color saturation used by the atmosphere pass.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.atmosphere_vignette = 0.60`            — Dark-edge vignette strength.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.skydistance = 60000`                   — Horizontal radius of the player-centered sky horizon. Lower values bring the dome closer and automatically prepare fewer world sectors; higher values move it farther away and prepare more. Menu range: 1200-60000. Sector coverage is derived automatically.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.sky_height = -400`                     — Vertical sky-dome offset relative to the player. More negative values lower the dome; higher values raise it. Menu range: -3000..3000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_fade_mode = 0`                 — Distance-fade curve: 0 = smooth radial horizon fade, 1 = classic linear camera-depth fade. Missing/invalid values use 0. Classic uses 1 with full black darkening from 1000 to 3100 units.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_fog_start = 4000`              — Distance at which transparent horizon fog begins.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_fog_length = 2000`             — Length of the horizon-fog transition. Higher values produce a softer fade.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_fog_strength = 0.80`           — Horizon-fog strength. 0.00 disables it; 1.00 is maximum strength.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_dark_start = 2000`             — Distance at which horizon darkening begins.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_dark_length = 2000`            — Length of the horizon-darkening transition.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.horizon_dark_strength = 0.65`          — Horizon-darkening strength. 0.00 disables it; 1.00 is maximum strength.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `game.world_ui_max_distance = 5700`         — Maximum distance for supported world-space unit UI. Menu range: 100-20000.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.video_filter_mode = 0`                 — Final-frame filter: 0 = existing VHS, 1 = Original Video (240-line reduction with bilinear enlargement, without VHS noise or distortion). Missing/invalid values use 0. Classic selects 1.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.vhs_filter_strength = 0.60`            — Selected video-filter strength: 0.00 bypasses it; 1.00 applies it fully. Classic uses 1.00. The legacy key name is retained for compatibility.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `gfx.particles.limit = 9000`                — Maximum active-particle limit stored by the profile. The menu supports values up to 20000.
------------------------------------------------------------------------------------------------------------------------

## Levels (LDF files)

### begin_level block

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_level`                        — Opens the general level block. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `title_default = Il_mio_livello`     — Level title. Language-specific lines such as title_italian take priority when that language is active; title_default is the fallback.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `set = 1`                            — Graphic set number (sectors and textures). Write it before anything else; changing it after building the sectors breaks the map.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sky = objects/sky1.base`            — Sky object of the level (a .base file).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `typ = typ1.iff`                     — Original format: file name of the image holding the sector types. Used instead of the begin_maps block.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `own = own1.iff`                     — Original format: file name of the image holding the sector owners.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hgt = hgt1.iff`                     — Original format: file name of the image holding the heights.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `blg = blg1.iff`                     — Original format: file name of the image holding the functional buildings.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `palette = palette/standard.pal`     — Color palette of the level. Equivalent to slot0.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `slot0 = palette/standard.pal`       — Palette of slot 0. Slots 0 to 7 hold the palettes used by color effects (`pal_*_slot`).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `slot1 = palette/red.pal`            — Palette of slot 1 (red).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `slot2 = palette/blue.pal`           — Palette of slot 2 (blue).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `slot3 = palette/green.pal`          — Palette of slot 3 (green).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `slot4 = palette/inverse1.pal`       — Palette of slot 4 (inverse).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `slot5 = palette/inverse2.pal`       — Palette of slot 5 (dark inverse).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `slot6 = palette/white.pal`          — Palette of slot 6 (black and white).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `slot7 = palette/inverse3.pal`       — Palette of slot 7 (turquoise inverse).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `script = data:scripts/startup2.scr` — Prototype script loaded for the level (weapons, vehicles, buildings), normally startup2.scr or net_ypa.scr. The level fails to load if the script cannot be read.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `ambiencetrack = 3_30000_60000`      — Ambient music: track number, then optionally the minimum and maximum pause in milliseconds between repeats.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `movie = mov:intro.mpg`              — Movie shown when the level starts.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `win_movie = mov:win.mpg`            — Movie shown when the level is won.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `lose_movie = mov:lose.mpg`          — Movie shown when the level is lost.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `event_loop = 1`                     — Tutorial lesson hook: the values 1, 2 and 3 load lesson1.lua, lesson2.lua and lesson3.lua. Other values do nothing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `slow_connection = yes`              — Multiplayer only: mode intended for slow connections.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `unit_limit = 800`                   — Maximum number of units in the level. Overrides the unit_limit of World.ini.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `unit_limit_type = 1`                — What happens beyond the limit: 0 is a hard block, 1 applies a penalty to power stations, 2 makes units more expensive.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `unit_limit_arg = 5`                 — Extra argument of the limit, used by types 1 and 2.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `multi_building = yes`               — Allows several buildings to be built in the same sector for this level.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hidden_fractions = 0`               — Factions hidden from the enemy, as a bit mask over the faction number.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `end`                                — Closes the block.
------------------------------------------------------------------------------------------------------------------------

Level-side settings override broader defaults where explicitly documented. This lets a map define its own ambience, SuperItem profile and atmospheric presentation without forcing the same setup on every level.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `ambient_sound = sounds/ambient/wind.wav` — High-priority level ambience. If the path identifies a folder instead of a specific file, OpenNeoUA can choose a sound from that folder and loop it. When present in a level file, this overrides the nucleus.ini setting. The path is relative to Data. Studio: Level Info > Ambient sound.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `superitem_id = 1`                        — ID of the new_superitem profile linked by the level begin_item block. -1, absent or unknown ID = vanilla SuperItem.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `cloud_id = 1`                            — ID of the new_cloud profile active in the level. -1, absent or unknown ID = no Cloud. Studio: Level Info > Clouds (with animated map preview).
------------------------------------------------------------------------------------------------------------------------
- 🟢 `lua_script = level_intro.lua`            — Optional per-level Lua script, loaded from Data/res/. If absent or unloadable, the level runs with vanilla behavior and no errors. Tutorial levels (EventLoopID 1..3) use lessonN.lua instead. The engine calls init(ts) on level start, update(ts, delta) on every game tick, and makesave() to persist a custom string into saves.
------------------------------------------------------------------------------------------------------------------------

### Host Stations (begin_robo)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_robo`            — Opens a Host Station of the level (7 at most). Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner = 1`             — Owning faction (1 Resistance, 2 Sulgogar, 3 Mykonian, 4 Taerkasten, 5 Black Sect, 6 Ghorkov). The first Host Station of the file is the player's.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vehicle = 56`          — ID of the Vehicle (model = robo) used as the Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sec_x = 8`             — Sector column. Places the Host Station at the center of that sector.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sec_y = 5`             — Sector row.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pos_x = 9960`          — X position in world units (alternative to sec_x). A sector is 1200 x 1200 units. Avoid coordinates ending in 0 or 5, which sit on sector edges.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pos_y = -300`          — Height in world units (negative is above ground).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pos_z = -6600`         — Z position in world units (rows go negative).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `energy = 400000`       — Starting energy of the Host Station.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `reload_const = 500000` — Reload constant. Required when the energy exceeds 500000 (enemies) or 550000 (player); otherwise the station can never be depleted.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `viewangle = 0`         — Initial view / facing angle.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `con_budget = 50`       — AI budget for conquest.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `def_budget = 20`       — AI budget for defense.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `rob_budget = 20`       — AI budget for attacks against enemy Host Stations.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `rec_budget = 10`       — AI budget for reconnaissance.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pow_budget = 10`       — AI budget for building power stations.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `rad_budget = 10`       — AI budget for building radar stations.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `saf_budget = 10`       — AI budget for building flak defenses.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `cpl_budget = 10`       — AI budget for building the defense center.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `con_delay = 0`         — Delay in milliseconds before the AI starts spending its conquest budget.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `def_delay = 0`         — Delay before the defense budget.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `rob_delay = 0`         — Delay before the attack budget.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `rec_delay = 0`         — Delay before the reconnaissance budget.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pow_delay = 0`         — Delay before the power-station budget.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `rad_delay = 0`         — Delay before the radar budget.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `saf_delay = 0`         — Delay before the flak budget.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `cpl_delay = 0`         — Delay before the defense-center budget.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `mb_status = known`     — Briefing visibility: known (visible), unknown (marked but unidentified) or hidden.
------------------------------------------------------------------------------------------------------------------------

### Squads (begin_squad)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_squad`       — Opens a squad of pre-placed units (32 units at most). Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `owner = 1`         — Owning faction of the squad.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vehicle = 2`       — ID of the Vehicle in the squad. Required.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `num = 4`           — Number of vehicles in the squad. Required.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pos_x = 5000`      — X position in world units. Required.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `pos_z = -3000`     — Z position in world units. Required.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `useable`           — When present, the player can command the squad.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `mb_status = known` — Briefing visibility (known, unknown, hidden).
------------------------------------------------------------------------------------------------------------------------

### Beam gates (begin_gate)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_gate`        — Opens a beam gate (a passage to other levels). Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sec_x = 10`        — Sector column of the gate.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sec_y = 7`         — Sector row of the gate.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `closed_bp = 5`     — Building ID used while the gate is closed.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `opened_bp = 6`     — Building ID used while the gate is open.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `target_level = 12` — Level the gate leads to. Repeat the line for several destinations.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `keysec_x = 3`      — Column of a key sector needed to open the gate (up to 16). Always followed by keysec_y.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `keysec_y = 4`      — Row of the key sector, written right after keysec_x.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `mb_status = known` — Briefing visibility (known, unknown, hidden).
------------------------------------------------------------------------------------------------------------------------

### Gems and upgrades (begin_gem)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_gem`                      — Opens a technology-upgrade sector (gem). Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sec_x = 8`                      — Sector column of the gem.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sec_y = 1`                      — Sector row of the gem.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `building = 4`                   — ID of the Building shown on the gem sector.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `type = 3`                       — Sound played by the upgrade, from 0 to 6.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `script = data:scripts/gem1.scr` — Optional script run by the gem. The file must exist.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `msg_default = Nuova_tecnologia` — Message displayed. A language-specific `msg_<language>` line takes priority.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `nw_vproto_num = 11_12`          — Vehicle numbers (up to 4) listed as newly available in the briefing.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `nw_bproto_num = 5`              — Building numbers (up to 4) listed as newly available.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_action`                   — Opens the list of changes applied when the gem is collected (modify_vehicle, modify_weapon, modify_building). Closed by end_action.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `mb_status = known`              — Briefing visibility (known, unknown, hidden).
------------------------------------------------------------------------------------------------------------------------

### Enables (begin_enable)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_enable = 1` — Opens a list of units and buildings enabled for a faction (the faction number). Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `vehicle = 6`      — ID of a Vehicle to enable for the faction of the block.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `building = 4`     — ID of a Building to enable for the faction of the block.
------------------------------------------------------------------------------------------------------------------------

### Level SuperItems (begin_item)

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_item`         — Opens a SuperItem (super bomb or wave) in the level. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sec_x = 1`          — Sector column of the SuperItem.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `sec_y = 12`         — Sector row of the SuperItem.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `inactive_bp = 35`   — Building ID while the SuperItem is inactive.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `active_bp = 36`     — Building ID while the SuperItem is active.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `trigger_bp = 37`    — Building ID in the trigger phase.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `type = 1`           — Type: 1 is a super bomb, 2 is a wave.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `countdown = 998000` — Countdown in milliseconds.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `keysec_x = 3`       — Column of a key sector that arms the SuperItem; always followed by keysec_y.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `keysec_y = 4`       — Row of the key sector.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `mb_status = known`  — Briefing visibility (known, unknown, hidden).
------------------------------------------------------------------------------------------------------------------------

### Briefing and maps

------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_mbmap`        — Opens the mission-briefing background definition. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_dbmap`        — Opens the debriefing background definition. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `name = DEFAULT.IFF` — Image file name (under levels/mbpix/). Used by both mbmap and dbmap.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `size_x = 640`       — Image width.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `size_y = 480`       — Image height.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `begin_maps`         — Opens the four maps of the level. Closed by end.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `typ_map`            — Sector type map (hexadecimal): the visual type of every sector. It is followed by the width and height line and then the grid.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `own_map`            — Owner map: 0 nobody, 1 Resistance, 2 Sulgogar, 3 Mykonian, 4 Taerkasten, 5 Black Sect, 6 Ghorkov, 7 neutral (power stations, radars, beam gates).
------------------------------------------------------------------------------------------------------------------------
- 🔵 `hgt_map`            — Height map, from 0 to 255.
------------------------------------------------------------------------------------------------------------------------
- 🔵 `blg_map`            — Map of functional buildings (power, radar, flak and others), in hexadecimal.
------------------------------------------------------------------------------------------------------------------------

Lua API: World.ShowMessageID(msgId), World.StartIterateUnits() -> iterator, first_unit | nil, World.GetUnits() -> 0-based list, World.GetUserRobo(), World.GetUserUnit(), World.IsInRoboGun() -> bool, World.GetUserSquadsCount(), World.GetSector(x, y), World.GetSectorInfo(sector) -> {owner, w_type, x, y, type_id}, World.GetPowerStations() -> list of {sector, power, effpwr}, World.GetGates() -> list of {sector, mbstatus, keysectors}, World.ExitTutorialWindow(), World.IsRoboMapOpen() -> bool. Bact.GetStatus() -> `Bact.STATUS_*` constants (NOPE, NORMAL, DEAD, IDLE, CREATE, BEAM), Bact.GetVehicleId(), Bact.GetOwner(), Bact.GetType() -> `Bact.TYPE_*` constants (NOPE, BACT, TANK, ROBO, MISSLE, ZEPP, FLYER, UFO, CAR, GUN), Bact.StartIterateKids() -> iterator, first_child | nil, Bact.GetKids() -> 0-based list, Bact.GetPSector(). IterUnits.Next(iterator) / IterUnits.Get(iterator) -> next unit | nil, IterUnits.Free(iterator) releases the iterator; System.RunScript(filename) loads another script. All standard Lua libraries are available.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `begin_briefing_text` — Level-local mission briefing (OpenNeoUA only). Optional block in the LDF file, after the begin_mbmap / begin_dbmap blocks. Non-blank text overrides the language-file briefing; a missing or blank block keeps the locale briefing. Studio: Level Info > Briefing text.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `Secure the power station.`
- 🟢 `Keep your Host Station alive.`
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end` — Closes the block. Line breaks are kept as written and semicolons are literal text. A line containing only "end" must be escaped with a leading backslash (Studio does this automatically). Remove the block when targeting the original engine.
------------------------------------------------------------------------------------------------------------------------

### Level locations (OpenNeoUA only, in LDF files)

These optional blocks belong in an LDF, never in OpenNeoUA.ini or Nucleus. Without begin_location blocks, location notifications are inactive and existing level behavior is preserved. A level containing these blocks requires OpenNeoUA and is not compatible with the vanilla engine.

------------------------------------------------------------------------------------------------------------------------
- 🟢 `begin_location 1`                       — Unique positive ID. A level supports up to 256 locations.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `name = Dairopoli`                       — Display name, 1-96 characters. UTF-8 and Windows-1252 accents are supported. No semicolons/newlines.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `color = white`                          — Title text colour. Default white; allowed values: white, yellow, orange, red, pink, purple, blue, cyan, green, lime, gray.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `sound = Data/Sounds/Locations/City.wav` — Optional entry sound. A missing/invalid sound does not prevent the title from appearing.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fade_in_ms = 450`                       — Fade-in duration, 0-10000 ms; default 450. Zero means immediate appearance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `hold_ms = 2800`                         — Fully visible duration, 0-60000 ms; default 2800.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `fade_out_ms = 750`                      — Fade-out duration, 0-10000 ms; default 750. Zero means immediate disappearance.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `point = 4800|-6000`                     — Polygon vertex in world-space X|Z coordinates. Repeat for each vertex.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `point = 8400|-6000`
- 🟢 `point = 9600|-8400`
- 🟢 `point = 4800|-8400`
------------------------------------------------------------------------------------------------------------------------
- 🟢 `end` — Implicitly joins the last vertex to the first.
------------------------------------------------------------------------------------------------------------------------

Use 3-256 distinct vertices forming a simple, non-zero-area polygon. Concave polygons are supported. Only the directly controlled unit triggers entry, at any altitude. Exiting and re-entering repeats the notice. Switching controlled units within the same effective location does not repeat it. The last listed area wins overlaps. A fresh mission starting inside an area announces it. Loading a save inside does not fabricate an entry. The central HUD title uses Xolonium Bold and independent opacity; ordinary game UI fonts remain unchanged. The colour applies only to the location name; the LOCATION label and divider lines keep their current colours. New saves preserve location definitions. Older saves without these blocks retain definitions from the original LDF. Sound paths must be game-root-relative Data/... paths, with / separators and no absolute paths, virtual prefixes or traversal. Invalid geometry/IDs/timing are logged and skipped by the engine. An invalid sound is ignored separately; an unknown colour falls back to white. Studio provides the Locations tab, red OpenNeoUA-only marker, polygon editing with the chosen map colour, text colour, Undo/Redo and animated/audio preview.

============================================================================= End of OpenNeoUA Parameter Guide =============================================================================

## Debug controls (F1-F12)

------------------------------------------------------------------------------------------------------------------------
- 🟢 `F1`  — New debug mode: toggles a Minecraft F3-style information panel at the top left. It shows: build, FPS (measured, average and 1% low), frame time, display refresh rate (Hz), VSync, FPS cap, resolution, the graphics chip in use (flagged as integrated, iGPU, or dedicated), OpenGL version, position and sector, level and time, draw distance, sectors, polygons, unit and effect counts. The old DPS meter has been removed.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F2`  — New debug mode: slows the world to 20% of normal speed; the controlled unit, its mounted parts, actions and player-fired projectiles remain at normal speed. Other allies stay slowed. Toggles on/off; F4 still freezes everything. Single-player only.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F3`  — New debug mode: speeds game time up to 200% of normal speed (+100%); toggles on/off.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F4`  — New debug mode: toggles game-time freeze; useful for screenshots and video capture. It can freeze time while slow or fast debug speed is active.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F5`  — New debug mode: instantly destroys the vehicle or gun under the mouse.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F6`  — New debug mode: toggles invulnerability for the allied vehicle under the mouse, or the player-controlled vehicle when the mouse is not over a unit.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F7`  — New debug mode: toggles whether enemy AI ignores the allied vehicle under the mouse, or the player-controlled vehicle when the mouse is not over a unit. This runtime-only setting is not saved; the vehicle stays visible and can still attack, and its attacks do not cancel the exclusion. Projectiles already fired, crossfire and area damage can still hit it.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F8`  — New debug mode: toggles invulnerability for all units.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F9`  — New debug mode: displays legacy collisions, main compound spheres and below/forward box and cone AI fire volumes. The selected Weapon slot is drawn in orange without a dimension label. Invisible bridges are not shown.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F10` — New debug mode: toggles the in-game HUD visibility; useful for screenshots and video capture.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F11` — New debug mode: fills the three Host Station bars (Genesis, Energy and Move), makes all Host-issued in-game actions free and bypasses max_active_at_once for player production. Disabling the cheat restores the limit without removing existing units or changing prototype/GEM values.
------------------------------------------------------------------------------------------------------------------------
- 🟢 `F12` — New debug mode: switches the player faction and lets the player control the enemy vehicle or gun under the mouse. Requires a living Host Station of that faction; does not change the unit owner. Single-player only.
------------------------------------------------------------------------------------------------------------------------

F2 and F3 are mutually exclusive; enabling one disables the other. F4 can freeze time while it is slowed or accelerated. Slowdown effects already provided by the game take priority over debug acceleration.

### Integrated example: Mykonian Bomber and KNOCKER (ID 70)

This is a mod configuration, not a new engine default. The Bomber uses `num_weapons = 3` to drop three bombs **simultaneously**, rather than `salve_shots`, which defines a timed burst. `num_weapons_snd_events = 1` prevents three overlapping launch sounds. Launch points use `fire_x = 45`, `fire_y = 75`, `fire_z = 0`, below the hull's lower bound (+Y points down). `weapon_spread_xyz = 3|0|3` adds slight ground-plane scatter.

Weapon 70 keeps the KNOCKER name and uses `model = homing_bomb`, `multi_target = 3`, `force = 2500`, `homing_force = 1.5` and `homing_time = 5000`. The existing targeting system searches for up to three targets; three distinct targets are not guaranteed when fewer are available. Lifetime is 8000 ms, including shots without a target. The downward AI cone is `60|60|1500`, with a dead zone of 150. Additional player guidance retains the lock requirement documented under `homing_force`.

Each bomb has `energy = 18000`, down from 60000: 54000 per drop before class multipliers and shields. Area damage uses `Blast_energy = 10000`, radius 130 and distance falloff. Do not automatically add direct and area damage for the directly hit target: engine damage rules apply. Drop interval is 2800 ms for AI and 2200 ms for the player. `weapon_energy_cost = 0.4%` is paid once per drop, not three times.

The carrier has 40000 energy, production cost 800, shield 5 and a cap of five active units. Below 20% energy it emits smoke/electrical damage effects and applies the existing `damaged_*` penalties. External collision: `Data/Collisions/Bomber.collision`, a simple envelope matching the original model bounds. Destruction and impact effects reuse Mykonian electrical VPs; ground-only effects use `impact_world`, while contact effects also use `detonate`. No new parameter, model or save format change is introduced.
