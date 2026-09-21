include(BoostTestTargets)

# The aa-*/ab-* tests are integration tests: each test binary is a game client
# that expects a headless game server listening on localhost:32222 and hosting
# the multiplayer level named after the two-letter test prefix
# (levels/multiplayer/aa.level, ab.level, ...). The 00-* tests are plain unit
# tests and need no server.
# On Unix we wrap the integration tests in a launcher script that starts and
# stops that server, so they can be run through a plain `ctest` (or by running
# scripts/unix/run_unit_tests.sh from the build directory). On Windows, use
# scripts/win32/OpenDungeonsTests.bat instead.
if(UNIX)
    set(SERVER_LAUNCHER_AA LAUNCHER
        ${CMAKE_SOURCE_DIR}/scripts/unix/run_boost_test_with_server.sh
        $<TARGET_FILE:${PROJECT_BINARY_NAME}> aa.level)
    set(SERVER_LAUNCHER_AB LAUNCHER
        ${CMAKE_SOURCE_DIR}/scripts/unix/run_boost_test_with_server.sh
        $<TARGET_FILE:${PROJECT_BINARY_NAME}> ab.level)
else()
    set(SERVER_LAUNCHER_AA)
    set(SERVER_LAUNCHER_AB)
endif()

add_boost_test(00-Random
        SOURCES
        test_Random.cpp
        ${SRC}/utils/Random.h
        ${SRC}/utils/Random.cpp)

add_boost_test(00-ODPacket
        SOURCES
        test_ODPacket.cpp
        ${SRC}/network/ODPacket.h
        ${SRC}/network/ODPacket.cpp
        LIBRARIES
        ${SFML_LIBRARIES})

add_boost_test(00-ConsoleInterface
        SOURCES
        test_ConsoleInterface.cpp
        ${SRC}/modes/AbstractModeManager.h
        ${SRC}/modes/ConsoleInterface.h
        ${SRC}/modes/ConsoleInterface.cpp
        ${SRC}/modes/Command.h
        ${SRC}/modes/Command.cpp)

add_boost_test(00-Goal
        SOURCES
        test_Goal.cpp
        ${SRC}/goals/Goal.cpp
        ${SRC}/utils/LogManager.cpp
        ${SRC}/utils/LogSinkConsole.cpp
        LIBRARIES
        ${OGRE_LIBRARIES}
        ${SFML_LIBRARIES}
        ${Boost_FILESYSTEM_LIBRARY_RELEASE}
        ${Boost_SYSTEM_LIBRARY_RELEASE})

add_boost_test(00-Pathfinding
        SOURCES
        test_Pathfinding.cpp)

add_boost_test(aa-LaunchGame
        ${SERVER_LAUNCHER_AA}
        SOURCES
        ${SRC}/tests/mocks/ODClientTest.cpp
        ${SRC}/game/SeatData.cpp
        ${SRC}/game/SkillType.cpp
        ${SRC}/network/ClientNotification.cpp
        ${SRC}/network/ODPacket.cpp
        ${SRC}/network/ODSocketClient.cpp
        ${SRC}/network/ODSocketServer.cpp
        ${SRC}/network/ServerMode.cpp
        ${SRC}/network/ServerNotification.cpp
        ${SRC}/utils/Helper.cpp
        ${SRC}/utils/LogManager.cpp
        ${SRC}/utils/LogSinkConsole.cpp
        test_LaunchGame.cpp
        LIBRARIES
        ${SFML_LIBRARIES}
        ${Boost_FILESYSTEM_LIBRARY_RELEASE}
        ${Boost_SYSTEM_LIBRARY_RELEASE}
        ${OGRE_LIBRARIES})

add_boost_test(aa-TestCreatures
        ${SERVER_LAUNCHER_AA}
        SOURCES
        ${SRC}/tests/mocks/ODClientTest.cpp
        ${SRC}/game/SeatData.cpp
        ${SRC}/game/SkillType.cpp
        ${SRC}/network/ClientNotification.cpp
        ${SRC}/network/ODPacket.cpp
        ${SRC}/network/ODSocketClient.cpp
        ${SRC}/network/ODSocketServer.cpp
        ${SRC}/network/ServerMode.cpp
        ${SRC}/network/ServerNotification.cpp
        ${SRC}/utils/Helper.cpp
        ${SRC}/utils/LogManager.cpp
        ${SRC}/utils/LogSinkConsole.cpp
        test_Creatures.cpp
        LIBRARIES
        ${SFML_LIBRARIES}
        ${Boost_FILESYSTEM_LIBRARY_RELEASE}
        ${Boost_SYSTEM_LIBRARY_RELEASE}
        ${OGRE_LIBRARIES})

add_boost_test(aa-TestRooms
        ${SERVER_LAUNCHER_AA}
        SOURCES
        ${SRC}/tests/mocks/ODClientTest.cpp
        ${SRC}/game/SeatData.cpp
        ${SRC}/game/SkillType.cpp
        ${SRC}/network/ClientNotification.cpp
        ${SRC}/network/ODPacket.cpp
        ${SRC}/network/ODSocketClient.cpp
        ${SRC}/network/ODSocketServer.cpp
        ${SRC}/network/ServerMode.cpp
        ${SRC}/network/ServerNotification.cpp
        ${SRC}/rooms/RoomType.cpp
        ${SRC}/utils/Helper.cpp
        ${SRC}/utils/LogManager.cpp
        ${SRC}/utils/LogSinkConsole.cpp
        test_Rooms.cpp
        LIBRARIES
        ${SFML_LIBRARIES}
        ${Boost_FILESYSTEM_LIBRARY_RELEASE}
        ${Boost_SYSTEM_LIBRARY_RELEASE}
        ${OGRE_LIBRARIES})

add_boost_test(aa-TestRoomSplit
        ${SERVER_LAUNCHER_AA}
        SOURCES
        ${SRC}/tests/mocks/ODClientTest.cpp
        ${SRC}/game/SeatData.cpp
        ${SRC}/game/SkillType.cpp
        ${SRC}/network/ClientNotification.cpp
        ${SRC}/network/ODPacket.cpp
        ${SRC}/network/ODSocketClient.cpp
        ${SRC}/network/ODSocketServer.cpp
        ${SRC}/network/ServerMode.cpp
        ${SRC}/network/ServerNotification.cpp
        ${SRC}/rooms/RoomType.cpp
        ${SRC}/utils/Helper.cpp
        ${SRC}/utils/LogManager.cpp
        ${SRC}/utils/LogSinkConsole.cpp
        test_RoomSplit.cpp
        LIBRARIES
        ${SFML_LIBRARIES}
        ${Boost_FILESYSTEM_LIBRARY_RELEASE}
        ${Boost_SYSTEM_LIBRARY_RELEASE}
        ${OGRE_LIBRARIES})

add_boost_test(ab-TestTraps
        ${SERVER_LAUNCHER_AB}
        SOURCES
        ${SRC}/tests/mocks/ODClientTest.cpp
        ${SRC}/game/SeatData.cpp
        ${SRC}/game/SkillType.cpp
        ${SRC}/network/ClientNotification.cpp
        ${SRC}/network/ODPacket.cpp
        ${SRC}/network/ODSocketClient.cpp
        ${SRC}/network/ODSocketServer.cpp
        ${SRC}/network/ServerMode.cpp
        ${SRC}/network/ServerNotification.cpp
        ${SRC}/rooms/RoomType.cpp
        ${SRC}/utils/Helper.cpp
        ${SRC}/utils/LogManager.cpp
        ${SRC}/utils/LogSinkConsole.cpp
        test_Traps.cpp
        LIBRARIES
        ${SFML_LIBRARIES}
        ${Boost_FILESYSTEM_LIBRARY_RELEASE}
        ${Boost_SYSTEM_LIBRARY_RELEASE}
        ${OGRE_LIBRARIES})


if(UNIX)
    # The integration tests all use the same server port: prevent ctest from
    # running two of them concurrently (e.g. with ctest -j).
    set_tests_properties(
        aa-LaunchGame-boost_test
        aa-TestCreatures-boost_test
        aa-TestRooms-boost_test
        aa-TestRoomSplit-boost_test
        ab-TestTraps-boost_test
        PROPERTIES RESOURCE_LOCK od-test-server)
endif()
