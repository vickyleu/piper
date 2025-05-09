cmake_minimum_required(VERSION 3.13)

project(piper C CXX)

file(READ "${CMAKE_CURRENT_LIST_DIR}/VERSION" piper_version)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  # Force compiler to use UTF-8 for IPA constants
  add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
  add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
elseif(NOT APPLE)
  # Linux flags
  string(APPEND CMAKE_CXX_FLAGS " -Wall -Wextra -Wl,-rpath,'$ORIGIN'")
  string(APPEND CMAKE_C_FLAGS " -Wall -Wextra")
endif()
add_library(piper_lib STATIC src/cpp/piper.cpp src/cpp/piper_wrapper.cpp)
add_executable(piper src/cpp/main.cpp)
add_executable(test_piper src/cpp/test.cpp)

# NOTE: external project prefix are shortened because of path length restrictions on Windows
# NOTE: onnxruntime is pulled from piper-phonemize

# ---- fmt ---

if(NOT DEFINED FMT_DIR)
  set(FMT_VERSION "10.0.0")
  set(FMT_DIR "${CMAKE_CURRENT_BINARY_DIR}/fi")
  # 获取当前工具链文件的绝对路径
  get_filename_component(TOOLCHAIN_FILE_ABS "${CMAKE_TOOLCHAIN_FILE}" ABSOLUTE)
  include(ExternalProject)
  ExternalProject_Add(
    fmt_external
    PREFIX "${CMAKE_CURRENT_BINARY_DIR}/f"
    URL "https://github.com/fmtlib/fmt/archive/refs/tags/${FMT_VERSION}.zip"
    CMAKE_ARGS
          -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${TOOLCHAIN_FILE_ABS}
          -DCMAKE_INSTALL_PREFIX:FILEPATH=${FMT_DIR}
          -DFMT_TEST:BOOL=OFF
          --no-warn-unused-cli  # 抑制无关警告
  )
  add_dependencies(piper fmt_external)
  add_dependencies(test_piper fmt_external)
endif()

# ---- soxr ---

if(NOT DEFINED SOXR_DOWNLOAD_DIR)
set(SOXR_VERSION "0.1.3")
set(SOXR_DIR "${CMAKE_CURRENT_BINARY_DIR}/soxr_install")
set(SOXR_DOWNLOAD_DIR "${CMAKE_CURRENT_BINARY_DIR}/soxr")
# 获取当前工具链文件的绝对路径
get_filename_component(TOOLCHAIN_FILE_ABS "${CMAKE_TOOLCHAIN_FILE}" ABSOLUTE)
include(ExternalProject)
ExternalProject_Add(
    soxr_external
    PREFIX "${SOXR_DOWNLOAD_DIR}"
    URL "https://sourceforge.net/projects/soxr/files/soxr-${SOXR_VERSION}-Source.tar.xz"
    CMAKE_ARGS
    -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${TOOLCHAIN_FILE_ABS}
    -DCMAKE_INSTALL_PREFIX:FILEPATH=${SOXR_DIR}
    -DBUILD_SHARED_LIBS:BOOL=OFF
    -DBUILD_TESTS:BOOL=OFF
    -DBUILD_EXAMPLES:BOOL=OFF
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
    --no-warn-unused-cli  # 抑制无关警告
)
add_dependencies(piper soxr_external)
add_dependencies(test_piper soxr_external)
endif()


# ---- spdlog ---

if(NOT DEFINED SPDLOG_DIR)
  set(SPDLOG_DIR "${CMAKE_CURRENT_BINARY_DIR}/si")
  set(SPDLOG_VERSION "1.12.0")
  # 获取当前工具链文件的绝对路径
  get_filename_component(TOOLCHAIN_FILE_ABS "${CMAKE_TOOLCHAIN_FILE}" ABSOLUTE)
  ExternalProject_Add(
    spdlog_external
    PREFIX "${CMAKE_CURRENT_BINARY_DIR}/s"
    URL "https://github.com/vickyleu/spdlog/archive/refs/tags/1.0.0.zip"
    CMAKE_ARGS
          -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE_ABS}
          -DCMAKE_INSTALL_PREFIX=${SPDLOG_DIR}
          -DSPDLOG_BUILD_SHARED:BOOL=OFF
          --no-warn-unused-cli  # 抑制无关警告
  )
  add_dependencies(piper spdlog_external)
  add_dependencies(test_piper spdlog_external)
endif()

# ---- piper-phonemize ---

if(NOT DEFINED PIPER_PHONEMIZE_DIR)
  set(PIPER_PHONEMIZE_DIR "${CMAKE_CURRENT_BINARY_DIR}/pi")
  # 获取当前工具链文件的绝对路径
  get_filename_component(TOOLCHAIN_FILE_ABS "${CMAKE_TOOLCHAIN_FILE}" ABSOLUTE)
  MESSAGE("马勒戈壁 CMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS} CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}")
  ExternalProject_Add(
    piper_phonemize_external
    PREFIX "${CMAKE_CURRENT_BINARY_DIR}/p"
    URL "https://github.com/vickyleu/piper-phonemize/archive/refs/heads/master.zip"
    CMAKE_ARGS
          -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE_ABS}
          -DCMAKE_INSTALL_PREFIX:PATH=${PIPER_PHONEMIZE_DIR}
          -DBUILD_SHARED_LIBS:BOOL=OFF
          --no-warn-unused-cli  # 抑制无关警告
  )
  add_dependencies(piper piper_phonemize_external)
  add_dependencies(test_piper piper_phonemize_external)
endif()

# ---- Declare executable ----

if((NOT MSVC) AND (NOT APPLE))
  # Linux flags
  string(APPEND CMAKE_CXX_FLAGS "-D_GLIBCXX_USE_CXX11_ABI=1 -Wall -Wextra -Wl,-rpath,'$ORIGIN'")
  string(APPEND CMAKE_C_FLAGS " -Wall -Wextra")
  target_link_options(piper_lib PRIVATE -static-libgcc -static-libstdc++)
  set(PIPER_EXTRA_LIBRARIES "pthread")
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)            # 优先使用 -pthread
find_package(Threads REQUIRED)                 # 查找线程库
# 使用关键字签名，避免 plain/keyword 混用
target_link_libraries(piper PRIVATE piper_lib)
target_link_libraries(test_piper PRIVATE piper_lib)

target_link_libraries(piper_lib PRIVATE
  fmt
  spdlog
  soxr
  espeak-ng
  piper_phonemize
  onnxruntime
  Threads::Threads    # 推荐用 Threads 模块链接 pthread
  dl
  ${PIPER_EXTRA_LIBRARIES}
)
add_dependencies(piper_lib fmt_external)
add_dependencies(piper_lib soxr_external)
add_dependencies(piper_lib spdlog_external)
add_dependencies(piper_lib piper_phonemize_external)

target_link_directories(piper_lib PUBLIC
  ${FMT_DIR}/lib
  ${SPDLOG_DIR}/lib
  ${SOXR_DIR}/lib
  ${PIPER_PHONEMIZE_DIR}/lib
)

target_include_directories(piper_lib PUBLIC
  ${FMT_DIR}/include
  ${SPDLOG_DIR}/include
  ${SOXR_DIR}/include
  ${PIPER_PHONEMIZE_DIR}/include
)

target_compile_definitions(piper_lib PUBLIC _PIPER_VERSION=${piper_version})

# ---- Declare test ----
include(CTest)
enable_testing()
add_test(
  NAME test_piper
  COMMAND test_piper "${CMAKE_SOURCE_DIR}/etc/test_voice.onnx" "${PIPER_PHONEMIZE_DIR}/share/espeak-ng-data" "${CMAKE_CURRENT_BINARY_DIR}/test.wav"
)

target_compile_features(test_piper PUBLIC cxx_std_17)

target_include_directories(
  test_piper PUBLIC
  ${FMT_DIR}/include
  ${SPDLOG_DIR}/include
  ${SOXR_DIR}/include
  ${PIPER_PHONEMIZE_DIR}/include
)

target_link_directories(
  test_piper PUBLIC
  ${FMT_DIR}/lib
  ${SPDLOG_DIR}/lib
  ${SOXR_DIR}/lib
  ${PIPER_PHONEMIZE_DIR}/lib
)

target_link_libraries(test_piper PUBLIC
  fmt
  spdlog
  soxr
  espeak-ng
  piper_phonemize
  onnxruntime
)

# ---- Declare install targets ----

install(
  TARGETS piper
  DESTINATION ${CMAKE_INSTALL_PREFIX})

# Dependencies
install(
  DIRECTORY ${PIPER_PHONEMIZE_DIR}/bin/
  DESTINATION ${CMAKE_INSTALL_PREFIX}
  USE_SOURCE_PERMISSIONS  # keep +x
  FILES_MATCHING
  PATTERN "piper_phonemize"
  PATTERN "espeak-ng"
  PATTERN "*.dll"
)

install(
  DIRECTORY ${PIPER_PHONEMIZE_DIR}/lib/
  DESTINATION ${CMAKE_INSTALL_PREFIX}
  FILES_MATCHING
  PATTERN "*.dll"
  PATTERN "*.so"
  PATTERN "*.a"
)
install(
  DIRECTORY ${SOXR_DIR}/lib/
  DESTINATION ${CMAKE_INSTALL_PREFIX}
  FILES_MATCHING
  PATTERN "*.dll"
  PATTERN "*.so"
  PATTERN "*.a"
)
# 添加静态库到安装目标
install(
        TARGETS piper piper_lib
        DESTINATION ${CMAKE_INSTALL_PREFIX}
)
install(
  DIRECTORY ${PIPER_PHONEMIZE_DIR}/../p/src/piper_phonemize_external-build/e/src/espeak_ng_external/espeak-ng-data
  DESTINATION ${CMAKE_INSTALL_PREFIX}
)

install(
  FILES ${PIPER_PHONEMIZE_DIR}/share/libtashkeel_model.ort
  DESTINATION ${CMAKE_INSTALL_PREFIX}
)
install(
  FILES ${CMAKE_CURRENT_LIST_DIR}/merge.sh
  DESTINATION ${CMAKE_INSTALL_PREFIX}
)
