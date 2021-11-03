#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <queue>

#include "klee/ConcreteValue.h"
#include "klee/Expr/Expr.h"
#include "witnessChecking/WitnessParser.h"
#include "klee/Internal/Support/ErrorHandling.h"




void rapidxml::parse_error_handler(const char *what, void *where) {
    klee::klee_error("Parsing failed: %s", what);
}

void print_err_invalid(const std::string& val, const char* attr) {
    klee::klee_error("Parsing failed: %s is not a valid value for key %s",
                     val.c_str(), attr);
}

// true on success (including missing attr), false on parse error
void set_bool_val(const char* str, const char* attr_name, bool& attr) {
    if (strcmp(str, "") == 0)
        return;
    if (str == std::string("true")) {
        attr = true; return;
    }
    if (str == std::string("false")) {
        attr = false; return;
    }
    print_err_invalid(str, attr_name);
}

void WitnessAutomaton::fill_data(rapidxml::xml_node<>* root) {
    rapidxml::xml_node<> *data_node = root->first_node("data");
    char * attr;
    while (data_node) {
        attr = data_node->first_attribute("key")->value();
        if (strcmp(attr, "witness-type") == 0) {
            if (strcmp(data_node->value(), "violation_witness") != 0)
                klee::klee_error("Only error witnesses are supported");
            data.type = data_node->value();
        }
        else if (strcmp(attr, "sourcecodelang") == 0) {
            char *lang = data_node->value();
            if (strcmp(lang, "C") != 0 && strcmp(lang, "c") != 0) {
                klee::klee_message("Only C language is supported");
                print_err_invalid(lang, "sourcecodelang");
            }
            data.lang = lang;
        }
        else if (strcmp(attr, "producer") == 0)
            data.producer = data_node->value();
        else if (strcmp(attr, "specification") == 0)
            load_spec(data_node->value());
        else if (strcmp(attr, "programfile") == 0)
            data.file = data_node->value();
        else if (strcmp(attr, "programhash") == 0)
            data.hash = data_node->value();
        else if (strcmp(attr, "architecture") == 0)
            data.arch = data_node->value();
        else if (strcmp(attr, "creationtime") == 0)
            data.time = data_node->value();
       // else {
       //     std::cerr << "parse error: unknown attribute " << attr << std::endl;
       // }
        data_node = data_node->next_sibling("data");
    }
    if (data.spec.empty()) {
        klee::klee_error("Parsing failed: Invalid or missing witness specification");
    }
    // if anything missing err
}

void WitnessAutomaton::fill_nodes(rapidxml::xml_node<> *root) {
    rapidxml::xml_node<> *child = root->first_node("node");
    std::string id;
    while (child) {
        if (!child->first_attribute("id")) {
            klee::klee_error("Parsing failed: Node missing attribute id");
        }
        id = child->first_attribute("id")->value();
        if (id.empty() || nodes.find(id) != nodes.end()) {
            klee::klee_error("Parsing failed: Missing or duplicate node id");
        }
        node_ptr node(std::make_shared<WitnessNode>());
        nodes[id] = node;
        node->id=id;

        fill_node_data(child, nodes[id]);

        if (nodes[id]->entry) {
            if (entry) {
                klee::klee_error("Parsing failed: Duplicate entry node");
            }
            entry = nodes[id];
        }
        if (nodes[id]->violation) {
            violation.emplace(node);
        }
        child = child->next_sibling("node");
    }
    if (!entry)
        klee::klee_error("Parsing failed: Missing entry node");
    if (violation.empty())
        klee::klee_error("Parsing failed: No violation node");

}

void WitnessAutomaton::fill_node_data(rapidxml::xml_node<> *xml_node, node_ptr node) {
    rapidxml::xml_node<> *data_node =xml_node->first_node("data");
    char *attr;
    char *value;
    while (data_node) {
        attr = data_node->first_attribute("key")->value();
        value = data_node->value();
        if (strcmp(attr, "entry") == 0)
            set_bool_val(value, attr, node->entry);
        if (strcmp(attr, "sink") == 0)
            set_bool_val(value, attr, node->sink);
        if (strcmp(attr, "violation") == 0)
            set_bool_val(value, attr, node->violation);
        data_node = data_node->next_sibling();
    }
}

void WitnessAutomaton::fill_edges(rapidxml::xml_node<>* root) {
    rapidxml::xml_node<> *child = root->first_node("edge");
    std::string src_id;
    std::string tar_id;
    while (child) {
        if (!child->first_attribute("source") || !child->first_attribute("target")) {
            klee::klee_error("Parsing failed: Edge missing attribute source or target");
        }
        src_id = child->first_attribute("source")->value();
        tar_id = child->first_attribute("target")->value();

        if (nodes.find(src_id) == nodes.end() || nodes.find(tar_id) == nodes.end()) {
            klee::klee_error("Parsing failed: Edge between non existent nodes");
        }
        edge_ptr edge = std::make_shared<WitnessEdge>();
        edge.get()->source = nodes[src_id];
        edge.get()->target = nodes[tar_id];
        fill_edge_data(child, edge);

        nodes[src_id].get()->edges.emplace(edge);
        nodes[tar_id].get()->edges_in.emplace(edge);
        edges.emplace(edge);

        child = child->next_sibling("edge");
    }
}

void WitnessAutomaton::fill_edge_data (rapidxml::xml_node<>* xml_node, edge_ptr edge) {
    rapidxml::xml_node<> *data_node =xml_node->first_node("data");
    char * attr;
    while (data_node) {
        attr = data_node->first_attribute("key")->value();
        if (strcmp(attr, "assumption") == 0)
            edge.get()->assumption = data_node->value();
        else if (strcmp(attr, "assumption.scope") == 0)
            edge.get()->assumScope = data_node->value();
        else if (strcmp(attr, "assumption.resultfunction") == 0)
            edge.get()->assumResFunc = data_node->value();
        else if (strcmp(attr, "control") == 0) {
            char * control = data_node->value();
            if (strcmp(control, "condition-true") != 0
                    && strcmp(control, "condition-false") != 0)
                print_err_invalid(control, "control");
            edge.get()->control = control;
        }
        else if (strcmp(attr, "startline") == 0) {
            edge.get()->startline = std::strtol(data_node->value(), nullptr, 10);
        }
        else if (strcmp(attr, "endline") == 0) {
            edge.get()->endline = std::strtol(data_node->value(), nullptr, 10);
        }
        else if (strcmp(attr, "startoffset") == 0) {
            edge.get()->startoffset = std::strtol(data_node->value(), nullptr, 10);
        }
        else if (strcmp(attr, "endoffset") == 0) {
            edge.get()->endoffset = std::strtol(data_node->value(), nullptr, 10);
        }
        else if (strcmp(attr, "enterLoopHead") == 0) {
            set_bool_val(data_node->value(), "enterLoopHead", edge->enterLoop);
        }
        else if (strcmp(attr, "enterFunction") == 0)
            edge.get()->enterFunc = data_node->value();
        else if (strcmp(attr, "returnFromFunction") == 0 || strcmp(attr, "returnFrom") == 0)
            edge.get()->retFromFunc = data_node->value();
        //else {
        //    std::cerr << "parse error: unknown attribute " << attr << std::endl;
        //   return false;
        //}
        data_node = data_node->next_sibling("data");
    }
}


void WitnessAutomaton::load (const char* filename){
    std::ifstream ifs(filename);
    if (!ifs.good()) {
        klee::klee_error("Parsing failed: Can not load file");
    }

    std::string content( (std::istreambuf_iterator<char>(ifs) ),
                           (std::istreambuf_iterator<char>()    ) );

    rapidxml::xml_document<> wit;
    wit.parse<0>(&content[0]);

    if (strcmp(wit.first_node()->name(), "graphml") != 0) {
        klee::klee_error("Parsing failed: Document missing element graphml");
    }
    rapidxml::xml_node<>* root = wit.first_node()->first_node("graph");
    if (strcmp(root->name(), "graph") != 0) {
        klee::klee_error("Parsing failed: Document missing element graph");
    }
    fill_data(root);
    fill_nodes(root);
    fill_edges(root);
    remove_sink_states();
}


void WitnessAutomaton::load_spec(const std::string& str){
    if (str.find("valid-free") != std::string::npos)
        data.spec.insert(WitnessSpec::valid_free);
    if (str.find("valid-deref") != std::string::npos)
        data.spec.insert(WitnessSpec::valid_deref);
    if (str.find("valid-memtrack") != std::string::npos)
        data.spec.insert(WitnessSpec::valid_memtrack);
    if (str.find("valid-memcleanup") != std::string::npos)
        data.spec.insert(WitnessSpec::valid_memcleanup);
    /** TODO: Other error functions!!!! **/
    if (str.find("reach_error") != std::string::npos) {
        data.err_function = "reach_error";
        data.spec.insert(WitnessSpec::unreach_call);
    }
    if (str.find("! overflow") != std::string::npos)
        data.spec.insert(WitnessSpec::overflow);
}

bool WitnessAutomaton::get_spec(WitnessSpec s){
    return data.spec.find(s) != data.spec.end();

}

// Returns a set of nodes, from which the given node is reachable.
// Sets the "multiple" argument to true, if there are multiple paths
// from the entry to the given node, false otherwise.
std::set<node_ptr> reverse_reachable(const node_ptr node, bool& multiple) {
    std::set<node_ptr> reached;
    std::queue<std::weak_ptr<WitnessNode>> q;
    reached.insert(node);
    q.push(node);
    multiple = false;

    while (!q.empty()) {
        std::weak_ptr<WitnessNode> n = q.front();
        for (edge_ptr e : n.lock()->edges_in) {
            if (reached.find(e->source.lock()) != reached.end())
                multiple = true;
            else {
                q.push(e->source);
                reached.emplace(e->source.lock());
            }
        }
        q.pop();
    }

    return reached;

}

/* Remove sink states */
void WitnessAutomaton::remove_sink_states() {
    bool no_replay = violation.size() > 1;
    std::set<node_ptr> non_sink;

    for (auto v_node : violation) {
        std::set<node_ptr> r = reverse_reachable(v_node, no_replay);
        non_sink.insert(r.begin(), r.end());
    }

    std::vector<std::tuple<std::string, unsigned, unsigned,
                           klee::ConcreteValue>> replay;

    std::queue<node_ptr> q;
    q.push(this->entry);
    std::set<node_ptr> visited;

    while (!q.empty()) {
        node_ptr n = q.front();
        visited.insert(n);
        for (edge_ptr e : n->edges) {

            if (non_sink.find(e->target.lock()) == non_sink.end()) {
              cut_branch(e);
              continue;
            }

            if (visited.find(e->target.lock()) == visited.end()) {
              q.push(e->target.lock());

              if (!no_replay && /* e->startline != 0 && */
                  e->assumResFunc.compare(0, 17, "__VERIFIER_nondet") == 0) {
                std::string value_string = get_result_string(e->assumption);
                if (value_string.empty()) {
                    klee::klee_warning("Parsing: Ignoring assumption.resultfuntion: invalid format");
                    no_replay = true; // change variable name
                    continue;
                }
                bool ok;
                auto value = create_concrete_v(e->assumResFunc, value_string, ok);
                if (!ok)
                    no_replay = true;
                replay.emplace_back(e->assumResFunc, e->startline, 0, value);
              }

            }
            else {
              no_replay = true;
            }

        }
        q.pop();
    }

    if (!no_replay)
        replay_nondets = replay;
}


/* Correctly discard subgraph starting from given entry node */
void WitnessAutomaton::free_subtree(node_ptr entry, std::set<node_ptr>& deadnodes) {
    deadnodes.emplace(entry);
    if (entry->edges.empty())
        return;
    for (auto e : entry->edges) {
        if (deadnodes.find(e->target.lock()) == deadnodes.end())
            free_subtree(e->target.lock(), deadnodes);
        entry->edges.erase(e);
        entry->edges_in.erase(e);
        edges.erase(e);
    }
}

/* Remove everything after this edge from the automaton */
void WitnessAutomaton::cut_branch(edge_ptr edge) {
    node_ptr n = edge->target.lock();
    if (n == nullptr){
        edge->source.lock()->edges.erase(edge);
        return;
    }
    std::set<node_ptr> deadnodes;
    free_subtree(n, deadnodes);
    for (auto dead : deadnodes) {
        nodes.erase(dead->id);
    }
    edge->source.lock()->edges.erase(edge);
}

/* Parse assumption, return substring containing result value */
std::string get_result_string(std::string assumption) {

    size_t start = assumption.find("\\result");
    if (start == std::string::npos)
        return std::string();
    start = assumption.find("==", start) + 2;
    if (start == std::string::npos)
        return std::string();

    while (start < assumption.size() && (assumption[start] == ' ' ||
                                         assumption[start] == '('))
        start++;
    size_t len = 0;
    while (start+len < assumption.size() && assumption[start+len] != ';'
           && assumption[start+len] != ' ' && assumption[start+len] != ')')
        len++;
    return assumption.substr(start, len);
}


klee::ConcreteValue create_concrete_v(std::string function, std::string val, bool& ok) {
    ok = false;
    int64_t value = 0;
    if (isdigit(val[0]) || val[0] == '-') {
        size_t end;
        value = std::stoll(val, &end, 0);
        if (end == val.size())
            ok = true;
    }
    // TODO: if starts with number and contains . and double -> get double
    //                --         ||       --      and float  -> get float
    //        create APFloat / APDouble and bitcast to APInt

    if (function == "__VERIFIER_nondet_int")
        return klee::ConcreteValue(klee::Expr::Int32, value, true);

    if (function == "__VERIFIER_nondet_uint")
        return klee::ConcreteValue(klee::Expr::Int32, value, false);

    if (function == "__VERIFIER_nondet_bool") {
        if (val.find("True") == 0 || val.find("true") == 0) {
            ok = true; value = 1;
        }
        if (val.find("False") == 0 || val.find("false") == 0) {
            ok = true; value = 0;
        }
        return klee::ConcreteValue(klee::Expr::Bool, value, false);
    }

    if (function == "__VERIFIER_nondet__Bool")
        return klee::ConcreteValue(klee::Expr::Bool, value, false);

    if (function == "__VERIFIER_nondet_char") {
        if ((val[0] == '"' || val[0] == '"') && val.size() >= 3 && val[0]==val[2]) {
                ok = true; value = (int64_t)val[1];
        }
        return klee::ConcreteValue(klee::Expr::Int8, value, true);
    }
    if (function == "__VERIFIER_nondet_uchar")
        return klee::ConcreteValue(klee::Expr::Int8, value, false);

    if (function == "__VERIFIER_nondet_float")
        return klee::ConcreteValue(8*sizeof(float), value, true);

    if (function == "__VERIFIER_nondet_double")
        return klee::ConcreteValue(8*sizeof(double), value, true);

    if (function == "__VERIFIER_nondet_loff_t")
        return klee::ConcreteValue(klee::Expr::Int32, value, false);

    if (function == "__VERIFIER_nondet_long")
        return klee::ConcreteValue(klee::Expr::Int64, value, true);

    if (function == "__VERIFIER_nondet_ulong")
        return klee::ConcreteValue(klee::Expr::Int64, value, false);

    if (function == "__VERIFIER_nondet_pointer")
        return klee::ConcreteValue(klee::Expr::Int64, value, false);

    if (function == "__VERIFIER_nondet_pchar")
        return klee::ConcreteValue(klee::Expr::Int64, value, false);

    if (function == "__VERIFIER_nondet_pthread_t")
        return klee::ConcreteValue(klee::Expr::Int64, value, false);

    if (function == "__VERIFIER_nondet_short")
        return klee::ConcreteValue(klee::Expr::Int16, value, true);

    if (function == "__VERIFIER_nondet_ushort")
        return klee::ConcreteValue(klee::Expr::Int16, value, false);

    if (function == "__VERIFIER_nondet_u32")
        return klee::ConcreteValue(klee::Expr::Int32, value, false);

    if (function == "__VERIFIER_nondet_size_t")
        return klee::ConcreteValue(klee::Expr::Int64, value, false);

    if (function == "__VERIFIER_nondet_unsigned")
        return klee::ConcreteValue(klee::Expr::Int32, value, false);

    if (function == "__VERIFIER_nondet_sector_t")
        return klee::ConcreteValue(klee::Expr::Int64, value, false);

    ok = false;
    klee::klee_warning("Parsing: unknown function %s or invalid value", function.c_str());
    return klee::ConcreteValue(klee::Expr::Int32, value, true);
}
