# ---------------------------------------------------------------------------------------
# IDE support for headers
# ---------------------------------------------------------------------------------------

set(EASYLOCAL_HEADERS_DIR "${CMAKE_CURRENT_LIST_DIR}/../include")
file(GLOB EASYLOCAL_HEADERS "${EASYLOCAL_HEADERS_DIR}/*.hh")
source_group("Header Files\\easylocal" FILES ${EASYLOCAL_HEADERS})