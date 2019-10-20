set(SOURCE_FILES
    TextTraceWriter.cpp
    VerificationResult.cpp
)

add_library(GazerTrace SHARED ${SOURCE_FILES})
target_link_libraries(GazerTrace GazerCore)
