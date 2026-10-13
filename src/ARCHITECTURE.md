# OpenNeoUA engine - code map

Where things live in `src/`, so a person or an AI assistant can find the right file quickly.
Keep this file short and update it when files move.

## Build and test

```bash
# From Data/ (MSYS2 MinGW64 shell), see "Build And Deploy OpenNeoUA.bat":
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -S src -B Build && cmake --build Build -j8

# Focused native tests (they link the production objects, no game session needed):
python src/world/tests/run_native_probe.py src/world/tests/<name>_test.cpp
python src/world/tests/run_native_probe.py src/world/tests/gameplay_parameters_test.cpp --replace-ypabact
```

`src` is the include root: write includes as `"game/yw.h"`, `"common/utils.h"`, ...

## Folders

| Folder | Contents |
|---|---|
| `main.cpp` | Program entry, main loop, level/shell transitions. |
| `common/` | Small shared utilities: vectors/matrices, lists, INI and script parsing, logging, crash diagnostics. No game logic. |
| `nucleus/` | The original object/resource system: `nucleas` (base object), `rsrc` (resources), `base` (models), bitmaps/ILBM, skeletons, areas, 3DS, particles, `IFFile` (IFF reader/writer with loose-file overrides). |
| `platform/` | OS-facing layers: input (`winp`), audio (`wrapal`, `wav`, `sample`), timers, multiplayer provider (`windp`). |
| `system/` | Engine services: virtual file system (`fsmgr`), rendering (`gfx`, `glfuncs`, shaders), input engine, sound engine, `inivals` (every INI setting), movie, Lua, transforms, game clock. |
| `gui/` | Widgets of the in-game GUI (`gui/root`, `uabutton`, ...) and the classic `button`, `lstvw` (list view), `font`. |
| `game/` | The game itself: the world `yw*` and the units `ypa*` (see below). |
| `world/` | OpenNeoUA gameplay data and helpers: prototypes (`protos`), script parsers, collision shapes, ground decals, tracers, energy FX, speech, Lua events. `world/tests/` holds the native tests. |
| `netlib/` | Network library (client, server, sessions). |
| `locale/` | Language strings. |
| `third_party/bullet`, `fmtlib`, `any-lite` | Bundled libraries (collision, formatting, `any`). |

## Units (`game/ypa*.cpp`)

`NC_STACK_ypabact` is the base class of every unit. Its implementation is split by responsibility:

| File | Responsibility |
|---|---|
| `ypabact.cpp` | Construction, update/render loop, state changes, waypoints, network updates |
| `ypabact_ai.cpp` | AI layers, target assessment, path finding, group organisation, fight decisions |
| `ypabact_user.cpp` | Player control, targeting, hand brake, sprint |
| `ypabact_physics.cpp` | Movement, terrain corrections, collisions, crashes, Genesis separation |
| `ypabact_damage.cpp` | Energy/damage, death, plasma currency, medals, progressive fire rate, stun |
| `ypabact_fire.cpp` | Missile launch, minigun, weapon direction patterns |
| `ypabact_weapons.cpp` | Weapon slots, projectile counts, multi-target and homing selection |
| `ypabact_laser.cpp`, `_artillery.cpp`, `_kamikaze.cpp`, `_carrier.cpp`, `_proximity.cpp` | One OpenNeoUA weapon/feature each |
| `ypabact_projectile.cpp`, `_recoil.cpp`, `_status_fx.cpp`, `_camera.cpp`, `_control.cpp` | Projectile visual motion, recoil/push, debuffs and status FX, cockpit camera, mindcontrol |
| `ypabact_internal.h` | What those files share (helpers, constants, small types). Public interface stays in `ypabact.h`. |

Subclasses: `ypatank`, `ypacar`, `ypaflyer`, `ypaufo`, `ypagun`, `ypamissile` (projectiles) and `yparobo` (host stations).

## The world (`game/yw*.cpp`)

`NC_STACK_ypaworld` (`yw.h`) is large and spread over several files:

| File | Responsibility |
|---|---|
| `yw.cpp` | Construction, Init/Deinit, per-frame `Process`, level teardown, sector access, `ypaworld_funcNNN` entry points |
| `yw_shell.cpp` | Game shell (title/menus) controls, settings files, input bindings, save/load entry points |
| `yw_sprint.cpp`, `yw_gem.cpp`, `yw_plasma.cpp` | Player sprint, gem notifications, plasma currency |
| `yw_ambient.cpp`, `yw_protos.cpp`, `yw_attached_fx.cpp` | Ambient/atmospheric sound and FX, prototype scripts and names, attached FX volumes |
| `yw_game.cpp` | Level loading, units creation, sector ownership, per-frame world updates |
| `yw_spectator.cpp`, `yw_superitem.cpp`, `yw_building_spawn.cpp`, `yw_mobile_power.cpp` | Spectator mode, super items, spawning buildings, power stations and sector energy |
| `yw_fx.cpp`, `yw_render.cpp` | Transient visuals and effects, rendering of the game view |
| `yw_recorder.cpp`, `yw_debug.cpp`, `yw_artillery_markers.cpp` | Replay recorder, debug overlays, artillery shell map markers |
| `yw_game_ui.cpp` | In-game GUI core (windows, buy/creation menus, player panel, legacy input handlers) |
| `yw_ui_map.cpp`, `yw_ui_squadron.cpp`, `yw_ui_hud.cpp`, `yw_ui_orders.cpp`, `yw_ui_artillery.cpp`, `yw_ui_windows.cpp`, `yw_ui_voice.cpp` | Map/toolbar, squadron manager, HUD, order feedback and selection, artillery UI, priority windows, voice messages |
| `yw_func2.cpp` | Shell menus: save/load, language, about, the big shell input handler |
| `yw_options.cpp`, `yw_database.cpp`, `yw_input_config.cpp` | Options/atmosphere/graphic profiles, database screen, key configuration |
| `yw_funcs.cpp`, `yw_brif.cpp`, `yw_net.cpp`, `yw_save.cpp` | World helpers, briefings, multiplayer, save games |

Each split area shares a small internal header next to it (`yw_impl.h`, `yw_game_impl.h`, `yw_game_ui_internal.h`,
`yw_shell_menus.h`); public declarations stay in `yw.h`.

Other classes follow the same pattern: `yparobo.cpp` (host station core) with `yparobo_ai.cpp` and
`yparobo_player.cpp`; `ypamissile.cpp` with `ypamissile_blast.cpp`, `_chain.cpp`, `_flight.cpp`, `_tracer.cpp`.

## Conventions and legacy notes

- New gameplay parameters go through `system/inivals` (INI) or the script parsers in `world/parsers.cpp`
  and are documented in `OpenNeoUA_Parameter_Guide.ini`; defaults must keep vanilla behaviour.
- Much of the original code came from a decompiler: names like `sub_4A9F24`, `sb_0x4a7010__sub1`, `field_3DA`,
  `v16` or `ypaworld_func64__sub7` carry no meaning. Rename them only when the behaviour is understood.
- Files with many static helpers used to hide them as `static` functions; the helpers shared by the
  `ypabact*` files are now declared in `ypabact_internal.h`.
- Prefer a shared helper over a second copy: duplicated blocks are a source of drift (the per-unit feature state
  reset of `ypabact` and `ypamissile` now lives in `ResetFeatureRuntimeState` / `ResetRuntimeBlastState`).

## Keeping it clean (rules for new work)

1. **Find before you write.** Look for an existing helper or parser first (`world/tools.*`, `common/utils.*`,
   the parsers in `world/parsers.cpp`). Do not paste a block twice: give it a function (see `ResetFeatureRuntimeState`).
2. **Put code where its responsibility lives.** A new unit feature gets its own `ypabact_<feature>.cpp` (add it to
   `CMakeLists.txt` and mention it in the core file comment); a new GUI area goes next to `yw_ui_*.cpp`; data and
   parsing of new parameters go in `world/`. Do not grow the core files (`ypabact.cpp`, `yw.cpp`, `yw_game.cpp`).
3. **Share through the internal header.** Helpers used by several files of one class go in that class's
   `*_internal.h` / `yw_*_impl.h` header, not copied and not made `static` in each file.
4. **New parameters are optional.** Off by default, safe fallback for missing/zero/invalid values, vanilla behaviour
   unchanged, and documented in `OpenNeoUA_Parameter_Guide.ini`.
5. **Modern C++ for new code.** `nullptr`, `override`, `std::` containers, `const` references; no new global state
   and no `dword_XXXX`-style names. Name things by what they mean.
6. **No dead or commented-out code.** Delete unused functions; git keeps the history.
7. **Comments explain why** (constraint, engine quirk, order dependency). Each new file starts with one line saying
   what it is responsible for.
8. **Before finishing:** build (`cmake --build Build -j8`) and run the native probes in `world/tests`
   (`python src/world/tests/run_native_probe.py src/world/tests/<name>_test.cpp`). A change that touches behaviour
   needs a probe or a test case.
9. **Moving files?** Update `CMakeLists.txt`, the includes (root is `src/`), the tests that include production units,
   and this file.
