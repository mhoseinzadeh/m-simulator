#include "exception.h"
#include <cstdio>
#include <cstdarg>

using namespace std;

int warnings_count = 0;
int errors_count = 0;
bool reporting = true;
list<char*> msim_messages;

exception_t exp_gen(exp_type type, int line, const char* filename, const char* fmt, ...) {
    char msg[1024];
    va_list argptr;
    va_start(argptr, fmt);
    vsprintf(msg, fmt, argptr);
    va_end(argptr);
    exception_t e;
    msg[1023] = '\0';
    strcpy(e.msg, msg);
    strcpy(e.filename, filename);
    e.line = line;
    e.type = type;
    return e;
}

void turn_off_reporting() {
    reporting = false;
}

void turn_on_reporting() {
    reporting = true;
}

void report_exp(exception_t e) {
    if(!reporting)
        return;
    e.line++;
    char msg[2048] = "";

    char * path = new char[strlen(e.filename) + 1];
    strcpy(path, e.filename);
    switch(e.type) {
    case etFatalError: sprintf(msg, "*FATAL Error*: %s", e.msg); break;
    case etNote: sprintf(msg, "*Note*: %s", e.msg); break;
    case etFileError: sprintf(msg, "*I/O Error* \"%s\" (%d): %s", path, e.line, e.msg); break;
    case etSyntaxError: sprintf(msg, "*Error* \"%s\" (%d): %s", path, e.line, e.msg); break;
    case etWarning: sprintf(msg, "*Warning* \"%s\" (%d): %s", path, e.line, e.msg); break;
    case etCompile: sprintf(msg, "*C++ Error* \"%s\" (%d): %s", path, e.line, e.msg); break;
    }
    for (list<char*>::iterator ln = msim_messages.begin(); ln != msim_messages.end(); ln++) {
        if (!strcmp(*ln, msg))
            return;
    }
    if (e.type == etWarning)
        warnings_count++;
    else if (e.type != etNote)
        errors_count++;
    cout << msg << endl;
    char *msg_str = new char[strlen(msg)+1];
    strcpy(msg_str, msg);
    msim_messages.push_back(msg_str);
}

void report_exp(const char* msg, int line, char* file) {
    report_exp(exp_gen(etSyntaxError, line, file, msg));
}

void report_number_of_errors_and_warnings() {
    if (errors_count > 0)
        cout << "Compiling failed with ";
    else
        cout << "Compiling accomplished with ";
    cout << errors_count << " error(s) and " << warnings_count << " warning(s)." << endl;
}
