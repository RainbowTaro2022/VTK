if ("$ENV{CI_JOB_NAME}" MATCHES "windows")
  set(CMAKE_C_COMPILER_LAUNCHER "buildcache" CACHE STRING "")
  set(CMAKE_CXX_COMPILER_LAUNCHER "buildcache" CACHE STRING "")
  set(CMAKE_CUDA_COMPILER_LAUNCHER "buildcache" CACHE STRING "")
  set(vtk_replace_uncacheable_flags ON CACHE BOOL "")
else ()
  set(CMAKE_C_COMPILER_LAUNCHER "sccache" CACHE STRING "")
  set(CMAKE_CXX_COMPILER_LAUNCHER "sccache" CACHE STRING "")
  set(CMAKE_CUDA_COMPILER_LAUNCHER "sccache" CACHE STRING "")
endif ()