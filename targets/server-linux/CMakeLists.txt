message (STATUS "Building sliderule executable")

add_executable (sliderule ${CMAKE_CURRENT_LIST_DIR}/SlideRule.cpp)

set_target_properties (sliderule PROPERTIES VERSION ${TGTVER})
set_target_properties (sliderule PROPERTIES OUTPUT_NAME sliderule)
set_target_properties (sliderule PROPERTIES ENABLE_EXPORTS true)
set_target_properties (sliderule PROPERTIES CXX_STANDARD ${CXX_VERSION})

target_link_libraries (sliderule PUBLIC slideruleLib)

install (TARGETS sliderule DESTINATION ${INSTALLDIR}/bin)
install (FILES ${CMAKE_CURRENT_LIST_DIR}/asset_directory.csv DESTINATION ${CONFDIR})
