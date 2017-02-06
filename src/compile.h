#ifndef COMPILE_H
#define COMPILE_H

#include "syntax.h"

extern bool has_report_function;
extern bool link_cpp_objects;
extern bool use_virtual_address;
extern long size_threshold;
extern string compiler_directives;
extern string compile_options;
extern string compile_output_filename;
extern string cpp_compiler_path;
extern int max_thread_id_counter;

void display_block(node* module, node_type block_type);
void display_module(node* module);
void display_node(node* n, string indent);

void report_notused_warnings();
void report_errors_and_warnings();

void generate_c_file(char*);
void compile_and_link();

node* top_level_module();
node* main_memory_module();
char* itoa(int number);

#endif
