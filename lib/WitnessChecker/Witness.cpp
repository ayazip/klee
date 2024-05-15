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




Witness::Type parse_type(YAML::Node yaml_waypoint){
    if (yaml_waypoint["type"].as<std::string>() == "assumption")
        return Witness::Type::Assume;
    if (yaml_waypoint["type"].as<std::string>() == "branching")
        return Witness::Type::Branch;
    if (yaml_waypoint["type"].as<std::string>() == "function_return")
        return Witness::Type::Return;
    if (yaml_waypoint["type"].as<std::string>() == "function_enter")
        return Witness::Type::Enter;
    if (yaml_waypoint["type"].as<std::string>() == "target")
        return Witness::Type::Target;
    klee::klee_error("Invalid waypoint type!");

}

Witness::Location parse_location(YAML::Node yaml_waypoint, std::string key = "location"){

    Witness::Location loc;

    if (key == "location")
        loc.filename = yaml_waypoint[key]["file_name"].as<std::string>();

    if (yaml_waypoint[key]["line"].Type() == YAML::NodeType::Undefined)
        if (key == "location")
            klee::klee_error("Missing line number in location");
        else
            klee::klee_warning("Can't get target location, the result may not be accurate");
    else
        loc.line = yaml_waypoint[key]["line"].as<uint>();

    if (yaml_waypoint[key]["column"].Type() != YAML::NodeType::Undefined)
        loc.column = yaml_waypoint[key]["column"].as<uint>();

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

    for (std::size_t i=0; i<sequence.size(); i++) {
        YAML::Node yaml_segment = sequence[i]["segment"];
        assert(yaml_segment.Type() == YAML::NodeType::Sequence);

        Segment segment;

        for (std::size_t j=0; j<yaml_segment.size(); j++) {

            YAML::Node yaml_waypoint = yaml_segment[j]["waypoint"];

            Waypoint waypoint;

            waypoint.loc = parse_location(yaml_waypoint);
            waypoint.type = parse_type(yaml_waypoint);

            if (waypoint.type == Witness::Type::Target){
                assert(i == sequence.size() - 1);
                assert(j == yaml_segment.size() - 1);

                waypoint.loc2 = parse_location(yaml_waypoint, "location2");

                segment.follow = waypoint;
                break;
            }

            YAML::Node constraint = yaml_waypoint["constraint"]["value"];

            if (constraint.Type() != YAML::NodeType::Undefined)
                waypoint.constraint = constraint.as<std::string>();

            if (j == yaml_segment.size() - 1
                || yaml_waypoint["action"].as<std::string>() == "follow") {
                assert(j == yaml_segment.size() - 1);
                assert(yaml_waypoint["action"].as<std::string>() == "follow");
                segment.follow = waypoint;
                break;

            }

            segment.avoid.push_back(waypoint);
        }

        witness.push_back(segment);

    }

    ErrorWitness ew;
    ew.segments = witness;

    std::string specification = node[0]["metadata"]["task"]["specification"].as<std::string>();
    ew.property = get_property(specification);

    if (ew.of_property(Property::unreach_call))
        ew.error_function = get_error_function(specification);
    return ew;
}

std::set<size_t> Witness::Segment::check_avoid(const klee::KInstruction& ki, unsigned type){
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


std::set<Witness::Property> Witness::get_property(const std::string& str){
    std::set<Witness::Property> prp;
    if (str.find("valid-free") != std::string::npos)
        prp.emplace(Witness::Property::valid_free);
    if (str.find("valid-deref") != std::string::npos)
        prp.emplace(Witness::Property::valid_deref);
    if (str.find("valid-memtrack") != std::string::npos)
        prp.emplace(Witness::Property::valid_memtrack);
    if (str.find("valid-memcleanup") != std::string::npos)
        prp.emplace(Witness::Property::valid_memcleanup);
    if (str.find("! overflow") != std::string::npos)
        prp.emplace(Witness::Property::no_overflow);
    if (str.find("G ! call(") != std::string::npos)
        prp.emplace(Property::unreach_call);

    return prp;

}

std::string Witness::get_error_function(const std::string& str){
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
        return false;

    default:
        std::cout << "Invalid waypoint type!\n";
        return false;
    }

}

bool Witness::Waypoint::match_target(std::tuple<std::string, unsigned, unsigned> error_loc){
    if (type != Witness::Type::Target) // || std::get<0>(error_loc) != loc.filename)
        return false;

    if (loc2.column == 0)
        return (std::get<1>(error_loc) == loc.line
                && (!loc.column || std::get<2>(error_loc) == loc.column));

    if (loc.line == loc2.line && std::get<1>(error_loc) == loc.line)
        return std::get<2>(error_loc) >= loc.column && std::get<2>(error_loc) <= loc2.column;

    if (std::get<1>(error_loc) == loc.line)
        return std::get<2>(error_loc) >= loc.column;

    if (std::get<1>(error_loc) == loc2.line)
        return std::get<2>(error_loc) <= loc.column;

    return std::get<1>(error_loc) > loc.line && std::get<1>(error_loc) < loc2.line;
}

std::pair<bool, bool> Witness::Segment::get_condition_constraint(uint64_t line, uint64_t col) {

    bool go_true = true;
    bool go_false = true;

    // go_x is set to false if the follow waypoint says to take the !x branch
    if (follow.type ==  Witness::Type::Branch && follow.loc.match(line, col)) {
        bool result = get_value(follow.constraint);
        go_true = result;
        go_false = !result;
    }

    bool avoid_true = !go_true;
    bool avoid_false = !go_false;

    for (size_t i=0; i<avoid.size(); i++) {
        if (avoid[i].type == Witness::Type::Branch && avoid[i].loc.match(line, col)) {

            bool avoid_value = get_value(avoid[i].constraint);
            if ((!go_true && !avoid_value) || (!go_false && avoid_value)) {
                klee::klee_warning("Conflicting branching info in segment");
                return std::make_pair(false, false);
            }
            avoid_true = avoid_true || avoid_value;
            avoid_false = avoid_false || !avoid_value;
        }
    }

    return std::make_pair(!avoid_true, !avoid_false);

}

bool get_value(const std::string& constraint){
    if (constraint == "true")
        return true;
    if (constraint == "false")
        return false;
    klee::klee_error("Unsupported constraint value for branching waypoint");

}

klee::ref<klee::Expr> Witness::Waypoint::get_return_constraint(klee::ref<klee::Expr> left){

    size_t start = constraint.find("\\result");

    if (start == std::string::npos)
        klee::klee_error("invalid constraint");

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

    while (start < constraint.size() && (constraint[start] == ' ' ||
                                         constraint[start] == '('))
        start++;

    size_t len = 0;
    while (start+len < constraint.size() && constraint[start+len] != ';'
           && constraint[start+len] != ' ' && constraint[start+len] != ')')
        len++;

    std::string result = constraint.substr(start, len);
    klee::Expr::Width width = (*left.get()).getWidth();


    int64_t s_value = 0;
    uint64_t u_value = 0;
    bool is_signed = true;

    if (isdigit(result[0]) || result[0] == '-') {
        size_t end;
        if(result[result.size() - 1] == 'u' || result[result.size() - 1] == 'U') {
            is_signed = false;
            u_value = std::stoull(result, &end, 0);
            end++;
        } else
            s_value = std::stoll(result, &end, 0);

        if (end != result.size())
            klee::klee_warning("Cant parse return constraint");

    } else {
        klee::klee_error("Cant parse return constraint");
    }


    klee::ref<klee::Expr> right = klee::ref<klee::Expr>(
                klee::ConstantExpr::alloc(llvm::APInt(width,
                                                      is_signed ? s_value : u_value,
                                                      is_signed)));

    if (op == "==")
        return (klee::EqExpr::alloc(left, right));
    if (op == "!=")
        return klee::NotExpr::alloc(klee::EqExpr::alloc(left, right));
    if (is_signed) {
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

int Witness::Waypoint::get_switch_value(){
    size_t idx = 0;
    int value = std::stoi(constraint, &idx, 0);
    if (idx != constraint.size())
        klee::klee_error("Can't parse switch constraint value");
    return value;
}
