# 这是专门为您的环境配置的工具链文件
# arm-linux-toolchain.cmake

# 基本路径设置
set(TOOLCHAIN_DIR "/home/vickyleu/build/arm-linux-gnueabihf")
set(TARGET_TRIPLE "arm-buildroot-linux-gnueabihf")
set(COMPILER_VERSION "9.3.0")

# 基本编译器设置
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7)

# 指定交叉编译器
set(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}/bin/${TARGET_TRIPLE}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/bin/${TARGET_TRIPLE}-g++)

# 设置sysroot
set(CMAKE_SYSROOT ${TOOLCHAIN_DIR}/${TARGET_TRIPLE}/sysroot)
set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})

# 查找程序、库和包的设置
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 定义关键路径
set(GCC_INCLUDE_DIR "${TOOLCHAIN_DIR}/lib/gcc/${TARGET_TRIPLE}/${COMPILER_VERSION}/include")
set(CXX_INCLUDE_DIR "${TOOLCHAIN_DIR}/${TARGET_TRIPLE}/include/c++/${COMPILER_VERSION}")
set(TARGET_INCLUDE_DIR "${CXX_INCLUDE_DIR}/${TARGET_TRIPLE}")

# 自定义编译标志
set(CMAKE_C_FLAGS_INIT "-nostdinc \
-isystem ${GCC_INCLUDE_DIR} \
-isystem ${GCC_INCLUDE_DIR}-fixed \
-isystem ${TOOLCHAIN_DIR}/${TARGET_TRIPLE}/libc/usr/include \
-isystem ${CMAKE_SYSROOT}/usr/include \
 -march=armv7-a -mfloat-abi=hard  -ldl -pthread -mfpu=neon \
" CACHE STRING "C compiler flags")

set(CMAKE_CXX_FLAGS_INIT "-nostdinc -nostdinc++ \
-isystem ${CXX_INCLUDE_DIR} \
-isystem ${TARGET_INCLUDE_DIR} \
-isystem ${CXX_INCLUDE_DIR}/backward \
-isystem ${GCC_INCLUDE_DIR} \
-isystem ${GCC_INCLUDE_DIR}-fixed \
-isystem ${TOOLCHAIN_DIR}/${TARGET_TRIPLE}/libc/usr/include \
-isystem ${CMAKE_SYSROOT}/usr/include \
  -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=1 -ldl -pthread -march=armv7-a -mfloat-abi=hard -mfpu=neon \
" CACHE STRING "C++ compiler flags")

# 链接器标志
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--sysroot=${CMAKE_SYSROOT}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-Wl,--sysroot=${CMAKE_SYSROOT}")

# 解决特定问题的补丁
add_definitions(-D_GNU_SOURCE)
# 不使用内建函数可能会解决ARM NEON问题
add_definitions(-fno-builtin -fno-tree-vectorize)

# 将原生编译器标志传递给CMAKE变量
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_INIT}" CACHE STRING "C flags" FORCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_INIT}" CACHE STRING "CXX flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS_INIT}  -pthread -ldl" CACHE STRING "Linker flags" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS_INIT}  -pthread -ldl" CACHE STRING "Shared linker flags" FORCE)