# ==============================================================================
# tests/package_consumer/CMakeLists.txt
#
# A separate project that uses an *installed* ActiveRecord through
# find_package(activerecord), the way a downstream user would (docs/hardening.md check 8).
# It is not part of the main build.
# ==============================================================================

cmake_minimum_required(VERSION 3.20)
project(activerecord_package_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  # Must match the installed static library's runtime (/MT, /MTd)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

find_package(activerecord CONFIG REQUIRED)

add_executable(package_consumer main.cpp)
target_link_libraries(package_consumer PRIVATE activerecord::activerecord)

# Same warning policy as the library (cmake/Warnings.cmake): warnings are errors
if(MSVC)
  target_compile_options(package_consumer PRIVATE /W4 /WX /permissive- /EHsc)
else()
  target_compile_options(package_consumer PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
endif()
