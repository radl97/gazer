set(SOURCE_FILES
    WitnessWriter.cpp
    sha256.cpp
)

add_library(GazerWitness SHARED ${SOURCE_FILES})
target_link_libraries(GazerWitness GazerTrace)