/**
 * @file evaluation.h
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

#ifndef EVALUATION_H
#define EVALUATION_H

#include "exception.h"
#include "syntax.h"

bool variable_exists(const char* name);
bool file_exists(const char * filename);
string exec(char* cmd);

char* evaluate(const char* exp, int line = 0, char* filename = "unknown");
string evaluate_expression_result(node* module, node* exp, node* iterators = NULL);
string evaluate_string_expression_result(node* module, node* exp, node* iterators = NULL);

string trim(string str);
string remove_qoutes(const char* str);
string read_file(char* filename);

int evaluate_condition(node* module, node* cond);

#endif

