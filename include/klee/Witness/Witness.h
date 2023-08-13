#ifndef WITNESSPARSER_H
#define WITNESSPARSER_H

#include "klee/Module/KInstruction.h"
#include "llvm/IR/Instruction.h"

#include <vector>
#include <string>

namespace Witness {

  enum Type { Assume, Branch, Return, Enter, Undefined};

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

}

#endif // WITNESSPARSER_H
