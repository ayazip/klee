#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include "WitnessParser.h"

void rapidxml::parse_error_handler(const char *what, void *where) {
    std::cout << "Parse error: " << what << "\n";
    std::abort();
}

void print_err_invalid(const std::string& val, const char* attr) {
    std::cerr << "parse error: "<< val << " is not a valid value for key " << attr << "\n";
}

// true on success (including missing attr), false on parse error
bool set_bool_val(const char* str, const char* attr_name, bool& attr) {
    if (strcmp(str, "") == 0)
        return true;
    if (str == std::string("true")) {
        attr = true;
        return true;
    }
    if (str == std::string("false")) {
        attr = false;
        return true;
    }
    print_err_invalid(str, attr_name);
    return false;
}

//nepisat toto ako uplny idiot lmao
//perhaps pridat funkciu? maybe not
bool WitnessAutomaton::fill_data(rapidxml::xml_node<>* root) {
    rapidxml::xml_node<> *data_node = root->first_node("data");
    char * attr;
    while (data_node) {
        attr = data_node->first_attribute("key")->value();
        if (strcmp(attr, "witness-type") == 0)
            data.type = data_node->value();
        else if (strcmp(attr, "sourcecodelang") == 0) {
            char *lang = data_node->value();
            if (strcmp(lang, "C") != 0)
                std::cerr << "only C language is supported" << std::endl;
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
    // if anything missing print err, ret false
    return true;
}

bool WitnessAutomaton::fill_nodes(rapidxml::xml_node<> *root) {
    rapidxml::xml_node<> *child = root->first_node("node");
    std::string id;
    while (child) {
        if (!child->first_attribute("id")) {
            std::cerr << "parse error: node missing attribute id" << std::endl;
            return false;
        }
        id = child->first_attribute("id")->value();
        if (id.empty() || nodes.find(id) != nodes.end()) {
            std::cerr << "parse error: missing or duplicate node id" << std::endl;
            return false;
        }
        node_ptr node(std::make_shared<WitnessNode>());
        nodes[id] = node;
        node->id=id;

        if (!fill_node_data(child, nodes[id]))
            return false;
        if (nodes[id]->entry) {
            if (entry) {
                std::cerr << "parse error: duplicate entry node" << std::endl;
                return false;
            }
            entry = nodes[id];
        }
        child = child->next_sibling("node");
    }
    if (!entry) {
        std::cerr << "parse error: missing entry node" << std::endl;
        return false;
    }
    return true;
}

bool WitnessAutomaton::fill_node_data(rapidxml::xml_node<> *xml_node, node_ptr node) {
    rapidxml::xml_node<> *data_node =xml_node->first_node("data");
    char *attr;
    char *value;
    while (data_node) {
        attr = data_node->first_attribute("key")->value();
        value = data_node->value();
        if ((strcmp(attr, "entry") == 0 && !set_bool_val(value, attr, node->entry))
            || (strcmp(attr, "sink") == 0 && !set_bool_val(value, attr, node->sink))
            || (strcmp(attr, "violation") == 0 && !set_bool_val(value, attr, node->violation)))
                return false;
        data_node = data_node->next_sibling();
    }
    return true;
}

bool WitnessAutomaton::fill_edges(rapidxml::xml_node<>* root) {
    rapidxml::xml_node<> *child = root->first_node("edge");
    std::string src_id;
    std::string tar_id;
    while (child) {
        if (!child->first_attribute("source") || !child->first_attribute("target")) {
            std::cerr << "parse error: edge missing attribute source or target" << std::endl;
            return false;
        }
        src_id = child->first_attribute("source")->value();
        tar_id = child->first_attribute("target")->value();

        if (nodes.find(src_id) == nodes.end() || nodes.find(tar_id) == nodes.end()) {
            std::cerr << "parse error: edge between non existent nodes";
            return false;
        }
        edge_ptr edge = std::make_shared<WitnessEdge>();
        edge.get()->source = nodes[src_id];
        edge.get()->target = nodes[tar_id];
        if (!fill_edge_data(child, edge))
            return false;
        nodes[src_id].get()->edges.push_back(edge);
        edges.push_back(edge);

        child = child->next_sibling("edge");
    }
    return true;
}

bool WitnessAutomaton::fill_edge_data (rapidxml::xml_node<>* xml_node, edge_ptr edge) {
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
                std::cerr << "parse error: invalid value for attribute control" << std::endl;
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
            if (!set_bool_val(data_node->value(), "enterLoopHead", edge->enterLoop))
                return false;
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
    return true;
}


bool WitnessAutomaton::load (const char* filename){
    std::ifstream ifs(filename);
    if (!ifs.good()) {
        std::cout << "error loading file" << std::endl;
        return false;
    }

    std::string content( (std::istreambuf_iterator<char>(ifs) ),
                           (std::istreambuf_iterator<char>()    ) );

    rapidxml::xml_document<> wit;
    wit.parse<0>(&content[0]);

    if (strcmp(wit.first_node()->name(), "graphml") != 0) {
        std::cerr << "parse error: document missing element graphml" << std::endl;
        return false;
    }
    rapidxml::xml_node<>* root = wit.first_node()->first_node("graph");
    if (strcmp(root->name(), "graph") != 0) {
        std::cerr << "parse error: document missing element graph" << std::endl;
        return false;
    }

    return fill_data(root) && fill_nodes(root) && fill_edges(root);
}


bool WitnessAutomaton::load_spec(const std::string& str){
    if (str.find("valid-free") != std::string::npos)
        data.spec.insert(WitnessSpec::valid_free);
    if (str.find("valid-deref") != std::string::npos)
        data.spec.insert(WitnessSpec::valid_deref);
    if (str.find("valid-memtrack") != std::string::npos)
        data.spec.insert(WitnessSpec::valid_memtrack);
    if (str.find("valid-memcleanup") != std::string::npos)
        data.spec.insert(WitnessSpec::valid_memcleanup);
    /** TODO: Other functions!!!! **/
    if (str.find("reach_error") != std::string::npos) {
        data.err_function = "reach_error";
        data.spec.insert(WitnessSpec::unreach_call);
    }
    if (str.find("! overflow") != std::string::npos)
        data.spec.insert(WitnessSpec::overflow);
    return true;
}

bool WitnessAutomaton::get_spec(WitnessSpec s){
    return data.spec.find(s) != data.spec.end();

}
