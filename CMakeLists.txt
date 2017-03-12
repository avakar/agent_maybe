cmake_minimum_required(VERSION 3.0)
project(agent_maybe)

include(deps.cmake)

add_executable(agent_maybe
    argparse.cpp argparse.hpp
    chan.hpp win32_chan.cpp
    file.hpp win32_file.cpp
    format.hpp format_impl.hpp guid.cpp guid.hpp
    known_paths.cpp known_paths.hpp
    main.cpp
    process.hpp win32_process.cpp
    tar.hpp tar.cpp
    tls.hpp tls.cpp
    utf.hpp utf.cpp
    win32_socket.hpp win32_socket.cpp
    win32_error.hpp win32_error.cpp)

if(WIN32)
    target_link_libraries(agent_maybe ws2_32)

    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
        target_link_libraries(agent_maybe "${CMAKE_SOURCE_DIR}/${dep_openssl_vc14}/amd64/libcrypto.lib")
        target_link_libraries(agent_maybe "${CMAKE_SOURCE_DIR}/${dep_openssl_vc14}/amd64/libssl.lib")
    else()
        target_link_libraries(agent_maybe "${CMAKE_SOURCE_DIR}/${dep_openssl_vc14}/x86/libcrypto.lib")
        target_link_libraries(agent_maybe "${CMAKE_SOURCE_DIR}/${dep_openssl_vc14}/x86/libssl.lib")
    endif()
endif()

target_include_directories(agent_maybe PRIVATE ${CMAKE_SOURCE_DIR}/${dep_openssl_vc14}/include)
target_link_libraries(agent_maybe nlohmann_json libhttp string_utils string_view zlib_stream)
