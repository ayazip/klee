#ifndef WITNESS_H
#define WITNESS_H

#include "klee/Module/KInstruction.h"
#include "klee/Expr/Expr.h"
#include "klee/Core/TerminationTypes.h"


#include <vector>
#include <string>
#include <utility>
#include <set>


namespace Witness {

  enum Type {
      Assume,
      Branch,
      Return,
      Enter,
      Target,
      Undefined};


  enum Property {
      valid_free,
      valid_deref,
      valid_memtrack,
      valid_memcleanup,
      termination,
      no_overflow,
      unreach_call
  };

  struct Location {
    std::string filename;
    uint64_t line;
    uint64_t column = 0;
    std::string identifier = "";

    bool match(uint64_t line, uint64_t col);
  };

  struct Waypoint {
    Type type;
    Location loc;
    Location loc2 = Location();
    std::string constraint = "true";

    bool match(const klee::KInstruction& ki,  unsigned type = 0);
    bool matchTarget(std::tuple<std::string, unsigned, unsigned>);
    klee::ref<klee::Expr> getReturnConstraint(klee::ref<klee::Expr> left, const llvm::Type& type);
    int getSwitchValue();
    std::pair<std::string, std::string> parseConstraint();
    llvm::APFloat getFloatInitializer(std::string, const llvm::Type&);

  };

  struct Segment {
    std::vector<Waypoint> avoid;
    Waypoint follow;
    std::set<size_t> checkAvoid(const klee::KInstruction& ki, unsigned type = 0);
    std::pair<bool, bool> getConditionConstraint(uint64_t line, uint64_t col);
  };

  struct ErrorWitness {
      std::vector<Segment> segments;
      std::set<Property> property;
      std::string errorFunction;
      size_t cycleFrom;

      bool ofProperty(Property p) {return property.find(p) != property.end(); }
      std::string matchError(StateTerminationType);
  };

  std::set<Property> getProperty(const std::string& str);
  std::string getErrorFunction(const std::string& str);
  ErrorWitness parse(const std::string& filename);

}
bool getValue(const std::string& constraint);

#endif // WITNESS_H
