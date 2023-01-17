cmake_minimum_required( VERSION 3.10 )

project(gupty)

set(CMAKE_CXX_STANDARD 20)

set(Boost_USE_STATIC_LIBS ON)
find_package( Boost REQUIRED COMPONENTS log program_options container )

add_library( libgupty )
target_sources(
libgupty
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/session.cpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/lines.cpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/keycodes.cpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/mode_insert.cpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/mode_command.cpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/mode_passthrough.cpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/mode_auto.cpp>
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/libgupty.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/session.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/lines.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/keycodes.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/keymap.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/mode_insert.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/mode_command.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/mode_passthrough.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/mode_auto.h>
)
target_include_directories( libgupty
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/>
)
target_link_libraries( libgupty PUBLIC Boost::boost Boost::log Boost::container )


add_executable( gupty src/gupty.cpp )
target_link_libraries( gupty
    PRIVATE
        libgupty
        Boost::boost
        Boost::program_options
)

install( TARGETS gupty DESTINATION bin )
install( FILES PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE DESTINATION bin )

