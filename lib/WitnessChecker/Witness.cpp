#include <iostream>
#include "yaml-cpp/yaml.h"
#include <vector>
#include <set>
#include <string>
#include <fstream>
#include <cassert>

#include "klee/Witness/Witness.h"
#include "llvm/IR/Instructions.h"
#include "klee/Support/ErrorHandling.h"




Witness::Type parseType(YAML::Node yamlWaypoint){
    if (yamlWaypoint["type"].as<std::string>() == "assumption")
        return Witness::Type::Assume;
    if (yamlWaypoint["type"].as<std::string>() == "branching")
        return Witness::Type::Branch;
    if (yamlWaypoint["type"].as<std::string>() == "function_return")
        return Witness::Type::Return;
    if (yamlWaypoint["type"].as<std::string>() == "function_enter")
        return Witness::Type::Enter;
    if (yamlWaypoint["type"].as<std::string>() == "target")
        return Witness::Type::Target;
    klee::klee_error("Invalid waypoint type! %s", yamlWaypoint["type"].as<std::string>().c_str());

}

Witness::Location parseLocation(YAML::Node yamlWaypoint, std::string key = "location"){

    Witness::Location loc;

    if (key == "location")
        loc.filename = yamlWaypoint[key]["file_name"].as<std::string>();

    if (yamlWaypoint[key]["line"].Type() == YAML::NodeType::Undefined)
        if (key == "location")
            klee::klee_error("Missing line number in location");
        else
            klee::klee_warning("Can't get target location, the result may not be accurate");
    else
        loc.line = yamlWaypoint[key]["line"].as<uint>();

    if (yamlWaypoint[key]["column"].Type() != YAML::NodeType::Undefined)
        loc.column = yamlWaypoint[key]["column"].as<uint>();

    return loc;

}

Witness::ErrorWitness Witness::parse(const std::string& filename) {

    std::ifstream ifs(filename);
    std::string content( (std::istreambuf_iterator<char>(ifs) ),
                           (std::istreambuf_iterator<char>()    ) );
    YAML::Node node = YAML::Load(content);

    assert(node.size() == 1);
    assert(node.Type() == YAML::NodeType::Sequence);
    assert(node[0]["entry_type"].as<std::string>() == "violation_sequence");

    YAML::Node sequence = node[0]["content"];

    std::vector<Segment> witness;
    assert(sequence.Type() == YAML::NodeType::Sequence);
    size_t cycleFrom = sequence.size();

    for (std::size_t i=0; i<sequence.size(); i++) {
        YAML::Node yamlSegment = sequence[i]["segment"];
        assert(yamlSegment.Type() == YAML::NodeType::Sequence);

        Segment segment;

        for (std::size_t j=0; j<yamlSegment.size(); j++) {

            YAML::Node yamlWaypoint = yamlSegment[j]["waypoint"];

            Waypoint waypoint;

            waypoint.loc = parseLocation(yamlWaypoint);
            waypoint.type = parseType(yamlWaypoint);

            if (waypoint.type == Witness::Type::Target){
                assert(i == sequence.size() - 1);
                assert(j == yamlSegment.size() - 1);

                waypoint.loc2 = parseLocation(yamlWaypoint, "location2");

                segment.follow = waypoint;
                break;
            }

            YAML::Node constraint = yamlWaypoint["constraint"]["value"];

            if (cycleFrom > i && yamlWaypoint["action"].as<std::string>() == "cycle")
              cycleFrom = i;

            if (constraint.Type() != YAML::NodeType::Undefined)
                waypoint.constraint = constraint.as<std::string>();

            if (j == yamlSegment.size() - 1) {
              if (yamlWaypoint["action"].as<std::string>() != "follow" &&
                  !(cycleFrom <= i || yamlWaypoint["action"].as<std::string>() == "cycle"))
                klee::klee_error("Invalid waypoint action: %s", yamlWaypoint["action"].as<std::string>().c_str() );
              segment.follow = waypoint;
              break;

            }

            segment.avoid.push_back(waypoint);
        }

        witness.push_back(segment);

    }

    ErrorWitness ew;
    ew.segments = witness;
    ew.cycleFrom = cycleFrom;

    std::string specification = node[0]["metadata"]["task"]["specification"].as<std::string>();
    ew.property = getProperty(specification);

    if (ew.ofProperty(Property::unreach_call))
        ew.errorFunction = getErrorFunction(specification);
    return ew;
}

std::set<size_t> Witness::Segment::checkAvoid(const klee::KInstruction& ki, unsigned type){
    std::set<size_t> matched;

    for (size_t i=0; i<avoid.size(); i++) {
        if (avoid[i].match(ki, type))
           matched.insert(i);
    }
    return matched;
}


bool Witness::Location::match(uint64_t l, uint64_t c) {
    return (l == line && c == column);
}


std::set<Witness::Property> Witness::getProperty(const std::string& str){
    std::set<Witness::Property> prp;
    if (str.find("valid-free") != std::string::npos)
        prp.emplace(Witness::Property::valid_free);
    if (str.find("valid-deref") != std::string::npos)
        prp.emplace(Witness::Property::valid_deref);
    // if (str.find("valid-memtrack") != std::string::npos)
    //    prp.emplace(Witness::Property::valid_memtrack);
    // if (str.find("valid-memcleanup") != std::string::npos)
    //    prp.emplace(Witness::Property::valid_memcleanup);
    if (str.find("! overflow") != std::string::npos)
        prp.emplace(Witness::Property::no_overflow);
    if (str.find("G ! call(") != std::string::npos)
        prp.emplace(Property::unreach_call);

    if (prp.empty())
      klee::klee_error("Missing property!");
    return prp;

}

std::string Witness::getErrorFunction(const std::string& str){
    size_t pos;
    if ((pos = str.find("G ! call(")) != std::string::npos) {
        pos += 9;
        while (pos < str.size() && (str[pos] == '(' || str[pos] == ' '))
          pos++;
        size_t len = 0;
        while (pos+len < str.size() && !(str[pos+len] == '(' ||
                                         str[pos+len] == ' ' ||
                                         str[pos+len] == ')' ))
            len++;
        if (len != 0)
            return str.substr(pos, len);
        else
            klee::klee_error("Invalid specification: missing error function");
    }
    return "";
}

bool Witness::Waypoint::match(const klee::KInstruction& ki, unsigned t) {

    // in case of returns, ki is the caller and we match the callsite location
    if (!loc.match(ki.info->line, ki.info->column) && type != Type::Target)
        return false;

    switch (type) {


    case Witness::Type::Enter:
        return (ki.inst->getOpcode() == llvm::Instruction::Call && t != llvm::Instruction::Ret);;

    case Witness::Type::Return:
        return (t == llvm::Instruction::Ret);
         //if (loc.identifier && (cast<ReturnInst>(ki->inst))->getFunction()->getName()) {
         //   break;

    case Witness::Type::Assume:
    case Witness::Type::Target:
    case Witness::Type::Branch:
        return false;

    default:
        std::cout << "Invalid waypoint type!\n";
        return false;
    }

}

bool Witness::Waypoint::matchTarget(std::tuple<std::string, unsigned, unsigned> errorLoc){
    if (type != Witness::Type::Target) // || std::get<0>(errorLoc) != loc.filename)
        return false;

    if (loc2.column == 0)
        return (std::get<1>(errorLoc) == loc.line
                && (!loc.column || std::get<2>(errorLoc) == loc.column));

    if (loc.line == loc2.line && std::get<1>(errorLoc) == loc.line)
        return std::get<2>(errorLoc) >= loc.column && std::get<2>(errorLoc) <= loc2.column;

    if (std::get<1>(errorLoc) == loc.line)
        return std::get<2>(errorLoc) >= loc.column;

    if (std::get<1>(errorLoc) == loc2.line)
        return std::get<2>(errorLoc) <= loc.column;

    return std::get<1>(errorLoc) > loc.line && std::get<1>(errorLoc) < loc2.line;
}

std::pair<bool, bool> Witness::Segment::getConditionConstraint(uint64_t line, uint64_t col) {

    bool goTrue = true;
    bool goFalse = true;

    // go_x is set to false if the follow waypoint says to take the !x branch
    if (follow.type ==  Witness::Type::Branch && follow.loc.match(line, col)) {
        bool result = getValue(follow.constraint);
        goTrue = result;
        goFalse = !result;
    }

    bool avoidTrue = !goTrue;
    bool avoidFalse = !goFalse;

    for (size_t i=0; i<avoid.size(); i++) {
        if (avoid[i].type == Witness::Type::Branch && avoid[i].loc.match(line, col)) {

            bool avoidValue = getValue(avoid[i].constraint);
            if ((!goTrue && !avoidValue) || (!goFalse && avoidValue)) {
                klee::klee_warning("Conflicting branching info in segment");
                return std::make_pair(false, false);
            }
            avoidTrue = avoidTrue || avoidValue;
            avoidFalse = avoidFalse || !avoidValue;
        }
    }

    return std::make_pair(!avoidTrue, !avoidFalse);

}

bool getValue(const std::string& constraint){
    if (constraint == "true")
        return true;
    if (constraint == "false")
        return false;
    klee::klee_error("Unsupported constraint value for branching waypoint");

}

std::pair<std::string, std::string> Witness::Waypoint::parseConstraint(){
  size_t start = constraint.find("\\result");

  if (start == std::string::npos)
    klee::klee_error("Unsupported constraint for function_return: %s",
                     constraint.c_str());

  std::string op;

  start += 7;
  while (start < constraint.size() && (constraint[start] == ' '))
    start++;

  if (start + 1 < constraint.size() && constraint[start+1] == '=') {
    op = constraint.substr(start, 2);
    start += 2;
  }
  else {
    op = constraint[start];
    start++;
  }

  while (start < constraint.size() && (constraint[start] == ' '))
    start++;

  size_t len = 0;
  while (start+len < constraint.size() && constraint[start+len] != ' ')
    len++;

  return std::make_pair(constraint.substr(start, len), op);
}


llvm::APFloat Witness::Waypoint::getFloatInitializer(std::string val, const llvm::Type& type) {

  if (type.isFloatTy()) {
    size_t end;
    float fValue = std::stof(val, &end);
    if (end == val.size()) {
      llvm::APFloat AP_fValue(fValue);
      return AP_fValue;
    }
  }

  if (type.isDoubleTy()) {
    size_t end;
    float dValue = std::stod(val, &end);
    if (end == val.size()) {
      llvm::APFloat AP_fValue(dValue);
      return AP_fValue;
    }
  }

  klee::klee_error("Failed parsing constraint");
}


klee::ref<klee::Expr> Witness::Waypoint::getReturnConstraint(klee::ref<klee::Expr> left, const llvm::Type& type){
    auto parsed = parseConstraint();
    std::string result = parsed.first;
    std::string op = parsed.second;

    klee::Expr::Width width = (*left.get()).getWidth();
    klee::ref<klee::Expr> right;
    bool isSigned = true;

    if (type.isFloatingPointTy()) {
      right = klee::ref<klee::Expr>(
          klee::ConstantExpr::alloc(this->getFloatInitializer(result, type)));
    } else {

      int64_t sValue = 0;
      uint64_t uValue = 0;

      // ascii char
      if (width == 8 && result.size() == 3
          && result[0] == '\'' && result[2] == '\'') {
        sValue = (int8_t) result[1];
      } else {
        // assume the value is signed, only try unsigned if strtoll fails
        // use strtoll to avoid exceptions
        const char * c_result = result.c_str();
        char * pEnd;
        errno = 0;
        sValue = std::strtoll(c_result, &pEnd, 0);

        if (errno == ERANGE || *pEnd != 0) {
          errno = 0;
          // try unsigned
          isSigned = false;
          size_t end;
          uValue = std::stoull(result, &end, 0);
          if (end != result.size())
            klee::klee_error("Cant parse return constraint");;
        }

      }

      right = klee::ref<klee::Expr>(
                klee::ConstantExpr::alloc(llvm::APInt(width,
                                                      isSigned ? sValue : uValue,
                                                      isSigned)));

    }

    if (op == "==")
        return (klee::EqExpr::alloc(left, right));
    if (op == "!=")
        return klee::NotExpr::alloc(klee::EqExpr::alloc(left, right));
    if (isSigned) {
        if (op == ">")
            return klee::SltExpr::alloc(right, left);
        if (op == ">=")
            return klee::SleExpr::alloc(right, left);
        if (op == "<")
            return klee::SltExpr::alloc(left, right);
        if (op == "<=")
            return klee::SleExpr::alloc(left, right);
    } else {
        if (op == ">")
            return klee::UltExpr::alloc(right, left);
        if (op == ">=")
            return klee::UleExpr::alloc(right, left);
        if (op == "<")
            return klee::UltExpr::alloc(left, right);
        if (op == "<=")
            return klee::UleExpr::alloc(left, right);
    }
    klee::klee_error("Invalid operand in return constraint");
}

int Witness::Waypoint::getSwitchValue(){
    size_t idx = 0;
    int value = std::stoi(constraint, &idx, 0);
    if (idx != constraint.size())
        klee::klee_error("Can't parse switch constraint value");
    return value;
}

std::string Witness::ErrorWitness::matchError(StateTerminationType tt){

  switch (tt) {

  case StateTerminationType::Free:
  if (this->ofProperty(Witness::Property::valid_free))
      return "valid-free";
  break;

  case StateTerminationType::Ptr:
  case StateTerminationType::BadVectorAccess:
    if(this->ofProperty(Witness::Property::valid_deref))
      return "valid-deref";
    break;
  case StateTerminationType::Overflow:
    if (this->ofProperty(Witness::Property::no_overflow))
      return "no-overflow";
    break;

  case StateTerminationType::Leak:
    if (this->ofProperty(Witness::Property::valid_memtrack))
      return "valid-memtrack";

    if (this->ofProperty(Witness::Property::valid_memcleanup))
      return "valid-memcleanup";
    break;
  default:
    return "";
  }
  return "";
}
