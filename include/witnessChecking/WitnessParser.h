#ifndef WITNESSPARSER_H
#define WITNESSPARSER_H

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <set>
#include <tuple>


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
    std::set<edge_ptr> edges;
    std::set<edge_ptr> replayEdges;

    //std::set<edge_ptr> edges_in;
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
    int result_index = -1;
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
    std::set<edge_ptr> edges;
    node_ptr entry;
    std::set<node_ptr> violation;

    void fill_edges(rapidxml::xml_node<>* root);
    void fill_data(rapidxml::xml_node<>* root);
    void fill_nodes(rapidxml::xml_node<>* node);
    void fill_node_data (rapidxml::xml_node<>* xml_node, node_ptr node);
    void fill_edge_data (rapidxml::xml_node<>* xml_node, edge_ptr edge);
    void load_spec(const std::string& str);



public:
    std::vector<klee::ConcreteValue> replay_nondets;
    void load (const char* filename);
    std::set<WitnessSpec> get_spec() { return data.spec; }
    WitnessNode get_entry() { return *entry; }
    std::string get_err_function() { return data.err_function; }
    bool get_spec(WitnessSpec s);
    size_t get_nodes_number(){ return nodes.size(); }
};

klee::ConcreteValue create_concrete_v(std::string function, std::string val, bool& ok);
std::string get_result_string(std::string assumption);
std::pair<bool, klee::ConcreteValue> fill_replay(WitnessEdge e);



#endif // WITNESSPARSER_H
