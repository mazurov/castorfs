FIND_PACKAGE (Doxygen)
if (NOT DOXYGEN_EXECUTABLE)
  MESSAGE (STATUS "WARNING: Doxygen not found so documentation not generated.")
ENDIF (NOT DOXYGEN_EXECUTABLE)

IF (DOXYGEN_EXECUTABLE AND UNIX)
  
  # N.B. Both the following custom rules assume the doc directory exists
  # at make time, and the following install(DIRECTORY... must have doc exist
  # at cmake time.  Therefore, create the doc directory at CMake time.
  # (Linux experimentation indicates this is a no-op if the empty or
  # non-empty directory already exists.)
  FILE (MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/doc)

  # The initial rm command gets rid of everything previously built by this
  # custom command.
  ADD_CUSTOM_COMMAND (
     OUTPUT ${CMAKE_SOURCE_DIR}/doc/API/html/index.html
     COMMAND rm -rf ${CMAKE_SOURCE_DIR}/doc/API
     COMMAND mkdir ${CMAKE_SOURCE_DIR}/doc/API
     COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
     DEPENDS ${CMAKE_SOURCE_DIR}/Doxyfile
     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )


  ADD_CUSTOM_TARGET (
    documentation
    DEPENDS
    ${CMAKE_SOURCE_DIR}/doc/API/html/index.html
  )
  
  # Install the documentation generated at "make" time.
  # install(DIRECTORY ${CMAKE_SOURCE_DIR}/doc/ DESTINATION ${docdir}/html)
endif(DOXYGEN_EXECUTABLE AND UNIX)


#INCLUDE (CMakeVariables.txt)
