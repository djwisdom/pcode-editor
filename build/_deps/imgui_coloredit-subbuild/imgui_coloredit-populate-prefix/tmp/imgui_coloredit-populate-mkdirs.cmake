# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/djwisdom/pcode-editor/build/_deps/imgui_coloredit-src")
  file(MAKE_DIRECTORY "/home/djwisdom/pcode-editor/build/_deps/imgui_coloredit-src")
endif()
file(MAKE_DIRECTORY
  "/home/djwisdom/pcode-editor/build/_deps/imgui_coloredit-build"
  "/home/djwisdom/pcode-editor/build/_deps/imgui_coloredit-subbuild/imgui_coloredit-populate-prefix"
  "/home/djwisdom/pcode-editor/build/_deps/imgui_coloredit-subbuild/imgui_coloredit-populate-prefix/tmp"
  "/home/djwisdom/pcode-editor/build/_deps/imgui_coloredit-subbuild/imgui_coloredit-populate-prefix/src/imgui_coloredit-populate-stamp"
  "/home/djwisdom/pcode-editor/build/_deps/imgui_coloredit-subbuild/imgui_coloredit-populate-prefix/src"
  "/home/djwisdom/pcode-editor/build/_deps/imgui_coloredit-subbuild/imgui_coloredit-populate-prefix/src/imgui_coloredit-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/djwisdom/pcode-editor/build/_deps/imgui_coloredit-subbuild/imgui_coloredit-populate-prefix/src/imgui_coloredit-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/djwisdom/pcode-editor/build/_deps/imgui_coloredit-subbuild/imgui_coloredit-populate-prefix/src/imgui_coloredit-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
