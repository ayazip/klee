#include <iostream>
#include "yaml-cpp/yaml.h"
#include <vector>
#include <string>
#include <fstream>
#include <cassert>

#include "klee/Witness/Witness.h"


Witness::Type parse_type(YAML::Node yaml_waypoint){
    assert(yaml_waypoint["action"].as<std::string>() != "target");

    if (yaml_waypoint["type"].as<std::string>() == "assumption")
        return Witness::Type::Assume;
    if (yaml_waypoint["type"].as<std::string>() == "branching")
        return Witness::Type::Branch;
    if (yaml_waypoint["type"].as<std::string>() == "function_return")
        return Witness::Type::Return;
    if (yaml_waypoint["type"].as<std::string>() == "identifier_evaluation")
        return Witness::Type::Eval;
    return Witness::Type::Undefined;

}

Witness::Location parse_location(YAML::Node yaml_waypoint){

    Witness::Location loc;
    loc.filename = yaml_waypoint["location"]["file_name"].as<std::string>();
    loc.line = yaml_waypoint["location"]["line"].as<uint>();
    if (yaml_waypoint["location"]["column"].Type() != YAML::NodeType::Undefined)
        loc.column = yaml_waypoint["location"]["column"].as<uint>();
    if (yaml_waypoint["location"]["identifier"].Type() != YAML::NodeType::Undefined)
        loc.identifier = yaml_waypoint["location"]["identifier"].as<std::string>();

    return loc;

}

std::vector<Witness::Segment> parse(std::string filename) {

    std::ifstream ifs(filename);
    std::string content( (std::istreambuf_iterator<char>(ifs) ),
                           (std::istreambuf_iterator<char>()    ) );
    YAML::Node node = YAML::Load(content);

    assert(node.size() == 1);
    assert(node.Type() == YAML::NodeType::Sequence);
    assert(node[0]["entry_type"].as<std::string>() == "violation_sequence");

    YAML::Node sequence = node[0]["content"];

    std::vector<Witness::Segment> witness;
    assert(sequence.Type() == YAML::NodeType::Sequence);

    for (std::size_t i=0; i<sequence.size(); i++) {
        YAML::Node yaml_segment = sequence[i]["segment"];
        assert(yaml_segment.Type() == YAML::NodeType::Sequence);

        Witness::Segment segment;

        for (std::size_t j=0; j<yaml_segment.size(); j++) {

            YAML::Node yaml_waypoint = yaml_segment[j]["waypoint"];
            Witness::Waypoint waypoint;

            waypoint.loc = parse_location(yaml_waypoint);

            if (yaml_waypoint["action"].as<std::string>() == "target"){
                assert(i == sequence.size() - 1);
                assert(j == yaml_segment.size() - 1);
                waypoint.type = Witness::Type::Undefined;
                segment.follow = waypoint;
                break;
            }

            waypoint.type = parse_type(yaml_waypoint);

            YAML::Node constraint = yaml_waypoint["constraint"]["string"];

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

    return witness;
}

