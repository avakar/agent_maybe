cmake_minimum_required(VERSION 3.0)
project(agent_maybe)

include(deps.cmake)

if(WIN32)
    set(platform_sources
        utf.hpp utf.cpp
        win32_chan.cpp win32_file.cpp
        win32_process.cpp win32_error.hpp win32_error.cpp)
else()
    set(platform_sources
        posix_chan.cpp
        posix_process.cpp
        posix_file.cpp)
endif()

add_executable(agent_maybe
    argparse.cpp argparse.hpp
    chan.hpp
    file.hpp
    format.hpp format_impl.hpp guid.cpp guid.hpp
    known_paths.cpp known_paths.hpp
    main.cpp
    process.hpp
    tar.hpp tar.cpp
    tls.hpp tls.cpp
    ${platform_sources}
    )

if(WIN32)
    target_link_libraries(agent_maybe ws2_32)

    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
        target_link_libraries(agent_maybe "${CMAKE_SOURCE_DIR}/${dep_openssl_vc14}/amd64/libcrypto.lib")
        target_link_libraries(agent_maybe "${CMAKE_SOURCE_DIR}/${dep_openssl_vc14}/amd64/libssl.lib")
    else()
        target_link_libraries(agent_maybe "${CMAKE_SOURCE_DIR}/${dep_openssl_vc14}/x86/libcrypto.lib")
        target_link_libraries(agent_maybe "${CMAKE_SOURCE_DIR}/${dep_openssl_vc14}/x86/libssl.lib")
    endif()

else()
    find_package(OpenSSL REQUIRED)

    target_include_directories(agent_maybe PRIVATE ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(agent_maybe ${OPENSSL_LIBRARIES})
endif()

target_include_directories(agent_maybe PRIVATE ${CMAKE_SOURCE_DIR}/${dep_openssl_vc14}/include)
target_link_libraries(agent_maybe nlohmann_json libhttp string_utils string_view zlib_stream)
set_property(TARGET agent_maybe PROPERTY CXX_STANDARD 14)
