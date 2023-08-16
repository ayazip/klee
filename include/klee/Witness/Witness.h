#ifndef WITNESS_H
#define WITNESS_H

#include "klee/Module/KInstruction.h"
#include "llvm/IR/Instruction.h"

#include <vector>
#include <string>

namespace Witness {

  enum Type {
      Assume,
      Branch,
      Return,
      Enter,
      Undefined};


  enum Property {
      valid_free,
      valid_deref,
      valid_memtrack,
      valid_memcleanup,
      termination,
      overflow,
      unreach_call
  };

  struct Location {
    std::string filename;
    uint64_t line;
    uint64_t column = 0;
    std::string identifier = "";

    bool match(const klee::KInstruction &ki);
  };

  struct Waypoint {
    Type type;
    Location loc;
    std::string constraint = "true";

    bool match(const klee::KInstruction &ki);
  };

  struct Segment {
    std::vector<Waypoint> avoid;
    Waypoint follow;
    std::set<int> check_avoid(const klee::KInstruction &ki);
  };

  struct ErrorWitness {
      std::vector<Segment> segments;
      Property property;
      std::string error_function;
  };

  Property get_property(const std::string& str);
  std::string get_error_function(const std::string& str);
  ErrorWitness parse(const std::string& filename);

}


#endif // WITNESS_H
