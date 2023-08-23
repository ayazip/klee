#ifndef WITNESS_H
#define WITNESS_H

#include "klee/Module/KInstruction.h"
#include "klee/Expr/Expr.h"
#include "klee/Module/KValue.h"

#include "llvm/IR/Instruction.h"

#include <vector>
#include <string>
#include <utility>

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

    bool match(const klee::KInstruction& ki,  unsigned type = 0);
    klee::ref<klee::Expr> get_return_constraint(klee::ref<klee::Expr> left);

  };

  struct Segment {
    std::vector<Waypoint> avoid;
    Waypoint follow;
    std::set<size_t> check_avoid(const klee::KInstruction& ki, unsigned type = 0);
    std::pair<bool, bool> get_condition_constraint(const klee::KInstruction &ki);
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
bool get_value(const std::string& constraint);

#endif // WITNESS_H
