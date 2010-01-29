CONFIGURE_FILE(castorfs.1.in ${PROJECT_BINARY_DIR}/man/castorfs.1) 
INCLUDE (${PROJECT_SOURCE_DIR}/macros/FindGZIP.cmake)
SET(castorfs_man_src ${PROJECT_BINARY_DIR}/man/castorfs.1)
SET(castorfs_man_gz  ${PROJECT_BINARY_DIR}/castorfs.1.gz)

# Compression of the manuals
ADD_CUSTOM_COMMAND(OUTPUT ${castorfs_man_gz}
                   COMMAND ${GZIP_TOOL} -c ${castorfs_man_src} > ${castorfs_man_gz}
                   DEPENDS ${castorfs_man_src}
                   COMMENT "Building ${castorfs_man_gz}")

ADD_CUSTOM_TARGET(man ALL DEPENDS ${castorfs_man_gz} castorfs)

# Installation of the manuals
INSTALL(FILES ${castorfs_man_gz} DESTINATION "share/man/man1" COMPONENT manual)

#FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/AllVariables.txt "")
#GET_CMAKE_PROPERTY(res VARIABLES)
#FOREACH(var ${res})
#  FILE(APPEND ${CMAKE_CURRENT_BINARY_DIR}/AllVariables.txt 
#               "${var} \"${${var}}\"\n")
#ENDFOREACH(var)  
