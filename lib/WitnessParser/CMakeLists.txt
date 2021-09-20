#===------------------------------------------------------------------------===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
klee_add_component(witnessParser
  WitnessParser.cpp
)

target_link_libraries(witnessParser PUBLIC ${LLVM_LIBS})

