
set(NRPPA_DIR "${SRC_TOP_DIR}/nrppa")
include_directories(${NRPPA_DIR})
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/utils)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/utils/bstr)

file(GLOB NRPPA_src_files
    ${NRPPA_DIR}/*.c
    ${NRPPA_DIR}/*.cpp
)   

add_library(NRPPA STATIC ${NRPPA_src_files})
target_compile_definitions(NRPPA PUBLIC ASN_DISABLE_OER_SUPPORT)
#add_definitions("-DASN_EMIT_DEBUG=1")
#add_definitions("-DASN_THREAD_SAFE")
