set(SOURCE_FILES
    BMC.cpp
)

add_library(GazerBMC ${SOURCE_FILES})
target_link_libraries(GazerBMC GazerCore)
