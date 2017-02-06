/**
 * @file evaluation.cpp
 *
 * @brief Functions for evaluating expressions containing parameters, constants,
 * and macro directives. Also some functions for string manipulation.
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

#include "exception.h"
#include "parser.h"
#include "syntax.h"

#include <fstream>
#include <cstring>

using namespace std;

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

bool file_exists(const char * filename) { 
    if(strlen(filename) == 0) return false;   
    ifstream file(filename);
    return file;
}

string exec(char* cmd) {
    FILE* pipe = popen(cmd, "r");
    if (!pipe)
        return "ERROR";
    char buffer[128];

    std::string result = "";

    while (!feof(pipe)) {
        if (fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }
    pclose(pipe);
    return result;
}

string trim(string str) {
    string result = "";
    string whitespace = "";
    for (unsigned int i = 0; i < str.length(); i++) {
        if (str[i] == ' ' || str[i] == '\t')
            whitespace += str[i];
        else {
            if (!result.empty())
                result += whitespace;
            result += str[i];
            whitespace = "";
        }
    }
    return result;
}

string read_file(char* filename) {
    ifstream f;
    f.open(filename);
    string result;
    while(f.good()) {
        char l[1025];
        readline(f, l);
        result += l;
        result += '\n';
    }
    f.close();
    return result;
} 
 
string remove_qutes(string w) {
    string res = "";
    for (unsigned int i = 1; i < w.length()-1; i++) {
        res += w[i];
    }
    return res;
}

Parser prs;
const char* evaluate(const char* exp, int line, char* filename) {
    const char* ans;
    try {
		ans = prs.parse(exp);
    }
    catch(Error err) {
        throw exp_gen(etSyntaxError, line, filename, err.get_msg());
    } catch(...) {
        throw exp_gen(etSyntaxError, line, filename, "Evaluation failed.");
    }
    return ans;
}

bool variable_exists(const char* name) {
    return prs.var_exist(name);
}

string evaluate_string_expression_result(node* module, node* exp, node* iterators) {
    string val = "";
    for (int i = 0; i < exp->size; i++) {
        node *n = exp->children[i];
        if(is_arithmatic_operator(n->name[0])) {
            if(n->name != "+")
                throw exp_gen(etSyntaxError, n->line, n->file,
                              "Operator %s is not defined for strings.", n->name.c_str());
            continue;
        }
        if(!is_number(n->name) && !is_string(n->name)) {
            if (!macros[n->name].empty())
                n->name = trim(macros[n->name]);
            if(n->name[0] == '#'){
                if (iterators) {
                    for (int k = 0; k < iterators->size; k++) {
                        if(iterators->children[k]->name == n->name){
                            val += iterators->children[k]->content[0];
                            break;
                        }
                    }
                }
            } else {
                node *n = exp->children[i];
                int index = 0;
                try {
                    index = get_parameter_index(module, n);
                }
                catch(exception_t e) {
                    report_exp(e);
                    return "";
                }
                n = get_parameter(module, n->name, n->type);
                if (index >= n->content_size) {
                    report_exp(exp_gen(etSyntaxError, exp->line, exp->file,
                                       "Index out of range."));
                }
                else if (!n->assigned[index] && !n->initialized[index] && n->children[2]->size == 0) {
                    report_exp(exp_gen(etSyntaxError, exp->line, exp->file,
                                       "Parameter `%s' has not a value at index of %d.",
                                       n->name.c_str(), index));
                } else if (!n->assigned[index] && !n->initialized[index])
                    report_exp(exp_gen(etSyntaxError, exp->line, exp->file,
                                       "Parameter `%s' was not assigned at index of %d.",
                                       n->name.c_str(), index));
                else {
                    val += n->content[n->index];
                }
            }
        } else
            val += remove_qutes(n->name);
    }
    return val;
}

string evaluate_expression_result(node* module, node* exp, node* iterators) {
    if (exp->size == 0) return "";
    char s[1024] = "";
    char stmp[1024] = "";
    for (int i = 0; i < exp->size; i++)
        if (is_string(exp->children[i]->name))
            return evaluate_string_expression_result(module, exp, iterators);
    if (module) {
        for (int i = 0; i < exp->size; i++) {
            node *n = exp->children[i];
            if (!macros[n->name].empty())
                n->name = trim(macros[n->name]);
            if(n->name[0] == '#') {
                if (iterators) {
                    for (int k = 0; k < iterators->size; k++) {
                        if(iterators->children[k]->name == n->name){
                            sprintf(stmp, "%s%s", s, iterators->children[k]->content[0].c_str());
                            break;
                        }
                    }      //maybe not exist
                } else
                    sprintf(stmp, "%s0", s); //not exist
            } else if (n->type == ntParameter || n->type == ntConstant) {
                node* tmodule = module;
//                while(n->name == "owner" && i < exp->size) {
//                    if(exp->children[++i]->name != "::") {
//                        return "Statement `::' is expected.";
//                    }
//                    if(i < exp->size)
//                        n = exp->children[++i];
//                    else
//                        return "Expecting a parameter after `::'";
//                    tmodule = tmodule->owner;
//                    if(!tmodule)
//                        return "NULL pointer to owner.";
//                }
                int index = 0;
                try {
                    index = get_parameter_index(tmodule, n);
                }
                catch(exception_t e) {
                    report_exp(e);
                    return "";
                }
                n = n->children[1];// get_parameter(tmodule, n->name, n->type);
                n->used = true;
                if (index >= n->content_size) {
                    report_exp(exp_gen(etSyntaxError, exp->line, exp->file, "Index out of range."));
                } else if (!n->assigned[index] && !n->initialized[index] && n->children[2]->size == 0) {
                    report_exp(exp_gen(etSyntaxError, exp->line, exp->file, "Parameter `%s[%d]' has never been assigned a value.", n->name.c_str(), index));
                } else if (!n->assigned[index] && !n->initialized[index])
                    report_exp(exp_gen(etSyntaxError, exp->line, exp->file, "Parameter `%s[%d]' has never been assigned a value", n->name.c_str(), index));
                else {
                    sprintf(stmp, "%s %s", s, n->content[index].c_str());
                }
            } else
                sprintf(stmp, "%s %s", s, exp->children[i]->name.c_str());
            strcpy(s, stmp);
        }
    } else
        for (int i = 0; i < exp->size; i++) {
            sprintf(stmp, "%s %s", s, exp->children[i]->name.c_str());
        strcpy(s, stmp);
    }
    string str = s;
//    if(!module) clog << s << endl;
    try {
//			if(!strcmp(s, "2e-9"))
//				cout << "here";
		str = evaluate(s, exp->line, exp->file);
    } catch(...) {
        //        str = "";
    }
    exp->evaluated = true;
    return str;
}

int evaluate_condition(node* module, node* cond) {
    //	if(cond->evaluated) return cond->value;
    string cond_str = evaluate_expression_result(module, cond->children[0], NULL);
	if(!is_int(cond_str))
        report_exp(exp_gen(etSyntaxError, cond->line, cond->file, "An error occured while evaluating the condition of `whenever' phrase: %s", cond_str.c_str()));
	else
		cond->value = atoi(cond_str.c_str());
    return cond->value;
}



