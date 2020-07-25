set(SOURCE_FILES
    WitnessWriter.cpp
)

add_library(GazerWitness SHARED ${SOURCE_FILES})
target_link_libraries(GazerTrace)