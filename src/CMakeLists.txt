cmake_minimum_required(VERSION 3.10)

project(OpenNeoUA)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 14)

set(SOURCES
    nucleus/3ds.cpp
    nucleus/ade.cpp
    nucleus/area.cpp
    nucleus/amesh.cpp
    nucleus/bitmap.cpp
    nucleus/base.cpp
    nucleus/bmpAnm.cpp
    gui/button.cpp
    common/crashdiag.cpp
    common/crc32.cpp
    common/def_parser.cpp
    nucleus/embed.cpp
    common/env.cpp
    fmtlib/format.cc
    gui/font.cpp
    gui/root.cpp
    gui/uabutton.cpp
    gui/uacommon.cpp
    gui/uaempty.cpp
    gui/uamsgbox.cpp
    gui/uascroll.cpp
    gui/uawindow.cpp
    gui/widget.cpp
    platform/idev.cpp
    nucleus/ilbm.cpp
    nucleus/image.cpp
    common/ini.cpp
    platform/itimer.cpp
    platform/iwimp.cpp
    common/log.cpp
    nucleus/loaders.cpp
    locale/default.cpp
    locale/locale.cpp
    gui/lstvw.cpp
    main.cpp
    nucleus/IFFile.cpp
    platform/network.cpp
    netlib/client.cpp
    netlib/client_threads.cpp
    netlib/server.cpp
    netlib/server_session.cpp
    netlib/server_threads.cpp
    netlib/single.cpp
    netlib/single_threads.cpp
    netlib/zndNet.cpp
    nucleus/nucleas.cpp
    nucleus/obj3d.cpp
    nucleus/particle.cpp
    nucleus/rsrc.cpp
    platform/sample.cpp
    nucleus/skeleton.cpp
    nucleus/sklt.cpp
    system/fsmgr.cpp
    system/gametime.cpp
    system/gfx.cpp
    system/glfuncs.cpp
    system/inivals.cpp
    system/inpt.cpp
    system/luascript.cpp
    system/movie.cpp
    system/sound.cpp
    system/stdshdr.cpp
    system/system.cpp
    system/tform.cpp
    common/utils.cpp
    platform/wav.cpp
    platform/windp.cpp
    platform/winp.cpp
    platform/wintimer.cpp
    world/analyzer.cpp
    world/blacksecttint.cpp
    world/energyfx.cpp
    world/grounddecal.cpp
    world/collision_shape.cpp
    world/weapontracer.cpp
    world/luaevents.cpp
    world/parsers.cpp
    world/nparticle.cpp
    world/protos.cpp
    world/saveparsers.cpp
    world/speech.cpp
    world/tools.cpp
    platform/wrapal.cpp
    game/yw.cpp
    game/yw_shell.cpp
    game/yw_sprint.cpp
    game/yw_gem.cpp
    game/yw_plasma.cpp
    game/yw_ambient.cpp
    game/yw_protos.cpp
    game/yw_attached_fx.cpp
    game/yw_brif.cpp
    game/yw_funcs.cpp
    game/yw_func2.cpp
    game/yw_database.cpp
    game/yw_options.cpp
    game/yw_input_config.cpp
    game/yw_game.cpp
    game/yw_spectator.cpp
    game/yw_superitem.cpp
    game/yw_building_spawn.cpp
    game/yw_mobile_power.cpp
    game/yw_fx.cpp
    game/yw_render.cpp
    game/yw_recorder.cpp
    game/yw_debug.cpp
    game/yw_artillery_markers.cpp
    game/yw_game_ui.cpp
    game/yw_ui_artillery.cpp
    game/yw_ui_squadron.cpp
    game/yw_ui_map.cpp
    game/yw_ui_hud.cpp
    game/yw_ui_orders.cpp
    game/yw_ui_voice.cpp
    game/yw_ui_windows.cpp
    game/yw_net.cpp
    game/yw_save.cpp
    game/ypabact.cpp
    game/ypabact_ai.cpp
    game/ypabact_user.cpp
    game/ypabact_physics.cpp
    game/ypabact_damage.cpp
    game/ypabact_fire.cpp
    game/ypabact_laser.cpp
    game/ypabact_artillery.cpp
    game/ypabact_kamikaze.cpp
    game/ypabact_carrier.cpp
    game/ypabact_proximity.cpp
    game/ypabact_projectile.cpp
    game/ypabact_recoil.cpp
    game/ypabact_status_fx.cpp
    game/ypabact_weapons.cpp
    game/ypabact_camera.cpp
    game/ypabact_control.cpp
    game/ypacar.cpp
    game/ypaflyer.cpp
    game/ypagun.cpp
    game/ypamissile.cpp
    game/ypamissile_tracer.cpp
    game/ypamissile_chain.cpp
    game/ypamissile_blast.cpp
    game/ypamissile_flight.cpp
    game/yparobo.cpp
    game/yparobo_player.cpp
    game/yparobo_ai.cpp
    game/ypatank.cpp
    game/ypaufo.cpp
)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
add_subdirectory(third_party/bullet)

pkg_search_module(SDL2 REQUIRED sdl2)
link_directories(${SDL2_LIBRARY_DIRS})

pkg_search_module(SDL2_IMAGE REQUIRED SDL2_image)
link_directories(${SDL2_IMAGE_LIBRARY_DIRS})

pkg_search_module(SDL2_TTF REQUIRED SDL2_ttf)
link_directories(${SDL2_TTF_LIBRARY_DIRS})

pkg_search_module(SDL2_NET REQUIRED SDL2_net)
link_directories(${SDL2_NET_LIBRARY_DIRS})

pkg_search_module(VORBISFILE REQUIRED vorbisfile)
link_directories(${VORBISFILE_LIBRARY_DIRS})

# FFmpeg moderno.
# Non cerca le vecchie librerie avresample e postproc.
pkg_check_modules(
    FFMPEG
    REQUIRED
    IMPORTED_TARGET
    libavformat
    libavcodec
    libavutil
    libswscale
    libswresample
)

find_package(OpenGL REQUIRED)
find_package(OpenAL REQUIRED)
find_package(Lua REQUIRED)

if(WIN32)
    enable_language(RC)
    set(
        WIN32_RESOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/../svg/icon/icon.rc"
    )
endif()

if(WIN32)
    add_executable(
        OpenNeoUA
        WIN32
        ${SOURCES}
        ${WIN32_RESOURCES}
    )
else()
    add_executable(
        OpenNeoUA
        ${SOURCES}
    )
endif()

target_link_libraries(
    OpenNeoUA

    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    ${SDL2_NET_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${VORBISFILE_LIBRARIES}
    ${OPENAL_LIBRARY}
    ${LUA_LIBRARIES}

    PkgConfig::FFMPEG
    Threads::Threads
    OpenNeoUACollision
)

if(WIN32)
    target_link_libraries(OpenNeoUA dbghelp)
endif()

target_include_directories(
    OpenNeoUA
    PUBLIC

    ${PROJECT_SOURCE_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
    ${SDL2_NET_INCLUDE_DIRS}
    ${OpenGL_INCLUDE_DIR}
    ${VORBISFILE_INCLUDE_DIRS}
    ${OPENAL_INCLUDE_DIR}
    ${LUA_INCLUDE_DIR}
)

if(CMAKE_BUILD_TYPE MATCHES [dD][eE][bB][uU][gG])
    target_compile_options(
        OpenNeoUA
        PUBLIC
        "-g"
        "-O0"
    )

    if(NOT WIN32)
        target_compile_options(
            OpenNeoUA
            PUBLIC
            "-fsanitize=address"
        )

        target_link_libraries(
            OpenNeoUA
            "-fsanitize=address"
        )
    endif()
endif()

# Non-Debug configurations deliberately keep CMake's native optimization
# flags; RelWithDebInfo must retain -O2 instead of being overridden by -O0.
