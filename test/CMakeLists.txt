if (DEFINED GTEST_PATH)
    link_directories(${GTEST_PATH}/lib)
endif (DEFINED GTEST_PATH)

set(GLIB_LIBRARIES "glib-2.0" CACHE STRING "GLib libraries")

# Compiling plugin sources again is not optimal but it's the easy way
add_executable(tests EXCLUDE_FROM_ALL
    test-main.cpp
    login-test.cpp
    private-chat-test.cpp
    group-chat-test.cpp
    supergroup-test.cpp
    file-transfer-test.cpp
    secret-chat-test.cpp
    message-split-test.cpp
    message-order-test.cpp
    test-transceiver.cpp
    libpurple-mock.cpp
    printout.cpp
    purple-events.cpp
    fixture.cpp

    ../tdlib-purple.cpp
    ../td-client.cpp
    ../transceiver.cpp
    ../account-data.cpp
    ../purple-info.cpp
    ${CMAKE_BINARY_DIR}/config.cpp
    ../client-utils.cpp
    ../receiving.cpp
    ../format.cpp
    ../sticker.cpp
    ../file-transfer.cpp
    ../call.cpp
    ../identifiers.cpp
    ../secret-chat.cpp
)

set_property(TARGET tests PROPERTY CXX_STANDARD 14)
target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tests PRIVATE gtest fmt::fmt Td::TdStatic ${GLIB_LIBRARIES})

if (DEFINED GTEST_PATH)
    target_include_directories(tests PRIVATE ${GTEST_PATH}/include)
    link_directories(${GTEST_PATH}/lib)
endif (DEFINED GTEST_PATH)

if (NOT NoWebp)
    target_link_libraries(tests PRIVATE ${libwebp_LIBRARIES} ${libpng_LIBRARIES})
endif (NOT NoWebp)

if (NOT NoLottie)
    if (NOT NoBundledLottie)
        target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR}/rlottie/inc)
    endif (NOT NoBundledLottie)
    target_link_libraries(tests PRIVATE rlottie)
    target_compile_definitions(tests PRIVATE LOT_BUILD)
endif (NOT NoLottie)

if (NOT NoTranslations)
    target_include_directories(tests PRIVATE ${Intl_INCLUDE_DIRS})
    target_link_libraries(tests PRIVATE ${Intl_LIBRARIES})
endif (NOT NoTranslations)

if (NOT tgvoip_INCLUDE_DIRS STREQUAL "")
    target_include_directories(tests SYSTEM PRIVATE ${tgvoip_INCLUDE_DIRS})
endif (NOT tgvoip_INCLUDE_DIRS STREQUAL "")
if (NOT NoVoip)
    target_link_libraries(tests PRIVATE ${tgvoip_LIBRARIES})
endif (NOT NoVoip)

add_custom_target(run-tests ${CMAKE_CURRENT_BINARY_DIR}/tests DEPENDS tests)
