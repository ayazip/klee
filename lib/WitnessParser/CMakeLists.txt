klee_add_component(witnessParser
  WitnessParser.cpp
)

add_subdirectory(../../../yaml-cpp build)

target_link_libraries(witnessParser PUBLIC ${LLVM_LIBS} yaml-cpp)

