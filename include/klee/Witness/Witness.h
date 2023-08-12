#ifndef WITNESSPARSER_H
#define WITNESSPARSER_H

#include <vector>
#include <string>

namespace Witness {

  enum Type { Assume, Branch, Return, Eval, Undefined};

  struct Location {
    std::string filename;
    uint64_t line;
    uint64_t column = 0;
    std::string identifier = "";

  };

  struct Waypoint {
    Type type;
    Location loc;
    std::string constraint = "true";

  };

  struct Segment {
    std::vector<Waypoint> avoid;
    Waypoint follow;
  };

}

#endif // WITNESSPARSER_H
