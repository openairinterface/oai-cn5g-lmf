set(LPP_DIR "${SRC_TOP_DIR}/lpp")
include_directories(${LPP_DIR})
file(GLOB LPP_src_files
    ${LPP_DIR}/*.c
    ${LPP_DIR}/*.cpp
)   

add_library(LPP ${LPP_src_files})