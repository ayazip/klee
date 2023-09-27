klee_add_component(witnessChecker
  Witness.cpp
)

add_subdirectory(../../../yaml-cpp ../../../../yaml-cpp/build)

target_link_libraries(witnessChecker PUBLIC ${LLVM_LIBS} yaml-cpp)

