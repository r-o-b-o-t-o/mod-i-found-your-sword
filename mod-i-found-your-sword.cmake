target_include_directories(modules PUBLIC "mod-i-found-your-sword/libs")

# The module database connection sizes MySQLConnection's prepared statement container, which
# needs the complete MySQLPreparedStatement type and, through it, mysql.h. The core keeps that
# dependency private to its database library, so the module has to pull it in itself.
target_link_libraries(modules PRIVATE mysql)

# AP_WebSocketClient.cpp instantiates the boost::beast websocket stream templates and
# emits ~86k COMDAT sections, past the 65,279 limit of MSVC's default object format
# (fatal error C1128). AzerothCore only appends /bigobj to CMAKE_CXX_FLAGS_DEBUG in
# src/cmake/compiler/msvc/settings.cmake, so Release, RelWithDebInfo and MinSizeRel
# builds have to opt in here.
if(MSVC)
  file(GLOB_RECURSE MOD_ARCHIPELAWOW_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/mod-i-found-your-sword/src/*.cpp")
  set_source_files_properties(${MOD_ARCHIPELAWOW_SOURCES} PROPERTIES COMPILE_OPTIONS "/bigobj")
endif()
