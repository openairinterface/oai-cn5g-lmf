set(NRPPA_DIR "${SRC_TOP_DIR}/nrppa")
include_directories(${NRPPA_DIR})
file(GLOB NRPPA_src_files
    ${NRPPA_DIR}/*.c
    ${NRPPA_DIR}/*.cpp
)   

add_library(NRPPA STATIC ${NRPPA_src_files})
#add_definitions(-DASN_EMIT_DEBUG=1)
#add_definitions(-DASN_THREAD_SAFE)
