find_program(DOXYGEN NAMES doxygen)
execute_process(
	COMMAND ${DOXYGEN} ${CMAKE_CURRENT_SOURCE_DIR}/libmkcal.cfg; ${CMAKE_CURRENT_SOURCE_DIR}/doc/xmlize.pl;
)

install(DIRECTORY html
	DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/libmkcal-doc)
