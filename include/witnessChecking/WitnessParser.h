#ifndef WITNESSPARSER_H
#define WITNESSPARSER_H

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <set>

#include "../../../rapidxml-1.13/rapidxml.hpp"

enum WitnessSpec {
    valid_free,
    valid_deref,
    valid_memtrack,
    valid_memcleanup,
    termination,
    overflow,
    unreach_call
};

struct WitnessNode;
struct WitnessEdge;
using node_ptr = std::shared_ptr<WitnessNode>;
using edge_ptr = std::shared_ptr<WitnessEdge>;

struct WitnessNode {
    std::string id;
    std::vector<edge_ptr> edges;
    std::vector<edge_ptr> edges_in;
    bool entry;
    bool sink;
    bool violation;

    bool operator <(const WitnessNode &b) const {return id < b.id ;};
    bool operator >(const WitnessNode &b) const {return id > b.id ;};

    //string invariant?
    //string inv scope
};

struct WitnessEdge {
    std::weak_ptr<WitnessNode> source;
    std::weak_ptr<WitnessNode> target;
    std::string assumption;
    std::string assumScope;
    std::string assumResFunc;
    std::string control;
    long startline;
    long endline;
    long startoffset;
    long endoffset;
    bool enterLoop;
    std::string enterFunc;
    std::string retFromFunc;
};


struct WitnessData {
    std::string type;
    std::string lang;
    std::string producer;
    std::set<WitnessSpec> spec;
    std::string err_function;
    std::string file;
    std::string hash;
    std::string arch;
    std::string time;
};

class WitnessAutomaton {
    WitnessData data;
    std::map<std::string, node_ptr> nodes;
    std::vector<edge_ptr> edges;
    node_ptr entry;

    bool fill_edges(rapidxml::xml_node<>* root);
    bool fill_data(rapidxml::xml_node<>* root);
    bool fill_nodes(rapidxml::xml_node<>* node);
    bool fill_node_data (rapidxml::xml_node<>* xml_node, node_ptr node);
    bool fill_edge_data (rapidxml::xml_node<>* xml_node, edge_ptr edge);
    bool load_spec(const std::string& str);
public:
    bool load (const char* filename);
    std::set<WitnessSpec> get_spec() { return data.spec; }
    WitnessNode get_entry() { return *entry; }
    std::string get_err_function() { return data.err_function; }
    bool get_spec(WitnessSpec s);
};




#endif // WITNESSPARSER_H
