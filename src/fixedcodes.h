#ifndef FIXEDCODES_H
#define FIXEDCODES_H
#include <string>

#define CAPTURE_AT_ONCE 0
#define CAPTURE_FROM_FILE 1
#define CAPTURE_FROM_PIPE 2
#define MSIM_VERSION "1.0.15"
 
extern int number_of_threads;
extern int number_of_bytes_per_instruction;
extern bool capture_code_string;
extern bool capture_exception_string;
extern bool capture_data;
extern bool propagate_data;
extern bool capture_opcode;
extern bool capture_memory_content;
extern bool automatic_simulation;
extern bool show_progress;
extern bool show_window;
extern bool allow_exceptions;
extern bool count_exceptions;
//extern bool multithreaded;
extern bool run_simulation;
extern int instruction_queue_size;
extern int max_data_blocks;
extern int max_time_slots;
extern int max_replacing_blocks;
extern int memory_dump_size;
extern int capture_memory_type;

void generate_header_codes(string& str, int& line);
void generate_footer_codes(string& str);

#endif
