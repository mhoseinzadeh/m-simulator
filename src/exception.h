#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <iostream>
#include <list>
#include <string>
#include <cstring>
#include <stdarg.h>
using namespace std;


typedef enum {
        etFatalError, etSyntaxError, etFileError, etWarning, etCompile, etNote
} exp_type;

typedef struct {
        char msg[2048];
        char filename[512];
        exp_type type;
        int line;
} exception_t;

extern int warnings_count;
extern int errors_count;
extern bool reporting;
extern list<char*> msim_messages;

exception_t exp_gen(exp_type type, int line, const char* filename, const char* fmt, ...);

void turn_off_reporting();
void turn_on_reporting();
void report_exp(exception_t e);
void report_exp(const char* msg, int line, char* file);
void report_number_of_errors_and_warnings();

#endif

