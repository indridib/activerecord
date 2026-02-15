function(ar_enable_sanitizers target)
  set(_enabled FALSE)
  set(_flags "")

  if(AR_SANITIZE_ADDRESS)
    list(APPEND _flags -fsanitize=address)
    set(_enabled TRUE)
  endif()

  if(AR_SANITIZE_UNDEFINED)
    list(APPEND _flags -fsanitize=undefined)
    set(_enabled TRUE)
  endif()

  if(AR_SANITIZE_LEAK)
    list(APPEND _flags -fsanitize=leak)
    set(_enabled TRUE)
  endif()

  if(NOT _enabled)
    return()
  endif()

  if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "Sanitizers are only wired for Clang/GCC toolchains. Ignoring sanitizer flags for MSVC.")
    return()
  endif()

  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    message(WARNING "Sanitizers are only supported for Clang/GCC in this project. Compiler: ${CMAKE_CXX_COMPILER_ID}")
    return()
  endif()

  target_compile_options(${target} PRIVATE -fno-omit-frame-pointer ${_flags})
  target_link_options(${target} PRIVATE ${_flags})
endfunction()

