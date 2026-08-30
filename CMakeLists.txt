cmake_minimum_required(VERSION 3.30)
project(quilibrium_cpp_sdk VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(QUILIBRIUM_BUILD_TESTS "Build SDK tests" ON)
option(QUILIBRIUM_BUILD_EXAMPLES "Build SDK examples" ON)
option(QUILIBRIUM_BUILD_C_API "Build stable C ABI shared library" ON)
option(QUILIBRIUM_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(QUILIBRIUM_ENABLE_LIVE_TESTS "Enable credentialed live network integration tests" OFF)

find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCURL REQUIRED IMPORTED_TARGET libcurl)

add_library(quilibrium_sdk)
add_library(Quilibrium::SDK ALIAS quilibrium_sdk)
set_target_properties(quilibrium_sdk PROPERTIES EXPORT_NAME SDK)

target_compile_features(quilibrium_sdk PUBLIC cxx_std_23)
# C++ module BMIs require consumers to use the same language dialect. GNU's
# gnu++23 changes libstdc++ declarations (notably __int128 traits) versus c++23.
target_compile_options(quilibrium_sdk INTERFACE
  $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-std=c++23>)
target_link_libraries(quilibrium_sdk PUBLIC OpenSSL::Crypto PkgConfig::LIBCURL)

target_sources(quilibrium_sdk
    PUBLIC
      FILE_SET cxx_modules TYPE CXX_MODULES FILES
        src/core.cppm
        src/net.cppm
        src/http.cppm
        src/json.cppm
        src/crypto.cppm
        src/sigv4.cppm
        src/protocol.cppm
        src/curl_transport.cppm
        src/token.cppm
        src/hypergraph.cppm
        src/compute.cppm
        src/qstorage.cppm
        src/qkms.cppm
        src/hypersnap.cppm
        src/farcaster.cppm
        src/sdk_facade.cppm
        src/sdk.cppm
    PRIVATE
        src/core.cpp
        src/net.cpp
        src/json.cpp
        src/sigv4.cpp
        src/protocol.cpp
        src/curl_transport.cpp
        src/qstorage.cpp
        src/qkms.cpp
        src/hypersnap.cpp
        src/sdk_facade.cpp
)

if(MSVC)
  target_compile_options(quilibrium_sdk PRIVATE /W4 /permissive-)
  if(QUILIBRIUM_WARNINGS_AS_ERRORS)
    target_compile_options(quilibrium_sdk PRIVATE /WX)
  endif()
else()
  target_compile_options(quilibrium_sdk PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
  if(QUILIBRIUM_WARNINGS_AS_ERRORS)
    target_compile_options(quilibrium_sdk PRIVATE -Werror)
    # GCC 14 Modules TS can emit a spurious -Wattributes diagnostic while
    # loading standard-library declarations from BMIs. Keep project warnings
    # fatal without turning this compiler-internal module diagnostic into an error.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(quilibrium_sdk PRIVATE -Wno-error=attributes)
    endif()
  endif()
endif()

if(APPLE)
  target_compile_definitions(quilibrium_sdk PRIVATE QUILIBRIUM_PLATFORM_APPLE=1)
elseif(WIN32)
  target_compile_definitions(quilibrium_sdk PRIVATE QUILIBRIUM_PLATFORM_WINDOWS=1)
elseif(UNIX)
  target_compile_definitions(quilibrium_sdk PRIVATE QUILIBRIUM_PLATFORM_UNIX=1)
endif()

if(QUILIBRIUM_BUILD_C_API)
  add_library(quilibrium_c SHARED bindings/c/quilibrium_c.cpp)
  add_library(Quilibrium::C ALIAS quilibrium_c)
  set_target_properties(quilibrium_c PROPERTIES EXPORT_NAME C)
  target_link_libraries(quilibrium_c PRIVATE Quilibrium::SDK)
  target_include_directories(quilibrium_c PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bindings/c/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  set_target_properties(quilibrium_c PROPERTIES OUTPUT_NAME quilibrium)
endif()

if(QUILIBRIUM_BUILD_EXAMPLES)
  add_executable(quilibrium_example examples/basic.cpp)
  target_link_libraries(quilibrium_example PRIVATE Quilibrium::SDK)

  add_executable(quilibrium_hypersnap_example examples/hypersnap_client.cpp)
  target_link_libraries(quilibrium_hypersnap_example PRIVATE Quilibrium::SDK)

  add_executable(quilibrium_qstorage_example examples/qstorage.cpp)
  target_link_libraries(quilibrium_qstorage_example PRIVATE Quilibrium::SDK)

  add_executable(quilibrium_qstorage_presign_example examples/qstorage_presign.cpp)
  target_link_libraries(quilibrium_qstorage_presign_example PRIVATE Quilibrium::SDK)

  add_executable(quilibrium_qkms_example examples/qkms.cpp)
  target_link_libraries(quilibrium_qkms_example PRIVATE Quilibrium::SDK)

  add_executable(quilibrium_native_rpc_example examples/native_rpc.cpp)
  target_link_libraries(quilibrium_native_rpc_example PRIVATE Quilibrium::SDK)
endif()

if(QUILIBRIUM_BUILD_TESTS)
  include(CTest)
  add_executable(quilibrium_core_tests tests/core_tests.cpp)
  target_link_libraries(quilibrium_core_tests PRIVATE Quilibrium::SDK)
  add_test(NAME quilibrium_core_tests COMMAND quilibrium_core_tests)

  add_executable(quilibrium_json_tests tests/json_tests.cpp)
  target_link_libraries(quilibrium_json_tests PRIVATE Quilibrium::SDK)
  add_test(NAME quilibrium_json_tests COMMAND quilibrium_json_tests)

  add_executable(quilibrium_facade_tests tests/facade_tests.cpp)
  target_link_libraries(quilibrium_facade_tests PRIVATE Quilibrium::SDK)
  add_test(NAME quilibrium_facade_tests COMMAND quilibrium_facade_tests)

  add_executable(quilibrium_service_tests tests/service_tests.cpp)
  target_link_libraries(quilibrium_service_tests PRIVATE Quilibrium::SDK)
  add_test(NAME quilibrium_service_tests COMMAND quilibrium_service_tests)

  add_executable(quilibrium_presign_tests tests/presign_tests.cpp)
  target_link_libraries(quilibrium_presign_tests PRIVATE Quilibrium::SDK)
  # GCC 14 Modules TS has an optimizer ICE in ipa-comdats when this large
  # deterministic module-consumer test is compiled at Release -O3. Keep the
  # SDK itself fully optimized; lower only this test target to -O1.
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15)
    target_compile_options(quilibrium_presign_tests PRIVATE -O1)
  endif()
  add_test(NAME quilibrium_presign_tests COMMAND quilibrium_presign_tests)

  add_executable(quilibrium_qstorage_presign_live_tests tests/qstorage_presign_live.cpp)
  target_link_libraries(quilibrium_qstorage_presign_live_tests PRIVATE Quilibrium::SDK)
  if(QUILIBRIUM_ENABLE_LIVE_TESTS)
    add_test(NAME quilibrium_qstorage_presign_live_tests COMMAND quilibrium_qstorage_presign_live_tests)
    set_tests_properties(quilibrium_qstorage_presign_live_tests PROPERTIES SKIP_RETURN_CODE 77)
  endif()

  if(QUILIBRIUM_BUILD_C_API)
    add_executable(quilibrium_c_api_tests tests/c_api_tests.c)
    target_link_libraries(quilibrium_c_api_tests PRIVATE Quilibrium::C)
    add_test(NAME quilibrium_c_api_tests COMMAND quilibrium_c_api_tests)

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
      add_test(NAME quilibrium_python_binding_tests
        COMMAND ${CMAKE_COMMAND} -E env
          QUILIBRIUM_SDK_LIB=$<TARGET_FILE:quilibrium_c>
          PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/bindings/python
          ${Python3_EXECUTABLE} -c "from quilibrium_sdk import SDK, version; assert version() == '1.1.0'; q=SDK(); q.close()")
    endif()
  endif()
endif()

install(TARGETS quilibrium_sdk
  EXPORT QuilibriumTargets
  FILE_SET cxx_modules DESTINATION ${CMAKE_INSTALL_LIBDIR}/quilibrium/modules
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(QUILIBRIUM_BUILD_C_API)
  install(TARGETS quilibrium_c
    EXPORT QuilibriumTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES bindings/c/include/quilibrium.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

install(EXPORT QuilibriumTargets
  NAMESPACE Quilibrium::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Quilibrium)

configure_package_config_file(
  cmake/QuilibriumConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/QuilibriumConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Quilibrium)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/QuilibriumConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/QuilibriumConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/QuilibriumConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Quilibrium)
