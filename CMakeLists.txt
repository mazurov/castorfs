# The name of our project is "HELLO". CMakeLists files in this project can
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and
# to the root binary directory of the project as ${HELLO_BINARY_DIR}.
cmake_minimum_required (VERSION 2.6)
PROJECT (castorfs)

SET (DATE "")
SET (CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/macros")
EXECUTE_PROCESS(COMMAND "/bin/date -R" OUTPUT_VARIABLE DATE OUTPUT_STRIP_TRAILING_WHITESPACE)

# Recurse into the "Hello" and "Demo" subdirectories. This does not actually
# cause another cmake executable to run. The same process will walk through
# the project's entire directory structure.
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
INCLUDE (macros/Version.cmake)
CONFIGURE_FILE(config.h.in ${PROJECT_SOURCE_DIR}/config.h) 

ADD_SUBDIRECTORY (man)
ADD_SUBDIRECTORY (init)
ADD_SUBDIRECTORY (src)
# add_subdirectory (tests) 

INCLUDE (InstallRequiredSystemLibraries)

IF ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
  SET (CPACK_DEBIAN_SYSTEM_PROCESSOR "amd64")
ELSE ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
  SET (CPACK_DEBIAN_SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})
ENDIF ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")

SET(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}_${CPACK_DEBIAN_SYSTEM_PROCESSOR}")
SET(CPACK_PACKAGE_INSTALL_DIRECTORY "castorfs-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}")
SET(CPACK_SOURCE_IGNORE_FILES "Makefile;CMakeCache.txt;CMakeCache.txt;cmake_install.cmake;CPackConfig.cmake;CPackSourceConfig.cmake;install_manifest.txt;.*.tar.gz;CMakeFiles;_CPack_Packages;.*.dir;.*.(swp|swo);configure.ac;.deps;.*.deb;.*.rpm;.*.exe;Testing;RPM/.*;bin/.*")
SET(CPACK_GENERATOR "TGZ;DEB;")
SET(CPACK_SOURCE_GENERATOR "TGZ")
SET(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/README")
SET(CPACK_DEBIAN_PACKAGE_DEPENDS "fuse-utils (>= 2.7.4), castor-lib  (>= 2.1.9)")


#INCLUDE (Doxygen)
INCLUDE (CPack)
INCLUDE (UseRPMTools)

RPMTools_ADD_RPM_TARGETS(castorfs "${PROJECT_SOURCE_DIR}/castorfs.spec.in")
ADD_DEPENDENCIES (castorfs_rpm man)
ADD_DEPENDENCIES (castorfs_srpm man)



#ENABLE_TESTING()
#INCLUDE(Dart)

