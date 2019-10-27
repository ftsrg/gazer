set(SOURCE_FILES
    gazer-cfa.cpp
)

add_executable(gazer-cfa ${SOURCE_FILES})
target_link_libraries(gazer-cfa GazerLLVM)