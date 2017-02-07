/**
 * @file syntax.h
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

#ifndef SYNTAX_H
#define SYNTAX_H

#include "mnemonics.h"

#include <list>
#include <set>
#include <map>
#include <iostream>
#include <string>
#include <fstream>

#include "exception.h"

typedef enum {
    // General types
	ntNone = 0x0000000, ntGeneral = 0x0000002,
	ntIterator = 0x0000004,

    // Member types
    ntConstant = 0x0000008, ntParameter = 0x0000010, ntVariable = 0x0000020,
	ntRecord = 0x0000040, ntField = 0x0000080, ntSubmodule = 0x04000000,

    // Inside block expressions
	ntConnectTo = 0x0000200, ntExpression = 0x0000400,
	ntCondition = 0x0000800,

    // Function associated types
	ntProcedure = 10, ntFunction = 6,
	ntTraceFunction = 7,
	ntBreakpointFunction = 5,  ntExternalParameter = 3,

    // Block types
    ntBracketsBlock = 0x80000000, //ntInstConnectsToBlock = 0x40000000,
    ntExpressionBlock = 0x20000000, ntSpecificationsBlock = 0x10000000,
	ntDefinitionsBlock = 0x08000000, ntTopologyBlock = 0x0008000,
    ntConnectsToBlock = 0x02000000,  ntParametersBlock = 0x01000000,
    ntImplementationBlock = 0x00800000, ntArchitectureBlock = 0x00400000,
    ntFieldsBlock = 0x00200000, ntIf = 0x00100000, 
} node_type;

struct source_line {
    char* filename;
    string str;
    int line;
};

class node;
typedef struct {
    node* cond;
    int val;
} overwrite_obj_t;

class node {
private:
    node* ancestor;
public:
    int line;
    char* file;
    string name;
    bool extended;
    union {
        int index;
        int value;
    };
    bool used, engaged;
    union {
        bool evaluated;
        bool merged;
    };
    bool is_used_module;
    bool is_used_module_assigned;
    bool connected;
    bool has_initialize_function;
    bool inserted;
    node_type type;
    node** children;
    node* parent;   // it extends parent
    node* owner;    // it is a submodule of owner
    node* container;
    long content_size;
    string* content;
    bool* assigned;
    bool* initialized;
    int size;
    int max_size;
    int used_var_size;
    map<node*, overwrite_obj_t> overwrited_modules;
    set<node*> inserted_for;

    node(node_type type, source_line p);
    node(const char* name, source_line p);
    node(const char* name, node* nt);
    node(node_type type, node* nt);
    node(const char* name);
    node(bool val, source_line p);
	bool has_child(string name);
	bool is_child_of(node* module);
	bool is_child_of(string p, string submodule_name);
    void alloc(long len, source_line p);
    void assign(string v, int idx = 0);
    void assign(string pname, string v, int idx = 0);
    void remove_child(int idx);
    node* add_child(node* n);
    node* get_ancestor();
    node* get_block(int block_type);
    node* get_property(string name, bool recursive = true, int block_type = ntNone, bool cond=false);
    node* get_property_owner(string name, bool recursive = true, int block_type = ntNone);
    node* get_property(int type, bool recursive = true);
    node* explore_for_variable(node* block, string name, bool cond=false);
    void print(string indent = "");
    string toString();

};


extern list<source_line>misc;
extern list<source_line>sim;
extern list<source_line> sourcecode;
extern map<string, string> macros;

extern node** type_modules;
extern node** global_functions;
extern int type_modules_size;
extern int global_functions_size;

extern string include_paths[100];
extern int paths_count;
extern int endif_num;
extern bool has_breakpoint;

void readline(ifstream& f, char* l);
bool file_exists(const char* filename);
string relative_path(char* _base, const char* path);

string next(string &str, int& i, char finish = '\0', bool trim = false);
string next(source_line&s, int&i, bool thr = true);
source_line gen_line(int line, char* filename, string s);
void load_file(char* filename, bool first = false);

bool is_user_type(string w);
bool is_type(string w, node* block=NULL);
bool is_number(string s);
bool is_int(string s);
bool is_string(string w);
bool is_msim_configuration_file(string f);
bool is_hierarchy_delimiter(string w);
bool is_word(char c);
bool is_digit(char c);
bool is_hex(char c);
bool is_dim(char c);
bool is_delimiter(char c);
bool is_arithmatic_operator(char c);
bool is_container(char c);
bool is_math_func(string w);
bool is_physical_property(string w);
bool is_block_identifier(string w);
bool is_keyword(string w);
bool is_boolean(string s);
bool is_primary_type(string w);
bool is_not_used_module(string w);
bool is_expression(source_line& s, int i, int d);
bool is_sub_module(node* module, string name);
bool is_owner_sub_type(node* module, string name);
bool is_external_scope(string scope);

bool check_name(string name);
bool check_type(string name);

const char* set_dim(char c, long double d, bool c_ex=false);

void add_include_path(char* f);
const char* type2str(node_type t);

int get_parameter_index(node* module, node* n);
node* get_module(string name);
node* get_submodule(node* module, string name);
node* get_owner_sub_type(node* module, string name);
node* get_global_functions(string name);
node* get_parameter(node* module, string w, int t = ntParameter, int bt = ntNone);
node* get_type_node(node* module, node* n);

void parse();

#endif

