set(SOURCE_FILES
    main.cpp
)

add_executable(gazer-sandbox ${SOURCE_FILES})
target_link_libraries(gazer-sandbox GazerCore GazerZ3Solver)