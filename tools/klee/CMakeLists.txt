#===------------------------------------------------------------------------===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
add_executable(witch-klee
  main.cpp
)

set(KLEE_LIBS
  kleeCore
)

target_link_libraries(witch-klee ${KLEE_LIBS})

install(TARGETS witch-klee RUNTIME DESTINATION bin)

# The KLEE binary depends on the runtimes
add_dependencies(witch-klee BuildKLEERuntimes)
