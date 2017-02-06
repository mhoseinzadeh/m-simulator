/**
 * @file compile.cpp
 *
 * @brief C++ code generation and compiling using an external command
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

#include <list>
#include <map>
#include <set>
#include <istream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <stdlib.h>
#include <sstream>

#include "evaluation.h"
#include "exception.h"
#include "syntax.h"
#include "compile.h"
#include "fixedcodes.h"

#define TRACE 0

using namespace std;

struct msim_line {
    int line;
    char* filename;
};

struct cpp_line: public msim_line {};
long size_threshold = 8192;
bool has_report_function = false;
bool only_prototype = false;
bool use_virtual_address = true;
list<source_line> source;
map<int, msim_line> line_map; // to map the generated c++ line-number to msim line-number
node* current_module=NULL;
int add_function_line_dim_idx = 1;
int add_function_line_typ_idx = 0;

map<string, string> external_parameter_values;

string generate_source();

void initial_parameter(node* module, node* param, bool initial = true);
void assign_parameter(node* module, node* param);
void update_parameters(node* module, int block_type = ntParametersBlock, bool initial = true);
void reset_parameters(node* module);
void update_all(node* module);
void assign_specifications(node* module);

void generate_function(node * n);
void generate_miscs();
void generate_module(node* module, bool is_top_level = false);
void generate_not_extended_modules();
void generate_top_level();

void add_function_line(node* module, node* n, int &i, bool &is_pointer, node* &chain);

string get_type_scope(node* module, string type);

const char* strlwr(string s) {
    string r = "";
    for(unsigned int i=0; i<s.length(); i++)
        if(s[i] >= 'A' && s[i] <= 'Z')
            r += (char)(s[i]+'a'-'A');
    else
        r += s[i];
    return r.c_str();
}

char* itoa(int number) {
    char* res=new char[10];
    sprintf(res, "%d", number);
    return res;
}


msim_line code_line(int l, char* f) {
    msim_line ll;
    ll.line = l;
    ll.filename = f;
    return ll;
}

inline source_line gen_line(node*n) {
    return gen_line(n->line, n->file, n->name);
}

inline void add_line(node* n) {
    source.push_back(gen_line(n));
}

void add_line(node* m, const char* fmt, ...) {
    char line[1024];
    va_list argptr;
    va_start(argptr, fmt);
    vsprintf(line, fmt, argptr);
    va_end(argptr);
    add_line(new node(line, m));
}

void display_block(node* module, node_type block_type) {
    node* block = module->get_block(block_type);
    if (block)
        for (int i = 0; i < block->size; i++) {
        cout << "    " << block->children[i]->name;
        for (int j = 0; j < block->children[i]->content_size; j++) {
            if (block->children[i]->assigned[j])
                cout << " A:" << block->children[i]->content[j].c_str();
            else if (block->children[i]->initialized[j])
                cout << " I:" << block->children[i]->content[j].c_str();
            else
                cout << " NAN(" << block->children[i]->content[j].c_str() << ")";
        }
        cout << endl;
    }
    if (module->parent)
        display_block(module->parent, block_type);
}

void display_module(node* module) {
    cout << module->name.c_str() << endl;
    display_block(module, ntParametersBlock);
    display_block(module, ntDefinitionsBlock);
}

void display_node(node* n, string indent="") {
    if(!n) {
        cout << indent.c_str() << "[null]" << endl;
        return;
    }
    cout << indent.c_str() << "[" << n->name.c_str() << "]: " << type2str(n->type) << endl;
    if (n->size == 0) return;
    cout << indent.c_str() << "{" << endl;
    for (int i = 0; i < n->size; i++) {
        display_node(n->children[i], indent+"  ");
    }
    cout << indent.c_str() << "  " << "assigned = { ";
    for (int i = 0; i < n->content_size; i++) {
        cout<< n->assigned[i] << " ";
    }
    cout << "}" << endl;
    cout << indent.c_str() << "}" << endl;
}

string generate_indent(int cnt) {
    string out = "";
    while(cnt-->0)
        out += "    ";
    return out;
}

bool is_used_module(node* module, node* look);
void report_notused_warnings(node* module, node* block, bool top_level) {
    if(!is_used_module(top_level_module(), module) && module->get_ancestor()->name != "typical_system") return; 
    turn_on_reporting();
    for (int j = 0; j < block->size; j++) {
        if (block->children[j]->type == ntIf) {
            if(evaluate_condition(module, block->children[j]))
                report_notused_warnings(module, block->children[j]->children[1], top_level);
            else
                report_notused_warnings(module, block->children[j]->children[2], top_level);
        } else if (top_level) {
            if (!is_user_type(block->children[j]->children[0]->name) && block->children[j]->type != ntSubmodule && !is_sub_module(module, block->children[j]->children[0]->name))
                report_exp (exp_gen(etWarning, block->children[j]->line, block->children[j]->file, "Variable "OPENING_QUOTE"%s"CLOSING_QUOTE" is defined but never used.", block->children[j]->name.c_str()));
        } else {
            if (!block->children[j]->used && block->children[j]->type != ntSubmodule) {
                string str;
                switch(block->children[j]->type) {
                case ntParameter: str = "Parameter"; break;
                case ntConstant: str = "Constant";   break;
                case ntField: str = "Field";         break;
                case ntSubmodule: str = "Sub-module";break;
                default: str = "Variable";
                }
                report_exp(exp_gen(etWarning, block->children[j]->line, block->children[j]->file, "%s "OPENING_QUOTE"%s"CLOSING_QUOTE" is defined but never used.", str.c_str(), block->children[j]->name.c_str()));
            }
        }
    }
}

void report_notused_warnings() {
    for (int idx = 0; idx < type_modules_size; idx++) {
        node* n = type_modules[idx];
        bool is_top_level = (n->get_ancestor()->name == "typical_system");
        for (int i = 0; i < n->size; i++) {
            if (n->children[i]->type == ntDefinitionsBlock && is_top_level)
                report_notused_warnings(n, n->children[i], true);
            if (n->children[i]->type & (ntDefinitionsBlock|ntParametersBlock|ntFieldsBlock))
                report_notused_warnings(n, n->children[i], false);
        }
    }
}

string new_empty_line(list<source_line>::iterator it, int codeline, int indent) {
    string src = "\n";
#if TRACE == 1
    char b[42];
    sprintf(b, "%03d [%s:%03d]:  ", codeline, it->filename, it->line + 1);
    src += b;
#endif
    src += generate_indent(indent);
    line_map[codeline] = code_line(it->line + 1, it->filename);
    return src;
}
void generate_initial_function();
string generate_source() {
    int lastline = 0;
    int codeline = 0;
    int indent = 0;
    char* file;
    bool newline = false;
    bool inserted = false;
    bool was_semicolon = false;
    string src, prc;
    generate_header_codes(src, codeline);
    int semicolons = 1;
    bool directive = false;
    for (list<source_line>::iterator it = source.begin(); it != source.end(); it++) {
        if(lastline != it->line) {
            if(directive) 
                newline = true;
            if (it->str[0] == '#')
                directive = true;
            else
                directive = false;
        }
        if (it->str[it->str.length()-1] == '{' && !directive) {
            if(newline) {
                src = prc;
                src += new_empty_line(it, codeline, indent);
            }
            if(it->str == "{")
                it->str = " {";
            else if(!newline) it->str = " "+it->str;
            src += it->str;
            prc = src;
            src += new_empty_line(it, codeline, ++indent);
            if(newline)
                codeline++;
            newline = true;
            was_semicolon = true;
        } else if (it->str[0] == '}' && !directive) {
            char temp[5]="";
            list<source_line>::iterator next = it;
            next++;
            if(next != source.end()) {
                if(next->str.length() > 3) {
                    for(int i=0; i < 4; i++) {
                        temp[i] = next->str[i];
                    }   
                    temp[4] = 0;
                }
            }
            src = prc;
            src += new_empty_line(it, codeline, --indent);
            src += it->str;
            if(!strcmp(temp, "else")) {
                //src += " ";
                prc = src;
                newline = false;
                was_semicolon = false;
                codeline++;
            } else {
                prc = src;
                src += new_empty_line(it, ++codeline, indent);
                newline = true;
                was_semicolon = true;
            }
        } else if (it->str[it->str.length()-1] == ';' && !directive) {
            if(newline) {
                src = prc;
                src += new_empty_line(it, codeline, indent);
            }
            src += it->str;
            prc = src;
            if(semicolons-- > 1) {
                src += " ";
            } else {
                if(newline)
                    codeline++;
                src += new_empty_line(it, codeline, indent);
                newline = true;
            }
            was_semicolon = true;
        } else if((it->line != lastline || it->filename != file)) {
            if(was_semicolon){
                src = prc;
            }
            src += new_empty_line(it, codeline, indent);
            src += it->str;
            prc = src;
            codeline++;
            newline = false;
            was_semicolon = false;
        } else {
            if(!newline)
                src += " ";
            else
                codeline++;
            src += it->str;
            prc = src;
            newline = false;
        }
        if(it->str == "for")
            semicolons = 3;
        lastline = it->line;
        file = it->filename;
    }
    generate_footer_codes(src);
    return src;
}

void generate_expression(node *module, node *exp) {
    bool is_pointer = false;
    add_function_line_dim_idx = 0;
    add_function_line_typ_idx = 1;
    node* chain = module;
    for (int j=0; j < exp->size; j++) {
        add_function_line(module, exp, j, is_pointer, chain);
    }
    add_function_line_dim_idx = 1;
    add_function_line_typ_idx = 0;
}

void generate_physical_attributes(node *n, node* r, int what = 3) {
    if (!n) return;
    if (n->size == 0) return;
    if (!current_module) current_module = top_level_module();
    if(what&1==1)
        add_line(n, "if(sim_step >= offset) {");
    for (int i=0; i < n->size; i++) {
        if(((n->children[i]->name == "latency") && ((what&1) == 1)) || ((n->children[i]->name == "energy") && ((what&2) == 2))) {
            if(n->children[i]->name == "latency" && r->size > 4) {
                if(!r->children[4]->has_child("slip") && r->children[4]->has_child("offline")) {
                    add_line(n->children[i], "add_unavailable_timeslot(updating_blocks_table, block_address(addr), latency + ");
                    generate_expression(current_module, n->children[i]->children[0]);
                    add_line(n->children[i], ", updating_blocks_table_index);");
                    continue;
                }
                if(r->get_ancestor()->name == "typical_module" && !r->children[4]->has_child("slip") && 
                   (r->name == "read_block" || r->name == "write_block" || r->children[4]->has_child("reader") || r->children[4]->has_child("writer"))) {
                    add_line(n->children[i], "latency = time_to_availability(latency, block_address(addr), updating_blocks_table) + (");
                    generate_expression(current_module, n->children[i]->children[0]);
                    add_line(n->children[i], ");");
                    continue;
                }
            }
            //            if(n->children[i]->name == "latency" && (what&1))
            //                add_line(n->children[i], "_old_latency = latency;");
            add_line(n->children[i]);
            //            if(n->children[i]->name == "latency")
            //                add_line(n->children[i], "=");
            //            else
            add_line(n->children[i], "+=");
            generate_expression(current_module, n->children[i]->children[0]);
            add_line(n->children[i], ";");
            //            if(n->children[i]->name == "latency")
            //                add_line(n, "latency = _old_latency+ceil(latency*frequency)/frequency;");
            //                add_line(n, "latency = _old_latency+latency;");
        }
    }
    if(what&1==1)
        add_line(n, "}");
}

node* stack[32];
int stack_idx = 0;

void generate_function_body(node* n, int start=0) {
    for (int i = start; i < n->size; i++) {
        node* cc = n->children[i];
        if (cc->type == ntTraceFunction && macros["VERBOSE"].empty()) {
            add_line(cc, "{} ");
            while (i < n->size) {
                if (n->children[++i]->name == ";") break;
            }
        } else if (cc->type == ntBreakpointFunction && (macros["STEP"].empty() && macros["VERBOSE"].empty())) {
            add_line(cc, "{} ");
            while (i < n->size) {
                if (n->children[++i]->name == ";") break;
            }
        } else if (cc->type == ntIf) {
            if(evaluate_condition(NULL, cc)) 
                generate_function_body(cc->children[1]->children[0]);
            else
                generate_function_body(cc->children[2]->children[0]);
        } else {
            add_line(cc);
        }
    }
}

void generate_function(node * n) {
    current_module = NULL;
    node* void_node = new node("void");
    add_line(n->children[1]);
    add_line(n);
    for (int i = 0; i < n->children[0]->size; i++)
        add_line(n->children[0]->children[i]);
    add_line(n->children[3]->children[0]);
    generate_physical_attributes(n->children[2], n);
    generate_function_body(n->children[3], 1);
}

string generate_field_use(node * module, node * val) {
    node* n = val->children[0];
    if (n->type != ntField)
        return "";
    string str = "((";
    str += val->name;
    string lsb = evaluate_expression_result(module, n->children[0]);
    if (!is_int(lsb))
        report_exp(exp_gen(etSyntaxError, n->line, n->file, "An error occurred while compiling field "OPENING_QUOTE"%s"CLOSING_QUOTE": %s.", n->name.c_str(), lsb.c_str()));
    if (lsb != "0") {
        str += " >> ";
        str += lsb;
        str += ")";
    } else
        str += ")";
    
    if (n->children[1]->name != "end") {
        string msb = evaluate_expression_result(module, n->children[1]);
        if (!is_int(msb))
            report_exp(exp_gen(etSyntaxError, n->line, n->file, "An error occurred while compiling field "OPENING_QUOTE"%s"CLOSING_QUOTE": %s.", n->name.c_str(), msb.c_str()));
        int m = atoi(msb.c_str()) - atoi(lsb.c_str()) + 1;
        //        char len_str[64];
        //        sprintf(len_str, "/* m=%d, msb=%s, lsb=%s */", m, msb.c_str(), lsb.c_str());
        //        str += len_str;
        string b = "0x";
        int r = m % 4;
        switch(r) {
        case 1: b += '1'; break;
        case 2: b += '3'; break;
        case 3: b += '7'; break;
        }
        m -= r;
        while (m > 0) {
            b += "f";
            m -= 4;
        }
        if (b == "0x") {
            b = "0x0";
            string ext="";
            if(module != current_module) {
                ext = " (extended by module "OPENING_QUOTE"" + current_module->name + ""CLOSING_QUOTE")";
            }
            report_exp(exp_gen(etWarning, n->line, n->file, "Empty field "OPENING_QUOTE"%s' in module `%s"CLOSING_QUOTE"%s.", n->name.c_str(), module->name.c_str(), ext.c_str()));
        }
        str += " & ";
        str += b;
        str += ")";
    } else
        str += ")";
    
    return str.c_str();
}

node* has_function(node* module, node* block, string fn) {    
    if(!block) return NULL;
    for(int i=0; i<block->size; i++) {
        if(block->children[i]->type == ntIf) {
            if(evaluate_condition(module, block->children[i])) {
                node* n = has_function(module, block->children[i]->children[1], fn);
                if(n) return n;
            } else {
                node* n = has_function(module, block->children[i]->children[2], fn);
                if(n) return n;
            }
        } else if(block->children[i]->name == fn) 
            return block->children[i];
    }
    return NULL;
}

node* has_function(node* module, string fn) {
    if(module->parent) {
        node* n = has_function(module->parent, fn);
        if(n) return n;
    }
    return has_function(module, module->get_block(ntImplementationBlock), fn);
}

void generate_initialize_modules_function(node* module, node* block, string chain="top_level_module->") {
    //return;
    if(!module) return;
    if(!block) return;
    for(int i = 0; i < block->size; i++) {
        node* n = block->children[i];
        if(n->type == ntIf) {
            if(evaluate_condition(module, n)) {
                generate_initialize_modules_function(module, n->children[1], chain);
            } else {
                generate_initialize_modules_function(module, n->children[2], chain);
            }
        } else if(n->type == ntVariable) {
            node* m = get_submodule(module, n->children[0]->name);
            if(!m) m = get_owner_sub_type(module, n->children[0]->name);
            if(!m) m = get_module(n->children[0]->name);
            if(m) {
                node* fn = has_function(m, "initialize");
                if(fn) {
                    string size_str = evaluate_expression_result(module, n->children[1]);
                    if (!is_int(size_str) && !size_str.empty()) {
                        report_exp(exp_gen(etSyntaxError, n->line, n->file, "Could not evaluate variable's size expression: %s.", size_str.c_str()));
                    }
                    if (size_str.empty())
                        size_str = "1";
                    long size = atol(size_str.c_str());
                    if(size > 1) {
                        add_line(fn, "for(int i=0; i<%ld; i++)", size);
                        chain += n->name+"[i]->";
                    }
                    generate_initialize_modules_function(m, m->get_block(ntDefinitionsBlock), chain);
                    if(n->inserted)
                        add_line(fn, "%sinitialize();", chain.c_str());
                    
                }
            }
        }
    }
}

void generate_initialize_modules_function() {
    node* module = top_level_module();
    if(!module) return;
    add_line(module, "%s* top_level_module;", module->name.c_str());
    add_line(module, "void __initialize_modules() {");
    generate_initialize_modules_function(module, module->get_block(ntDefinitionsBlock));
    if(module->has_initialize_function)
        add_line(module, "top_level_module->initialize();");
    add_line(module, "}");
}

void generate_top_level_step_function() {
    node* module = top_level_module();
    if(!module) return;
    add_line(module, "void __top_level_step() {");
    add_line(module, "top_level_module->__step();");
    add_line(module, "}");
}

void generate_initial_function() {
    bool init = false;
    for (list<source_line>::iterator it = misc.begin(); it != misc.end(); it++)
    {
        if (it->str == " function ") {
            it++;
            node* fn = get_global_functions(it->str);
            if(fn->name == "initialize") {
                generate_function(fn);
                return;
            }
        }
    }
    if(!init) {
        node* n = new node("");
        //add_line(n, "void initialize() {} ");
    }
}

void generate_miscs() {
    for (list<source_line>::iterator it = misc.begin(); it != misc.end(); it++)
    {
        if (it->str == " function ") {
            it++;
            node* fn = get_global_functions(it->str);
            if(fn->name != "initialize")
                generate_function(fn);
        } else
            source.push_back(*(it));
    }
}

void initial_parameter(node* module, node* param, bool initial) {
    //Update a parameter or a constant to its initial/final value
    string size_str = evaluate_expression_result(module, param->children[1]);
    if (!is_int(size_str) && !size_str.empty()) {
        report_exp(exp_gen(etSyntaxError, param->line, param->file, "Could not evaluate parameter's size expression: %s.", size_str.c_str()));
    }
    if (size_str.empty())
        size_str = "1";
    long size = atol(size_str.c_str());
    if (size != param->content_size || initial) {
        if (param->content_size > 0) {
            delete[]param->content;
            delete[]param->assigned;
            delete[]param->initialized;
        }
        if (!initial)
            report_exp(exp_gen(etWarning, param->line, param->file, "Parameter "OPENING_QUOTE"%s' was initialized again and reset to it"CLOSING_QUOTE"s initial value.",
                               param->name.c_str()));
        param->content_size = size;
        if(size != -1) {
            param->content = new string[size];
            param->assigned = new bool[size];
            param->initialized = new bool[size];
            for (int i = 0; i < size; i++) {
                param->assigned[i] = false;
                param->initialized[i] = false;
            }
        } else {
            // size = inf
        }
    }
    try {
        for (int i = 0; i < param->children[2]->size; i++) {
            if (i > param->content_size) {
                report_exp(exp_gen(etSyntaxError, param->line, param->file, "Out of range initialization."));
                return;
            }
            param->initialized[i] = true;
            if (param->assigned[i] && !initial)
                continue;
            param->content[i] = evaluate_expression_result(module, param->children[2]->children[i]);
        }
    }
    catch(...) {
        report_exp(exp_gen(etSyntaxError, param->line, param->file, "Could not assign initial value."));
    }
}

void assign_parameter(node* module, node* param) {
    //set value of exp to a parameter or a constant
    //param has a specification format
    //[name] <0: index> <1: pointer to parameter | external parameter name> <2: value>
    if(is_external_scope(param->name))
        //It is an external parameter
        return;
    int idx = get_parameter_index(module, param);
    if (idx >= param->children[1]->content_size) {
        report_exp(exp_gen(etSyntaxError, param->line, param->file, "Index "OPENING_QUOTE"%s"CLOSING_QUOTE" out of range.", param->name.c_str()));
        return;
    }
    param->children[1]->content[idx] = evaluate_expression_result(module, param->children[2]);
    param->children[1]->assigned[idx] = true;
    //cout << param->name.c_str() << " (" << module->name.c_str() << "): " << param->children[1]->content[idx].c_str() << endl;
}

void assign_external_parameter(node* module, node* param) {
    //set value of exp to a parameter or a constant
    //param has a specification format
    //[name] <0: index> <1: pointer to parameter | external parameter name> <2: value>
    if(!is_external_scope(param->name))
        //It is not an external parameter
        return;
    string ext_param = module->name+"::"+param->name+"::"+param->children[1]->name;
    string value_param = evaluate_expression_result(module, param->children[2]);
    external_parameter_values[ext_param] = value_param;
    while(module->parent) {
        module = module->parent;
        ext_param = module->name+"::"+param->name+"::"+param->children[1]->name;
        external_parameter_values[ext_param] = value_param;
    }
}

void update_parameters(node* module, node* block, bool initial) {
    if (block) {
        for (int i = 0; i < block->size; i++)
            if(block->children[i]->type == ntIf) {
            if(evaluate_condition(module, block->children[i]))
                update_parameters(module, block->children[i]->children[1], initial);
            else
                update_parameters(module, block->children[i]->children[2], initial);
        } else if (block->children[i]->type == ntRecord) {
            update_parameters(module, block->children[i]->children[1], initial);
        } else if (block->children[i]->type == ntSubmodule) {
            if(initial)
                reset_parameters(block->children[i]);
            else
                update_all(block->children[i]);
        } else
            initial_parameter(module, block->children[i], initial);
    }
}

void update_parameters(node* module, int block_type, bool initial) {
    //Reset all parameters of this and super modules to their initial values
    if (!module) return;
    update_parameters(module->parent, block_type, initial);
    node* params = module->get_block(block_type);
    update_parameters(module, params, initial);
}

void reset_parameters(node* module) {
    update_parameters(module);
    update_parameters(module, ntDefinitionsBlock);
    update_parameters(module, ntArchitectureBlock);
}

void update_all(node* module) {
    //initialize all constants and variables
    update_parameters(module, ntParametersBlock, false);
    update_parameters(module, ntDefinitionsBlock, false);
}

void assign_specifications(node* module, node* block) {
    if (!block) return;
    for (int i = 0; i < block->size; i++)
        if(block->children[i]->type == ntIf) {
        if(evaluate_condition(module, block->children[i]))
            assign_specifications(module, block->children[i]->children[1]);
        else
            assign_specifications(module, block->children[i]->children[2]);
    } else
        assign_parameter(module, block->children[i]);
}

void assign_specifications(node* module) {
    //Assign all parameters to specified values
    if (!module)
        return;
    assign_specifications(module->parent);
    node* params = module->get_block(ntSpecificationsBlock);
    assign_specifications(module, params);
}


void assign_external_specifications(node* module, node* block) {
    if (!block) return;
    for (int i = 0; i < block->size; i++)
        if(block->children[i]->type == ntIf) {
        if(evaluate_condition(module, block->children[i]))
            assign_external_specifications(module, block->children[i]->children[1]);
        else
            assign_external_specifications(module, block->children[i]->children[2]);
    } else if(block->children[i]->type == ntExternalParameter)
        assign_external_parameter(module, block->children[i]);
}

void assign_external_specifications(node* module) {
    //Assign all parameters to specified values
    if (!module)
        return;
    assign_external_specifications(module->parent);
    node* params = module->get_block(ntSpecificationsBlock);
    assign_external_specifications(module, params);
}

void estimate_external_parameters_with_cacti(node* module);

bool overwritten(node* n, node* module) {
    if(!module || module == n->parent) return false;
    if(n->overwrited_modules.count(module)) {
        overwrite_obj_t obj = n->overwrited_modules[module];
        if(!obj.cond) return true;
        //cout << "*WHENEVER* \"" << obj.cond->file << "\" (" << (obj.cond->line+1) << "): " << obj.val << " =?= " << evaluate_condition(module, obj.cond) << " " << module->name.c_str() << " " << n->parent->name.c_str() << " " << n->name.c_str() << endl;
        if(obj.val == 0x03) 
            return true;
        if(obj.val == 1) {
            if(evaluate_condition(module, obj.cond))
                return true;
        } else if(obj.val == 2) {
            if(!(evaluate_condition(module, obj.cond)))
                return true;
        }
    }
    if(module->parent) return overwritten(n, module->parent);
    return false;
}

string generate_variable_type(node* t) {
    string res = t->name;
    for(int i = 0; i < t->size; i++) {
        res += "::" + t->children[i]->name;
    }
    return res;
}

bool count_module_inserted_for(node* module, node* n) {
    if(n->inserted_for.count(module)) return true;
    if(module->parent) if(count_module_inserted_for(module->parent, n)) return true;
    return false;
}

void generate_variables(node* main_module, node* module, node* block) {
    if (!block) return;
    for (int i = 0; i < block->size; i++) {
        node* n = block->children[i];
        if (n->type == ntVariable) {
            node* tmodule = get_type_node(module, n->children[0]);
            if(tmodule) if(tmodule->type == ntSubmodule) continue;
            //            cout << "main_module (" << main_module->name.c_str() << "), module (" << module->name.c_str() << "), variable(" << n->name.c_str() << ") " << count_module_inserted_for(module, n) << endl;
            if(overwritten(n, main_module)) continue;
            if(count_module_inserted_for(module, n) && module->parent) continue;
            n->inserted_for.insert(module);
            n->inserted = true;
            if(n->content_size == -1)
                add_line(n->children[0], "dmap < uint64, %s > %s;", generate_variable_type(n->children[0]).c_str(), n->name.c_str());
            else if(is_user_type(n->children[0]->name) || is_sub_module(module, n->children[0]->name) || is_owner_sub_type(module, n->children[0]->name)) {
                if(n->content_size > 1)
                    add_line(n->children[0], "%s *%s[%d];", generate_variable_type(n->children[0]).c_str(), n->name.c_str(), n->content_size);
                else
                    add_line(n->children[0], "%s *%s;", generate_variable_type(n->children[0]).c_str(), n->name.c_str());
            } else if(n->content_size > 1)
                add_line(n->children[0], "%s %s[%d];", generate_variable_type(n->children[0]).c_str(), n->name.c_str(), n->content_size);
            else
                add_line(n->children[0], "%s %s;", generate_variable_type(n->children[0]).c_str(), n->name.c_str());
        } else if (n->type == ntIf) {
            if(evaluate_condition(module, n))
                generate_variables(main_module, module, n->children[1]);
            else
                generate_variables(main_module, module, n->children[2]);
        }
    }
}

void generate_variables(node* main_module, node* module) {
    if(module->parent) generate_variables(main_module, module->parent);
    node* block = module->get_block(ntDefinitionsBlock);
    generate_variables(main_module, module, block);
}

void generate_submodule_instanciations(node* module, node* block) {
    if (!block) return;
    for (int i = 0; i < block->size; i++) {
        node* n = block->children[i];
        if (n->type == ntVariable) {
            node* tmodule = get_type_node(module, n->children[0]);
            if(tmodule) {
                if(tmodule->type != ntSubmodule) continue;
            } else
                continue;
            //if(overwritten(n, module)) continue;
            //if(n->inserted_for.count(module)) continue;
            n->inserted_for.insert(module);
            n->inserted = true;
            if(n->content_size == -1)
                add_line(n->children[0], "dmap < uint64, %s > %s;", generate_variable_type(n->children[0]).c_str(), n->name.c_str());
            else if(is_user_type(n->children[0]->name) || is_sub_module(module, n->children[0]->name) || is_owner_sub_type(module, n->children[0]->name)) {
                if(n->content_size > 1)
                    add_line(n->children[0], "%s *%s[%d];", generate_variable_type(n->children[0]).c_str(), n->name.c_str(), n->content_size);
                else
                    add_line(n->children[0], "%s *%s;", generate_variable_type(n->children[0]).c_str(), n->name.c_str());
            } else if(n->content_size > 1)
                add_line(n->children[0], "%s %s[%d];", generate_variable_type(n->children[0]).c_str(), n->name.c_str(), n->content_size);
            else
                add_line(n->children[0], "%s %s;", generate_variable_type(n->children[0]).c_str(), n->name.c_str());
        } else if (n->type == ntIf) {
            if(evaluate_condition(module, n))
                generate_submodule_instanciations(module, n->children[1]);
            else
                generate_submodule_instanciations(module, n->children[2]);
        }
    }
}

void generate_submodule_instanciations(node* module) {
    if(module->parent)
        generate_submodule_instanciations(module->parent);
    node* block = module->get_block(ntDefinitionsBlock);
    generate_submodule_instanciations(module, block);
}

void generate_internal_variables(node* module) {
    //    module->
    //	add_line(module, "_data* __data_stream;");
}

void generate_internal_functions(node* module) {
    
}



bool a_block_has_it(node* module, node* block, node* fn) {
    if(!block) return false;
    for(int i=0; i<block->size; i++) {
        node* n = block->children[i];
        if (n->type == ntIf) {
            if(evaluate_condition(module, n)) {
                if(a_block_has_it(module, n->children[1], fn))
                    return true;
            } else {
                if(a_block_has_it(module, n->children[2], fn))
                    return true;
            }
        } else if(fn->name == n->name) return true;
    }
    return false;
}

bool parents_have_it(node* module, node* parent, node* fn) {
    if(!parent) return false;
    if(a_block_has_it(module, parent->get_block(ntImplementationBlock), fn))
        return true;
    return parents_have_it(module, parent->parent, fn);
}

/*
bool has_pipelined_function(node* n, node* module) {
    map<node*, overwrite_obj_t>::iterator iter = n->overwrited_modules.begin();
    cout << "--------------" << n->parent->name.c_str() << ":" << n->name.c_str() << "--------------" << endl;
    for (; iter != n->overwrited_modules.end(); ++iter) {
        cout << iter->first->name.c_str() << endl;
        node* sib = iter->first->get_property(n->name, false, ntImplementationBlock);
        if(n->children[4]->has_child("pipelined"))
            return true;
    }
    return false;
}
*/

set<string> time_variables, slip_time_variables;
bool updating_table_added=false;
bool access_timeslot_added=false;
void generate_primitive_time_variables(node* module, node* block) {
    if(!block) return;
    for(int i=0; i < block->size; i++) {
        node* n = block->children[i];
        if (n->type == ntIf) {
            if(evaluate_condition(module, n)) 
                generate_primitive_time_variables(module, n->children[1]);
            else
                generate_primitive_time_variables(module, n->children[2]);
        } else {
            if (!n->children[4]->has_child("serial") && !n->children[4]->has_child("parallel")) {
                map<node*, overwrite_obj_t>::iterator iter = n->overwrited_modules.begin();
                bool is_pipelined=false;
                for (; iter != n->overwrited_modules.end(); ++iter) {
                    node* sib = iter->first->get_property(n->name, false, ntImplementationBlock);
                    if(sib->children[4]->has_child("serial") || sib->children[4]->has_child("parallel"))
                        is_pipelined = true;
                }
                if(!is_pipelined) continue;
            }
            //if(!parents_have_it(module, module->parent, n)) 
            add_line(n, "double %s_exiting_time;", n->name.c_str());
            
            //if(n->children[4]->has_child("slip")) {
            if(n->children[4]->has_child("writer") || n->children[4]->has_child("reader") || n->name == "write_block" || n->name == "read_block")  {
                if(!access_timeslot_added){
                    add_line(n, "timeslot_t access_busy_timeslots[%d];", max_time_slots);
                    add_line(n, "int access_timeslot_index;");
                    access_timeslot_added = true;
                }
            } else {
                add_line(n, "timeslot_t %s_busy_timeslots[%d];", n->name.c_str(), max_time_slots);
                add_line(n, "int %s_timeslot_index;", n->name.c_str());
            }
            //}
            //            if(module->get_ancestor()->name == "typical_module") {
            if(!updating_table_added) {
                add_line(n, "att_entry_t updating_blocks_table[%d];", max_replacing_blocks);
                add_line(n, "int updating_blocks_table_index;");
                updating_table_added = true;
            }
            //            }

            time_variables.insert(n->name);
            if(n->children[4]->has_child("slip")) {
                if(n->children[4]->has_child("writer") || n->children[4]->has_child("reader") || n->name == "write_block" || n->name == "read_block")  
                    slip_time_variables.insert("access");
                else
                    slip_time_variables.insert(n->name);
            }
        }
    }
}

void generate_primitive_time_variables(node* module) { 
    updating_table_added = false;
    access_timeslot_added = false;
    generate_primitive_time_variables(module, module->get_block(ntImplementationBlock));
}

void generate_primitive_variables(node* module) {
    if(!module->parent) {
        //        add_line(module, "double exiting_time;");
        //        add_line(module, "double pump_exiting_time;");
        add_line(module, "bool search_enable;");
    }
}

void generate_submodules(node* main_module, node* module, node* block) {
    if (!block) return;
    for (int i = 0; i < block->size; i++) {
        node* n = block->children[i];
        if (n->type == ntSubmodule) {
            generate_module(n, false);
        } else if (n->type == ntIf) {
            if(evaluate_condition(module, n))
                generate_submodules(main_module, module, n->children[1]);
            else
                generate_submodules(main_module, module, n->children[2]);
        }
    }
}

void generate_submodules(node* main_module, node* module) {
    if(module->parent)
        if(/*module->parent->name != "typical_cache" && */module->parent->name != "typical_module")
            generate_submodules(main_module, module->parent);
    node* block = module->get_block(ntDefinitionsBlock);
    generate_submodules(main_module, module, block);
}

void generate_record(node* , node* , node*, string);

void generate_record_items(node* main_module, node* module, node* block, string chain = "") {
    for (int i = 0; i < block->size; i++) {
        node* n = block->children[i];
        if(n->type == ntVariable) {
            if(overwritten(n, main_module)) continue;
            string size_str2 = evaluate_expression_result(module, n->children[1]);
            if (!is_int(size_str2) && !size_str2.empty()) {
                report_exp(exp_gen(etSyntaxError, n->line, n->file, "An error occurred while evaluating size of record member "OPENING_QUOTE"%s"CLOSING_QUOTE": %s.", n->name.c_str(), size_str2.c_str()));
                continue;
            }
            
            if (!size_str2.empty())
                if(size_str2 != "-1")
                    if(is_int(size_str2))
                        if(atof(size_str2.c_str()) > size_threshold)
                            size_str2 = "-1";
            
            if (size_str2.empty())
                add_line(n, "%s %s;", n->children[0]->name.c_str(), n->name.c_str());
            else if (size_str2 == "-1") {
                add_line(n, "dmap < uint64, %s > %s;", n->children[0]->name.c_str(), n->name.c_str());
            } else {
                add_line(n, "%s %s[%s];", n->children[0]->name.c_str(), n->name.c_str(), size_str2.c_str());
            }
        } else if(n->type == ntIf) {
            if(evaluate_condition(module, n))
                generate_record_items(main_module, module, n->children[1], chain + n->name + "_");
            else
                generate_record_items(main_module, module, n->children[2], chain + n->name + "_");
        } else
            generate_record(main_module, module, n, chain);
    }
}

bool is_empty_record(node* rec) {
    if(rec->children[1]->size == 0)
        return true;
    for(int i=0; i < rec->children[1]->size; i++ ) {
        if(rec->children[1]->children[i]->type != ntRecord) {
            return false;
        } else if(!is_empty_record(rec->children[1]->children[i])) {
            return false;
        }
    }
    return true;
}

void generate_record(node* main_module, node* module, node* r, string chain = "") {
    if(overwritten(r, main_module)) return;
    string size_str = evaluate_expression_result(module, r->children[0]);
    if (!is_int(size_str) && !size_str.empty()) {
        report_exp(exp_gen(etSyntaxError, r->line, r->file, "An error occurred while evaluating dimension expression of record "OPENING_QUOTE"%s"CLOSING_QUOTE": %s.", r->name.c_str(), size_str.c_str()));
        return;
    }
    
    if(is_empty_record(r)) { // It does not have body
        report_exp(exp_gen(etNote, r->line, r->file, "Eliminating empty record "OPENING_QUOTE"%s"CLOSING_QUOTE".", r->name.c_str()));
        return;
    }
    
    if (!size_str.empty())
        if(size_str != "-1")
            if(is_int(size_str))
                if(atof(size_str.c_str()) > size_threshold)
                    size_str = "-1";
    
    r->content_size = atol(size_str.c_str());
    add_line(r, "typedef struct");
    add_line(r, "{");
    generate_record_items(main_module, module, r->children[1], chain+r->name + "_");
    add_line(r, "} %s%s_t;", chain.c_str(), r->name.c_str());
    if (size_str.empty())
        add_line(r, "%s%s_t %s;", chain.c_str(), r->name.c_str(), r->name.c_str(), size_str.c_str());
    else if(size_str == "-1") {
        add_line(r, "dmap < uint64, %s%s_t > %s;", chain.c_str(), r->name.c_str(), r->name.c_str());
    } else
        add_line(r, "%s%s_t %s[%s];", chain.c_str(), r->name.c_str(), r->name.c_str(), size_str.c_str());
}

void generate_architecture(node* main_module, node* module, node* block) {
    if (!block) return;
    for (int i = 0; i < block->size; i++)
    {
        if (block->children[i]->type == ntIf) {
            if(evaluate_condition(module, block->children[i]))
                generate_architecture(main_module, module, block->children[i]->children[1]);
            else
                generate_architecture(main_module, module, block->children[i]->children[2]);
        } else
            generate_record(main_module, module, block->children[i]);
    }
}

void generate_architecture(node* main_module, node* module) {
    if(module->parent)
        generate_architecture(main_module, module->parent);
    node* block = module->get_block(ntArchitectureBlock);
    generate_architecture(main_module, module, block);
}

node* implementation_block=NULL;
bool latency_is_considered=false;
void generate_entring_function_codes(node* r) {
    if(r->name == "set_data") return;
    if(r->name == "get_data") return;
    if(r->name == "initial") return;
    if(r->name == "report") return;
    string ext_var = r->name+"_exiting_time";
    if(r->size > 4) {
        if(r->children[4]->has_child("parallel") || r->children[4]->has_child("offline")) {                
            add_line(r, "double saved_latency = latency;");
        }
    }
    add_line(r, "if(sim_step >= offset) {");   
    if(r->children[4]->has_child("serial")) 
        add_line(r, "if(latency == 0) latency = %s;", ext_var.c_str());
    bool stall = r->children[4]->has_child("serial");
    if(r->size>4)
        if(r->children[4]->has_child("slip"))
        {
        if(!r->children[4]->has_child("serial")) {
            report_exp(exp_gen(etSyntaxError, r->line, r->file, "Invalid use of function declarator "OPENING_QUOTE"slip'. No token `serial"CLOSING_QUOTE" is found."));
        } else {
            stall = false;
            //                add_line(r, "else if(latency < %s) {", ext_var.c_str());
            add_line(r, "else {");
            if(r->get_ancestor()->name == "typical_module" && (r->name == "read_block" || r->name == "write_block" || r->children[4]->has_child("reader") || r->children[4]->has_child("writer")))
                add_line(r, "double last_latency = time_to_availability(latency, block_address(addr), updating_blocks_table);");
            else
                add_line(r, "double last_latency = latency;");
            add_line(r, "latency = 0;");
            latency_is_considered = true;
            generate_physical_attributes(r->children[2], r, 1);
            //                add_line(r, "cout << \"min=\" << last_latency << \" max=\" << %s << \" len=\" << latency << endl;", ext_var.c_str());
            //                add_line(r, "for(int k=0; k<16; k++)", ext_var.c_str());
            //                add_line(r, "    if(%s_busy_timeslots[k].valid) printf(\"(%%e, %%e) \", %s_busy_timeslots[k].start, %s_busy_timeslots[k].end);", r->name.c_str(), r->name.c_str(), r->name.c_str());
            //                add_line(r, "cout << endl;");


            add_line(r, "if(latency > 0) {");
            if(r->children[4]->has_child("writer") || r->children[4]->has_child("reader") || r->name == "read_block" || r->name == "write_block") {

                node* writer = NULL;
                node* reader = NULL;
                for(int i=0; i<implementation_block->size; i++) {
                    node* n=implementation_block->children[i];
                    if(n->children[4]->has_child("reader")) {
                        if(reader) {
                            report_exp(exp_gen(etSyntaxError, n->line, n->file, "Multiple declarations for reader function."));
                            report_exp(exp_gen(etSyntaxError, reader->line, reader->file, "Previous declaration is here."));
                        } else reader = n;
                    } else if(n->children[4]->has_child("writer")) {
                        if(writer) {
                            report_exp(exp_gen(etSyntaxError, n->line, n->file, "Multiple declarations for writer function."));
                            report_exp(exp_gen(etSyntaxError, writer->line, writer->file, "Previous declaration is here."));
                        } else writer = n;
                    }
                }
                string r_name = "read_block", w_name = "write_block";
                if(reader) r_name = reader->name;
                if(writer) w_name = writer->name;

                add_line(r, "double timeslot = find_first_free_timeslot(last_latency, max(%s_exiting_time, %s_exiting_time), latency, access_busy_timeslots);", w_name.c_str(), r_name.c_str());
                //add_line(r, "double timeslot = find_first_free_timeslot(last_latency, %s_exiting_time, latency, access_busy_timeslots);", r->name.c_str());
                add_line(r, "add_busy_timeslot(timeslot, timeslot+latency, access_busy_timeslots, access_timeslot_index);");
            } else {
                add_line(r, "double timeslot = find_first_free_timeslot(last_latency, %s, latency, %s_busy_timeslots);", ext_var.c_str(), r->name.c_str());
                add_line(r, "add_busy_timeslot(timeslot, timeslot+latency, %s_busy_timeslots, %s_timeslot_index);", r->name.c_str(), r->name.c_str());
            }
            add_line(r, "latency = timeslot;");
            add_line(r, "} else latency += last_latency;");
            add_line(r, "}");
        }
    }
    if(stall) {
	if(r->children[4]->has_child("offline"))
            add_line(r, "else latency = time_to_availability(latency, block_address(addr), updating_blocks_table);");
        else {
            add_line(r, "else latency = max(%s, latency);", ext_var.c_str());
            if(r->get_ancestor()->name == "typical_module" && (r->name == "read_block" || r->name == "write_block" || r->children[4]->has_child("reader") || r->children[4]->has_child("writer")))
                add_line(r, "latency = time_to_availability(latency, block_address(addr), updating_blocks_table);");
        }
    }
    add_line(r, "}");
}
void generate_exiting_function_codes(node* r, bool add_pipeline_view=false) {
    if(r->name == "set_data") return;
    if(r->name == "get_data") return;
    if(r->name == "initial") return;
    if(r->name == "report") return;
    //if(r->children[4]->has_child("primitive")) return;
    add_line(r, "if(sim_step >= offset) {");
    if(!r->children[4]->has_child("parallel")) {
        add_line(r, "latency = ceil(latency*frequency)/frequency;");
        if(add_pipeline_view) {
            if(!macros["DISPLAY_PIPELINE"].empty()) {
                //add_line(r, "cout << \"\\r <\" << sim_step << \"> \" << name << \":%s = \" << display_number(latency, \"s              \");", r->name.c_str());
                //add_line(r, "getch();");
                add_line(r, "__pipeline_file << sim_step << \" > \" << name << \":%s = [\" << _old_latency << \", \" << latency << \"]\" << endl;", r->name.c_str());
            }
        }
        add_line(r, "if(latency > %s_exiting_time) %s_exiting_time = latency;", r->name.c_str(), r->name.c_str());
    }
    if(r->size > 4) {
        //        if(r->children[4]->has_child("slip"))
        //            add_line(r, "add_busy_timeslot(_old_latency, %s_exiting_time, %s_busy_timeslots, %s_timeslot_index);", r->name.c_str(), r->name.c_str(), r->name.c_str(), r->name.c_str());
        if(r->children[4]->has_child("slip") && r->children[4]->has_child("serial") && (r->children[4]->has_child("writer") || r->name == "write_block"))
            add_line(r, "add_unavailable_timeslot(updating_blocks_table, block_address(addr), latency, updating_blocks_table_index);");
        if(r->children[4]->has_child("parallel") || r->children[4]->has_child("offline")) {
            add_line(r, "latency = saved_latency;");
        }
    }
    add_line(r, "}");
}

bool is_end_line(string s) {
    if (s == ";") return true;
    if (s == "}") return true;
    if (s == "{") return true;
    if (s == ")") return true;
    return false;
}

node* generating_fn = NULL;
bool saved_access_type = false;
void add_function_line(node* module, node* n, int &i, bool &is_pointer, node* &chain) {
    node* cc = n->children[i];
    static bool brace_opened_for_return = false;
    if (cc->type == ntTraceFunction && macros["VERBOSE"].empty()) {
        add_line(cc, "{} ");
        while (i < n->size) {
            if (n->children[i]->name == ";")
                return;
            i++;
        }
    } else if (cc->type == ntBreakpointFunction && (macros["STEP"].empty() && macros["VERBOSE"].empty())) {
        add_line(cc, "{} ");
        while (i < n->size) {
            if (n->children[i]->name == ";")
                return;
            i++;
        }
    } else if (cc->type & (ntConstant | ntParameter)) {
        char type[24];
        if(cc->type == ntConstant)
            strcpy(type, "Constant");
        else {
            strcpy(type, "Parameter");
            /*
            cout << cc->name.c_str() << endl;
            if(cc->children[0]->size > 2)
            for(int i=0; i<cc->children[0]->children[2]->size; i++)
            	cout << "     " << cc->children[0]->children[2]->children[i]->name.c_str() << endl;
            */
        }
        string idx_str = evaluate_expression_result(module, cc->children[add_function_line_dim_idx]);
        if(!is_int(idx_str) && !idx_str.empty()) {
            report_exp(exp_gen(etSyntaxError, cc->line, cc->file, "An error occurred while evaluating index of %s "OPENING_QUOTE"%s"CLOSING_QUOTE": %s.", strlwr(type), cc->name.c_str(), idx_str.c_str()));
            return;
        }
        if(idx_str.empty())
            idx_str = "0";
        int idx = atoi(idx_str.c_str());
        if(idx >= cc->children[add_function_line_typ_idx]->content_size) {
            report_exp(exp_gen(etSyntaxError, cc->line, cc->file, "Index "OPENING_QUOTE"%s"CLOSING_QUOTE" out of range.", cc->name.c_str()));
            return;
        }
        if(!cc->children[add_function_line_typ_idx]->assigned[idx] && !cc->children[add_function_line_typ_idx]->initialized[idx]) {
            report_exp(exp_gen(etSyntaxError, cc->line, cc->file, "%s "OPENING_QUOTE"%s[%d]"CLOSING_QUOTE" has never been assigned a value.", type, cc->children[add_function_line_typ_idx]->name.c_str(), idx));
            return;
        }
        add_line(cc, cc->children[add_function_line_typ_idx]->content[idx].c_str());
        return;
    } else if (is_external_scope(cc->name)) {
        //add_line(cc);
        string ext_param_name = module->name+"::"+cc->name+"::"+cc->children[0]->name;
        string ext_param_value = external_parameter_values[ext_param_name];
        while(ext_param_value.empty() && module->parent) {
            module = module->parent;
            ext_param_name = module->name+"::"+cc->name+"::"+cc->children[0]->name;
            ext_param_value = external_parameter_values[ext_param_name];
        }
        if(ext_param_value.empty()) {
            report_exp(exp_gen(etWarning, cc->line, cc->file, "External parameter "OPENING_QUOTE"%s' has not any data. `0"CLOSING_QUOTE" is considered.", ext_param_name.c_str()));
            ext_param_value = "0";
        }
        add_line(cc, ext_param_value.c_str());
        return;
    } else if(cc->name == ".") {
        if (is_pointer)
            cc->name = "->";
        is_pointer = false;
    } if(cc->name == "->") {
    is_pointer = false;
} if(cc->name == ")") {
chain = module;
} else if(cc->name == ";") {
    is_pointer = false;
    chain = module;
    if(brace_opened_for_return) {
        add_line(cc);
        add_line(cc, "}");
        brace_opened_for_return = false;
        return;
    }
} else if(cc->name == "return") {
    if(i > 0) {
        if(n->children[i-1]->name == ")") {
            brace_opened_for_return = true;
            add_line(cc, "{");
        }
    }
    if(saved_access_type)
        add_line(cc, "access_type = saved_access_type;");
    if(n->parent) {
        if(n->parent->size > 4) {
            if(n->parent->children[4]->has_child("serial") || n->parent->children[4]->has_child("parallel")) {
                generate_exiting_function_codes(generating_fn, true);
            }
        }
    }
} else if(cc->type == ntIf) {
    node* sub = cc->children[2]->children[0];
    if(evaluate_condition(module, cc))
        sub = cc->children[1]->children[0];
    if(sub) {
        for(int j=1; j< sub->size-1; j++)
            add_function_line(module, sub, j, is_pointer, chain);
    }
} else {
    if (cc->name == "this" || cc->name == "top_level_module" || cc->name == "__main_memory") {
        chain = module;
        is_pointer = true;
    } else {
        node* r = chain->get_property(cc->name, true, ntDefinitionsBlock);
        if (r) {
            if (i > 0) if(n->children[i-1]->name == "." || n->children[i-1]->name == "->") {
                add_line(cc);
                return;
            }
            if(!is_primary_type(r->children[0]->name) && r->type != ntConstant) {
                node* m = get_submodule(module, r->children[0]->name);
                if(!m) m = get_owner_sub_type(module, r->children[0]->name);
                if(!m) m = get_module(r->children[0]->name);
                if (m) {
                    chain = m;
                    is_pointer = true;
                    if(i < n->size - 1) {
                        if(n->children[i+1]->name == "[") {
                            add_line(n->children[i]);
                            add_line(n->children[i+1]);
                            i += 2;
                            node* m = chain;
                            int b = 1;
                            bool b_is_pointer = false;
                            while(b > 0 && i < n->size) {
                                if(n->children[i]->name == "[")
                                    b++;
                                else if(n->children[i]->name == "]")
                                    b--;
                                add_function_line(module, n, i, b_is_pointer, m);
                                i++;
                            }
                            i--;
                            return;
                        }
                    }
                }
            }
        } else if (cc->type == ntBreakpointFunction) {
            if (i < n->size-1) {
                if (n->children[i+1]->name == "(") {
                    char tmp[10];
                    sprintf(tmp, "(%d, ", cc->value);
                    n->children[i+1]->name = tmp;
                }
            }
        } else if(is_physical_property(cc->name)) {
            bool add_if = true;
            if (i > 0) {
                if(!is_end_line(n->children[i-1]->name)) add_if = false;
                /*
                    if(n->children[i-1]->name == "." || n->children[i-1]->name == "->") {
                        add_line(cc);
                        return;
                    } else */ 
                if(n->children[i-1]->name == ")") {
                    brace_opened_for_return = true;
                    add_line(cc, "{");
                }
            }
            if(add_if) add_line(cc, "if(sim_step >= offset)");
        }
    }
}
if( i < n->size - 2) {
    if (n->children[i+1]->name == "@") {
        add_line(n->children[i], generate_field_use(module, n->children[i+2]).c_str());
        i += 2;
        return;
    }
}
if(!cc->name.empty())
    add_line(cc);
}

void generate_module_function(node* main_module, node* module, node* r) {
    if(overwritten(r, main_module)) return;
    if(main_module->extended)
        add_line(r->children[1], "virtual");
    add_line(r->children[1]);
    add_line(r);
    for (int i = 0; i < r->children[0]->size; i++)
        add_line(r->children[0]->children[i]);
    
    if(only_prototype) {
        add_line(r, "{} ");
        return;
    }
    
    node* n = r->children[3];
    bool is_pointer = false;
    add_line(n->children[0]);
    
    if(!macros["DISPLAY_PIPELINE"].empty())
        add_line(r, "double _old_latency = 0;");
    latency_is_considered = false;  
    if(r->size > 4) {
        if((r->children[4]->has_child("serial") || r->children[4]->has_child("parallel"))) {
            generate_entring_function_codes(r);
        }
    }
    
    if((r->name == "read_block" || r->name == "write_block" || r->children[4]->has_child("reader") || r->children[4]->has_child("writer")) && (r->children[3]->size > 2)) {
        if(main_module->get_ancestor()->name == "typical_module") {
            add_line(n, "int saved_access_type = access_type;");
            saved_access_type = true;
            if(r->name == "read_block" || r->children[4]->has_child("reader"))
                add_line(n, "access_type = ttDataRead;");
            else
                add_line(n, "access_type = ttDataWrite;");
            add_line(n, "if(search_enable) pump(__current_transaction);");
            add_line(n, "if(!lookup(addr)) {");
            add_line(n, "if(search_enable) {");
            if(r->name == "read_block" || r->children[4]->has_child("reader"))
                add_line(n, "if(sim_step >= offset) read_miss++;");
            else
                add_line(n, "if(sim_step >= offset) write_miss++;");
            add_line(n, "search_enable = false;");
            //generate_exiting_function_codes(r);
            add_line(n, "replace(addr);");
            /*            if(r->size>4)
                if(r->children[4]->has_child("slip")) 
                    add_line(n, "add_unavailable_timeslot(updating_blocks_table, block_address(addr), latency, updating_blocks_table_index);");
                    */
            add_line(n, "search_enable = true;");
            add_line(n, "}");
            add_line(n, "}");
            if(r->name == "read_block" || r->children[4]->has_child("reader"))
                add_line(n, " else if(search_enable && sim_step >= offset) read_hit++;");
            else 
                add_line(n, " else if(search_enable && sim_step >= offset) write_hit++;");
        }
    }
    
    generating_fn = r;
    //    if(!latency_is_considered) {
    if(!macros["DISPLAY_PIPELINE"].empty())
        add_line(r, "_old_latency = latency;");
    generate_physical_attributes(r->children[2], r);
    /*        if(r->children[4]->has_child("slip")) {
            if(r->children[4]->has_child("writer") || r->children[4]->has_child("reader") || r->name == "read_block" || r->name == "write_block") 
                add_line(r, "add_busy_timeslot(_old_latency, latency, access_busy_timeslots, access_timeslot_index);", r->name.c_str(), r->name.c_str());
            else 
                add_line(r, "add_busy_timeslot(_old_latency, latency, %s_busy_timeslots, %s_timeslot_index);", r->name.c_str(), r->name.c_str());
        }
*/
    //    } else
    //        generate_physical_attributes(r->children[2], 2);
    node* chain = module;
    bool added_once = false;
    for (int i = 1; i < n->size-1; i++)
        add_function_line(module, n, i, is_pointer, chain);
    if(r->size > 4)
        if(r->children[4]->has_child("serial") || r->children[4]->has_child("parallel"))
            generate_exiting_function_codes(r, true);
    if(saved_access_type)
        add_line(n, "access_type = saved_access_type;");
    saved_access_type = false;
    add_line(r, "}");
}

void generate_implementation(node* main_module, node* module, node* block, bool is_top_level) {
    if (!block) return;
    implementation_block = block;
    for (int i = 0; i < block->size; i++) {
        if (block->children[i]->type == ntIf) {
            if(evaluate_condition(module, block->children[i]))
                generate_implementation(main_module, module, block->children[i]->children[1], is_top_level);
            else
                generate_implementation(main_module, module, block->children[i]->children[2], is_top_level);
        } else {
            generate_module_function(main_module, module, block->children[i]);
            if (is_top_level && block->children[i]->name == "report")
                has_report_function = true;
        }
    }
}

void generate_implementation(node* main_module, node* module, bool is_top_level = false) {
    if(module->parent)
        generate_implementation(main_module, module->parent);
    node* block = module->get_block(ntImplementationBlock);
    generate_implementation(main_module, module, block, is_top_level);
}

node* next_until(node* str, string last, int& ii) {
    node* lst = new node("list");
    int p = 0;
    int b = 0;
    int r = 0;
    for (int i = ii; i < str->size; i++) {
        if (p == 0 && b == 0 && r == 0 && str->children[i]->name == last) {
            ii = i;
            return lst;
        } else {
            lst->add_child(str->children[i]);
            if (str->children[i]->name == "(") {
                p++;
            } else if (str->children[i]->name == ")") {
                p--;
            } else if (str->children[i]->name == "[") {
                b++;
            } else if (str->children[i]->name == "]") {
                b--;
            } else if (str->children[i]->name == "{") {
                r++;
            } else if (str->children[i]->name == "}") {
                r--;
            }
        }
    }
    ii = -1;
    return lst;
}

node* iterator_list_from_expression(node* module, node* exp) {
    // find iteration expressions #i(s,e)
    // children [name] <0: start exp> <1: end exp>
    node* lst = new node("", exp);
    for (int i = 0; i < exp->size; i++) {
        node* n = exp->children[i];
        if (!n) continue;
        if (n->name[0] == '#') {
            string name = n->name;
            bool defined = false;
            for (int j = 0; j < lst->size; j++) {
                if (lst->children[j]->name == name) {
                    if (i < exp->size-1) {
                        if (exp->children[i+1]->name == "[") {
                            report_exp(exp_gen(etSyntaxError, n->line, n->file, "Multiple declaration for iterator "OPENING_QUOTE"%s"CLOSING_QUOTE".", name.c_str()));
                        }
                    }
                    defined = true;
                    break;
                }
            }
            if (defined) {
                continue;
            }
            
            bool no_range = false;
            node *start, *end;
            
            node *rng = exp->children[i]->children[0];
            if (!rng)
                no_range = true;
            else if (rng->size > 0) {
                int ii = 0;
                start = next_until(rng, ",", ii);
                ii++;
                if(ii != -1) {
                    end = next_until(rng, "]", ii);
                } else
                    no_range = true;
            } else
                no_range = true;
            
            if (no_range) {
                report_exp(exp_gen(etSyntaxError, n->line, n->file, "No range is define for iterator "OPENING_QUOTE"%s"CLOSING_QUOTE".", name.c_str()));
            } else {
                node* m = new node(name.c_str(), n);
                
                string start_str = evaluate_expression_result(module, start, lst);
                if(!is_int(start_str)) {
                    report_exp(exp_gen(etSyntaxError, n->line, n->file,
                                       "An error occurred while evaluating lower bound of iterator "OPENING_QUOTE"%s"CLOSING_QUOTE": %s.", name.c_str(), start_str.c_str()));
                    continue;
                }
                
                string end_str = evaluate_expression_result(module, end, lst);
                if(!is_int(end_str)) {
                    report_exp(exp_gen(etSyntaxError, n->line, n->file,
                                       "An error occurred while evaluating upper bound of iterator "OPENING_QUOTE"%s"CLOSING_QUOTE": %s.", name.c_str(), end_str.c_str()));
                    continue;
                }
                
                start->assign(start_str, 0);
                end->assign(end_str, 0);
                
                m->assign(start_str, 0);
                m->add_child(start);
                m->add_child(end);
                lst->add_child(m);
            }
        }
    }
    return lst;
}

bool is_condition_delimiter(string w) {
    if (w == "==") return true;
    if (w == "!=") return true;
    if (w == "&&") return true;
    if (w == "||") return true;
    if (w == ">=") return true;
    if (w == "<=") return true;
    if (w == ">") return true;
    if (w == "<") return true;
    return false;
}

string evaluate_dimentions(node* module, node* n, node* it_lst) {
    if(n->type == ntExpression)
        return evaluate_expression_result(module, n, it_lst);
    if(n->size == 0)
        return n->name;
    string result = n->name;
    node* obj = module->get_property(n->name, true, ntDefinitionsBlock);
    int size = 0;
    if(obj) {
        string size_str = evaluate_expression_result(module, obj->children[1]);
        if (is_int(size_str))
            size = atoi(size_str.c_str());
        if(size == 0) return result;
        string dim_str = evaluate_dimentions(module, n->children[0], it_lst);
        int dim = -1;
        if (is_int(dim_str)) 
            dim = atoi(dim_str.c_str());
        result += "[";
        result += itoa(dim);
        result += "]";
    }
    return result;
}

string generate_connect_to_condition_segments(node* module, node* cond, node* it_lst) {
    node* segment = new node("", cond);
    string result = "";
    if (cond->size == 0)
        return "";
    int idx = 0;
    int p = 0;
    for (int i = 0; i < cond->size; i++) {
        if( i < cond->size - 2) {
            if (cond->children[i+1]->name == "@") {
                segment->add_child(new node(generate_field_use(module, cond->children[i+2]).c_str(), cond->children[i]));
                i += 2;
                continue;
            }
        }
        if (is_condition_delimiter(cond->children[i]->name) || (cond->children[i]->name == ")" && p == 0)) {
            string seg_str = evaluate_expression_result(module, segment, it_lst);
            if(!is_int(seg_str)) {
                seg_str = "";
                for(int j=0; j < segment->size; j++)
                    seg_str += evaluate_dimentions(module, segment->children[j], it_lst);
            }
            result += seg_str;
            result += " ";
            result += cond->children[i]->name;
            result += " ";
            idx = i+1;
            break;
        } else
            segment->add_child(cond->children[i]);
        if (cond->children[i]->name == "(") p++;
        else if (cond->children[i]->name == ")") p--;
    }
    node* segment2 = new node("", cond);
    for (int j = idx; j < cond->size; j++)
        segment2->add_child(cond->children[j]);
    if (result.empty()) {
        string seg_str = evaluate_expression_result(module, segment2, it_lst);
        if(!is_int(seg_str)) {
            seg_str = "";
            for(int j=0; j < segment2->size; j++)
                seg_str += evaluate_dimentions(module, segment2->children[j], it_lst);
        }
        result += seg_str;
    } else
        result += generate_connect_to_condition_segments(module, segment2, it_lst);
    return result;
}

void generate_connect_to_condition(node* module, node* cond, node* it_lst) {
    // if(condition) { ...
    if (cond->size == 0) {
        return;
    }
    node *n = cond->children[0];
    if (n->name == "(") {
        int p = 1;
        node *segment = new node("", cond);
        for (int i = 1; i < cond->size; i++) {
            if (cond->children[i]->name == "(")
                p++;
            else if (cond->children[i]->name == ")")
                p--;
            if (p==0) {
                add_line(cond->children[i], "(");
                generate_connect_to_condition(module, segment, it_lst);
                add_line(cond->children[i], ")");
            } else
                segment->add_child(cond->children[i]);
        }
    } else {
        add_line(n, generate_connect_to_condition_segments(module, cond, it_lst).c_str());
    }
}

bool generate_process_function_once = false;
void generate_contect_to_loop(node* module, node* obj, node* it_lst, int it_idx) {
    bool brace_opened = false;
    if (it_idx >= it_lst->size) {
        if (obj->children[1]->size > 0) {
            if (generate_process_function_once)
                add_line(obj->children[1], "if (");
            else
                add_line(obj->children[1], "if (");
            generate_process_function_once = true;
            generate_connect_to_condition(module, obj->children[1], it_lst);
            add_line(obj->children[1], ") {");
            brace_opened = true;
        } else if(generate_process_function_once) {
        } else {
        }
        
        bool is_system = module->get_ancestor()->name == "typical_system";
        
        string dim = "";
        if (obj->children[0]->size != 0) { // [index]
            string idx_str = evaluate_expression_result(module, obj->children[0], it_lst);
            if(!is_int(idx_str)) {
                report_exp(exp_gen(etWarning, obj->children[0]->line, obj->children[0]->file, "An error occurred while evaluating index of object "OPENING_QUOTE"%s"CLOSING_QUOTE": %s.", obj->name.c_str(), idx_str.c_str()));
                return;
            }
            dim = "[";
            dim += idx_str;
            dim += "]";
        }
        
        if(obj->children[3]->type == ntFunction) {
            if(obj->children[2]->name == "all" || obj->children[2]->name == "." || obj->children[2]->name == "...")
                add_line(obj, "%s(message);", obj->name.c_str());
            else
                add_line(obj, "%s(message.%s);", obj->name.c_str(), obj->children[2]->name.c_str());
        } else if(module->get_ancestor()->name == "typical_module") {
            add_line(obj, "%s%s->pump(transaction);", obj->name.c_str(), dim.c_str());
        } else {
            if(is_primary_type(obj->children[3]->children[0]->name)) {
                report_exp(exp_gen(etSyntaxError, obj->line, obj->file, "Object "OPENING_QUOTE"%s"CLOSING_QUOTE" was not recognized as a pssive module.", obj->name.c_str()));
            } else {
                node* m = module->get_property(obj->name, true, ntDefinitionsBlock)->children[3];
                if(m->get_ancestor()->name == "typical_module") {
                    if(obj->children[2]->name == "all" || obj->children[2]->name == "." || obj->children[2]->name == "...") {
                        add_line(module, "{");
                        add_line(module, "int di=0;");
                        add_line(module, "if(cpu_message[cpu_number].type & (itStore | itLoad))");
                        add_line(module, "di=1;");
                    } else {
                        if(obj->children[2]->name == "data") {
                            add_line(module, "for(int di=1; di<message.data_count; di++) {");
                        } else {
                            add_line(module, "{");
                            add_line(module, "int di=0;");
                        }
                        //	add_line(module, "__current_transaction = message.%s;", obj->children[2]->name.c_str());
                    }
                    add_line(module, "__current_transaction = message.data[di];");
                    add_line(module, "if(!tlb_lookup(__current_transaction.virtual_address)) tlb_add(__current_transaction.virtual_address, __current_transaction.physical_address);");
                    add_line(module, "__current_transaction.physical_address = translate(__current_transaction.virtual_address);");

                    add_line(module, "__top_level_step();");
                    add_line(module, "transaction = __current_transaction;");
                    add_line(module, "block = new _data[transaction.size];");
                    add_line(module, "if(transaction.type == ttDataWrite) {");
                    if(capture_data) {
                        add_line(module, "for(int i=0; i<transaction.size; i++) {");
                        add_line(module, "block[transaction.size-i-1] = (uint8)(transaction.data >> (8*i)) & 0xff;");
                        add_line(module, "}");
                    }
//                    if(use_virtual_address)
//                        add_line(module, "%s%s->write_block(transaction.virtual_address, block, transaction.size);", obj->name.c_str(), dim.c_str());
//                    else
                    add_line(module, "%s%s->write_block(transaction.physical_address, block, transaction.size);", obj->name.c_str(), dim.c_str());
                    add_line(module, "}");
                    add_line(module, "else {");
//                    if(use_virtual_address)
//                        add_line(module, "%s%s->read_block(transaction.virtual_address, block, transaction.size);", obj->name.c_str(), dim.c_str());
//                    else
                    add_line(module, "%s%s->read_block(transaction.physical_address, block, transaction.size);", obj->name.c_str(), dim.c_str());
                    if(capture_data && (allow_exceptions || count_exceptions)) {
                        add_line(module, "bool mismatch = false;");
                        if(allow_exceptions)
                            add_line(module, "bool* mismatch_vector = new bool[transaction.size];");
                        //add_line(module, "if(transaction.virtual_address < memory_size) {");
                        add_line(module, "for(int i=0; i<transaction.size; i++) {");
                        add_line(module, "if(block[transaction.size-i-1] != (uint8)((transaction.data >> (8*i)) & 0xff)) {");
                        //                        add_line(module, "%s%s->set_data(transaction.virtual_address+transaction.size-i-1, (uint8)(transaction.data >> (8*i)) & 0xff);", obj->name.c_str(), dim.c_str());
                        //                        add_line(module, "if(transaction.type != ttInstFetch) {");
                        add_line(module, "mismatch = true;");				
                        add_line(module, "unmatched++; ");
                        //                        add_line(module, "}");
                        if(allow_exceptions) 
                            add_line(module, "mismatch_vector[transaction.size-i-1] = true;");
                        add_line(module, "} else ");
                        add_line(module, "{");
                        add_line(module, "matched++; ");
                        if(allow_exceptions) 
                            add_line(module, "mismatch_vector[transaction.size-i-1] = false;");
                        add_line(module, "}");
                        //                        add_line(module, "}");
                        add_line(module, "}");
                        add_line(module, "if(mismatch) {");
                        add_line(module, "exceptions++;");
                        if(allow_exceptions) {
                            add_line(module, "print_meaningful_section(message.data[0], message.__inst_inst_string.c_str());");
                            //add_line(module, "if(message.type & (itLoad |itStore)) {");
                            add_line(module, "    for(int di=1; di<message.data_count; di++)");
                            add_line(module, "        print_meaningful_section(message.data[di], message.__data_inst_string[di].c_str(), false);");
                            //add_line(module, "}");
//                            if(use_virtual_address)
//                                add_line(module, "cerr << \"*Mismatch ERROR* in module "OPENING_QUOTE"\" << %s%s->name << \""CLOSING_QUOTE" at address 0x\" << hex << transaction.virtual_address << dec << endl;", obj->name.c_str(), dim.c_str());
//                            else
                            add_line(module, "cerr << \"*Mismatch ERROR* in module "OPENING_QUOTE"\" << %s%s->name << \""CLOSING_QUOTE" at address 0x\" << hex << transaction.physical_address << dec << endl;", obj->name.c_str(), dim.c_str());
                            add_line(module, "cerr << \"   valid data: \";");
                            add_line(module, "for(int j=0; j<transaction.size; j++) {", module->name.c_str());
                            add_line(module, "if(!__redirecting_cout)");
                            add_line(module, "if(mismatch_vector[j]) {");
#ifdef __WIN32__
                            add_line(module, "SetConsoleTextAttribute( hstdout, 4 );");
#else
                            add_line(module, "cerr << \"\\033[1;31m\";");
#endif
                            add_line(module, "}");
                            add_line(module, "fprintf(stderr, \"%%.02x\", (uint8)(transaction.data >> (8*(transaction.size-1-j))) & 0xff);");
                            add_line(module, "if(j%2 == 1) cerr << \" \";");
                            add_line(module, "if(!__redirecting_cout)");
                            add_line(module, "if(mismatch_vector[j]) {");
#ifdef __WIN32__
                            add_line(module, "SetConsoleTextAttribute( hstdout, csbi.wAttributes );");
#else
                            add_line(module, "cerr << \"\\033[0m\";");
#endif
                            add_line(module, "}");
                            add_line(module, "}");
                            add_line(module, "cerr << endl;");
                            add_line(module, "cerr << \"    data read: \";", module->name.c_str());
                            add_line(module, "for(int j=0; j<transaction.size; j++) {", module->name.c_str());
                            add_line(module, "if(!__redirecting_cout)");
                            add_line(module, "if(mismatch_vector[j]) {");
#ifdef __WIN32__
                            add_line(module, "SetConsoleTextAttribute( hstdout, 4 );");
#else
                            add_line(module, "cerr << \"\\033[1;31m\";");
#endif
                            add_line(module, "}");
                            add_line(module, "fprintf(stderr, \"%%.02x\", block[j]);");
                            add_line(module, "if(j%2 == 1) cerr << \" \";");
                            add_line(module, "if(!__redirecting_cout)");
                            add_line(module, "if(mismatch_vector[j]) {");
#ifdef __WIN32__
                            add_line(module, "SetConsoleTextAttribute( hstdout, csbi.wAttributes );");
#else
                            add_line(module, "cerr << \"\\033[0m\";");
#endif
                            add_line(module, "}");
                            add_line(module, "}");
                            add_line(module, "cerr << endl;");
//                            if(use_virtual_address)
//                                add_line(module, "%s%s->print_block(transaction.virtual_address);", obj->name.c_str(), dim.c_str());
//                            else
                            add_line(module, "%s%s->print_block(transaction.physical_address);", obj->name.c_str(), dim.c_str());
                            add_line(module, "cerr << \"Press [ESC] to terminate or any other keys to continue...\" << endl;");
                            add_line(module, "if(getch()==27) {");
                            add_line(module, "simulation_finished = true;");
                            add_line(module, "throw 35;");
                            add_line(module, "}");
                        }
                        add_line(module, "}");
                        if(allow_exceptions) 
                            add_line(module, "delete[] mismatch_vector;");
                    }
                    add_line(module, "}");
                    add_line(module, "delete[] block;");
                    //if(obj->children[2]->name == "data")
                    add_line(obj, "}");
                } else if(obj->children[2]->name == "all" || obj->children[2]->name == "." || obj->children[2]->name == "...") {
                    add_line(obj, "%s%s->pump(message);", obj->name.c_str(), dim.c_str());
                } else {
                    if(obj->children[2]->name == "data") {
                        add_line(module, "for(int di=0; di<message.data_count; di++)");
                        add_line(obj, "%s%s->pump(message.data[di]);", obj->name.c_str(), dim.c_str());
                    } else
                        add_line(obj, "%s%s->pump(message.%s);", obj->name.c_str(), dim.c_str(), obj->children[2]->name.c_str());
                }
            }
        } 
        
        if (brace_opened)
            add_line(obj, "}");
        
        return;
    }
    
    node* it = it_lst->children[it_idx];
    
    string start_str = evaluate_expression_result(module, it->children[0], it_lst);
    if(!is_int(start_str)) {
        report_exp(exp_gen(etSyntaxError, it->line, it->file, "An error occurred while evaluating lower bound of iterator "OPENING_QUOTE"%s"CLOSING_QUOTE": %s.", it->name.c_str(), start_str.c_str()));
        return;
    }
    
    string end_str = evaluate_expression_result(module, it->children[1], it_lst);
    if(!is_int(end_str)) {
        report_exp(exp_gen(etSyntaxError, it->line, it->file, "An error occurred while evaluating upper bound of iterator "OPENING_QUOTE"%s"CLOSING_QUOTE": %s.", it->name.c_str(), end_str.c_str()));
        return;
    }
    
    int start = atoi(start_str.c_str());
    int end = atoi(end_str.c_str())+1;
    
    for (int i = start; i < end; i++) {
        it->assign(itoa(i), 0);
        generate_contect_to_loop(module, obj, it_lst, it_idx+1);
    }
}

void generate_connect_to_expression(node* module, node* obj) {
    node* all = new node("", obj);
    for (int i = 0; i < obj->children[0]->size; i++) {
        all->add_child(obj->children[0]->children[i]);
    }
    for (int i = 0; i < obj->children[1]->size; i++) {
        all->add_child(obj->children[1]->children[i]);
    }
    node* lst = iterator_list_from_expression(module, all);
    generate_contect_to_loop(module, obj, lst, 0);
}

void generate_process_function(node* module, node* block) {
    for (int i = 0; i < block->size; i++) {
        if (block->children[i]->type == ntIf) {
            if (evaluate_condition(module, block->children[i]))
                generate_process_function(module, block->children[i]->children[1]);
            else
                generate_process_function(module, block->children[i]->children[2]);
        }
        else {
            generate_connect_to_expression(module, block->children[i]);
        }
    }
}
/*
void generate_set_word_propagetion_iterative_loop(node* module, node* obj, node* it_lst, int it_idx) {
    if (it_idx >= it_lst->size) {
        string dim = "->";
        if (obj->children[0]->size != 0) { // [index]
            string idx_str = evaluate_expression_result(module, obj->children[0], it_lst);
            if(!is_int(idx_str)) {
                report_exp(exp_gen(etWarning, obj->children[0]->line, obj->children[0]->file,
                                   "An error occurred while evaluating index of object "OPENING_QUOTE"%s"CLOSING_QUOTE": %s.", obj->name.c_str(), idx_str.c_str()));
                return;
            }
            dim = "[";
            dim += idx_str;
            dim += "]->";
        }
        add_line(obj, "%s%sset_data(message.data.virtual_address, (message.data.data >> (i*8))&0xff);", obj->name.c_str(), dim.c_str());
        return;
    }
    
    node* it = it_lst->children[it_idx];
    
    string start_str = evaluate_expression_result(module, it->children[0], it_lst);
    if(!is_int(start_str)) {
        report_exp(exp_gen(etSyntaxError, it->line, it->file, "An error occurred while evaluating lower bound of iterator "OPENING_QUOTE"%s"CLOSING_QUOTE": %s.", it->name.c_str(), start_str.c_str()));
        return;
    }
    
    string end_str = evaluate_expression_result(module, it->children[1], it_lst);
    if(!is_int(end_str)) {
        report_exp(exp_gen(etSyntaxError, it->line, it->file, "An error occurred while evaluating upper bound of iterator "OPENING_QUOTE"%s"CLOSING_QUOTE": %s.", it->name.c_str(), end_str.c_str()));
        return;
    }
    
    int start = atoi(start_str.c_str());
    int end = atoi(end_str.c_str())+1;
    
    for (int i = start; i < end; i++) {
        it->assign(itoa(i), 0);
        generate_set_word_propagetion_iterative_loop(module, obj, it_lst, it_idx+1);
    }
}

void generate_set_word_propagetion_iterative(node* module, node* obj) {
    node* all = new node("", obj);
    for (int i = 0; i < obj->children[0]->size; i++) {
        all->add_child(obj->children[0]->children[i]);
    }
    for (int i = 0; i < obj->children[1]->size; i++) {
        all->add_child(obj->children[1]->children[i]);
    }
    node* lst = iterator_list_from_expression(module, all);
    generate_set_word_propagetion_iterative_loop(module, obj, lst, 0);
}

void generate_set_word_propagetion(node* module, node* block) {
    for (int i = 0; i < block->size; i++) {
        node* n = block->children[i];
        if (n->type == ntIf) {
            if (evaluate_condition(module, n->children[0]))
                generate_set_word_propagetion(module, n->children[1]);
            else
                generate_set_word_propagetion(module, n->children[2]);
        } else
            generate_set_word_propagetion_iterative(module, n);
    }
}*/

void generate_step_function(node* module);
void generate_process_function(node* module) {
    if (only_prototype) {
        //add_line(module, "{} ");
        return;
    } 
    if(module->get_ancestor()->name == "typical_module") {
        add_line(module, "virtual void pump(transaction_t transaction)");
        add_line(module, "{");
        if(!module->get_block(ntConnectsToBlock)) {
            add_line(module, "}");
            return;
        }
        //generate_entring_function_codes(new node("pump"));
//        if(use_virtual_address)
//            add_line(module, "_address address = transaction.virtual_address;");
//        else
        add_line(module, "_address address = transaction.physical_address;");
        if (capture_data || capture_memory_content) {
            add_line(module, "uint64 data = transaction.data;");
        } else
            add_line(module, "uint64 data = 0;");
        add_line(module, "uint64 size = transaction.size;"); 
        //add_line(module, "if (lookup(address)) {");
        /*
        add_line(module, "if (transaction.type == ttDataWrite) {");
        add_line(module, "for(int i=0;  i<transaction.size; i++) {");
        add_line(module, "write_access(address+i, (transaction.data >> (i*8)) & 0xff);");
        add_line(module, "}");
        add_line(module, "}");
        add_line(module, "else {");
        add_line(module, "for(int i=0;  i<transaction.size; i++) {");
        add_line(module, "read_access(address+i);");
        add_line(module, "}");
        add_line(module, "}");
        */
        //add_line(module, "address = address+transaction.size-1;");
        /*
        if(capture_data) {
            add_line(module, "if(active) {");
            add_line(module, "if (transaction.type == ttDataWrite) {");
            add_line(module, "for(int i=0;  i<transaction.size; i++, address++) {");
            add_line(module, "write_access(address, (transaction.data >> (i*8)) & 0xff);");
            add_line(module, "}");
            add_line(module, "}");
            add_line(module, "else {");
            add_line(module, "for(int i=0;  i<transaction.size; i++, address++) {");
            add_line(module, "read_access(address);");
            add_line(module, "}");
            add_line(module, "}");
            add_line(module, "}");
        }
        */
        //generate_exiting_function_codes(new node("pump"));
        //add_line(module, "return;");
        //add_line(module, "}");         
    } else if(module->get_ancestor()->name == "typical_core") {
        add_line(module, "virtual void pump(message_t message)");
        add_line(module, "{");
        add_line(module, "_data *block=NULL;");
        add_line(module, "transaction_t transaction;");

        add_line(module, "for(int i=0; i<message.tlb_size; i++) {");
        add_line(module, "if(message.tlb[i].type == ttAddToTLB) {");
        add_line(module, "tlb_add(message.tlb[i].virtual_address, message.tlb[i].physical_address);");
        add_line(module, "}");
        add_line(module, "else {");
        add_line(module, "tlb_remove(message.tlb[i].virtual_address, message.tlb[i].physical_address);");
        add_line(module, "}");
        add_line(module, "}");

        if(propagate_data) {
            add_line(module, "if(transaction.type != ttDataWrite) {");
            add_line(module, "for(int i=0;  i<transaction.size; i++) {");
            add_line(module, "set_data(transaction.virtual_address+i, (transaction.data >> (i*8)) & 0xff);");
            add_line(module, "}");
            add_line(module, "}");
        }   
        //generate_entring_function_codes(new node("pump"));
    } else if(module->get_ancestor()->name == "typical_system") {
        add_line(module, "virtual void pump(message_t message)");
        add_line(module, "{");
        add_line(module, "_data *block=NULL;");
        add_line(module, "transaction_t transaction;");
        add_line(module, "iterations++;");
        add_line(module, "sim_step++;");
        add_line(module, "latency = 0;");
        //add_line(module, "if(message.type & (itLoad | itStore))");
        //add_line(module, "__current_transaction = message.data[0];");
        //add_line(module, "else __current_transaction = message.inst;");
        //add_line(module, "__step();");
        if(!macros["STEP"].empty() || !macros["VERBOSE"].empty()) {
            add_line(module, "if((__tracing || __itracing) && sim_step >= offset) {");
            add_line(module, "print_meaningful_section(message.data[0], message.__inst_inst_string.c_str());");
            //add_line(module, "if(message.type & (itLoad | itStore))");
            add_line(module, "for(int di=1; di<message.data_count; di++)");
            add_line(module, "    print_meaningful_section(message.data[di], message.__data_inst_string[di].c_str(), false);");
            add_line(module, "}");
        }
        add_line(module, "if (message.type & itStore) {");
        add_line(module, "access_type = ttDataWrite;");
        add_line(module, "}");
        add_line(module, "else {");
        add_line(module, "access_type = ttDataRead;");
        add_line(module, "}");
        /*
        add_line(module, "_address address = message.data.virtual_address;");
        if (capture_data || capture_memory_content) {
            add_line(module, "_data data = message.data.data;");
        } else
            add_line(module, "_data data = 0;");
		*/
    } else return;
    
    node* block = module->get_block(ntConnectsToBlock);
    generate_process_function_once = false;
    if(block) {
        generate_process_function(module, block);
    } else {
        node* curr = module->parent;
        while(!block && curr)  {
            block = curr->get_block(ntConnectsToBlock);
            curr = curr->parent;
        }
        if(block)
            generate_process_function(module, block);
    }
    
    if(module->get_ancestor()->name == "typical_module") {
        //add_line(module, "replace(address);");
        /*
        add_line(module, "if (transaction.type == ttDataWrite) {");
        add_line(module, "write_access(address, data);");
        add_line(module, "}");
        add_line(module, "else {");
        add_line(module, "read_access(address);");
        add_line(module, "}");
        */
        //add_line(module, "address = address+transaction.size-1;");
        /*
        if(capture_data) {
            add_line(module, "if(active) {");
            add_line(module, "if (transaction.type == ttDataWrite) {");
            add_line(module, "for(int i=0;  i<transaction.size; i++, address++) {");
            add_line(module, "write_access(address, (transaction.data >> (i*8)) & 0xff);");
            add_line(module, "}");
            add_line(module, "}");
            add_line(module, "else {");
            add_line(module, "for(int i=0;  i<transaction.size; i++, address++) {");
            add_line(module, "read_access(address);");
            add_line(module, "}");
            add_line(module, "}");
            add_line(module, "}");
        }
        */
    } else if(module->get_ancestor()->name == "typical_system") {
        if(!macros["STEP"].empty()) {
            //            if(macros["VERBOSE"].empty())
            //                add_line(module, "}");
            add_line(module, "if(sim_step >= offset && (__tracing || __itracing) && __one_trace_performed) {");
#ifdef __WIN32__
            add_line(module, "int w = csbi.dwSize.X;");
#else
            add_line(module, "int w = terminal_width();");
#endif
            add_line(module, "clog << endl << setw(w) << \"[R: Resume] [ESC: Break] [Any: Continue] \";");
            add_line(module, "char ch = getch();");
            add_line(module, "clog << endl;");
            add_line(module, "__one_trace_performed = false;");
            add_line(module, "if(ch == 27) { simulation_finished = true; return; } ");
            add_line(module, "if(ch == 'r') { trace_off(); __show_progress = true; return; } ");
            //            if(!macros["VERBOSE"].empty())
            add_line(module, "}");
        };
        //add_line(module, "double power = 0;");
        //add_line(module, "if (__machine_time > 0) power = energy/__machine_time;");
        add_line(module, "if (sim_step >= offset) {");
        add_line(module, "cpu_time[cpu_number] = latency;");
        add_line(module, "if (cpu_number == cpu_count-1) {");
        add_line(module,     "__machine_time = 0;");
        add_line(module,     "for(int i=0; i<cpu_count; i++) {");
        add_line(module,         "if(__machine_time < cpu_time[i]) __machine_time = cpu_time[i];");
        //add_line(module,     "__machine_time += cpu_time[i];");
        add_line(module, "}");
        //add_line(module, "__machine_time /= cpu_count;");
        add_line(module, "}");
        add_line(module,    "cpu_steps[cpu_number]++;");
        add_line(module, "}");
    } else if(module->get_ancestor()->name == "typical_core") {
    }
    add_line(module, "}");
}

std::map<string, bool> invoked;
bool is_invoked_chain(string chain) {
    if(chain.empty()) return false;
    if(invoked[chain]) return true;
    char s[256];
    sprintf(s, "%s", chain.c_str());
    //	string chain2 = chain;
    for(int i = strlen(s)-1; i >= 0; i--) {
        if (s[i] == '-') {
            s[i] = '\0';
            chain = s;
            if(invoked[chain]) return true;
        }
    }
    return false;
}


node* instantiate_type_module = NULL;
bool instantiate_only = true;
std::map<string, bool> assigned;
std::map<string, bool> instantiated;
void generate_instantiate(node *var, string chain="", bool loop = false);
void generate_instantiation(node* module, node* block, string chain="");
void generate_instantiations(node* module, string chain="");
void generate_instantiate_others(node *module, node *block, string chain, bool filter_not_extendeds=true);
void generate_connection_loop(node* module, node* c, node* it_lst, int it_idx, bool add);

void generate_step_function(node* module) {
    add_line(module, "void __step() {");
    if(module->get_property("step", false, ntImplementationBlock, true)) 
        add_line(module, "step();");
    instantiate_only = false;
    instantiated.clear();
    assigned.clear();
    generate_instantiations(module);
    add_line(module, "}");
}

void generate_connection_loop(string &out, node* module, node* c, node* it_lst, int it_idx, bool add) {
    if (it_idx >= it_lst->size) {
        string qe = " = ";
        string this_chain = "";
        for(int k=0; k < c->size; k++) {
            node* h=c->children[k];
            string trm = "->";
            if(k == c->size-1)
                qe = ";";
            for(int i=0; i < h->size; i++) {
                if(i == h->size-1)
                    trm = qe;
                node* obj = h->children[i];
                if (obj->type == ntExpression) {
                    string exp_str = evaluate_expression_result(module, obj, it_lst);
                    if(exp_str.empty())
                        exp_str = "0";
                    if(!is_int(exp_str)) {
                        report_exp(exp_gen(etSyntaxError, obj->line, obj->file, "An error occurred while evaluating index of "OPENING_QUOTE"%s"CLOSING_QUOTE": %s.", obj->name.c_str(), exp_str.c_str()));
                        return;
                    }
                    if(add) {
                        out += exp_str+trm;
                    }
                } else if(obj->children[1]->content_size == 1) {
                    if(trm != ";") {
                        obj->children[1]->connected = true;
                    }
                    this_chain += obj->name;
                    if(add) {
                        out += obj->name+trm;
                    }
                } else if(obj->children[1]->content_size <= 1) {
                    this_chain += obj->name;
                    if(add) {
                        out += obj->name+trm;
                    }
                } else {
                    if(trm != ";") {
                        obj->children[1]->connected = true;
                    }
                    string idx_str = evaluate_expression_result(module, obj->children[0], it_lst);
                    if(idx_str.empty())
                        idx_str = "0";
                    if(!is_int(idx_str)) {
                        report_exp(exp_gen(etSyntaxError, obj->line, obj->file, "An error occurred while evaluating index of "OPENING_QUOTE"%s"CLOSING_QUOTE": %s.", obj->name.c_str(), idx_str.c_str()));
                        return;
                    }
                    this_chain += obj->name;
                    this_chain += "[";
                    this_chain += idx_str;
                    this_chain += "]";
                    
                    if(atoi(idx_str.c_str()) >= obj->children[1]->content_size) {
                        report_exp(exp_gen(etSyntaxError, obj->line, obj->file, "Index "OPENING_QUOTE"%s"CLOSING_QUOTE" out of range (%d).", obj->name.c_str(), obj->children[1]->content_size));
                        return;
                    }
                    
                    if(add) {
                        out += obj->name+"["+idx_str+"]"+trm;
                    }
                }
                if(i == h->size-1){
                    if(trm != ";") {
                        obj->children[1]->connected = true;
                        assigned[this_chain] = true;
                    }
                    this_chain = "";
                } else {
                    this_chain += "->";
                }
            }
        }
        return;
    }
    
    node* it = it_lst->children[it_idx];
    
    string start_str = evaluate_expression_result(module, it->children[0], it_lst);
    if(!is_int(start_str)) {
        report_exp(exp_gen(etSyntaxError, it->line, it->file, "An error occurred while evaluating lower bound of iterator "OPENING_QUOTE"%s"CLOSING_QUOTE": %s.", it->name.c_str(), start_str.c_str()));
        return;
    }
    
    string end_str = evaluate_expression_result(module, it->children[1], it_lst);
    if(!is_int(end_str)) {
        report_exp(exp_gen(etSyntaxError, it->line, it->file, "An error occurred while evaluating upper bound of iterator "OPENING_QUOTE"%s"CLOSING_QUOTE": %s.", it->name.c_str(), end_str.c_str()));
        return;
    }
    
    int start = atoi(start_str.c_str());
    int end = atoi(end_str.c_str())+1;
    
    for (int i = start; i < end; i++) {
        it->assign(itoa(i), 0);
        string str = "";
        generate_connection_loop(module, c, it_lst, it_idx+1, add);
    }
}

void generate_connection_loop(node* module, node* c, node* it_lst, int it_idx, bool add) {
    string str = "";
    generate_connection_loop(str, module, c, it_lst, it_idx, add);
    if(!str.empty())
        add_line(c, str.c_str());
}

void generate_connection_chain(node* module, node* c, bool add) {
    node* all = new node("", c);
    for (int j = 0; j < c->size; j++) {
        for (int k = 0; k < c->children[j]->size; k++) {
            node* obj = c->children[j]->children[k];
            for (int i = 0; i < obj->children[0]->size; i++) {
                all->add_child(obj->children[0]->children[i]);
            }
        }
    }
    node* lst = iterator_list_from_expression(module, all);
    generate_connection_loop(module, c, lst, 0, add);
}

void generate_topology(node* module, node* block, bool add) {
    if (block) {
        for (int i = 0; i < block->size; i++) {
            node *n = block->children[i];
            if(n->type == ntIf) {
                if(evaluate_condition(module, n)) {
                    generate_topology(module, n->children[1], add);
                } else
                    generate_topology(module, n->children[2], add);
            } else
                generate_connection_chain(module, n, add);
        }
    }
}

void generate_topology(node* module, bool add) {
    node* block = module->get_block(ntTopologyBlock);
    generate_topology(module, block, add);
}

void generate_initializations(node* module, node* block, string chain="", string scope_chain="", string type_chain="");

bool has_initialization(node* r) {
    if (r->type == ntRecord) {
        for(int i = 0; i < r->children[1]->size; i++)
            if (has_initialization(r->children[1]->children[i]))
                return true;
    } else if (r->type == ntIf) {
        for (int i = 0; i < r->children[1]->size; i++)
            if (has_initialization(r->children[1]->children[i]))
                return true;
        for (int i = 0; i < r->children[2]->size; i++)
            if (has_initialization(r->children[2]->children[i]))
                return true;
    } else
        for (int i = 0; i < r->content_size; i++)
            if (r->initialized[i] || r->assigned[i])
                return true;
    return false;
}

void generate_infinit_records_initializations(node* module, node* r, string chain, string scope_chain = "", string type_chain = "") {
    
}

int record_iterator_idx = 1;
void generate_records_initializations(node* module, node* r, string chain, string scope_chain = "", string type_chain = "") {
    if(type_chain.empty())
        type_chain = r->name;
    else
        type_chain += "_"+r->name;
    if(scope_chain.empty())
        scope_chain = type_chain+"_t";
    else
        scope_chain += "::"+type_chain+"_t";
    if (r->content_size == -1 && has_initialization(r)) {
        add_line(r, "%s %s_default_value;", scope_chain.c_str(), type_chain.c_str());
        generate_initializations(module, r->children[1], type_chain+"_default_value.", scope_chain, type_chain);
        add_line(r, "%s%s.set_default(%s_default_value);", chain.c_str(), r->name.c_str(), type_chain.c_str());
        return;
    } else if (r->content_size == -1)
        return;
    char cc[64];
    int idx = record_iterator_idx;
    add_line(r, "for(long __r%d = 0; __r%d < %u; __r%d++) {", idx, idx, r->content_size, idx);
    sprintf(cc, "%s[__r%d].", r->name.c_str(), idx);
    chain += cc;
    record_iterator_idx++;
    generate_initializations(module, r->children[1], chain, scope_chain, type_chain);
    add_line(r, "}");
}

void generate_initializations(node* module, node* block, string chain, string scope_chain, string type_chain) {
    if (block) {
        for (int i = 0; i < block->size; i++) {
            node *n = block->children[i];
            if (n->type == ntRecord) {
                generate_records_initializations(module, n, chain, scope_chain, type_chain);
            } else if (n->type != ntConstant) {
                if (n->content_size == 1) {
                    if(n->assigned[0] || n->initialized[0]) {
                        n->content[0] = evaluate_expression_result(module, n->children[2]->children[0]);
                        add_line(n, "%s%s = %s;", chain.c_str(), n->name.c_str(), n->content[0].c_str());
                    }
                } else {
                    int last_assignment = -1;
                    for (int j=0; j < n->content_size; j++) {
                        if(n->assigned[j] || n->initialized[j]) {
                            last_assignment = j;
                            n->content[j] = evaluate_expression_result(module, n->children[2]->children[j]);
                            add_line(n, "%s%s[%d] = %s;", chain.c_str(), n->name.c_str(), j, n->content[j].c_str());
                        }
                    }                                   
                    //
                    if(n->size > 1 && n->type == ntVariable) {
                        string size_str = evaluate_expression_result(module, n->children[1]);
                        if (!is_int(size_str) && !size_str.empty()) {
                            report_exp(exp_gen(etSyntaxError, n->line, n->file, "Could not evaluate size expression for array "OPENING_QUOTE"%s"CLOSING_QUOTE": %s.", n->name.c_str(), size_str.c_str()));
                        }
                        if (size_str.empty())
                            size_str = "1";
                        long dim_size = atol(size_str.c_str());
                        if(dim_size > -1 && last_assignment > -1) {
                            if((last_assignment < dim_size-1) && (is_primary_type(n->children[0]->name))) {
                                int j=last_assignment+1;
                                n->content[j-1] = evaluate_expression_result(module, n->children[2]->children[j-1]);
                                add_line(n, "for(long i=%d; i<%d; i++) {", j, dim_size);
                                add_line(n, "%s%s[i] = %s;", chain.c_str(), n->name.c_str(), n->content[j-1].c_str());
                                add_line(n, "}", j, n->content_size);
                            }
                        } 
                    }
                }
            } else if(n->type == ntIf) {
                if (evaluate_condition(module, n))
                    generate_initializations(module, n->children[1], chain, scope_chain, type_chain);
                else
                    generate_initializations(module, n->children[2], chain, scope_chain, type_chain);
            }
        }
    }
}

void generate_initializations(node* module) {
    if (module->parent)
        generate_initializations(module->parent);
    generate_initializations(module, module->get_block(ntDefinitionsBlock));
    generate_initializations(module, module->get_block(ntArchitectureBlock));
}

bool is_assigned_chain(string chain) {
    if(chain.empty()) return false;
    if(assigned[chain]) return true;
    if(instantiated[chain]) return true;
    char s[256];
    sprintf(s, "%s", chain.c_str());
    //	string chain2 = chain;
    for(int i = strlen(s)-1; i >= 0; i--) {
        if (s[i] == '-') {
            s[i] = '\0';
            chain = s;
            if(assigned[chain]) return true;
        }
    }
    return false;
}

void generate_submodule_instantiate(node* block, node* last_node=NULL, string name_chain="") {
    node* module = instantiate_type_module;
    if(block && module) {
        if(last_node) if(module->get_property(last_node->name, true, ntDefinitionsBlock)) return;
        for(int i=0; i<block->size; i++) {
            instantiate_type_module = module;
            if(block->children[i]->type == ntVariable) {
                node* sm = get_submodule(module, block->children[i]->children[0]->name);
                if(!sm) sm = get_owner_sub_type(module, block->children[i]->children[0]->name);
                if(sm) {
                    string new_chain = name_chain;
                    if(new_chain.empty())
                        new_chain = block->children[i]->name;
                    else
                        new_chain += "->"+block->children[i]->name;
                    if(block->children[i]->content_size <= 1) {
                        generate_instantiate(block->children[i], new_chain);
                    } else {
                        char index_ch[32];
                        int len = block->children[i]->content_size;
                        for(int j = 0; j < len; j++) {
                            sprintf(index_ch, "[%d]", j);
                            generate_instantiate(block->children[i], new_chain+index_ch, true);
                        }
                    }
                }
            } else if(block->children[i]->type == ntIf) {
                if(evaluate_condition(module, block->children[i]->children[0])) {
                    generate_submodule_instantiate(block->children[i]->children[1], NULL, name_chain);
                } else {
                    generate_submodule_instantiate(block->children[i]->children[2], NULL, name_chain);
                }
            }
        }
    }
}

void generate_submodule_instantiate(node* last_node, string name_chain="") {
    node* block = instantiate_type_module->get_block(ntDefinitionsBlock);
    generate_submodule_instantiate(block, last_node, name_chain);
}

string get_type_scope(node* module, node* type) {
    string tp = generate_variable_type(type);
    if(type->size > 0) return tp;
    node* tmodule = get_type_node(module, type);
    node* extender = module;
    if(tmodule)
        while(tmodule->owner) {
        if(tmodule->owner->name != top_level_module()->name) {
            string t = tmodule->owner->name;
            if(tmodule->owner->extended) {
                t = module->name;
            }
            t += "::"+tp;
            tp = t;
        }
        tmodule = tmodule->owner;
    }
    return tp;
}

string type_chain="";
void generate_instantiate(node *var, string chain, bool loop) {
    char *new_chain = new char[chain.length()+64];                    
    string saved_type_chain=type_chain;
    node* p=NULL;
    if(type_chain.empty())
        type_chain = " ";
    else if(p=var->parent->get_property(var->children[0]->name, true, ntDefinitionsBlock)) {
        if(p->type == ntSubmodule)
            if(type_chain == " ")
                type_chain = var->children[0]->name;
        else
            type_chain += "::"+var->children[0]->name;
    } else {
        type_chain = get_type_scope(var->parent, var->children[0]);
    }
    if(!is_assigned_chain(chain) && !chain.empty()) {
        if(var->content_size == 1 || loop) {
            if(instantiate_only) {
                add_line(var, "%s = new %s(\"%s\");", chain.c_str(), type_chain.c_str(), chain.c_str());
            } else {
                node* tmodule = get_type_node(var->parent, var->children[0]);
                if(tmodule)
                    if(tmodule->get_property("step", true, ntImplementationBlock, true))
                        add_line(var, "%s->step();", chain.c_str());
            }
        } else if(loop) {
            for (int j = 0; j < var->content_size; j++) {
                sprintf(new_chain, "%s[%d]", chain.c_str(), j);
                generate_instantiate(var, new_chain, true);
            }
        } else if(instantiate_only) {
            add_line(var, "%s = new %s(\"%s\");", chain.c_str(), type_chain.c_str(), chain.c_str());
        } else {
            node* tmodule = get_type_node(var->parent, var->children[0]);
            if(tmodule)
                if(tmodule->get_property("step", true, ntImplementationBlock, true))
                    add_line(var, "%s->step();", chain.c_str());
        }
    }
    if(chain.empty()) {
        if(var->content_size == 1) {
            generate_instantiate(var, var->name);
        } else for (int j = 0; j < var->content_size; j++) {
            sprintf(new_chain, "%s[%d]", var->name.c_str(), j);
            generate_instantiate(var, new_chain, true);
        }
        type_chain = saved_type_chain;
        return;
    }
    node* last_instantiate_type_module = instantiate_type_module;
    if(is_user_type(var->children[0]->name) || is_sub_module(instantiate_type_module, var->children[0]->name) || is_owner_sub_type(instantiate_type_module, var->children[0]->name)) {
        node* tmodule = get_submodule(instantiate_type_module, var->children[0]->name);
        if(!tmodule) tmodule = get_owner_sub_type(instantiate_type_module, var->children[0]->name);
        if(tmodule)
            instantiate_type_module = tmodule;
        else
            instantiate_type_module = get_module(var->children[0]->name);
    }
    if(instantiate_type_module) {
        node* backup = instantiate_type_module;
        generate_submodule_instantiate(var, chain);
        instantiate_type_module = backup;
    }
    instantiated[chain] = true;
    generate_instantiate_others(instantiate_type_module, instantiate_type_module->get_block(ntDefinitionsBlock), chain);
    instantiate_type_module = last_instantiate_type_module;
    type_chain = saved_type_chain;
}

void generate_instantiate_others(node *module, node *block, string chain, bool filter_not_extendeds) {
    if(!module) return;
    if(module->parent)
        generate_instantiate_others(module->parent, module->parent->get_block(ntDefinitionsBlock), chain, false);
    if(block && (!module->extended || !filter_not_extendeds)) {
        for (int i = 0; i < block->size; i++)
            if(block->children[i]->type != ntSubmodule) {
            if(block->children[i]->type == ntIf) {
                if(evaluate_condition(module, block->children[i]))
                    generate_instantiate_others(module, block->children[i]->children[1], chain, filter_not_extendeds);
                else
                    generate_instantiate_others(module, block->children[i]->children[2], chain, filter_not_extendeds);
            } else if(is_user_type(block->children[i]->children[0]->name) || is_sub_module(module, block->children[i]->children[0]->name)) {
                node *obj = block->children[i];
                char *new_chain = new char[chain.length()+64];
                if(obj->content_size == 1) {
                    sprintf(new_chain, "%s->%s", chain.c_str(), obj->name.c_str());
                    generate_instantiate(obj, new_chain);
                } else for (int j = 0; j < obj->content_size; j++) {
                    sprintf(new_chain, "%s->%s[%d]", chain.c_str(), obj->name.c_str(), j);
                    generate_instantiate(obj, new_chain);
                }
            }
        }
    }
}

void generate_instantiation(node* module, node* block, string chain) {
    if (block) {
        instantiate_type_module = module;
        for (int i = 0; i < block->size; i++) {
            if(block->children[i]->type == ntIf) {
                if(evaluate_condition(module, block->children[i]))
                    generate_instantiation(module, block->children[i]->children[1], chain);
                else
                    generate_instantiation(module, block->children[i]->children[2], chain);
            } else {
                generate_instantiate(block->children[i], chain);
            }
        }
    }
}

void generate_instantiations(node* module, string chain) {
    if(module->parent)
        generate_instantiations(module->parent, chain);
    node* block = module->get_block(ntDefinitionsBlock);
    generate_instantiation(module, block, chain);
}

void generate_set_owner_thread(node* module, node* block) {
    if(!block) return;
    for(int i=0; i < block->size; i++) {
        node* n = block->children[i];
        if(n->type == ntIf) {
            if(evaluate_condition(module, n))
                generate_set_owner_thread(module, n->children[1]);
            else
                generate_set_owner_thread(module, n->children[2]);
        } else if(n->type == ntVariable && (is_user_type(n->children[0]->name) || is_sub_module(module, n->children[0]->name) || is_owner_sub_type(module, n->children[0]->name))) {
            if (n->content_size == 1) {
                add_line(n, "%s->set_owner_thread(tid);", n->name.c_str());
            } else {
                for (int j=0; j < n->content_size; j++) {
                    add_line(n, "%s[%d]->set_owner_thread(tid);", n->name.c_str(), j);
                }
            }
        }
    }
}

void generate_set_owner_thread(node* module) {
    if(only_prototype) {
        add_line(module, "virtual void set_owner_thread(int tid) {} ");
        return;
    }
    add_line(module, "virtual void set_owner_thread(int tid) {");
    add_line(module, "__owner_tid = tid;");
    node* block = module->get_block(ntDefinitionsBlock);
    generate_set_owner_thread(module, block);
    add_line(module, "}");
}

bool implement_module_initialize_function(node* module) {
    node* n=NULL;
    if(n=has_function(module, "initial"))
        add_line(n, "initial();");
}

void generate_init_function(node* module, bool is_top_level) {
    if(module->extended)
        add_line(module, "virtual");
    add_line(module, "void init()");
    add_line(module, "{");
    add_line(module, "search_enable = true;");
    if(!is_top_level) {
        for(set<string>::iterator it=time_variables.begin(); it!=time_variables.end();it++) 
            add_line(module, "%s_exiting_time = 0;", it->c_str());
        if(slip_time_variables.size() > 0) {
            for(set<string>::iterator it=slip_time_variables.begin(); it!=slip_time_variables.end();it++) { 
                add_line(module, "%s_timeslot_index = 0;", it->c_str());
            }
            add_line(module, "for(int __i=0; __i<%d; __i++) {", max_time_slots);
            for(set<string>::iterator it=slip_time_variables.begin(); it!=slip_time_variables.end();it++) 
                add_line(module, "%s_busy_timeslots[__i].valid = false;", it->c_str());
            add_line(module, "}");
            if(module->get_ancestor()->name == "typical_module") {
                if(slip_time_variables.size()>0)  {
                    add_line(module, "updating_blocks_table_index = 0;");
                    add_line(module, "for(int __i=0; __i<%d; __i++) {", max_replacing_blocks);
                    add_line(module, "updating_blocks_table[__i].end = 0;");
                    add_line(module, "}");
                }
            }
        }
    }
    if(!only_prototype) {
        if(is_top_level) {
            add_line(module, "this->name = new char[%d];", module->name.length()+1);
            add_line(module, "strcpy(this->name, \"%s\");", module->name.c_str());
        }
        if(is_top_level)
            generate_topology(module, false);
        record_iterator_idx = 1;
        generate_initializations(module);
        if(is_top_level)
            generate_instantiations(module);
        if(is_top_level)
            generate_topology(module, true);
    }
    implement_module_initialize_function(module);
    add_line(module, "}");
}

void generate_constructor(node* module, bool is_top_level) {
    generate_init_function(module, is_top_level);
    add_line(module, "%s ()", module->name.c_str());
    add_line(module, "{");
    add_line(module, "this->name = new char[10];");
    add_line(module, "strcpy(this->name, \"Unknown\");");
    add_line(module, "init();");
    add_line(module, "delete[] this->name;");
    add_line(module, "}");
    add_line(module, "%s (const char* n)", module->name.c_str());
    add_line(module, "{");
    add_line(module, "this->name = new char[strlen(n)+1];");
    add_line(module, "strcpy(this->name, n);");
    add_line(module, "init();");
    add_line(module, "}");
}

bool has_parent_name(node* module, string name) {
    if (module->parent) {
        if (module->parent->name == name)
            return true;
        return has_parent_name(module->parent, name);
    }
    return false;
}

node* get_engaged_parent(node* m) {
    if(!m) return NULL;
    if(m->engaged) return m;
    return get_engaged_parent(m->parent);
}

bool is_used_module(node* module, node* block, node* look) {
    if(!block) return false;
    for(int i=0; i < block->size; i++) {
        if(block->children[i]->type == ntIf) {
            if(evaluate_condition(module, block->children[i])) {
                if(is_used_module(module, block->children[i]->children[1], look))
                    return true;
            } else {
                if(is_used_module(module, block->children[i]->children[2], look))
                    return true;
            }
        } else if(block->children[i]->type == ntSubmodule) {
            if(is_used_module(block->children[i], look))
                return true;
        } else if(block->children[i]->type == ntVariable) {
            //            cout << look->name.c_str() << ":" << block->children[i]->children[0]->name.c_str() << " " <<  block->children[i]->name.c_str() << endl;
            if(is_used_module(block->children[i]->children[0], look))  
                return true; 
            else for(int j=0; j < block->children[i]->children[0]->size; j++)
            	if(is_used_module(block->children[i]->children[0]->children[j], look)) 
    	            return true;
        }
    }
    return false;
}

bool make_used_module(node* module) {
    module->is_used_module=true;
    if (module->parent)
        make_used_module(module->parent);
    return true;
}

bool is_used_module(node* module, node* look) {
    if(!module) return false;
    if(look->is_used_module) return true;
    if(is_primary_type(module->name)) return false;
    if(module->is_child_of(look)) return make_used_module(look);
    if(look->type != ntSubmodule || look->type != ntNone) {
        node* type_node = get_submodule(module, look->name);
        if(!type_node) type_node = get_owner_sub_type(module, look->name);
        if(!type_node) type_node = get_module(look->name);
        if(!type_node) return false;
        look = type_node;
    }
    node* block = module->get_block(ntDefinitionsBlock);
    if(is_used_module(module, block, look)) return make_used_module(look);
    if(is_used_module(module->parent, look)) return make_used_module(look);
    return false;
}

void generate_module(node* module, bool is_top_level) {
    current_module = module;
    if(!module->used && !module->engaged && !is_top_level) return; 
    if(module->extended && !module->engaged && /*module->name != "typical_cache" && */ module->name != "typical_module" ) only_prototype = true;
    //    cout << "generate module "OPENING_QUOTE"" << module->name << ""CLOSING_QUOTE"" << endl;
    time_variables.clear();
    reset_parameters(module);
    //    cout << "parameters reset" << endl;
    turn_off_reporting();
    assign_specifications(module);
    update_all(module);
    turn_on_reporting();
    assign_specifications(module);
    update_all(module);

    if(module->get_ancestor()->name != "typical_system" && module->type != ntSubmodule) 
        if(!is_used_module(top_level_module(), module))
            return;
    
    //    cout << "specifications assigned" << endl;
    module->evaluated = true;
    generate_process_function_once = false;
    
    estimate_external_parameters_with_cacti(module);
    assign_external_specifications(module);
    
    add_line(module, "class %s", module->name.c_str());
    
    if(module->parent) {
        //		if(module->parent->engaged) {
        add_line(module, ": public %s {", module->parent->name.c_str());
    }
    else
        add_line(module, "{");
    //	} else {
    //		if (module->name == "typical_cache") {
    //			add_line(module, ": public typical_memory {");
    //		} else if (has_parent_name(module, "typical_cache"))
    //			add_line(module, ": public typical_cache {");
    //		else if (module->get_ancestor()->name == "typical_memory" && module->name != "typical_memory")
    //			add_line(module, ": public typical_memory {");
    //		else
    //			add_line(module, "{");
    //	}
    
    //	if (module->name == "typical_memory" || module->get_ancestor()->name != "typical_memory")
    if(!module->parent) {
        add_line(module, "public: char* name;");
        generate_primitive_variables(module);
    } else
    	add_line(module, "public:");
    time_variables.clear();
    slip_time_variables.clear();
    generate_primitive_time_variables(module);
    if(!only_prototype) {generate_submodules(module, module); time_variables.clear();}
    if(!only_prototype) generate_submodule_instanciations(module);
    if(!only_prototype) generate_architecture(module, module);
    generate_variables(module, module);
    if(!only_prototype) generate_internal_variables(module);
    generate_constructor(module, is_top_level);
    if(!only_prototype) generate_internal_functions(module);
    generate_implementation(module, module, is_top_level);
    if (module->get_ancestor()->name == "typical_system" && !module->extended)
        generate_step_function(module);
    generate_process_function(module);
    add_line(module, "};");
    //only_prototype = false;
}

node* top_level_module() {
    static node* top_module = NULL;
    if(top_module)
        return top_module;
    node* top_level = NULL;
    for (int idx = 0; idx < type_modules_size; idx++) {
        node* n = type_modules[idx];
        if (!n->used)
            if (n->get_ancestor()->name == "typical_system" && n->name != "typical_system") {
            top_level = &*n;
            break;
        }
    }
    if (!top_level) {
        report_exp(exp_gen(etFatalError, 0, "", "Could not find top-level (system) module."));
        return NULL;
    }
    top_module = top_level;
    
    turn_off_reporting();
    assign_specifications(top_level);
    update_all(top_level);
    turn_on_reporting();
    assign_specifications(top_level);
    update_all(top_level);
    
    return top_level;
}

node* find_main_memory(node* module, node* block, bool con) {
    node* main_memory_module = NULL;
    for(int i=0; i<block->size; i++) {
        if(block->children[i]->type == ntIf) {
            if(evaluate_condition(module, block->children[i])) {
                node* r = find_main_memory(module, block->children[i]->children[1], con);
                if(r) main_memory_module = r;
            } else {
                node* r = find_main_memory(module, block->children[i]->children[2], con);
                if(r) main_memory_module = r;
            }
        } else {
            //cout << "searching " << block->children[i]->children[0]->name.c_str() << "   " << block->children[i]->name.c_str() << endl;
            node* tmodule = get_submodule(module, block->children[i]->children[0]->name);
            if(!tmodule) tmodule = get_owner_sub_type(module, block->children[i]->children[0]->name);
            if(!tmodule) tmodule = get_module(block->children[i]->children[0]->name);
            if(tmodule) {
                if(tmodule->get_ancestor()->name == "typical_module" && (con || !block->children[i]->connected)) {
                    main_memory_module = block->children[i];
                }
            }
        }
    }
    return main_memory_module;
}

node* main_memory_module() {
    static node* main_memory = NULL;
    if(main_memory)
        return main_memory;
    node* top_level = top_level_module();
    node* block = top_level->get_block(ntDefinitionsBlock);
    node* m = find_main_memory(top_level, block, true);
    if(!m) m = find_main_memory(top_level, block, false);
    if (!m) {
        report_exp(exp_gen(etFatalError, 0, "", "Could not find main memory module."));
        return NULL;
    } else {
        report_exp(exp_gen(etNote, 0, "", "Variable "OPENING_QUOTE"%s"CLOSING_QUOTE" is considered as the main-memory.", m->name.c_str()));
        cout << "You can change it by adding the following code within initialize function:" << endl;
        cout << "\n       __main_memory = top_level_module->[Variable Name];\n" << endl;
    }
    main_memory = m;
    return m;
}

void generate_engaged_and_extended_module(node* module) {
    if(module->name == "typical_module") return;
    //    if(module->name == "typical_cache") return;
    if(module->get_ancestor()->name == "typical_system") return;
    for (int idx = 0; idx < type_modules_size; idx++) {
        if (!type_modules[idx]->extended && type_modules[idx]->is_child_of(module)) {
            add_line(type_modules[idx], "class %s;", type_modules[idx]->name.c_str());
            add_line(module, "class %s: public %s;", module->name.c_str(), type_modules[idx]->name.c_str());
            return;
        }
    }
}

void generate_engaged_and_extended_modules() {
    for (int idx = 0; idx < type_modules_size; idx++) {
        if (type_modules[idx]->extended && type_modules[idx]->engaged) {
            generate_engaged_and_extended_module(type_modules[idx]);
        }
    }
}

void generate_not_extended_modules() {
    for (int idx = 0; idx < type_modules_size; idx++) 
        is_used_module(top_level_module(), type_modules[idx]);
    //	generate_engaged_and_extended_modules();
    for (int idx = 0; idx < type_modules_size; idx++) {
        //		if (((!type_modules[idx]->extended) && type_modules[idx]->get_ancestor()->name != "typical_system") ||
        //			(type_modules[idx]->name == "typical_memory") ||
        //			(type_modules[idx]->name == "typical_cache") ||
        //			(type_modules[idx]->extended && type_modules[idx]->engaged))
        generate_module(type_modules[idx]);
        only_prototype = false;
    }
}

void generate_top_level() {
    generate_module(top_level_module(), true);
    if (!has_report_function) {
        report_exp(exp_gen(etWarning, top_level_module()->line, top_level_module()->file, "No report function is declared."));
    }
}

void generate_c_code() {
    if (errors_count > 0) return;
    if(!top_level_module()) return;
    //    cout << "generate c code" << endl;
    top_level_module()->name += "_t";
    //    cout << "generate misc" << endl;
    generate_miscs();
    //    cout << "generate not extended modules" << endl;
    generate_not_extended_modules();
    //    cout << "generate top level" << endl;
    generate_top_level();
    //    cout << "generate memory module" << endl;
    if(!main_memory_module()) return;
    //    cout << "generate initial function" << endl;
    generate_initial_function();
    //    cout << "generate initialize modules function" << endl;
    generate_initialize_modules_function();
    generate_top_level_step_function();
}

bool link_cpp_objects = true;
string compile_output_filename("");

string extract_pure_filename(const char* f) {
    char* input = new char[strlen(f)+1];
    strcpy(input, f);
    int dot_idx = strlen(input);
    for (int i=dot_idx; i >= 0; i--)
    {
        if (input[i] == '\\' || input[i] == '/')
            break;
        else if (input[i] == '.') {
            dot_idx = i;
            break;
        }
    }
    input[dot_idx] = '\0';
    return input;
}

void report_errors_and_warnings() {
    if(!macros["STEP"].empty() && automatic_simulation) {
        report_exp(exp_gen(etWarning, top_level_module()->line, top_level_module()->file, "Using both STEP and AUTOMATIC modes may cuase the simulation take large memory space in case of running millions of cycles."));
    }
    report_notused_warnings();
}

string cpp_source_file_path("");
void generate_c_file(char* input) {
    int cnt = 0;
    for(int i=0; i<type_modules_size; i++)
        if(type_modules[i]->get_ancestor()->name == "typical_system" && !type_modules[i]->extended)
            cnt++;
    if(cnt > 1)
        report_exp(exp_gen(etSyntaxError, top_level_module()->line, top_level_module()->file, "Too many declarations for top-level module."));
    
    if (errors_count > 0) {
        cout << "Could not generate c++ file." << endl;
        return;
    } else {
        generate_c_code();
        report_errors_and_warnings();
    }	
    if (errors_count > 0) {
        cout << "Could not generate c++ file." << endl;
        return;
    } 
    if (compile_output_filename.empty())
        compile_output_filename = extract_pure_filename(input);
    else
        compile_output_filename = extract_pure_filename(compile_output_filename.c_str());
#ifdef __WIN32__
    compile_output_filename += ".exe";
#endif
    
    cpp_source_file_path = extract_pure_filename(input);
    cpp_source_file_path += ".cpp";
    
    ofstream file(trim(cpp_source_file_path).c_str(), ios::out);
    file << generate_source().c_str() << endl;
}

bool is_mapped(string file, int ln) {
    if (file == cpp_source_file_path)
        return true;
    return false;
}

void parse_compiler_errors(string str) {
    string line = "";
    string filename, line_num, type;
    int lidx = 0, token = 0;
    
    int pos = str.find("%");
    while(pos > -1) {
        str.replace(pos, 1, "%%");
        pos = str.find("%", pos+2);
    }
    
    for(unsigned int i=0; i < str.length(); i++) {
        if(str[i] == '\n') {
            type = trim(type);
            line = trim(line);
            if (is_int(line_num)) {
                if (is_mapped(filename, atoi(line_num.c_str()))) {
                    msim_line l = line_map[atoi(line_num.c_str())];
                    if(l.filename != NULL)
                        report_exp(exp_gen((type == "error")?etSyntaxError:etWarning,
                                           l.line - 1, l.filename, line.c_str()));
                    else
                        report_exp(exp_gen((type == "error")?etSyntaxError:etWarning,
                                           atoi(line_num.c_str())-1, filename.c_str(), line.c_str()));
                } else {
                    report_exp(exp_gen((type == "error")?etSyntaxError:etWarning,
                                       atoi(line_num.c_str())-1, filename.c_str(), line.c_str()));
                }
            }
            filename = "";
            line_num = "";
            line = "";
            type = "";
            lidx = 0;
            token = 0;
        } else {
            if(lidx > 2 && str[i] == ':' && token < 4) {
                switch(token++) {
                case 0: filename = line; line = ""; break;
                case 1: line_num = line; line = ""; break;
                case 2: if(!is_int(line)) type = line; line = ""; break;
                case 3: if(type.empty()) {type = line; line = ""; break;}
                default: line += ':';
                }
            } else
                line += str[i];
            lidx++;
        }
    }
}

void estimate_external_parameters_with_cacti(node* module) {
    //TODO:
}

string compiler_directives;
string cpp_compiler_path;
string compile_options="";
void compile_and_link() {
    if(!link_cpp_objects) return;
    compile_output_filename = trim(compile_output_filename);
    cpp_source_file_path = trim(cpp_source_file_path);
    if(cpp_compiler_path.empty()) {
        cerr << "Compiler not found." << endl;
        return;
    }
    char cmd[256];
    char lib[32];
    strcpy(lib, " ");
    /*
    if(multithreaded) {
#ifdef __WIN32__
        strcpy(lib, "-lpthreadGC2");
#else
        strcpy(lib, "-lpthread");
#endif
    }
    */
    sprintf(cmd, "%s %s -Wdisabled-optimization -o \"%s\" \"%s\" %s %s 2>&1", cpp_compiler_path.c_str(), compile_options.c_str(), compile_output_filename.c_str(), cpp_source_file_path.c_str(), compiler_directives.c_str(), lib);
    parse_compiler_errors(exec(cmd));
}
