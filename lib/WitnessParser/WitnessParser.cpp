#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <queue>
#include <utility>


#include "klee/ConcreteValue.h"
#include "klee/Expr/Expr.h"
#include "witnessChecking/WitnessParser.h"
#include "klee/Internal/Support/ErrorHandling.h"
#include "llvm/Support/CommandLine.h"

namespace klee {
llvm::cl::OptionCategory WitnessCat("Witness validator options",
                            "Options for witness validation");

llvm::cl::opt<bool> RefuteWitness(
    "refute-witness",
    llvm::cl::desc("Print \"Witness not validated.\" if the decribed error cannot be found."),
    llvm::cl::init(true),
    llvm::cl::cat(WitnessCat));
};


void rapidxml::parse_error_handler(const char *what, void *where) {
    klee::klee_error("Parsing failed: %s", what);
}

void print_err_invalid(const std::string& val, const char* attr) {
    klee::klee_error("Parsing failed: %s is not a valid value for key %s",
                     val.c_str(), attr);
}

// Set attr value according to the string str
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

// Parse data nodes of element graph and load into the automaton
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
}

// Add nodes to the automaton
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

// Parse data nodes of element node and load into the automaton node
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

// Add edges to the automaton
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

        //nodes[tar_id].get()->edges_in.emplace(edge);
         if (edge->assumResFunc.compare(0, 17, "__VERIFIER_nondet") == 0)
            nodes[src_id]->replayEdges.emplace(edge);
         else
            nodes[src_id].get()->edges.emplace(edge);


        edges.emplace(edge);

        child = child->next_sibling("edge");
    }
}

// Parse data nodes of element edge and load into the automaton edge
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
            if (refute) {
                klee::klee_message("Using unsupported atttribute, witness refutation disabled.");
                refute = false;
            }
        }
        else if (strcmp(attr, "endoffset") == 0) {
            edge.get()->endoffset = std::strtol(data_node->value(), nullptr, 10);
            if (refute) {
                klee::klee_message("Using unsupported atttribute, witness refutation disabled.");
                refute = false;
            }
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
    edge->assumption = parseAssumption(edge->assumption, refute);
}

// Load the file and build the automaton
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

    refute = klee::RefuteWitness;
}

// Get the necessary info out of the specification
void WitnessAutomaton::load_spec(const std::string& str){
    if (str.find("valid-free") != std::string::npos)
        data.spec.insert(WitnessSpec::valid_free);
    if (str.find("valid-deref") != std::string::npos)
        data.spec.insert(WitnessSpec::valid_deref);
    if (str.find("valid-memtrack") != std::string::npos)
        data.spec.insert(WitnessSpec::valid_memtrack);
    if (str.find("valid-memcleanup") != std::string::npos)
        data.spec.insert(WitnessSpec::valid_memcleanup);

    /* SV-COMP only */
    if (str.find("reach_error") != std::string::npos) {
        data.err_function = "reach_error";
        data.spec.insert(WitnessSpec::unreach_call);
    }
    /*
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
        if (len != 0) {
            data.err_function = str.substr(pos, len);
            data.spec.insert(WitnessSpec::unreach_call);
        } else {
            klee::klee_error("Invalid specification: missing error function");
        }

    }
    */
    if (str.find("! overflow") != std::string::npos)
        data.spec.insert(WitnessSpec::overflow);
}

bool WitnessAutomaton::get_spec(WitnessSpec s){
    return data.spec.find(s) != data.spec.end();

}


/* Fill nondet_* function return values provided by the witness */
std::pair<bool, klee::ConcreteValue> fill_replay(WitnessEdge e) {
    std::string value_string = e.assumption;
    if (value_string.empty()) {
        klee::klee_warning("Parsing: Ignoring assumption.resultfuntion: invalid format");
        // Hack, just returns zero.
        return std::make_pair(false, klee::ConcreteValue(klee::Expr::Int32, 0, true));
    }

    bool ok;
    klee::ConcreteValue value = create_concrete_v(e.assumResFunc, value_string, ok);
    if (!ok) {
       klee::klee_warning("Parsing: Ignoring assumption.resultfuntion: invalid format");
    }
    return std::make_pair(ok, value);

}


/* Parse assumption, return substring containing result value */
std::string parseAssumption(std::string assumption, bool &refute) {

    size_t start0 = assumption.find("\\result");

    if (start0 == std::string::npos)
        return std::string();
    size_t start = assumption.find("==", start0) + 2;
    if (start == std::string::npos)
        return std::string();

    while (start < assumption.size() && (assumption[start] == ' ' ||
                                         assumption[start] == '('))
        start++;
    size_t len = 0;
    while (start+len < assumption.size() && assumption[start+len] != ';'
           && assumption[start+len] != ' ' && assumption[start+len] != ')')
        len++;

    if (refute) {
        for (char c : assumption.substr(0, start0) +
             assumption.substr(start + len, std::string::npos))
            if (!std::isspace(c) && !(c == ';')){
                refute = false;
                klee::klee_message("Using unsupported assumptions, witness refutation disabled.");
                break;
            }
        }

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
        if ((val[0] == '"' || val[0] == '"') && val.size() >= 3
                && val[0]==val[2]) {
                ok = true;
                value = (int64_t)val[1];
        }
        return klee::ConcreteValue(klee::Expr::Int8, value, true);
    }
    if (function == "__VERIFIER_nondet_uchar")
        return klee::ConcreteValue(klee::Expr::Int8, value, false);

    if (function == "__VERIFIER_nondet_float") {
        size_t end;
        float f_value = std::stof(val, &end);
        if (end == val.size())
            ok = true;
        llvm::APFloat ap_fvalue(f_value);
        return klee::ConcreteValue(ap_fvalue.bitcastToAPInt(), true);
    }

    if (function == "__VERIFIER_nondet_double") {
        size_t end;
        float d_value = std::stod(val, &end);
        if (end == val.size())
            ok = true;
        llvm::APFloat ap_fvalue(d_value);
        return klee::ConcreteValue(ap_fvalue.bitcastToAPInt(), true);
    }

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
    klee::klee_warning("Parsing: unknown function %s or invalid value",
                       function.c_str());
    return klee::ConcreteValue(klee::Expr::Int32, value, true);
}
