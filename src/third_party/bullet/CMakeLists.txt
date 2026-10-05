# Bullet 3.25, upstream commit 2c204c49e56ed15ec5fcfa71d199ab6d6570b3f5.
# Only the CPU collision detector and math library are built.
file(GLOB_RECURSE ONUA_BULLET_COLLISION_SOURCES BulletCollision/*.cpp)
file(GLOB ONUA_BULLET_MATH_SOURCES LinearMath/*.cpp)
list(FILTER ONUA_BULLET_COLLISION_SOURCES EXCLUDE REGEX "btCollisionWorldImporter.cpp$")
add_library(OpenNeoUACollision STATIC ${ONUA_BULLET_COLLISION_SOURCES} ${ONUA_BULLET_MATH_SOURCES})
target_include_directories(OpenNeoUACollision SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(OpenNeoUACollision PROPERTIES CXX_STANDARD 14)
