/**
 * @file syntax.cpp
 *
 * @brief Parsing M-Sim codes and producing lexer tree in a special data
 * structure.
 *
 * @license
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 *
 * Copyright (C) 20012-2013 HPCAN, http://hpcan.ce.sharif.edu
 *
 * @author 	Morteza Hoseinzadeh, <m.hoseinzadeh85@gmail.com>
 * @date	2012-05-03
 */

#ifndef QUOTE_CHARS
#define QUOTE_CHARS
#ifdef __WIN32__
#define OPENING_QUOTE "`"
#define CLOSING_QUOTE "'"
#else
#define OPENING_QUOTE "\u2018"
#define CLOSING_QUOTE "\u2019"
#endif
#endif
 
#include "syntax.h"
#include "evaluation.h"
#include "fixedcodes.h"
#include <stdlib.h>
#include <string>
#include <cmath>

list<source_line> misc;
list<source_line> sourcecode;
map<string, string> macros;

node** type_modules;
node** global_functions;
int type_modules_size=0;
int global_functions_size=0;
string include_paths[100];

int paths_count = 0;
int endif_num=0;

const char* type2str(node_type t);

string next_line(source_line&s, int&i);
node* next_until(source_line& s, int& i, const char* last);
void next_until(node* n, source_line& s, int& i, const char* last);

void add_module(node* module);
void add_global_function(node* fn);

bool check_name(string name);
bool check_type(string name);

string get_name(source_line& s, int& i);
int get_parameter_index(node* module, node* n);
node_type get_node_type(string str);
node* get_global_functions(string name);
node* get_block(node* module, node_type block_type, source_line& s, int& i2);

bool make_used(string name, node* module = NULL);
void missing(string w, source_line s, int &i, string exp = ";");

void parse(source_line& s, int&i, bool finishing = false);
void parse_block(node& module, node* block, source_line& s, int& i);
void parse_inherit(node& module, source_line& s, int& i, node* block);
bool parse_connection(node& module, source_line& s, int& i, node* block);
bool parse_connects_to(node& module, source_line& s, int& i, node* block, string what="all");
bool parse_specifications(node& module, source_line& s, int& i, node* block);

node* parse_module(node_type type, string name, node* super, node* owner, source_line& s, int& i);
node* parse_prototype(source_line& s, int& i);
node* parse_expression(node& module, source_line& s, int& i, string last,string& w, bool iterators = false);
node* parse_expression(node& module, source_line& s, int& i, string last = ";", bool iterators = false);
node* parse_dimension_expression(node& module, source_line& s, int& i, int d=0, bool iterators = false);
node* parse_physical_properties(node& module, source_line& s, int& i);
node* parse_connection_condition(node* module, source_line& s, int& i);
node* parse_initial_values(node& module, source_line& s, int& i);
node* parse_declaration(node& module, source_line& s, int& i, string& type, node_type block_type, bool check_multiple=true);
node* parse_record(node& module, source_line& s, int& i, node_type block_type);
node* parse_field(node& module, source_line& s, int& i, node_type block_type);
node* parse_function_body(node& module, source_line& s, int& i, bool single_line = false);
node* parse_function(node& module, source_line& s, int& i, node_type block_type);
node* parse_procedure(node& module, source_line& s, int& i, node_type block_type);
node* parse_hierarchy(node& module, source_line& s, int& i, string delim = "<-", string terminator = ";");
node* parse_const_val(node& module, source_line& s, int& i, node_type block_type, string& w);

node::node(node_type type, source_line p) {
    alloc(100, p);
    this->type = type;
}

node::node(const char* name, source_line p) {
    alloc(100, p);
    this->type = ntNone;
    this->name = name;
}

node::node(const char* name, node* nt) {
    source_line p;
    p.line = nt->line;
    p.filename = nt->file;
    alloc(100, p);
    this->type = ntNone;
    this->name = name;
}

node::node(node_type type, node* nt) {
    source_line p;
    p.line = nt->line;
    p.filename = nt->file;
    alloc(100, p);
    this->type = type;
}

node::node(const char* name) {
    source_line p;
    p.line = 0;
    p.filename = NULL;
    alloc(100, p);
    this->type = ntNone;
    this->name = name;
}

node::node(bool val, source_line p) {
    alloc(100, p);
    this->type = ntNone;
    this->extended = val;
}

void node::remove_child(int idx) {
    //delete children[idx];
    for (int i = idx; i < this->size-1; i++) {
        children[i] = children[i+1];
    }
    this->size--;
}

node* node::add_child(node* n) {
    if(!n) n=new node("UNKNOWN");    
    if (this->size >= this->max_size) {
        node **temp = (node **)malloc(sizeof(node*) * this->max_size);
        for (int i=0; i < this->max_size; i++)
            temp[i] = children[i];
        int len = this->max_size;
        this->max_size *= 2;
        children = (node * *)malloc(sizeof(node*) * this->max_size);
        for (int i = 0; i < this->max_size; i++) {
            if(i < len)
                children[i] = temp[i];
            else
                children[i] = NULL;
        }
    }
    return this->children[this->size++] = n;
}

void node::alloc(long len, source_line p) {
    used = false;
    evaluated = false;
    engaged = false;
    connected = false;
    inserted = false;
    is_used_module = false;
    is_used_module_assigned = false;
    has_initialize_function = false;
    assigned = NULL;
    line = p.line;
    file = p.filename;
    parent = NULL;
    owner = NULL;
    ancestor = NULL;
    size = 0;
    max_size = len;
    content_size = 0;
    used_var_size = 0;
    extended = false;
    index = 0;
    children = (node * *)malloc(sizeof(node*) * len);
    for (int i = 0; i < len; i++) {
        children[i] = NULL;
    }
}

bool node::has_child(string name) {
    for(int i=0; i<size; i++)
        if(children[i]->name == name)
            return true;
    return false;
}

bool node::is_child_of(node* module) {
	if (this->name == module->name)
		return true;
	if (parent) return parent->is_child_of(module);
	return false;
}

bool node::is_child_of(string p, string submodule_name) {
	if (parent) {
		if(parent->name == p) {
			node* sm = NULL;
			if((sm = parent->get_property(submodule_name, true, ntDefinitionsBlock)))
				return sm->type == ntSubmodule;
			return false;
		}
		return parent->is_child_of(p, submodule_name);
	}
	node* sm = NULL;
	if((sm = get_property(submodule_name, true, ntDefinitionsBlock)))
		return sm->type == ntSubmodule;
	return false;
}

void node::assign(string v, int idx) {
    if (content_size >= idx) {
        content_size = idx+1;
        content = new string[content_size];
        assigned = new bool[content_size];
        initialized = new bool[content_size];
    }
    assigned[idx] = true;
    content[idx] = v;
}

void node::assign(string pname, string v, int idx) {
    node* param = get_property(pname);
    param->assign(v, idx);
}

node* node::get_ancestor() {
    if(this->ancestor) return ancestor;
    if (!parent) {
        ancestor = this;
        return ancestor;
    }
    ancestor = parent->get_ancestor();;
    return ancestor;
}

node* node::get_block(int block_type) {
    for (int i = 0; i < size; i++)
        if (children[i]->type == block_type)
            return children[i];
    return NULL;
}

node* node::explore_for_variable(node* block, string name, bool cond) {
    if(!block) return NULL;
    for (int j = 0; j < block->size; j++) {
        if (block->children[j]->name == name)
            return block->children[j];
        else if (block->children[j]->type == ntIf) {
            node* res;
            if(cond) {
               if(evaluate_condition(this, block->children[j]))
                  return explore_for_variable(block->children[j]->children[1], name, cond);
               else
                  return explore_for_variable(block->children[j]->children[2], name, cond);
            } else {
               res = explore_for_variable(block->children[j]->children[1], name);
               if(res) return res;
               res = explore_for_variable(block->children[j]->children[2], name);
               if(res) return res;
            }
        }
    }
    return NULL;
}

node* node::get_property(string name, bool recursive, int block_type, bool cond) {
    if(is_delimiter(name[0])) return NULL;
    if(is_number(name)) return NULL;
    if(is_string(name)) return NULL;
    if(is_type(name)) return NULL;
    for (int i = 0; i < size; i++)
        if ((children[i]->type != ntSpecificationsBlock && children[i]->type != ntImplementationBlock) || (block_type == ntImplementationBlock || block_type == ntSpecificationsBlock))
        if (block_type == ntNone || (children[i]->type & block_type)) {
            node* res = explore_for_variable(children[i], name, cond);
            if(res) return res;
        }
    if (!recursive)
        return NULL;
    if (parent) {
        node* parent_property = parent->get_property(name, recursive, block_type, cond);
        if(parent_property) return parent_property;
    }
    if(owner) {
        node* owner_property = owner->get_property(name, recursive, block_type, cond);
        if(owner_property) {
            if(owner_property->type & (ntConstant | ntParameter))
                return owner_property;
        }
    }
    return NULL;
}

node* node::get_property(int type, bool recursive) {
    for (int i = 0; i < size; i++)
        for (int j = 0; j < children[i]->size; j++)
            if (children[i]->children[j]->type == type)
                return children[i]->children[j];
    if (!recursive)
        return NULL;
    if (parent)
        return parent->get_property(type, recursive);
    return NULL;
}

node* node::get_property_owner(string name, bool recursive, int block_type) {
    for (int i = 0; i < size; i++)
        if (children[i]->type != ntSpecificationsBlock && children[i]->type != ntImplementationBlock)
            if (block_type == ntNone || (children[i]->type & block_type))
                for (int j = 0; j < children[i]->size; j++)
                    if (children[i]->children[j]->name == name)
                        return this;
    if (!recursive)
        return NULL;
    if (parent)
        return parent->get_property_owner(name, recursive);
    return NULL;
}

void node::print(string indent) {
    const char* ind = indent.c_str();
    cout << endl << ind << "name:  " << name << endl;
    cout << ind << "type:  " << type2str(type) << endl;
    cout << ind << "Extended: " << extended << endl;
    cout << ind << "children: " << size << endl;
    indent += "    ";
    for (int i = 0; i < size; i++)
        children[i]->print(indent);
}

string node::toString() {
    string str = "";
    for (int i = 0; i < this->size; i++) {
        if (!str.empty())
            str += " ";
        str += this->children[i]->name;
    }
    return str;
}

void readline(ifstream& f, char* l) {
#if defined (__WIN32__)
    f.getline(l, 1024);
    //#define getline(a, b) _getline(a, b);
#else
    string line;
    getline(f, line);
    strcpy(l, line.c_str());
#endif
}

void load_file(char* filename, bool first) {
    int ln = 0;
    int ln2 = 0;
    filename++;
    filename[strlen(filename) - 1] = '\0';

    char *filename_c = new char[strlen(filename) + 1];
    strcpy(filename_c, filename);
    bool comment = false;
    ifstream file;
    file.open(filename, ifstream::in);
    if (!file.is_open())
        throw exp_gen(etFatalError, -1, "", "Failed to load configuration file.");
    while (file.good()) {
        char line[1025];
        int start_comment = 0;
        readline(file, line);
        for (int i = 0; line[i] && line[i + 1]; i++) {
            if (line[i] == '/') {
                if (line[i + 1] == '/') {
                    line[i + 1] = line[i] = '\0';
                } else if (line[i + 1] == '*') {
                    comment = true;
                    start_comment = i;
                    int last = 1024;
                    for (int j = i+2; line[j]; j++) {
                        if (line[j] == '*' && line[j + 1] == '/') {
                            comment = false;
                            last = j + 1;
                            break;
                        }
                    }
                    if(last > i)
                        for (int k = i; k <= last && line[k]; k++)
                            line[k] = ' ';
                }
            }
        }
        if (strlen(line) == 0) {
            ln2++;
            continue;
        }
        if (comment) {
            int last = 1024;
            for (int j = 0; line[j]; j++) {
                if (line[j] == '*' && line[j + 1] == '/') {
                    comment = false;
                    last = j + 1;
                    break;
                }
            }
            if(start_comment < last)
                for (int k = start_comment; k <= last && line[k]; k++)
                    line[k] = ' ';
        }
        source_line s;
        s.filename = filename_c;
        s.str = line;
        s.line = ln2 + (ln++);
        if (!first)
            sourcecode.push_back(s);
        else {
            list<source_line>::iterator it = sourcecode.begin();
            for (int f = 0; f < ln - 1; f++) {
                it++;
            }
            sourcecode.insert(it, s);
        }
    }
    file.close();
}

string relative_path(char* _base, const char* path) {
    string s;
    char * base = new char[strlen(_base) + 1];
    strcpy(base, _base);
    for (int i = strlen(base) - 1; i > 0; i--)
        if (base[i] == '\\' || base[i] == '/') {
        base[i] = '\0';
        break;
    }
    s = base;
    s += '/';
    s += path;
    delete[] base;
    return s;
}

int begining_line = -1;
int last_i = 0;
bool end_of_file = false;

string next(string &str, int& i, char finish, bool trim) {
    string word("");
    static char last_char = ';';
    last_i = i;
    int len = str.length();
    bool _num = false;
    if (len <= i) {
        i = 999999;
        throw exp_gen(etSyntaxError, -1, "", "");
    }
    if (finish == '\0') {
        while (!is_word(str[i]) && i < len)
            if (is_container(str[i]))
                return next(str, ++i, str[i], trim);
        else if (is_delimiter(str[i])) {
            word = str[i];
            bool finished = true;
            bool _num = false;
            if ((unsigned int)i < str.length() - 1) {
                if (str[i] == '<') {
                    i++;
                    if (str[i] == '<')      word = "<<";
                    else if (str[i] == '-') word = "<-";
                    else if (str[i] == '=') word = "<=";
                    else i--;
                } else if (str[i] == '>') {
                    i++;
                    if (str[i] == '>')      word = ">>";
                    else if (str[i] == '=') word = ">=";
                    else i--;
                } else if (str[i] == '+') {
                    i++;
                    if (str[i] == '+')      word = "++";
                    else if (str[i] == '=') word = "+=";
                    else if (is_digit(str[i])) {
                        if(is_delimiter(last_char)) {
                            word = "+";
                            finished = false;
                            _num = true;
                        }
                        i--;
                    } else i--;
                } else if (str[i] == '-') {
                    i++;
                    if (str[i] == '-')      word = "--";
                    else if (str[i] == '=') word = "-=";
                    else if (str[i] == '>') word = "->";
                    else if (is_digit(str[i])) {
                        if(is_delimiter(last_char)) {
                            word = "-";
                            finished = false;
                            _num = true;
                        }
                        i--;
                    } else i--;
                } else if (str[i] == '.') {
                    i++;
                    if (str[i] == '*') 
                        word = ".*";
                    else if (is_digit(str[i])) {
                        word = ".";
                        finished = false;
                        _num = true;
                        i--;
                    } else if(str[i] == '.') {
                        word = "..";
                        i++;
                        if (str[i] == '.') 
                            word = "...";
                        else i--;
                    } else i--;
                } else if (str[i] == '&') {
                    i++;
                    if (str[i] == '&')      word = "&&";
                    else if (str[i] == '=') word = "&=";
                    else i--;
                } else if (str[i] == '|') {
                    i++;
                    if (str[i] == '|')      word = "||";
                    else if (str[i] == '=') word = "|=";
                    else i--;
                } else if (str[i] == '=') {
                    i++;
                    if (str[i] == '=')      word = "==";
                    else i--;
                } else if (str[i] == '!') {
                    i++;
                    if (str[i] == '=')      word = "!=";
                    else i--;
                } else if (str[i] == '*') {
                    i++;
                    if (str[i] == '=')      word = "*=";
                    else i--;
                } else if (str[i] == '/') {
                    i++;
                    if (str[i] == '=')      word = "/=";
                    else i--;
                } else if (str[i] == '%') {
                    i++;
                    if (str[i] == '=')      word = "%=";
                    else i--;
                } else if (str[i] == ':') {
                    i++;
                    if (str[i] == ':')      word = "::";
                    else i--;
                }
                if (word == "<<" || word == ">>" || word == "==") {
                    i++;
                    if (str[i] == '=') word += "=";
                    else i--;
                } else if(word == "->") {i++;
                    if (str[i] == '*') word = "->*";
                    else i--;
                }
            }
            i++;
            last_char = word[word.length()-1];
            if(finished) return word; else break;
        } else
            i++;
        if(i < str.length()-1) {
            if(str[i] == '0' && str[i+1] == 'x') {
                i+=2;
                word = "0x";
                while (i < str.length() && is_hex(str[i]))
                    word += str[i++];
                last_char = word[word.length()-1];
                return word;
            }
        }

        bool _j_exp = false;
        while (i < str.length() && (is_digit(str[i]) || (_num && _j_exp && (str[i] == '+' || str[i] == '-')) || (_num && str[i] == '.') || (_num && (str[i] == 'e' || (str[i] == 'E'))))) {
            if (str[i] == 'e' || str[i] == 'E') _j_exp = true;
            else _j_exp = false;
            word += str[i++];
            _num = true;
        }

		if(_num) {
			if (is_dim(str[i])) {
                long double d = atof(word.c_str());
				char s[32];
				if(str[i+1] == 'B' || str[i+1] == 'b') {
				    sprintf(s, "%s", set_dim(str[i++], d, true));
				    i++;
				} else
				    sprintf(s, "%s", set_dim(str[i++], d));
				while(str[i]&&((str[i] >= 'a' && str[i] <= 'z') || (str[i] >= 'A' && str[i] <= 'Z'))) i++;
				if(word[0] == '+' || word[0] == '-') {
                    word = word.c_str()[0];
                    word +=s;
                } else word = s;
            }  else
                while(str[i]&&((str[i] >= 'a' && str[i] <= 'z') || (str[i] >= 'A' && str[i] <= 'Z'))) i++;
        } else {
            if (begining_line == -1) begining_line = i;
            while (is_word(str[i])) word += str[i++];
            if (!macros[word].empty()) {
                last_char = word[word.length()-1];
                if (begining_line > -1)
                    if (str[begining_line] == '#')
                        return word;
                int l = i - word.length();
                string s = str.substr(0, l);
                s += macros[word];
                s += str.substr(i, len-i+1);
                str = s;
                i = l;
                return next(str, i, finish, trim);
            }
        }
    } else {
        if (!trim)
            word += finish;
        while (str[i] != finish && i < len) {
            if (str[i] == '\\')
                word += str[i++];
            word += str[i++];
        }
        if (!trim)
            word += finish;
        i++;
    }
    if (word.length() == 0)
        throw exp_gen(etSyntaxError, -1, "", "");
    last_char = word[word.length()-1];
    return word;
}

string next(source_line& s, int& i, bool thr) {
    try {
        string str = next(s.str, i);
        return str;
    }
    catch(exception_t e) {
		if (sourcecode.size() == 0) {
            end_of_file = true;
            return ""; 
        } else {
            begining_line = -1;
            i = 0;
            s = sourcecode.front();
            sourcecode.pop_front();
            while(trim(s.str).empty() && sourcecode.size()>0) {
                s = sourcecode.front();
                sourcecode.pop_front();
            }
            if (sourcecode.size() == 0 && trim(s.str).empty()) {
                end_of_file = true;
                return "";
            }
            return next(s, i);
        }
    }
}

string next_line(source_line& s, int& i) {
    string str = "";
    try {
        while (1) {
            str += next(s.str, i);
            str += " ";
        }
    }
    catch(...) {
    }
    return str;
}

node* next_until(source_line& s, int& i, const char* last) {
    node* n = new node(ntNone /*ntBody*/, s);
    string w = "";
    while ((w = next(s, i)) != last && !end_of_file)
        n->add_child(new node(w.c_str(), s));
    return n;
}

void next_until(node* n, source_line& s, int& i, const char* last)
{
    string w = "";
    while ((w = next(s, i)) != last && !end_of_file)
        n->add_child(new node(w.c_str(), s));
}

void pass_until(source_line& s, int& i, const char* last) {
    int p = 0;
    int b = 0;
    int r = 0;
    string w = "";
    while (!end_of_file) {
        w = next(s, i);
        if (p == 0 && b == 0 && r == 0 && w == last) {
            return;
        } else {
            if (w == "(") {
                p++;
            } else if (w == ")") {
                p--;
            } else if (w == "[") {
                b++;
            } else if (w == "]") {
                b--;
            } else if (w == "{") {
                r++;
            } else if (w == "}") {
                r--;
            }
        }
    }
}

const char* type2str(node_type t) {
    switch(t) {
    case ntNone: return "ntNone";
    //case ntNumber: return "ntNumber";
    case ntGeneral: return "ntGeneral";
    //case ntPhysicalProperties: return "ntPhysicalProperties";
    //case ntPhysicalProperty: return "ntPhysicalProperty";
    case ntConstant: return "ntConstant";
    case ntParameter: return "ntParameter";
    case ntVariable: return "ntVariable";
    case ntRecord: return "ntRecord";
    case ntField: return "ntField";
    //case ntConnection: return "ntConnection";
    case ntConnectTo: return "ntConnectTo";
    case ntExpression: return "ntExpression";
    //case ntCondition: return "ntCondition";
    //case ntHeirarchy: return "ntHeirarchy";
    case ntProcedure: return "ntProcedure";
    case ntFunction: return "ntFunction";
    //case ntPrototype: return "ntPrototype";
    //case ntBody: return "ntBody";
    case ntBracketsBlock: return "ntBracketsBlock";
    case ntExpressionBlock: return "ntExpressionBlock";
    case ntSpecificationsBlock: return "ntSpecificationsBlock";
    case ntDefinitionsBlock: return "ntDefinitionsBlock";
    case ntTopologyBlock: return "ntTopologyBlock";
    case ntConnectsToBlock: return "ntConnectsToBlock";
    case ntParametersBlock: return "ntParametersBlock";
    case ntImplementationBlock: return "ntImplementationBlock";
    case ntArchitectureBlock: return "ntArchitectureBlock";
    case ntFieldsBlock: return "ntFieldsBlock";
    case ntIf: return "ntIf";
    case ntIterator: return "ntIterator";
    }
    return "";
}

source_line gen_line(int line, char* filename, string s) {
    source_line ss;
    ss.filename = filename;
    ss.line = line;
    ss.str = s;
    return ss;
}

void add_module(node* module) {
    type_modules[type_modules_size++] = module;
}

void add_global_function(node* fn) {
    global_functions[global_functions_size++] = fn;
}

void add_include_path(char* f) {
    int len = strlen(f);
    if(len == 0) return;
    char* filename = new char[len+1];
    strcpy(filename, f);
    if(filename[len-1] == '/' || filename[len-1] == '\\')
        filename[len-1] = '\0';
    include_paths[paths_count++] = filename;
    delete[] filename;
}

bool is_msim_configuration_file(string f) {
    return !(f.empty());
}

bool is_hierarchy_delimiter(string w) {
    if (w == ".")   return true;
    if (w == "->")  return true;
    if (w == ".*")  return true;
    if (w == "->*") return true;
    return false;
}

bool is_word(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || (c == '_') || (c == '#');
}

bool is_digit(char c) {
    return (c >= '0' && c <= '9');
}

bool is_hex(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool is_dim(char c) {
    if (c == 'f') return true;
    if (c == 'p') return true;
    if (c == 'n') return true;
    if (c == 'u') return true;
    if (c == 'm') return true;
    if (c == 'K') return true;
    if (c == 'M') return true;
    if (c == 'G') return true;
    if (c == 'T') return true;
    return false;
}

char* remove_extended_zeros(long double d) {
	static char ans_str[100];
	if(d > 1000) {
		sprintf(ans_str, "%Lf",
			#ifdef __WIN32__
			#ifdef __GNUC__
			(double)
			#endif
			#endif
			d);
		for(int i = strlen(ans_str)-1; i>0; i--) {
			if(ans_str[i] == '0')
				ans_str[i] = '\0';
			else if(ans_str[i] == '.') {
				ans_str[i] = '\0';
				break;
			} else if(ans_str[i] != '0' && ans_str[i] != '.')
				break;
		}
	}
	if((ans_str[0] == '0' && ans_str[1] == '\0') || d <= 1000) {
		sprintf(ans_str, "%g",(double)d);
	   return ans_str;
	}
	return ans_str;
}

const char* set_dim(char c, long double d, bool c_ex) {
	if (c == 'f') return remove_extended_zeros(d*1E-15);
	if (c == 'p') return remove_extended_zeros(d*1E-12);
	if (c == 'n') return remove_extended_zeros(d*1E-9);
	if (c == 'u') return remove_extended_zeros(d*1E-6);
	if (c == 'm') return remove_extended_zeros(d*1E-3);
	if (c == 'B' || c == 'b') return remove_extended_zeros(d);
	if (c == 'K') return remove_extended_zeros(d*((c_ex)?pow(1024.0, 1):1E+3));
	if (c == 'M') return remove_extended_zeros(d*((c_ex)?pow(1024.0, 2):1E+6));
	if (c == 'G') return remove_extended_zeros(d*((c_ex)?pow(1024.0, 3):1E+9));
	if (c == 'T') return remove_extended_zeros(d*((c_ex)?pow(1024.0, 4):1E+12));
	return remove_extended_zeros(d);
}

bool is_delimiter(char c) {
    char delimiters[32] = ".?/\\!^~$%+-*=:;,([{<@>}])&|";
    for (int i = 0; delimiters[i]; i++)
        if (delimiters[i] == c)
            return true;
    return false;
}

bool is_arithmatic_operator(char c) {
    char delimiters[32] = "^!+-*/,()[]%";
    for (int i = 0; delimiters[i]; i++)
        if (delimiters[i] == c)
            return true;
    return false;
}

bool is_bitwise_operator(string w) {
    if (w == "==") return true;
    if (w == "!=") return true;
    if (w == ">") return true;
    if (w == "<") return true;
    if (w == "<=") return true;
    if (w == ">=") return true;
    if (w == "&&") return true;
    if (w == "||") return true;

    return false;
}

bool is_container(char c) {
    char containers[32] = "\"'";
    for (int i = 0; containers[i]; i++)
        if (containers[i] == c)
            return true;
    return false;
}

bool is_user_type(string w) {
    return get_module(w);
}

bool is_not_used_module(string w) {
    node* m = get_module(w);
    if (!m) return false;
    for (int i = 0; i < type_modules_size; i++) {
        if (type_modules[i]->parent)
            if (type_modules[i]->parent == m)
                return false;
    }
    return true;
}

bool is_math_func(string w) {
    if (w == "log2") return true;
    if (w == "abs")  return true;
    if (w == "acos") return true;
    if (w == "asin") return true;
    if (w == "atan") return true;
    if (w == "ceil") return true;
    if (w == "cos")  return true;
    if (w == "exp")  return true;
    if (w == "floor")return true;
    if (w == "log")  return true;
    if (w == "log10")return true;
    if (w == "sin")  return true;
    if (w == "sqrt") return true;
    if (w == "tan")  return true;
    if (w == "factorial")return true;
    return false;
}

bool is_primary_type(string w) {
    if (w == "boolean")return true;
    if (w == "string") return true;
    if (w == "int")    return true;
    if (w == "double") return true;
    if (w == "float")  return true;
    if (w == "char")   return true;
    if (w == "byte")   return true;
    if (w == "bit")    return true;
    if (w == "bool")   return true;
    if (w == "long")   return true;
    if (w == "int8")   return true;
    if (w == "int16")  return true;
    if (w == "int32")  return true;
    if (w == "int64")  return true;
    if (w == "uint8")  return true;
    if (w == "uint16") return true;
    if (w == "uint32") return true;
    if (w == "uint64") return true;
    if (w == "float")  return true;
    if (w == "_data")   return true;
    if (w == "_address")return true;
    if (w == "type")return true;
    return false;
}

node* get_submodule(node *module, node *block, string name) {
    if(!block) return NULL;
    for(int i=0; i < block->size; i++) {
        if(block->children[i]->type == ntSubmodule) {
            if(block->children[i]->name == name) {
                block->children[i]->used = true;
                return block->children[i];
            }
        } else if(block->children[i]->type == ntIf) {
            node* sm;
            if(sm = get_submodule(module, block->children[i]->children[1], name)) return sm;
            if(sm = get_submodule(module, block->children[i]->children[2], name)) return sm;
        }
    }
    return NULL;
}

node* get_submodule(node* module, string name) {
    if(!module) return NULL;
    node* sm = get_submodule(module, module->get_block(ntDefinitionsBlock), name);
    if(sm) return sm;
    if(module->parent) return get_submodule(module->parent, name);
    return NULL;
}

bool is_sub_module(node* module, string name) {
    if(is_primary_type(name)) return false;
    return get_submodule(module, name);
}

node* get_owner_sub_type(node* module, string name) {
    if(!module) return NULL;
    node* sm = get_submodule(module, module->get_block(ntDefinitionsBlock), name);
    if(sm) { sm->used = true; return sm; }
    if(module->owner) return get_owner_sub_type(module->owner, name);
    return NULL;
}

bool is_owner_sub_type(node* module, string name) {
    if(is_primary_type(name)) return false;
    return get_owner_sub_type(module, name);
}

bool is_physical_property(string w) {
    if (w == "latency") return true;
    if (w == "energy")  return true;
    return false;
}

bool is_block_identifier(string w) {
    if (w == "end")            return true;
    if (w == "pass")       return true;
    if (w == "implementation") return true;
    if (w == "topology")    return true;
    if (w == "specifications") return true;
    if (w == "parameter")     return true;
    if (w == "definitions")    return true;
    if (w == "fields")         return true;
    if (w == "architecture")   return true;
    return false;
}

bool is_keyword(string w) {
    if (w == "module")  return true;
    if (w == "extends") return true;
    if (w == "whenever") return true;
    if (w == "otherwise") return true;
    if (w == "field") return true;
    if (w == "record") return true;
    if (w == "block") return true;
    if (w == "function") return true;
    if (w == "procedure") return true;
    return is_block_identifier(w);
}

bool is_type(string w, node* block) {
    if(is_primary_type(w)) return true;
    if(is_user_type(w)) return true;
    if(block) {
        if(block->type == ntDefinitionsBlock)
            return !is_keyword(w);
    }
    return false;
}

bool is_boolean(string s) {
    if (s == "true")     return true;
    if (s == "enabled")  return true;
    if (s == "disabled") return true;
    if (s == "false")    return true;
    return false;
}

bool is_number(string s) {
    bool period = false;
    if (s.empty())
        return false;
    try {
        float f = atof(s.c_str());
        if(f != 0)
            return true;
    } catch (...) {
        return false;
    }
    int i = 0;
    if(s[0] == '+' || s[0] == '-')
        i++;
    for (; s[i]; i++) {
        if ((s[i] < '0' || s[i] > '9') && s[i] != '.') {
            return false;
        } else if (s[i] == '.') {
            if (period) {
                return false;
            } else
                period = true;
        }
    }
    return true;
}

bool is_string(string w) {
    if(w.length() < 2) return false;
    return w[0] == '"';
}

bool is_int(string s) {
    if (s.empty())
        return false;
    try {
        long l = atol(s.c_str());
        if(l != 0)
            return true;
    } catch (...) {
        return false;
    }
    int i = 0;
    if(s[0] == '+' || s[0] == '-')
        i++;
    for (; s[i]; i++)
        if (s[i] < '0' || s[i] > '9')
            return false;
    return true;
}

bool is_expression(source_line&s, int i, int d) {
    string w = next(s, i);
    int p = 0;
    while (w != ";" && !end_of_file) {
        if (w == "(")
            p++;
        else if (w == ")")
            p--;
        if (w == "," && p == 0)
            if (d == 0)
                break;
        else
            d--;
        if (is_arithmatic_operator(w[0]))
            return true;
        w = next(s, i);
    }
    return false;
}

bool check_name(string name) {
    if (!is_word(name[0]))
        return false;
    if (is_keyword(name))
        return false;
    if (is_primary_type(name))
        return false;
    return true;
}

bool check_type(string name) {
    return is_type(name);
}

bool is_external_scope(string scope) {
	if(scope == "cacti") return true;
	if(scope == "sim") return true;
	return false;
}

bool is_external_parameter(string scope, string name) {
//TODO: uncompleted.          (Physical Attributes)
	if(scope == "cacti") {
		return true;
	} else if(scope == "sim") {
		if(name == "bytes_per_write_circuit")
			return true;
	} else return false;
	return false;
}

string get_name(source_line&s, int&i) {
    string name = next(s, i);
    if (!check_name(name)) {
		report_exp(exp_gen(etSyntaxError, s.line, s.filename, ""OPENING_QUOTE"%s"CLOSING_QUOTE" was reserved or refers to a datatype and could not be used as a name.", name.c_str()));
	}
	if (is_number(name)) 
		report_exp(exp_gen(etSyntaxError, s.line, s.filename, ""OPENING_QUOTE"%s"CLOSING_QUOTE" was a number and could not be used as a name.", name.c_str()));
	if (!macros[name].empty())
		report_exp(exp_gen(etSyntaxError, s.line, s.filename, ""OPENING_QUOTE"%s"CLOSING_QUOTE" was a macro identifier and could not be used as a name.", name.c_str()));
	if (name == "")
	   report_exp(exp_gen(etSyntaxError, s.line, s.filename, "No name specified."));
    return name;
}

int get_parameter_index(node* module, node* n) {
    string index_str = evaluate_expression_result(module, n->children[0]);
    if (index_str == "")
        index_str = "0";
    if (!is_int(index_str)) {
        report_exp(exp_gen(etSyntaxError, n->line, n->file, "Could not evaluate index of parameter "OPENING_QUOTE"%s"CLOSING_QUOTE".", n->name.c_str()));
        return 0;
    }
    return atoi(index_str.c_str());
}

node_type get_node_type(string str) {
    if (get_module(str) || str == "general_module")
        return ntGeneral;
    return ntNone;
}

node* get_module(string name) {
    for (int i = 0; i < type_modules_size; i++) {
        if (type_modules[i]->name == name) {
            type_modules[i]->used = true;
            return type_modules[i];
        }
    }
    /*
                for (list<node>::iterator it = type_modules.begin(); it != type_modules.end
        (); it++) {
        if (it->name == name)
        return&(*it);
        }
         */
    return NULL;
}

node* get_global_functions(string name) {
    for (int i = 0; i < global_functions_size; i++) {
        if (global_functions[i]->name == name)
            return global_functions[i];
    }
    return NULL;
}

node * get_block(node* module, node_type block_type, source_line& s, int& i2) {
    // <0: condition> <1..n: children>
    for (int i = 0; i < module->size; i++) {
        if (module->children[i]->type == block_type)
            return module->children[i];
    }
    node* block = new node(block_type, s);
    //	block->add_child(parse_dimension_expression((*module), s, i2));
    return module->children[module->size++] = block;
}

node* get_parameter(node* module, string w, int t, int bt) {
    node* param = module->get_property(w, true, bt);
    if (!param)
        return NULL;
    if (!(param->type & t))
        return NULL;
    return param;
}

bool make_used(string name, node* module) {
    if (module) {
        node* m = module->get_property(name, true, ntParametersBlock|ntDefinitionsBlock|ntImplementationBlock);
        if (m)
            if (m->type != ntFunction && m->type != ntProcedure)
                m = NULL;
        if (m) {
            m->used = true;
            return true;
        }
    } else {
        node* n = get_module(name);
        if (n) {
            n->used = true;
            n->engaged = true;
            return true;
        }
    }
    return true;
}

void missing(string w, source_line s, int& i, string exp) {
    if (w != exp) {
        report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Statement missing "OPENING_QUOTE"%s"CLOSING_QUOTE"", exp.c_str()));
        i = last_i;
    }
}

void parse_inherit(node& module, source_line& s, int& i, node * block) {
    missing(next(s, i), s, i, "from");
    string ans_name = next(s, i);
    node* ans = get_module(ans_name);
    if (!ans) {
		report_exp(exp_gen(etSyntaxError, s.line, s.filename, ""OPENING_QUOTE"%s"CLOSING_QUOTE" was not declared in this scope.", ans_name.c_str()));
		return;
    }
    missing(next(s, i), s, i, ";");
    node* ans_block = get_block(ans, block->type, s, i);
    node* prev;
    for (int i = 0; i < ans_block->size; i++)
        if (prev=module.get_property(ans_block->children[i]->name)) {
            report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Multiple declaration for "OPENING_QUOTE"%s"CLOSING_QUOTE" in inheritance.", ans_block->children[i]->name.c_str()));
            report_exp(exp_gen(etSyntaxError , prev->line, prev->file, "Previous declaration is here."));
        }
    else
        block->add_child(ans_block->children[i]);
}

node* parse_prototype(source_line& s, int& i) {
    string w = next(s, i);
    node* n = new node(ntNone /*ntPrototype*/, s);
    n->add_child(new node("(", s));
    if (w != "(") {
        n->add_child(new node(")", s));
        i = last_i;
        return n;
    } else {
        if(next(s, i) != ")") {
        }
        i = last_i;
    }
    next_until(n, s, i, ")");
    n->add_child(new node(")", s));
    return n;
}

bool error_undefined = true;

bool points_to_super(node* tmodule, node* module, string w) {
	if(w == "owner")
		return true;
	if(tmodule) {
		if(tmodule->owner) {
			if(w == tmodule->owner->name || tmodule->owner->is_child_of(w, module->name)) {
				return true;
			}
		}
	}
	return false;
}

node* parse_expression(node& module, source_line& s, int& i, string last, string& w, bool iterators) {
    node* exp_node = new node("exp", s);
    node* type_node = NULL;
    exp_node->type = ntExpression;
    w = next(s, i);
    int p = 0;
    int b = 0;
	node* tmodule = &module;
    while (!((w == last || w == ";" || w == "}") && p == 0 && b == 0) && !end_of_file) {
        type_node = NULL;
        if (w == "(")
            p++;
        else if (w == "[")
            b++;
        else if (w == ")")
            p--;
        else if (w == "]")
            b--;

		node* a = NULL;
		if (!is_arithmatic_operator(w[0])) {
            if (!is_number(w)) {
				if (!is_string(w)) {
					if (!is_boolean(w)) {
						if (w != "inf") {
							if (!is_bitwise_operator(w)) {
								if (macros[w].empty()) {
									if (!(iterators && w[0] == '#')) {
										if (!is_math_func(w)) {
											if (!is_external_scope(w)) {
												if (!points_to_super(tmodule, &module, w)) {
													if (get_parameter(tmodule, w)) {
														a = tmodule->get_property(w, true, ntParametersBlock);
														type_node = a;
														a->used = true;
														make_used(w, tmodule);
														tmodule = &module;
													} else if (get_parameter(tmodule, w, ntVariable, ntDefinitionsBlock)) {
														a = tmodule->get_property(w, true, ntDefinitionsBlock);
														a->used = true;
														type_node = get_module(a->children[0]->name);
														make_used(w, tmodule);
														tmodule = &module;
													} else if (get_parameter(tmodule, w, ntConstant, ntDefinitionsBlock)) {
														a = tmodule->get_property(w, true, ntDefinitionsBlock);
														type_node = a;
														a->used = true;
														make_used(w, tmodule);
														tmodule = &module;
													} else if (get_parameter(tmodule, w, ntFunction)) {
														make_used(w, tmodule);
														tmodule = &module;
													} else if(is_keyword(w) || end_of_file) {
                                                        i = last_i;
														report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Invalid statement "OPENING_QUOTE"%s"CLOSING_QUOTE".", w.c_str()));
                                                        return NULL;
                                                    } else {
                                                        string ww = evaluate(w.c_str(), s.line, s.filename);
														if(!is_number(ww)) {
                                                            if(error_undefined) 
                                                                report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Undefined symbol "OPENING_QUOTE"%s"CLOSING_QUOTE".", w.c_str()));
                                                        } else
                                                            w = ww;
                                                    }
												} else {
													//super module (owner)
													if(tmodule->owner)
														tmodule = tmodule->owner;
													else 
														report_exp(exp_gen(etSyntaxError, s.line, s.filename, "No refrence to owner module."));
													missing(next(s, i), s, i, "::");
													w = next(s, i);
													continue;
												}
											} else {
												a = new node(w.c_str(), s);
												a->type = ntExternalParameter;
												missing(next(s, i), s, i, "::");
												string param = next(s, i);
												if(!is_external_parameter(w, param))
													report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Unknown parameter "OPENING_QUOTE"%s"CLOSING_QUOTE" in scope %s.", param.c_str(), w.c_str()));
												type_node = new node(param.c_str(), s);
                                            }
										}
									} else {
                                        a = new node(ntIterator, s);
                                    }
                                }
                            }
                        } else {
                            w = "-1";
                        }
                    } else {
                        if(w=="enabled" || w=="true")
                            w = "1";
                        else
                            w = "0";
                    }
                }
            }
        }
        if (a) {
            node* g = new node(w.c_str(), s);
            g->type = a->type;
			if(a->type == ntIterator)
                g->add_child(parse_dimension_expression(module, s, i, 1, iterators));
			else if(a->type != ntExternalParameter)
				g->add_child(parse_dimension_expression(module, s, i, 0, iterators));
            g->add_child(type_node);
            exp_node->add_child(g);
        } else if(w != "$")
            exp_node->add_child(new node(w.c_str(), s));
        w = next(s, i);
    }
    return exp_node;
}

node* parse_expression(node& module, source_line& s, int& i, string last, bool iterators) {
    string w = "";
    return parse_expression(module, s, i, last, w, iterators);
}

node* parse_dimension_expression(node& module, source_line& s, int& i, int d, bool iterators) {
    string w = next(s, i);
    if (w == "[") {
        if (is_expression(s, i, d))
            return parse_expression(module, s, i, "]", iterators);
        if (is_number(w = next(s, i)))
            return new node(atol(w.c_str()), s);
        else if (get_parameter(&module, w)) {
            i = last_i;
            return parse_expression(module, s, i, "]", iterators);
        } else if (w == "inf") {
            return new node("-1", s);
        } else
            report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Undefined symbol "OPENING_QUOTE"%s"CLOSING_QUOTE".", w.c_str()));
    }
    i = last_i;
    return new node("1", s);
}

node* parse_physical_properties(node& module, source_line& s, int& i, string &w) {
    w = next(s, i);
    if (w == "{") {i = last_i; return NULL;}
    if (w != "~") {i = last_i; return NULL;}
    node* nn = new node(ntNone /*PhysicalProperty*/, s);
    w = next(s, i);
    if (!is_physical_property(w)) {
        report_exp(exp_gen(etSyntaxError, s.line, s.filename, ""OPENING_QUOTE"%s"CLOSING_QUOTE" was not a valid physical attribute.", w.c_str()));
        return NULL;
    }
    nn->name = w;
    missing(next(s, i), s, i, "(");
    error_undefined = false;
    nn->add_child(parse_expression(module, s, i, ")"));
    error_undefined = true;
    return nn;
}

node* parse_connection_condition(node* module, source_line& s, int& i) {
    string w = next(s, i);
    if (w == "when") {
        node* exp = new node(ntNone /*ntCondition*/, s);
        string w2 = "";
        string lst = "";
        while ((w2 = next(s, i)) != ";" && !end_of_file) {
            if (w2 == "@") {
                string w3 = next(s, i);
                node*field;
                if ((field = get_parameter(module, w3, ntField, ntFieldsBlock)) == NULL)
					report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Field "OPENING_QUOTE"%s"CLOSING_QUOTE" was not declared in this scope.", w3.c_str()));
				else {
                    field->used = true;
                    exp->add_child(new node("@", s));
                    node* f = new node(lst.c_str(), s);
                    f->add_child(field);
                    exp->add_child(f);
                }
            } else {
                node* p = module->get_property(w2, true, ntParametersBlock|ntDefinitionsBlock);
                int d = 0;
                if (!p) d = 1;
                if (p != NULL || w2[0] == '#') {
                    node *param = new node(w2.c_str(), s);
                    param->add_child(parse_dimension_expression(*module, s, i, d, true));
                    exp->add_child(param);
                } else
                    exp->add_child(new node(w2.c_str(), s));
                lst = w2;
            }
        }
        return exp;
    }
    i = last_i;
    missing(next(s, i), s, i, ";");
    return new node(1, s);
}

node* parse_initial_values(node& module, source_line& s, int& i) {
    string w = next(s, i);
    node* values = new node("none", s);
    if (w == "initially" || w == "=") {
        values->name = "Initial value";
        w = next(s, i);
        if (w == "{") {
            bool delim = true;
            while ((w = next(s, i)) != "}") {
                if (w == ",") {
                    if (delim) {
                        report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Unexpected delimiter "OPENING_QUOTE","CLOSING_QUOTE""));
                    } else {
                        delim = true;
                        continue;
                    }
                }
                delim = false;
                i = last_i;
                int error_no = errors_count;
                values->add_child(parse_expression(module, s, i, ",", w));
                if (errors_count > error_no) break;
                if (w == "}") break;
                if (w == ";") {
                    report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Missing statement "OPENING_QUOTE"}"CLOSING_QUOTE""));
                    break;
                }
                i = last_i;
            }
        } else {
        /*
            if(is_string(w)) {
            	node* exp = new node("exp", s);
            	while(w != "," && w != ";" && !end_of_file) {
            		exp->add_child(new node(w.c_str()));
            		w = next(s, i);
            	}
            	values->add_child(new node(w.c_str()));
            } else */ {
	            i = last_i;
            	values->add_child(parse_expression(module, s, i, ",", w));
            }
            i = last_i;
        }

    } else
        i = last_i;
    return values;
}

node* parse_parameter(node& module, source_line& s, int& i, node_type block_type) {
    //[name] <0:type> <1:size> <2:initial values>

    if (block_type != ntParametersBlock)
        report_exp(exp_gen(etSyntaxError, s.line, s.filename, "A parameter can be defined only inside a 'parameters' block."));

    node *n = new node(ntParameter, s);
	string name = get_name(s, i);
	node* prev;
	if (prev=module.get_property(name, false)) {
		report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Multiple declaration for "OPENING_QUOTE"%s"CLOSING_QUOTE".", name.c_str()));
        report_exp(exp_gen(etSyntaxError , prev->line, prev->file, "Previous declaration is here."));
    }

    string type = "parameter";
    n->name = name;
    n->add_child(new node("parameter", s));
    n->add_child(parse_dimension_expression(module, s, i));
    n->add_child(parse_initial_values(module, s, i));
    missing(next(s, i), s, i);
    return n;
}

node* if_clause=NULL;
int if_result = 0;
void overwrite_previous_declaration_item(node* module, node* r) {
    if(!module) return;
    if(module == r->parent) return;
    overwrite_obj_t obj;
    obj.val = 0;
    if(r->overwrited_modules.count(module))
       obj.val = r->overwrited_modules[module].val;
    obj.val += if_result;
    obj.cond = if_clause;
    r->overwrited_modules[module] = obj;
    //overwrite_previous_declaration_item(module->parent, r);
}

void overwrite_previous_declaration_block(node* module, node *n, node* block) {
    for (int j = 0; j < block->size; j++) {
        node* r = block->children[j];
        if (r->type == ntIf) {
            overwrite_previous_declaration_block(module, n, r->children[1]);
            overwrite_previous_declaration_block(module, n, r->children[2]);
        } else if ((r->type == ntVariable || r->type == ntRecord) && (n->type != ntFunction && n->type != ntProcedure)) {
            if(r->name == n->name) {
                overwrite_previous_declaration_item(module, r);
                return;
            }
        } else if ((r->type == ntProcedure || r->type == ntFunction) && (n->type == r->type)) {
            if(r->name == n->name) {
                int i1=0, i2=0;
                bool matched = true;
                bool new_item = true;
                for (; i1 < r->children[0]->size && i2 < n->children[0]->size; i1++, i2++) {
                    if(r->children[0]->children[i1]->name == "=") {
                        while(r->children[0]->children[i1]->name != "," && i1 < r->children[0]->size-1)
                            i1++;
                    }
                    if(n->children[0]->children[i2]->name == "=") {
                        while(n->children[0]->children[i2]->name != "," && i2 < n->children[0]->size-1)
                            i2++;
                    }
                    while(r->children[0]->children[i1]->name == "," && i1 < r->children[0]->size-1)
                        i1++;
                    while(n->children[0]->children[i2]->name == "," && i2 < n->children[0]->size-1)
                        i2++;
                    if(i1 < r->children[0]->size && i2 < n->children[0]->size) {
                        if ((is_type(r->children[0]->children[i1]->name) && is_type(n->children[0]->children[i2]->name)) ||
                            r->children[0]->children[i1]->name == "*" || n->children[0]->children[i2]->name == "*")
                            if (r->children[0]->children[i1]->name != n->children[0]->children[i2]->name) {
                            matched = false;
                            break;
                        }
                    }
                }
                if(matched) {
                    overwrite_previous_declaration_item(module, r);
                }
            }
        }
    }
}

void overwrite_previous_declaration(node* main_module, node* module, node *n) {
    if(!module) return;
    overwrite_previous_declaration(main_module, module->parent, n);
    for (int i = 0; i < module->size; i++)
        if(module->children[i]->type & (ntParametersBlock|ntDefinitionsBlock|ntFieldsBlock|ntImplementationBlock))
            overwrite_previous_declaration_block(main_module, n, module->children[i]);
}

void overwrite_previous_declaration(node* module, node *n) {
    overwrite_previous_declaration(module, module->parent, n);
}

node* parse_declaration(node& module, source_line& s, int& i, string& type, node_type block_type, bool check_multiple) {
	//[name] <0:type> <1:size> <2:initial values> <3:type node>
	if (block_type != ntDefinitionsBlock)
		report_exp(exp_gen(etSyntaxError, s.line, s.filename, "A submodule/variable should be declared only inside a 'definition' block."));
	node* n = new node(ntVariable, s);
	string stars = "";
	string s1 = next(s, i);
	while(s1 == "*") { stars += "*"; s1 = next(s, i); }
	i = last_i;
	node* type_node = new node((type+stars).c_str(), s);
    node* main = NULL;
	if(!is_primary_type(type)) {
		node* tmodule = get_submodule(&module, type);
        if(!tmodule) tmodule = get_owner_sub_type(&module, type);
        if(!tmodule) tmodule = get_module(type);
        if(tmodule) tmodule->engaged = true;
        main = tmodule;
        string w = next(s, i);
        if(w == "::") {
			while(w == "::") {
                w = next(s, i);
                if(tmodule = tmodule->get_property(w, true, ntDefinitionsBlock)) {
                    if(tmodule->type != ntSubmodule)
                        report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Variable "OPENING_QUOTE"%s' is not a submodule of `%s"CLOSING_QUOTE".", tmodule->name.c_str(), module.name.c_str()));
                } else
					report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Unknown type "OPENING_QUOTE"%s"CLOSING_QUOTE".", w.c_str()));
				tmodule->used = true;
				tmodule->engaged = true;
				type_node->add_child(tmodule);
				main = tmodule;
				w = next(s, i);
            }
        }
        i = last_i;
    }
	string name = get_name(s, i);
	if(check_multiple) {
    	node* prev;
    	if (prev=module.get_property(name, false)) {
    		report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Multiple declaration for "OPENING_QUOTE"%s"CLOSING_QUOTE".", name.c_str()));
            report_exp(exp_gen(etSyntaxError , prev->line, prev->file, "Previous declaration is here."));
        }
    }
    n->name = name;
    node* size_node = parse_dimension_expression(module, s, i);
    n->add_child(type_node);
    n->add_child(size_node);
    n->add_child(parse_initial_values(module, s, i));
    n->add_child(main);
    n->parent = &module;
    string w = next(s, i);
    overwrite_previous_declaration(&module, n);
    if (w == ",")
        return n;
    type = ";";
    missing(w, s, i);
    return n;
}


node* parse_conditional_block(node &module, source_line &s, int&i, node_type block_type, int is_function_body = 0) {
    // <0: condition> <1: if_body> <2: else_body>
    node* saved_if_clause = if_clause;
    int saved_if_result = if_result;
    node* block = new node(ntIf, s);
    if_clause = block;
    missing(next(s, i), s, i, "(");
    block->add_child(parse_expression(module, s, i, ")"));
    string w = next(s, i);
    node* if_body = new node(block_type, s);
    if_result = 1;
    if (w == "{") {
        while(1) {
			if(is_function_body)
				if_body->add_child(parse_function_body(module, s, i));
			else
				parse_block(module, if_body, s, i);
            w = next(s, i);
            if(w == "}" && !is_function_body) break;
            if(is_block_identifier(w) || w == "module" || end_of_file) {
                if(!end_of_file) i = last_i;
                report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Missing statement "OPENING_QUOTE"}"CLOSING_QUOTE"."));
                if_clause = saved_if_clause;
                if_result = saved_if_result;
                return NULL;
            }
            i = last_i;
            if(is_function_body) break;
        }
    } else {
        i = last_i;
        if(is_function_body) {
			int brace = 0;
			bool semicoloned = false;
			while((brace > 0 || !semicoloned) && !end_of_file) {
				if_body->add_child(parse_function_body(module, s, i, true));
				i = last_i;
				w = next(s, i);
				if(w != ";")
					i = last_i;
				if(w == ";")
					semicoloned = true;
				else if(w == "{") {
					brace++;
					semicoloned = true;
				} else if(w == "}") {
					brace--;
					semicoloned = true;
				}
			}
			if(end_of_file && brace > 0) {
                if_clause = saved_if_clause;
                if_result = saved_if_result;
			    throw exp_gen(etSyntaxError, s.line, s.filename, "Missing statement "OPENING_QUOTE"}"CLOSING_QUOTE".");
            } 
		} else
            parse_block(module, if_body, s, i);
    }
    w = next(s, i);
    node* else_body = new node(block_type, s);
    if(w == "otherwise") {
        if_result = 2;
        w = next(s, i);
        if (w == "{") {
            while(1) {
				if(is_function_body) {
					else_body->add_child(parse_function_body(module, s, i));
				} else
                    parse_block(module, else_body, s, i);
				w = next(s, i);
                if(w == "}" && !is_function_body) break;
                if(is_block_identifier(w) || w == "module" || end_of_file) {
                    if(!end_of_file) i = last_i;
                    report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Missing statement "OPENING_QUOTE"}"CLOSING_QUOTE"."));
                    if_clause = saved_if_clause;
                    if_result = saved_if_result;
                    return NULL;
                }
                i = last_i;
                if(is_function_body) break;
            }
		} else {
            i = last_i;
            if(is_function_body) {
				int brace = 0;
				bool semicoloned = false;
				while((brace > 0 || !semicoloned) && !end_of_file) {
					else_body->add_child(parse_function_body(module, s, i, true));
					i = last_i;
					w = next(s, i);
					if(w != ";")
						i = last_i;
					if(w == ";")
						semicoloned = true;
					else if(w == "{") {
						brace++;
						semicoloned = true;
					} else if(w == "}") {
						brace--;
						semicoloned = true;
					}
				}
    			if(end_of_file && brace > 0) {
                    if_clause = saved_if_clause;
                    if_result = saved_if_result;
    			    throw exp_gen(etSyntaxError, s.line, s.filename, "Missing statement "OPENING_QUOTE"}"CLOSING_QUOTE".");
                }
			} else
                parse_block(module, else_body, s, i);
        }
    } else
        i = last_i;
    block->add_child(if_body);
    block->add_child(else_body);
    block->parent = &module;
    if_clause = saved_if_clause;
    if_result = saved_if_result;
    return block;
}

void parse_record_block(node& module, node* body, source_line& s, int& i, string &w, node_type block_type);

node* parse_conditional_record_block(node &module, source_line &s, int&i, node_type block_type) {
    // <0: condition> <1: if_body> <2: else_body>
    node* saved_if_clause = if_clause;
    int saved_if_result = if_result;
    node* block = new node(ntIf, s);
    if_clause = block;
    if_result = 1;
    missing(next(s, i), s, i, "(");
    block->add_child(parse_expression(module, s, i, ")"));
    string w = next(s, i);
    node* if_body = new node(ntNone /*ntBody*/, s);
    if (w == "{") {
        while(1) {
            w = next(s, i);
            parse_record_block(module, if_body, s, i, w, block_type);
            if(w == "}") break;
            if(is_block_identifier(w) || w == "module" || end_of_file) {
                if(!end_of_file) i = last_i;
                report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Missing statement "OPENING_QUOTE"}"CLOSING_QUOTE"."));
                if_clause = saved_if_clause;
                if_result = saved_if_result;
                return NULL;
            }
            i = last_i;
        }
    } else {
        parse_record_block(module, if_body, s, i, w, block_type);
        i = last_i;
    }
    w = next(s, i);
    node* else_body = new node(ntNone /*ntBody*/, s);
    if(w == "otherwise") {
        w = next(s, i);
        if_result = 2;
        if (w == "{") {
            while(!end_of_file) {
                parse_record_block(module, else_body, s, i, w, block_type);
                w = next(s, i);
                if(w == "}") break;
                if(is_block_identifier(w) || w == "module" || end_of_file) {
                    if(!end_of_file) i = last_i;
                    report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Missing statement "OPENING_QUOTE"}"CLOSING_QUOTE"."));
                    if_clause = saved_if_clause;
                    if_result = saved_if_result;
                    return NULL;
                }
                i = last_i;
            }
        } else {
            parse_record_block(module, else_body, s, i, w, block_type);
            i = last_i;
        }
    } else
        i = last_i;
    block->add_child(if_body);
    block->add_child(else_body);
    block->parent = &module;
    if_clause = saved_if_clause;
    if_result = saved_if_result;
    return block;
}

bool is_type_decoration(string w) {
    if(w == "*") return true;
    if(w == "unsigned") return true;
    if(w == "long") return true;
    if(w == "const") return true;
    if(w == "static") return true;
    return false;
}

void parse_record_block(node& module, node* body, source_line& s, int& i,
                        string &w, node_type block_type) {
    if (w == "whenever") {
        body->add_child(parse_conditional_record_block(module, s, i, block_type));
    } else if (w == "record") {
        body->add_child(parse_record(module, s, i, block_type));
    } else if(is_type(w)) {
        if (is_user_type(w))
            report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Unable to create a record member of module "OPENING_QUOTE"%s"CLOSING_QUOTE".", w.c_str()));
		while (w != ";" && !end_of_file) {
            node* var = parse_declaration(module, s, i, w, ntDefinitionsBlock, false);
            body->add_child(var);
        }
    } else if(w == "delete") {
        while ((w=next(s,i)) != ";" && !end_of_file) {
            if (w == "end") { i=last_i; break; }
            if (w == ",") continue;
            int idx = -1;
            for(int i=0; i < body->size; i++) {
                if(body->children[i]->name == w) {
                    idx = i;
                }
            }
            if(idx > -1) {
                body->remove_child(idx);
            } else
                report_exp(exp_gen(etSyntaxError, s.line, s.filename, "No such record member named "OPENING_QUOTE"%s"CLOSING_QUOTE".", w.c_str()));
        }       
    } else
        report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Undefined symbol "OPENING_QUOTE"%s"CLOSING_QUOTE".", w.c_str()));
    w = next(s, i);
}

node* find_original_record(node* owner, string name) {
    if(!owner) return NULL;
    if(owner->type == ntRecord) {
        for(int i=0; i < owner->children[1]->size ; i++) {
            if(owner->children[1]->children[i]->name == name) {
                if(owner->children[1]->children[i]->type == ntRecord)
                    return owner->children[1]->children[i];
            }
        }
        return find_original_record(owner->children[2], name);
    } else {
        return owner->get_property( name, true, ntArchitectureBlock );
    }
    return NULL;    
}

node* owner_record = NULL;
node* parse_record(node& module, source_line& s, int& i, node_type block_type) {
    //[name] <0:size> <1:body> <2:last origin>
    if (block_type != ntArchitectureBlock)
        report_exp(exp_gen(etSyntaxError, s.line, s.filename, "A record can be defined only inside an 'architecture' block."));

    node* n = new node(ntRecord, s);
    string name = get_name(s, i);
    source_line ss = s; 
    n->name = name;
    n->parent = &module;
    n->add_child(parse_dimension_expression(module, s, i));
    string m = next(s, i);

    
    node* ow = owner_record;
    if(!ow) ow = &module; 
    node* origin = find_original_record(ow, name);
    if(m == "merged") {
        if(origin) {
            if(!ow->merged && ow->type == ntRecord) {
                report_exp(exp_gen(etSyntaxError, ss.line, ss.filename, "Cannot merge sub-record of an unmergable record.", name.c_str()));
            }
            overwrite_previous_declaration_item(&module, origin);
        } else
            report_exp(exp_gen(etWarning, ss.line, ss.filename, "No original declaration for "OPENING_QUOTE"%s"CLOSING_QUOTE" to be merged.", name.c_str()));
        n->merged = true;
    } else {
        if(origin) {
            report_exp(exp_gen(etSyntaxError, ss.line, ss.filename, "Multiple declaration of "OPENING_QUOTE"%s"CLOSING_QUOTE".", name.c_str()));
            report_exp(exp_gen(etSyntaxError , origin->line, origin->file, "Previous declaration is here."));
        }
        i = last_i;
    }
     
    ow = owner_record;
    int d = 0;
    if(ow) while(ow->owner) { ow = ow->owner; d++; }
        
    string w = next(s, i);
    node* body = new node(ntNone /*ntBody*/, s);
    
    if(n->merged) {
        if(origin) {
            for (int k=0; k < origin->children[1]->size; k++) {
                body->add_child(origin->children[1]->children[k]);
            } 
        }
    }    
    
    n->owner = owner_record;
    n->add_child(body);
    n->add_child(origin);
    owner_record = n;
    while (w != "end" && !end_of_file) {
        parse_record_block(module, body, s, i, w, block_type);
    }
    owner_record = n->owner;
    missing(next(s, i), s, i, ";");
    overwrite_previous_declaration(&module, n);
    return n;
}

node* parse_field(node& module, source_line& s, int& i, node_type block_type) {
    //[name] <0:from> <1:to>
    if (block_type != ntFieldsBlock)
        report_exp(exp_gen(etSyntaxError, s.line, s.filename, "An address field can be defined only inside a 'fields' block."));

    node* n = new node(ntField, s);
    n->name = next(s, i); //it can be everything
	if (!is_word(n->name[0]))
        report_exp(exp_gen(etSyntaxError, s.line, s.filename, ""OPENING_QUOTE"%s"CLOSING_QUOTE" was not a valid field identifier.", n->name.c_str()));

    missing(next(s, i), s, i, "from");
    n->add_child(parse_expression(module, s, i, "to"));
    string w = next(s, i);
    if (w == "end") {
        n->add_child(new node("end", s));
        missing(next(s, i), s, i, ";");
        return n;
    } else
        i = last_i;
    n->add_child(parse_expression(module, s, i));
    n->parent = &module;
    return n;
}

bool parse_specifications(node& module, source_line& s, int& i, node* block) {
	//[name] <0: index> <1: pointer to parameter | external_parameter_name> <2: value>
    string w = next(s, i);
    if (is_block_identifier(w)) {
        i = last_i;
        return false;
	}
	node* var = NULL;
	if(!is_external_scope(w)) {
		var = module.get_property(w);
		if (!var) {
			report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Parameter "OPENING_QUOTE"%s"CLOSING_QUOTE" was not declared in this scope.", w.c_str()));
        } else if (var->type != ntParameter)
			report_exp(exp_gen(etSyntaxError, s.line, s.filename, ""OPENING_QUOTE"%s"CLOSING_QUOTE" was not a parameter.", w.c_str()));
	}
	node* n = new node(w.c_str(), s);
	string param;
	if(w == "cacti" || w == "sim") {
		missing(next(s, i), s, i, "::");
		param = next(s, i);
		n->type = ntExternalParameter;
		if(!is_external_parameter(w, param))
			report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Unknown external parameter "OPENING_QUOTE"%s' in `%s"CLOSING_QUOTE" scope.", param.c_str(), w.c_str()));
		var = new node(param.c_str(), s);
	}
    n->add_child(parse_dimension_expression(module, s, i));
    missing(next(s, i), s, i, "is");
	n->add_child(var);
	n->add_child(parse_expression(module, s, i));
	block->add_child(n);
    return true;
}

bool has_breakpoint = false;
int breakpoint_id = 0;
string parsing_function_name = "";

node* parse_function_body(node& module, source_line& s, int& i, bool single_line) {
    string w("");
    string lst("");
    node* body = new node(ntNone /*ntBody*/, s);
    body->add_child(new node("{", s));
    int b = 1;
    node* param = NULL;
    node* tmp = new node("", s);
    while (1) {
        w = next(s, i);
        if (w.empty()) break;
        param = module.get_property(w);
        if(!param)
            param = tmp;
        else if(lst != "." && lst != "->")
            param->used = true;
        try {
            if (w == "{")
                b++;
            else if (w == "}")
				b--;
            if (b == 0) break;
            if ( (is_keyword(w) && w != "end" && w != "whenever" && w != "otherwise") || end_of_file ) {
                if(!end_of_file) i = last_i;
                break; 
            }
            if (w == "whenever") {
				body->add_child(parse_conditional_block(module, s, i, ntImplementationBlock, true));
				continue;
			} else if (w == "@") {
                string w2 = next(s, i);
                node*field;
                if ((field = get_parameter(&module, w2, ntField, ntFieldsBlock)) == NULL)
                    report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Field "OPENING_QUOTE"%s"CLOSING_QUOTE" was not declared in this scope.", w2.c_str()));
                else {
                    field->used = true;
                    body->add_child(new node("@", s));
                    node* f = new node(lst.c_str(), s);
                    f->add_child(field);
                    body->add_child(f);
                }
            } else if (w == "$") {
                w = next(s, i);
                node*c;
                if ((c=get_parameter(&module, w, ntConstant)) != NULL) {
                    c->used = true;
                    node* ref = new node(w.c_str(), s);
                    ref->type = ntConstant;
                    ref->add_child(c);
                    ref->add_child(parse_dimension_expression(module, s, i));
                    body->add_child(ref);
                } else
					report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Constant "OPENING_QUOTE"%s"CLOSING_QUOTE" was not declared.", w.c_str()));

			} else if(is_external_scope(w) && !is_hierarchy_delimiter(lst)) {
				node* ext_param = new node(w.c_str(), s);
				missing(next(s, i), s, i, "::");
				string param_name = next(s, i);
				if(!is_external_parameter(w, param_name))
					report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Unknown external parameter "OPENING_QUOTE"%s' in `%s"CLOSING_QUOTE" scope.", param_name.c_str(), w.c_str()));
				ext_param->add_child(new node(param_name.c_str(), s));
				body->add_child(ext_param);
			} else if (is_keyword(w) && w != "end") {
                report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Unexpected statement "OPENING_QUOTE"%s"CLOSING_QUOTE".", w.c_str()));
			} else if ((param->type == ntParameter) && !is_hierarchy_delimiter(lst)) {
                node* ref = new node(w.c_str(), s);
                param->used = true;
                ref->type = ntParameter;
				ref->add_child(param);
                ref->add_child(parse_dimension_expression(module, s, i));
                body->add_child(ref);
            } else {
                node* nn = new node(w.c_str(), s);
                if ((w == "trace" || w == "itrace" || w == "trace_on" || w == "trace_off") && !is_hierarchy_delimiter(lst))
                    nn->type = ntTraceFunction;
                if ((w == "breakpoint") && !is_hierarchy_delimiter(lst)) {
                    nn->type = ntBreakpointFunction;
                    nn->value = breakpoint_id++;
                    has_breakpoint = true;
                }
                node* m = body->add_child(nn);
                if (get_parameter(&module, w)) {
                    if (!is_hierarchy_delimiter(lst)) {
                        make_used(w,& module);
                    }
                }
            }
            lst = w;
        }
        catch(exception_t e) {
            report_exp(e);
        }
        if (single_line && w == ";") { b = 0; break; }
    }
    body->add_child(new node("}", s));
    body->parent = &module;
    if(b>0) {
        report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Missing statement "OPENING_QUOTE"}"CLOSING_QUOTE"."));
    }
    return body;
}

bool is_function_keyword(string s) {
    if(s == "serial") return true;
    if(s == "parallel") return true;
    if(s == "stall") return true;
    if(s == "slip") return true;
    if(s == "writer") return true;
    if(s == "reader") return true;
    if(s == "offline") return true;
//    if(s == "observed") return true;
//    if(s == "region_of_interest") return true;
    return false;
}

node* parse_function_keywords(node& module, source_line& s, int& i, string& w) {
    w = next(s, i);
    if(w == "{") {
        i = last_i;
        return NULL;
    }
    if(end_of_file) {
        return NULL;
    }
    if(w=="~") {
        i = last_i;
        return NULL;
    }
    if(is_function_keyword(w))
        return new node(w.c_str(), s);
    else {
        report_exp(exp_gen(etSyntaxError, s.line, s.filename, ""OPENING_QUOTE"%s"CLOSING_QUOTE" is not a function declarator.", w.c_str()));
        i = last_i;
        return NULL;
    }
}

node* parse_function(node& module, source_line& s, int& i, node_type block_type) {
    //[name] <0: prototype> <1: return type> <2:physical properties> <3: body> <4: keywords>
    if (block_type != ntImplementationBlock)
        report_exp(exp_gen(etSyntaxError, s.line, s.filename, "An action can be declared only inside an 'implementation' block."));

    node* n = new node(ntFunction, s);
    string name = get_name(s, i);
	//    if (module.get_property(name))
    //		throw exp_gen(etSyntaxError, s.line, s.filename,
    //					  "Multiple declaration for "OPENING_QUOTE"%s"CLOSING_QUOTE".", name.c_str());
    n->name = name;
    parsing_function_name = name;
    n->add_child(parse_prototype(s, i));
    
    string w = next(s, i);
    if(w==":") {
        w = next(s, i);
        string typ="";
        while((w != "~") && (w != "{") && (!is_function_keyword(w)) && (!end_of_file)) {
            typ += w + " ";
            w = next(s, i);
        }
        i = last_i;
        typ = trim(typ);
        make_used(typ, &module);
        n->add_child(new node(typ.c_str(), n));
    } else {
        i = last_i;
        n->add_child(new node("void", n));
    }
    
    node* n1 = new node(ntGeneral /*PhysicalProperties*/, s);
    node* n2 = new node("keywords");
    
    w = next(s, i);
    if(is_keyword(w) || w == "}") {
        report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Expected action body before "OPENING_QUOTE"%s"CLOSING_QUOTE" token.", w.c_str()));
        i = last_i;
        return n;
    } else while(w != "{" && w != ":" && !end_of_file) {
        i = last_i;
        node* nn1 = parse_physical_properties(module, s, i, w);
        node* nn2 = parse_function_keywords(module, s, i, w);
        if(nn1) n1->add_child(nn1);
        if(nn2) n2->add_child(nn2);
        w = next(s, i);
        if(is_keyword(w) || is_block_identifier(w)) {
            report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Expected action body before "OPENING_QUOTE"%s"CLOSING_QUOTE" token.", w.c_str()));
            i = last_i;
            return n;
        }
        if(!nn1 && !nn2) { i=last_i; break; }
    }
	n->add_child(n1);
	n->add_child(parse_function_body(module, s, i))->parent = n;
	n->add_child(n2);
    n->parent = &module;
    overwrite_previous_declaration(&module, n);
    if(name == "initialize" && n->children[0]->size == 2)
        module.has_initialize_function = true;
    return n;
}

node* parse_procedure(node& module, source_line& s, int& i, node_type block_type) {
    //[name] <0: prototype> <1: return type> <2:physical properties> <3: body>
    if (block_type != ntImplementationBlock)
        report_exp(exp_gen(etSyntaxError, s.line, s.filename, "A procedure can be declared only inside an 'implementation' block."));

    node* n = new node(ntFunction, s);
    string name = get_name(s, i);
    //    if (module.get_property(name))
    //        throw exp_gen(etSyntaxError, s.line, s.filename,
    //					  "Multiple declaration for "OPENING_QUOTE"%s"CLOSING_QUOTE".", name.c_str());
    n->name = name;
    parsing_function_name = name;
    n->add_child(parse_prototype(s, i));
    n->add_child(new node("void", s));
    node* n1 = new node(ntNone /*PhysicalProperties*/, s);
    node* n2 = new node("keywords");
    
    string w = next(s, i);
    if(is_keyword(w) || is_block_identifier(w) || w == "}") {
        report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Expected function body before "OPENING_QUOTE"%s"CLOSING_QUOTE" token.", w.c_str()));
        i = last_i;
        return n;
    } else while(w != "{" && !end_of_file) {
        i = last_i;
        node* nn1 = parse_physical_properties(module, s, i, w);
        node* nn2 = parse_function_keywords(module, s, i, w);
        if(nn1) n1->add_child(nn1);
        if(nn2) n2->add_child(nn2);
        w = next(s, i);
        if(is_keyword(w) || is_block_identifier(w) || w == "}") {
            report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Expected function body before "OPENING_QUOTE"%s"CLOSING_QUOTE" token.", w.c_str()));
            i = last_i;
            return n;
        }
        if(!nn1 && !nn2) { i=last_i; break; }
    }    
	n->add_child(n1);
	n->add_child(parse_function_body(module, s, i))->parent = n;
	n->add_child(n2);
    n->parent = &module;
    overwrite_previous_declaration(&module, n);
    if(name == "initialize")// && n->children[0]->size == 2)
        module.has_initialize_function = true;
    return n;
}

node* parse_submodule(node& module, source_line& s, int& i, node_type block_type) {
    //[name] <0: prototype> <1: return type> <2:physical properties> <3: body>
    if (block_type != ntDefinitionsBlock)
        report_exp(exp_gen(etSyntaxError, s.line, s.filename, "A sub-module can be declared only inside a 'definitions' block."));

    string name = get_name(s, i);
    string w = next(s, i);
    node* super = NULL;
    if (w == "extends") {
        w = next(s, i);
        if(w == name) {
            report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Module "OPENING_QUOTE"%s"CLOSING_QUOTE" cannot extend itself.", w.c_str()));
            missing(w, s, i, ":");
            w = "general_module";
        } else {
            super = get_submodule(&module, w);
            if (super == NULL)
                super = get_module(w);
            if (super == NULL)
                report_exp(exp_gen(etSyntaxError, s.line, s.filename, ""OPENING_QUOTE"%s"CLOSING_QUOTE" was not declared in this scope.", w.c_str()));
            else {
                //make_used(w);
                super->used = true;
                super->extended = true; //is extended
            }
            missing(next(s, i), s, i, ":");
        }
    } else {
        missing(w, s, i, ":");
        w = "general_module";
    }

    try {
        //		if (nt == ntNone)
        //			throw exp_gen(etSyntaxError, s.line, s.filename, "Expected a super module name, but found "OPENING_QUOTE"%s"CLOSING_QUOTE".", w.c_str());
        return parse_module(ntSubmodule, name, super, &module, s, i);
    }
    catch(exception_t e) {
        throw e;
    }
    return NULL;
}

bool parse_connects_to(node& module, source_line& s, int& i, node* block, string what) {
    //[name:object] <0:index> <1:condition> <2:what is passed> <3:object>
    string w = next(s, i);
    if (is_block_identifier(w)) {
        i = last_i;
        return false;
    }
    node* obj = module.get_property(w, true, ntDefinitionsBlock);
    if (!obj)
        obj = module.get_property(w, true, ntImplementationBlock);
    if (!obj)
        report_exp(exp_gen(etSyntaxError, s.line, s.filename,
                      "Object "OPENING_QUOTE"%s' doesn"CLOSING_QUOTE"t exist.", w.c_str()));
    else {
        if (obj->type != ntVariable && obj->type != ntFunction)
            report_exp(exp_gen(etSyntaxError, s.line, s.filename,
                          "Object "OPENING_QUOTE"%s"CLOSING_QUOTE" must be a variable/function and declared inside a definitions/implementation block.",
                          w.c_str()));
        obj->used = true;
    }
    node* c = new node(ntConnectTo, s);
    c->name = w;
    c->add_child(parse_dimension_expression(module, s, i, 0, true));
    c->add_child(parse_connection_condition(&module, s, i));
    c->add_child(new node(what.c_str()));
    c->add_child(obj);
    block->add_child(c);
    return true;
}

bool parse_inst_connects_to(node& module, source_line& s, int& i, node* block) {
    //[name:object] <0:index> <1:condition> <2:what is passed> <3:object>
    string what = "all";
    string w;
    if((what=next(s, i)) == "to") {
        return parse_connects_to(module, s, i, block, "all");
    } else if((w=next(s, i)) != "to")
        report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Expected 'to', but found "OPENING_QUOTE"%s"CLOSING_QUOTE".", w.c_str()));
    return parse_connects_to(module, s, i, block, what);
}

node* get_type_node(node* module, node* n) {
	if(!n) return NULL;
	if(n->size > 0)
        return n->children[n->size-1];
	node* type_node = get_submodule(module, n->name);
	if(!type_node) type_node = get_owner_sub_type(module, n->name);
    if(!type_node) type_node = get_module(n->name);
	return type_node;
}

node* parse_hierarchy(node& module, source_line& s, int& i, string delim, string terminator) {
	string w = next(s, i);
	if (w[0] == '#' || is_number(w) || is_string(w)) {
		i = last_i;
		node* exp = new node(ntExpression, s);
		exp->add_child(parse_expression(module, s, i, ";", true));
		i = last_i;
		return exp;
	}
	if (is_block_identifier(w)) {
		i = last_i;
		return NULL;
	}
	if (w == delim || w == "=")
		return NULL;
    make_used(w,& module);
	node* obj = module.get_property(w);
	if (!obj)
		report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Object "OPENING_QUOTE"%s' doesn"CLOSING_QUOTE"t exist.", w.c_str()));
	else {
    	obj->used = true;
    	obj->engaged = true;
    	if (obj->type != ntVariable)
    		report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Object "OPENING_QUOTE"%s' must be a variable and declared inside a 'definitions"CLOSING_QUOTE" block.", w.c_str()));
    }
	node* h = new node(ntNone /*ntHeirarchy*/, s);
	node* c = new node(w.c_str(), s);
	c->add_child(parse_dimension_expression(module, s, i, 0, true));
	c->add_child(obj);
	h->add_child(c);
	if(!obj) return h;
    node* type_node = get_type_node(&module, obj->children[0]);
    while (w != delim && w != "=" && !end_of_file) {
        w = next(s, i);
        if (w == ".")
            continue;
        if (w == delim || w == terminator || w == "=") {
            i = last_i;
            return h;
        }
        if (is_delimiter(w[0]))
            report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Unexpected statement "OPENING_QUOTE"%s"CLOSING_QUOTE".", w.c_str()));
        if (!type_node && obj)
            report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Module "OPENING_QUOTE"%s"CLOSING_QUOTE" has not any member.", obj->name.c_str()));
        //        node* nn = type_node->get_property(w, true, ntDefinitionsBlock);
        if(type_node)
            obj = type_node->get_property(w, true, ntDefinitionsBlock);
        else
            return h;
        if (!obj)
            report_exp(exp_gen(etSyntaxError, s.line, s.filename, ""OPENING_QUOTE"%s"CLOSING_QUOTE" was not a member of %s.", w.c_str(), type_node->name.c_str()));
        else {
            obj->used = true;
            obj->engaged = true;
        }
        node* cc = new node(w.c_str(), s);
        cc->add_child(parse_dimension_expression(module, s, i, 0, true));
        cc->add_child(obj);
		h->add_child(cc);
        node* last_type_node = type_node;
        if(obj)
          type_node = get_type_node(&module, obj->children[0]);
        else type_node = NULL;
		if(!type_node) {
            if(last_type_node && obj) {
                type_node = get_submodule(last_type_node, obj->children[0]->name);
            }
        }
    }
    h->parent = &module;
    return h;
}

bool parse_connection(node& module, source_line& s, int& i, node* block) {
    //<0:leftside> <1:right> .. <n:right>
    string w = next(s, i);
    i = last_i;
    if (is_block_identifier(w)) {
        i = last_i;
        return false;
    }
    node* c = new node(ntNone /*ntConnection*/, s);
    bool once = false;
    while (!end_of_file) {
        c->add_child(parse_hierarchy(module, s, i));
        w = next(s, i);
        if (w == ";") {
            if (!once)
                report_exp(exp_gen(etSyntaxError, s.line, s.filename, "No connection specified."));
            break;
        } else if (w == "<-" || w == "=") {
            once = true;
            continue;
        } else
            report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Bad connection syntax."));
    }
    if(c->children[c->size-1]->size == 1) {
        node* var = module.get_property(c->children[c->size-1]->children[0]->name);
		if (var) var->engaged = false;
    }
    block->add_child(c);
    block->parent = &module;
    return true;
}

node* parse_const_val(node& module, source_line& s, int& i, node_type block_type, string& w) {
    //[name] <0:type> <1:size> <2:initial values>
    if (block_type != ntDefinitionsBlock)
        report_exp(exp_gen(etSyntaxError, s.line, s.filename,
                      "A constant can be declared only inside a 'definitions' block."));

    node* n = new node(ntConstant, s);
    string name = get_name(s, i);
    node* prev;
    if (prev=module.get_property(name, false)) {
        report_exp(exp_gen(etSyntaxError, s.line, s.filename,
					  "Multiple declaration for "OPENING_QUOTE"%s"CLOSING_QUOTE".", name.c_str()));
        report_exp(exp_gen(etSyntaxError , prev->line, prev->file, "Previous declaration is here."));
    }
    n->name = name;
    n->add_child(new node("type", s));
    n->add_child(parse_dimension_expression(module, s, i));
    n->add_child(parse_initial_values(module, s, i));
    /*
        if (is_expression(s, i)) {
                                n->add_child(parse_expression(module, s, i, ","));
                i--;
                } else {
                w = next(s, i);
                if (w[0] == '"' && w.length() > 1)
                        n->add_child(new node(w, s));
                else
                        throw exp_gen(etSyntaxError, s.line, s.filename,
                        "Unable to assign "OPENING_QUOTE"%s' to constant `%s"CLOSING_QUOTE".", w.c_str(), n->name.c_str
                        ());
        }              */
    w = next(s, i);
    if (w == ",")
        return n;
    missing(w, s, i);
    w = ";";
    n->parent = &module;
    overwrite_previous_declaration(&module, n);
    return n;
}

void parse_block(node &module, node *block, source_line &s, int&i) {
    if (!block) {
        report_exp(exp_gen(etSyntaxError, s.line, s.filename, "No block was specified."));
        return;
    }
    string w = next(s, i);
    if (w == "whenever") {
        block->add_child(parse_conditional_block(module, s, i, block->type));
    } else if (w == "inherit")
        parse_inherit(module, s, i, block);
    else if (w == "const") {
        while (w != ";" && !end_of_file)
            block->add_child(parse_const_val(module, s, i, block->type, w));
    } else if (w == "record") {
        owner_record = NULL;
        block->add_child(parse_record(module, s, i, block->type));
    } else if (w == "field")
        block->add_child(parse_field(module, s, i, block->type));
    else if (w == "block")
        block->add_child(parse_function(module, s, i, block->type));
    else if (w == "submodule")
        block->add_child(parse_submodule(module, s, i, block->type));
    else if (is_type(w, block) || is_sub_module(&module, w) || is_owner_sub_type(&module, w) || w == "static" ) {
        if (w == "static") w += " "+next(s, i);
        while (w != ";" && !end_of_file)
            block->add_child(parse_declaration(module, s, i, w, block->type));
    } else if (block->type == ntParametersBlock) {
        i = last_i;
        block->add_child(parse_parameter(module, s, i, block->type));
    } else if (block->type == ntSpecificationsBlock) {
        i = last_i;
        parse_specifications(module, s, i, block);
    } else if (block->type == ntConnectsToBlock) {
        i = last_i;
        parse_inst_connects_to(module, s, i, block);
        /*
        parse_connects_to(module, s, i, block);
    } else if (block->type == ntInstConnectsToBlock) {
        i = last_i;
        parse_inst_connects_to(module, s, i, block);*/
    } else if (block->type == ntTopologyBlock) {
        i = last_i;
        parse_connection(module, s, i, block);
    } else if (get_parameter(&module, w)) {
        if (block->type != ntSpecificationsBlock)
            report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Parameter assignments should be declared inside a 'specifications' block."));
    } else if (block->type == ntDefinitionsBlock) {
        report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Undefined symbol "OPENING_QUOTE"%s"CLOSING_QUOTE".", w.c_str()));
        pass_until(s, i, ";");
    } else
        report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Unknown or invalid statement "OPENING_QUOTE"%s"CLOSING_QUOTE".", w.c_str()));
}

void parse_public_parameter(source_line& s, int& i) {
	string name =  next(s, i);
	node* module=new node("Discard");
	node* str = parse_initial_values(*module, s, i)->children[0];
	delete module;
	string expr=name+" = ";
	for(int j=0; j<str->size; j++)
		expr += " "+str->children[j]->name;
	if(str->size == 0) expr+="0";
	missing(next(s, i), s, i);
	if(!variable_exists(name.c_str())) {
        cout << "Parameter "OPENING_QUOTE"" << name.c_str() << ""CLOSING_QUOTE" is set to " << evaluate(expr.c_str(), str->line, str->file) << endl;
	}
	delete str;
}

node* parse_module(node_type type, string name, node* super, node* owner, source_line& s, int& i) {
    node* module = new node(type, s);
    module->name = name;
    module->parent = super;
    module->owner = owner;
    if(type != ntSubmodule)
        add_module(module);
    node * curr = NULL;
    string w;
    bool finished = false;
    int endif_num = 0;
    while (!finished && !end_of_file) {
        try {
            w = next(s, i);
            if (w == "specifications")
                curr = get_block(module, ntSpecificationsBlock, s, i);
            else if (w == "parameters")
                curr = get_block(module, ntParametersBlock, s, i);
            else if (w == "definitions")
                curr = get_block(module, ntDefinitionsBlock, s, i);
            else if (w == "topology")
                curr = get_block(module, ntTopologyBlock, s, i);
            else if (w == "implementation")
                curr = get_block(module, ntImplementationBlock, s, i);
            else if (w == "architecture")
                curr = get_block(module, ntArchitectureBlock, s, i);
            else if (w == "fields")
                curr = get_block(module, ntFieldsBlock, s, i);
            else if (w == "pass") {
                /*
                if(module->get_ancestor()->name == "typical_module") {
                    if((w = next(s, i)) != "to")
                        throw exp_gen(etSyntaxError, s.line, s.filename, "Expected 'to', but found "OPENING_QUOTE"%s"CLOSING_QUOTE".", w.c_str());
                    curr = get_block(module, ntConnectsToBlock, s, i);
                } else {
                    curr = get_block(module, ntInstConnectsToBlock, s, i);
                }*/
                curr = get_block(module, ntConnectsToBlock, s, i);
            } else if (w == "end") {
                finished = true;
                curr = NULL;
                missing(next(s, i), s, i);
            } else if (!curr) {
                report_exp(exp_gen(etSyntaxError, s.line, s.filename, ""OPENING_QUOTE"%s"CLOSING_QUOTE" was not recognized as a block identifier.", w.c_str()));
            } else {
                try {
                    i = last_i;
                    parse_block(*module, curr, s, i);
                }
                catch(exception_t e) {
                    report_exp(e);
                }
            }
        }
        catch(exception_t e) {
            report_exp(e);
            while (w != ";" && !end_of_file) {
                w = next(s, i);
                if (is_block_identifier(w) || w == "module") {
                    i = last_i;
                    break;
                }
                if (w == "{") {
                    int p = 1;
                    while (p > 0 && !end_of_file) {
                        w = next(s, i);
                        if (w == "{")
                            p++;
                        else if (w == "}")
                            p--;
                    }
                    break;
                }  
            }
        }
    }
    return module;
}

void parse(source_line& s, int& i, bool finishing) {
    string w = next(s, i);
    node* super = NULL;
    char *str = new char[s.str.length() + 1];
    try {
        if (w == "module") {
            string name = get_name(s, i);
            node* prev = NULL;
            if(prev=get_module(name)) {
                report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Multiple declaration for module "OPENING_QUOTE"%s"CLOSING_QUOTE".", name.c_str()));
                report_exp(exp_gen(etSyntaxError , prev->line, prev->file, "Previous declaration is here."));
                //name += "_copy";
            }
            w = next(s, i);
            if (w == "extends") {
                w = next(s, i);
                if(w == name) {
                    report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Module "OPENING_QUOTE"%s"CLOSING_QUOTE" cannot extend itself.", w.c_str()));
    				missing(w, s, i, ":");
                    w = "general_module";
                } else {
                    super = get_module(w);
                    if (super == NULL)
                        report_exp(exp_gen(etSyntaxError, s.line, s.filename, ""OPENING_QUOTE"%s"CLOSING_QUOTE" was not declared in this scope.", w.c_str()));
                    else {
                        //make_used(w);
                        super->used = true;
                        super->extended = true; //is extended
                    }
                    missing(next(s, i), s, i, ":");
                }
            } else {
				missing(w, s, i, ":");
                w = "general_module";
            }

            try {
                node_type nt = get_node_type(w);
                if (nt == ntNone) {
                    report_exp(exp_gen(etSyntaxError, s.line, s.filename, "Expected a super module name, but found "OPENING_QUOTE"%s"CLOSING_QUOTE".", w.c_str()));
                    nt = get_node_type("general_module");
                }
                parse_module(nt, name, super, NULL, s, i);
            }
            catch(exception_t e) {
                throw e;
            }
        } else if (w == "#include") {
            try {
                w = next(s.str, i, '\0', true);
            }
            catch(exception_t e) {
                throw exp_gen(etSyntaxError, s.line, s.filename, "Bad include directive syntax.");
            }
            string f = w;
            if (!file_exists(w.c_str()))
                w = relative_path(s.filename, w.c_str());
            if (!file_exists(w.c_str())) {
                for(int p=0; p<paths_count; p++) {
                    w = include_paths[p];
                    #ifdef __WIN32__
                    if (w[w.length()-1] != '\\') {
                        w += '\\';
                    }
                    #else
                    if (w[w.length()-1] != '/') {
                        w += '/';
                    }
                    #endif
                    w += f;
                    if(file_exists(w.c_str())) break;
                }
            }
            if (!file_exists(w.c_str())) {
                report_exp(exp_gen(etWarning, s.line, s.filename, "Failed to load file "OPENING_QUOTE"%s"CLOSING_QUOTE".", f.c_str()));
            } else if (is_msim_configuration_file(w)) {
                str = new char[w.length()+3];
                strcpy(str, (" " + w + " ").c_str());
                load_file(str, true);
            } else
                report_exp(exp_gen(etWarning, s.line, s.filename, "File "OPENING_QUOTE"%s"CLOSING_QUOTE" is not a M-Simulator configuration file.", f.c_str()));
        } else if (w == "#define") {
            misc.push_back(s);
            try {
                w = next(s.str, i);
            }
            catch(exception_t e) {
                throw exp_gen(etSyntaxError, s.line, s.filename, "Bad define directive syntax.");
            }
            if (!macros[w].empty())
                report_exp(exp_gen(etWarning, s.line, s.filename, "Identifier "OPENING_QUOTE"%s"CLOSING_QUOTE" was already defined.", w.c_str()));
            string val("1");
            try {
                val = next(s.str, i, '\n', true);
            }
            catch(exception_t e) {
                val = "1";
            }
            macros[w] = val;
        } else if (w == "#ifdef" || w == "#ifndef") {
            bool m = (w == "#ifdef");
            try {
                w = next(s.str, i);
            }
            catch(exception_t e) {
                throw exp_gen(etSyntaxError, s.line, s.filename, "Bad ifdef directive syntax.");
            }
            int n = 1;
            if ((macros[w].empty() && m) || (!macros[w].empty() && !m)) {
                while (n > 0 && !end_of_file) {
                    source_line ss = sourcecode.front();
                    sourcecode.pop_front();
                    int k = 0;
                    if (ss.str.length() > 0) {
                        string ww = next(ss, k);
                        if (ww == "#ifdef" || ww == "#ifndef" || ww == "#if")
                            n++;
                        if (ww == "#endif")
                            n--;
                        if (n == 1 && (ww == "#else" || ww == "#elif")) {
                            endif_num++;
                            break;
                        }
                    }
                }
            } else
                endif_num++;
        } else if (w == "#if") { // it can evaluate parameters (used for records)
            endif_num++;
            next_line(s, i);
        } else if (w == "#endif") {
            if (endif_num == 0)
                throw exp_gen(etSyntaxError, s.line, s.filename,
                              "Misplace endif directive.");
            else
                endif_num--;
            //misc.push_back(s);
        } else if (w == "function") {
            node m(ntProcedure, s);
            node* fn = parse_function(m, s, i, ntImplementationBlock);
            add_global_function(fn);
            misc.push_back(gen_line(s.line, s.filename, " function "));
            misc.push_back(gen_line(s.line, s.filename, fn->name));
        } else if (w == "procedure") {
            node m(ntProcedure, s);
            node* fn = parse_procedure(m, s, i, ntImplementationBlock);
            add_global_function(fn);
            misc.push_back(gen_line(s.line, s.filename, " function "));
            misc.push_back(gen_line(s.line, s.filename, fn->name));
        } else if (w == "parameter") {
            parse_public_parameter(s, i);
        } else if (w.substr(0, 2) == "#c") {
            int idx = s.str.find('#', 0);
            s.str[idx] = ' ';
            s.str[idx+1] = '#';
            s.str = trim(s.str);
            int _i = i;
            if(s.str == "#define") {
                try {
                    w = next(s.str, i);
                }
                catch(exception_t e) {
                    throw exp_gen(etSyntaxError, s.line, s.filename, "Bad define directive syntax.");
                }
                if (!macros[w].empty())
                    report_exp(exp_gen(etWarning, s.line, s.filename, "Identifier "OPENING_QUOTE"%s"CLOSING_QUOTE" was already defined.", w.c_str()));
                string val("1");
                try {
                    val = next(s.str, i, '\n', true);
                }
                catch(exception_t e) {
                    val = "1";
                }
                macros[w] = val;
            }
            i = _i;
            next_line(s, i);
            misc.push_back(s);
        } else if (w[0] == '#') {
            misc.push_back(s);
            next_line(s, i);
        } else {
            string lst = "";
            while (w != "module" && w != "procedure" && w != "function" && w != "parameter" && w[0] != '#' && w != "") {
                if (is_keyword(w) || w == "field" || w == "record")
                    throw exp_gen(etSyntaxError, s.line, s.filename, "Unexpected statement "OPENING_QUOTE"%s"CLOSING_QUOTE".", w.c_str());
                if (w == "{") {
                    int b = 1;
                    misc.push_back(gen_line(s.line, s.filename, w));
                    while (b > 0) {
                        w = next(s, i);
                        if (w == "{")
                            b++;
                        else if (w == "}") 
            				b--;
            			if(b > 0) {
                            if ( is_block_identifier(w) || is_keyword(w) || end_of_file ) {
                                throw exp_gen(etSyntaxError, s.line, s.filename, "Missing statement "OPENING_QUOTE"}"CLOSING_QUOTE".");
                            }
                        }
                        misc.push_back(gen_line(s.line, s.filename, w));
                        if(b==0) break;
                    }
                } else
                    misc.push_back(gen_line(s.line, s.filename, w));
                lst = w;
                w = next(s, i, false);
            }
            i = last_i;
            if (sourcecode.size() != 0 && !finishing)
                parse(s, i, true);            
        }
    }
    catch(exception_t e) {
        throw e;
    }
}

void parse() {
    source_line s;
    int i = 0;
    while (sourcecode.size() > 0) {
        try {
            parse(s, i);
        }
        catch(exception_t e) {
            try {
                report_exp(e);
                string w("");
                while (w != ";" && !end_of_file) {
                    w = next(s, i);
                    if (w == "module" || w[0] == '#' || w == "function" || w == "procedure" || is_type(w)) {
                        i = last_i;
                        break;
                    }
                }
            }
            catch(exception_t e) {
                report_exp( e );
            }
        }
    }
}


