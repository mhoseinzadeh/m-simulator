/**
 * @file main.cpp
 *
 * Some parts of this code is barrowed from Jos de Jong, <wjosdejong@gmail.com>:
 * expression_parser_cpp: Evaluates mathematical expressions to initialize
 *                        values and dimentions of parameters, constants, and
 *                        variable primary values.
 *
 *       parser.cpp  ( With some changes )
 *       error.cpp
 *       functions.cpp
 *       variablelist.cpp
 *       constant.h
 *
 * Other parts are developed by me and availale under below license:
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
 * @author     Morteza Hoseinzadeh, <m.hoseinzadeh85@gmail.com>
 * @date    2012-05-03
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
 
#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <cmath>
#ifdef __WIN32__
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

#include "syntax.h"
#include "compile.h"
#include "exception.h"
#include "fixedcodes.h"
#include "evaluation.h"

#define DEFAULT_LIBRARIES "../libs/list"

using namespace std;

void printCopyRight(bool copyright=true);
void printUsage();
void find_exe_path();
void add_default_directories();
char* process_args(int argc, char* argv[]);
string exe_path;

int main(int argc, char* argv[]) {

    bool copyright=true;
    for (int i=1; i<argc; i++) {
        if(!strcmp(argv[i], "--version") || !strcmp(argv[i], "-v")) copyright=false;
    }

    find_exe_path();
    printCopyRight(copyright);
    cpp_compiler_path = "g++";
    if(argc == 1) {
        cout << "msim-c: no input file" << endl;
        cout << "try "OPENING_QUOTE"msim-c --help"CLOSING_QUOTE" for more information" << endl << endl;
        return 0;
    }

    char* f = process_args(argc, argv);
    add_default_directories();
    for(int i=0; i<paths_count; i++) {
        cout << include_paths[i] << endl;
    }
    if(!f) return -1;
    string s = " ";
    s += f;
    s += " ";
    char * filename = new char[255];
    strcpy(filename, s.c_str());
    try {
        load_file(filename);
        parse();
    }
    catch(exception_t e) {
        report_exp(e);
    }
//    cout << "generate c file" << endl;
    generate_c_file(filename);
//    cout << "compile and link" << endl;
    compile_and_link();
    report_number_of_errors_and_warnings();
    return 0;
}

void initilize_lexer(int max_num_of_modules, int max_num_of_global_functions) {
    type_modules = new node*[max_num_of_modules];
    global_functions = new node*[max_num_of_global_functions];
}

void printCopyRight(bool copyright)
{
    cout << "     __    __" << endl;
    cout << "    /_/\\  /_/|" << endl;
    cout << "   |  \\ \\/  ||  High-Performance Computing Architectures and Networks (HPCAN)" << endl;
    cout << "   |   \\/   ||  Sharif University of Technology" << endl;
    cout << "   | |\\  /| ||  http://hpcan.ce.sharif.edu " << endl;
    cout << "   |_|/\\/ |_|/\n    Simulator" << endl << endl;
    cout << "   http://m-sim.org ( contact@m-sim.org )" << endl;
    if(copyright)
        cout << "   Copyright (c) 2014 by HPCAN, Sharif University of Technology." << endl;
    cout << endl;
}

void printLisence()
{
    cout << " ------------------------------<< Lisence  Notes >>----------------------------" << endl;

    string path = exe_path;
    path += "/../LICENSE";
    ifstream file(path.c_str());
    while (file.good()) {
        char line[1025];
        readline(file, line);
        cout << "  " << line << endl;
    }
    file.close();
                                                                               
    cout << " -------------------------------<< Contributors >>----------------------------- " << endl;
    cout << "  Designed & Programmed by Morteza Hoseinzadeh (hoseinzadeh@hotmail.com) under " << endl;
    cout << "  supervision of prof. Hamid Sarbazi-Azad." << endl << endl;
}

void printUsage()
{
    cout << "Usage: msim-c [options] file..." << endl;
    cout << "Options:" << endl;
    cout << "  --help, -h               Display this information" << endl;
    cout << "  --version, -v            Display msim-c version information" << endl;
    cout << "  --max-modules, -mm       Set the maximum number of modules" << endl;
    cout << "                             default value is 1000" << endl;
    cout << "  --max-functions, -mf     Set the maximum number of special global functons" << endl;
    cout << "                             default value is 1000" << endl;
//    cout << "  --physical-address, -pa  Use physical instead of virtual addresses" << endl;
    cout << "  --code-string, -cx       Capture code string" << endl;
    cout << "  --exception, -ex         Capture exception string" << endl;
    cout << "  --data, -d               Capture data" << endl;
    cout << "  --opcode, -op            Capture opcode" << endl;
    cout << "  --display-pipeline, -p   Add extra codes for displaying pipeline" << endl;
    cout << "  --propagate-data, -pd    Force M-Simulator to propagate accurate data using" << endl;
    cout << "                             "OPENING_QUOTE"set_data"CLOSING_QUOTE" functions. This option can be only used" << endl;
    cout << "                             with --data or -d." << endl;
    cout << "  --size-threshold, -st    Memory records size threshold (Uppers will be stored" <<endl;
    cout << "                             in hash-map)" << endl;
//    cout << "  --multi-thread, -t       Use multi-threading for simulation" << endl;
    cout << "  --queue-size, -qs <size> Set the instruction queue buffer size (only with -t)" << endl;
    cout << "                             default value is 100" << endl;
    cout << "  --data-size, -ds <size>  Set the maximum number of data transactions per each " << endl;
    cout << "                             message. default value is 8" << endl;
    cout << "  --exceptions, -e         Allow throwing exceptions" << endl;
    cout << "  --count-exceptions, -ce  Count number of all exceptions" << endl;
    cout << "  -tn <number>             Set the max number of threads (default: 16)" << endl;
    cout << "  -no-win                  Prevent Simics from opening any window." << endl;
    cout << "  -no-prog                 Prevent M-Simulator from displaying progress." << endl;
    cout << "  -no-sim                  Prevent M-Simulator from running the simulation in" << endl;
    cout << "                             automatic mode." << endl;
    cout << "  -verbose                 Turn on VERBOSE mode to enable all trace functions." << endl;
    cout << "                             You also can do it by defining an identifier " << endl;
    cout << "                             named "OPENING_QUOTE"VERBOSE"CLOSING_QUOTE" inside your code." << endl;
    cout << "  -step                    Turn on STEP mode to put stalls for debugging. " << endl;
    cout << "                             You also can do this by defining an identifier " << endl;
    cout << "                             named "OPENING_QUOTE"STEP"CLOSING_QUOTE" inside your code." << endl;
    cout << "  -mc {ram, file, pipe}    Load memory content" << endl;
    cout << "                             ram:  load all data at once using craff files" << endl;
    cout << "                             file: load data upon request from craff files" << endl;
    cout << "                             pipe: get data from simics in run-time (disabled)" << endl;
    cout << "  --dump-size, -mds <size> Memory dump size (in kilobytes) to partial loading " << endl;
    cout << "                             when -mc=file (default: 128 MB)." << endl;
    cout << "  -auto                    Automatic simulating" << endl;
    cout << "                             This argument force the compiler to generate an" << endl;
    cout << "                             automatic simulator which gets a checkpoint file" << endl;
    cout << "                             and the number of steps for simulation, starts" << endl;
    cout << "                             Simics automatically, and then pass required" << endl;
    cout << "                             commands to it. Simics will give back all cpu " << endl;
    cout << "                             transactions with memory." << endl;
    cout << "  -O1, -O2, -O3            Optimization level" << endl;
    cout << "  -g, -g1, -g2, -g3        Turn on debugging information" << endl;
    cout << "  -pg                      Generate extra code to write profile information" << endl;
    cout << "  -I <directory>           Add <directory> to the compiler's search paths" << endl;
    cout << "  -c                       Generate C++ source code, but do not compile" << endl;
    cout << "  -gcc <path>              Set the path of GNU C++ Compiler" << endl;
    cout << "  -o <file>                Place the output into <file>" << endl;
    cout << "Options -h, --help, -v, and --version will prevent the progress of compiling." << endl;
}

const char* get_name(const char* arg) {
    static string str(arg);
    size_t found = str.find_first_of("=");
    if(found!=string::npos) str = str.substr(0, found).c_str();
    return str.c_str();
}

string get_value(int argc, char* argv[], int &i, const char* msg, ...) {
    char message[128];
    va_list args;
    va_start(args, msg);
    vsprintf(message, msg, args);
    va_end(args);
    string str = argv[i];
    int found = str.find("=");
    bool error = false;
    if(found == string::npos) {
        if (argc == i) {
            error = true;
        } else
            return argv[++i];
    }
    str = str.substr(found+1).c_str();
    if(str.empty()) error = true;
    if(error) {
        cerr << message << endl;
        exit(-1);
    }
    return str;
}



char* process_args(int argc, char* argv[]) {
    int max_num_of_modules=1000, max_num_of_global_functions=1000;
    string filename(""); 
    evaluate("true=1", 0, 0);
    evaluate("false=0", 0, 0);
    bool dump_size_is_set=false;
    bool compiler_directors_set = false;
    for (int i=1; i<argc; i++) {
        string name = get_name(argv[i]);
        if (compiler_directors_set) {
            compiler_directives += argv[i];
            compiler_directives += " ";
        } else if(!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            //printCopyRight();
            printUsage();
            exit(0);
        } else if(!strcmp(argv[i], "--version") || !strcmp(argv[i], "-v")) {
            //printCopyRight();
            cout << "   Version: "MSIM_VERSION << endl;
            printLisence();
            exit(0);
        } else if(name == "--max-modules" || name == "-mm") {
            string value = get_value(argc, argv, i, "Needing a number for argument --max-modules.");
            try {
                max_num_of_modules = atoi(value.c_str());
            } catch (...) {
                cerr << "Needing a number for argument --max-modules." << endl;
                exit(-1);
            } 
        } else if(name == "--max-functions" || name == "-mf") {
            string value = get_value(argc, argv, i, "Needing a number for argument --max-functions.");
            try {
                max_num_of_global_functions = atoi(value.c_str());
            } catch (...) {
                cerr << "Needing a number for argument --max-functions." << endl;
                exit(-1);
            }
        } else if(name == "-o") {
            string value = get_value(argc, argv, i, "Missing <file> for argument -o.");
            compile_output_filename = value;
            clog << "Output file: " << value.c_str() << endl;
        } else if(!strcmp(argv[i], "-O1") || !strcmp(argv[i], "-O2") || !strcmp(argv[i], "-O3") ||
                  !strcmp(argv[i], "-g") || !strcmp(argv[i], "-g1") || !strcmp(argv[i], "-g2") ||
                  !strcmp(argv[i], "-g3") ||!strcmp(argv[i], "-pg")) {
            compile_options += argv[i];
            compile_options += " ";
        } else if(name == "-I") {
            string value = get_value(argc, argv, i, "Missing <directory> for argument -I.");
            char * filename = new char[value.length()];
            strcpy(filename, value.c_str());
            add_include_path(filename);
        } else if(name == "-gcc") {
            string value = get_value(argc, argv, i, "Missing <path> for argument -gcc.");
            cpp_compiler_path = value;
        } else if(name == "-st" || name == "--size-threshold") {
            if (argc == i) {
                cerr << "Missing <size> for argument " << argv[i] << "." << endl;
                exit(-1);
            }
            string value = get_value(argc, argv, i, "Missing <size> for argument %s.", name.c_str());
            long size = atol(value.c_str());
            if(size < 128) {
                cerr << "Unacceptable value for size threshold. Default value has been considered (" << size_threshold << ")." << endl;
            } else
                size_threshold = size;
        } else if(!strcmp(argv[i], "-t") || !strcmp(argv[i], "--multi-thread")) {
            if(!macros["STEP"].empty())
                cerr << "Multi-threading can not be used in STEP mode." << endl;
//            else
                //multithreaded = true;
        } else if(name == "-qs" || name == "--queue-size") {
            string value = get_value(argc, argv, i, "Missing <size> for argument %s.", name.c_str());
            if(!is_int(value)) 
                cerr << ""OPENING_QUOTE"" << value.c_str() << ""CLOSING_QUOTE" is not a valid integer." << endl;
            else
                instruction_queue_size = atoi(value.c_str());
        } else if(name == "-tn") {
            string value = get_value(argc, argv, i, "Missing <number> for argument %s.", name.c_str());
            long num = atol(value.c_str());
            number_of_threads = num;
        } else if(name == "--data-size" || name == "-ds") {
            string value = get_value(argc, argv, i, "Missing <number> for argument %s.", name.c_str());
            long num = atol(value.c_str());
            max_data_blocks = num+1;
        } else if(!strcmp(argv[i], "-c")) {
            link_cpp_objects = false;            
        } else if(!strcmp(argv[i], "--exceptions") || !strcmp(argv[i], "-e")) {
            allow_exceptions = true;
        } else if(!strcmp(argv[i], "--count-exceptions") || !strcmp(argv[i], "-ce")) {
            count_exceptions = true;
        } else if(!strcmp(argv[i], "-pa") || !strcmp(argv[i], "--physical-address")) {
            use_virtual_address = false;
        } else if(!strcmp(argv[i], "-cx") || !strcmp(argv[i], "--code-string")) {
            capture_code_string = true;
        } else if(!strcmp(argv[i], "-ex") || !strcmp(argv[i], "--exception-string")) {
            capture_exception_string = true;
        } else if(!strcmp(argv[i], "-d") || !strcmp(argv[i], "--data")) {
            capture_data = true;
        } else if(!strcmp(argv[i], "-pd") || !strcmp(argv[i], "--propagate-data")) {
            propagate_data = true;
        } else if(!strcmp(argv[i], "-op") || !strcmp(argv[i], "--opcode")) {
            capture_opcode = true;
        } else if(!strcmp(argv[i], "-verbose")) {
            macros["VERBOSE"] = "1";
        } else if(!strcmp(argv[i], "-step")) {
            macros["STEP"] = "1";
        } else if(!strcmp(argv[i], "-p") || !strcmp(argv[i], "--display-pipeline")) {
            macros["DISPLAY_PIPELINE"] = "1";
        } else if(!strcmp(argv[i], "-mc")) {
            if (argc == i) {
                cerr << "Missing <type> for argument " << argv[i] << "." << endl;
                exit(-1);
            }
            string value = get_value(argc, argv, i, "Missing <type> for argument %s.", name.c_str());
            if(value != "ram" && value != "file" && value != "pipe") {
                printf(OPENING_QUOTE"%s"CLOSING_QUOTE" is not a valid capture type.\n", value.c_str());
                exit(-1);
            }
            if(value == "ram")
                capture_memory_type = CAPTURE_AT_ONCE;
            else if(value == "file")
                capture_memory_type = CAPTURE_FROM_FILE;
            else
                capture_memory_type = CAPTURE_FROM_PIPE;
            capture_memory_content = true;
        } else if(!strcmp(argv[i], "-mds") || !strcmp(argv[i], "--dump-size")) {
            if (argc == i) {
                cerr << "Missing <size> for argument " << argv[i] << "." << endl;
                exit(-1);
            }
            string value = get_value(argc, argv, i, "Missing <size> for argument %s.", name.c_str());
            long size = atol(value.c_str());
            memory_dump_size = pow(2, log2(size));
            cout << "Memory dump size is set to " << memory_dump_size << " KBytes" << endl;
            dump_size_is_set = true;
        } else if(!strcmp(argv[i], "-auto")) {
            automatic_simulation = true;
        } else if(!strcmp(argv[i], "-no-win")) {
            show_window = false;
        } else if(!strcmp(argv[i], "-no-prog")) {
            show_progress = false;
        } else if(!strcmp(argv[i], "-no-sim")) {
            run_simulation = false;
        } else if(argv[i][0] == '-') {
            name = name.substr(1);
            if(!variable_exists(name.c_str())) {
                cerr << "Undefined switch -" << name << ". Trying to initiate global parameter "OPENING_QUOTE"" << name << ""CLOSING_QUOTE"." << endl;
                //exit(-1);
            } 
            {
                string str = get_value(argc, argv, i, "Missing <value> for parameter %s.", name.c_str());
                int i=0;
                string value = "";
                string part=next(str, i);
                try {
                    while(1) {
                        value += part+" ";
                        part = next(str, i);
                    }
                }
                catch(exception_t e) {}
                value = name+" = "+value;
                cout << "Parameter "OPENING_QUOTE"" << name.c_str() << ""CLOSING_QUOTE" is set to " << evaluate(value.c_str(), 0, 0) << endl;
                
            }
        } else {
            compiler_directors_set = true;
            filename = argv[i];
        }
    }
    if(propagate_data && !capture_data) {
        cerr << "Propagate data (-pd) does not work without capturing data (-d). It has been switched on automatically." << endl;
        capture_data = true;
    }
    
    if((allow_exceptions && !capture_exception_string) || !macros["STEP"].empty()) {
        cerr << "Capturing exception string (-ex) is switched on automatically." << endl;
        capture_exception_string = true;
    }
    
    if((!capture_memory_content || capture_memory_type!=CAPTURE_FROM_FILE) && dump_size_is_set) {
        clog << "Capturing memory content (-mc) is set to "OPENING_QUOTE"file"CLOSING_QUOTE" automatically." << endl;
        capture_memory_type = CAPTURE_FROM_FILE;
        capture_memory_content = true;
    }
    
    if(capture_memory_content && !automatic_simulation) {
        cerr << "Capturing memory option (-mc) can be used only in automatic mode." << endl;
        capture_memory_content = false;
    }
    
    if(capture_memory_content && !capture_data) {
        clog << "Capturing data (-d) is switched on automatically." << endl;
        capture_data = true;
    }
        
    char* f = new char[filename.length()+1];
    strcpy(f, filename.c_str());
    initilize_lexer(max_num_of_modules, max_num_of_global_functions);
    return f;
}

#ifdef __WIN32__
std::string getexepath()
{
#ifdef QT_CORE_LIB
    return "D:\\WorkSpace\\M-Simulator\\src\\m-simulator\\msim-c.exe";
#else
    char result[ MAX_PATH ];
    return std::string( result, GetModuleFileName( NULL, result, MAX_PATH ) );
#endif
}
#else
std::string getexepath()
{
    char result[ PATH_MAX ];
    ssize_t count = readlink( "/proc/self/exe", result, PATH_MAX );
    return std::string( result, (count > 0) ? count : 0 );
}
#endif

void find_exe_path() 
{
    exe_path = getexepath();
    char* folderpath = new char[exe_path.length()];
    strcpy(folderpath, exe_path.c_str());
    for(int i=exe_path.length(); i>0; i--) {
        if(folderpath[i] == '/' || folderpath[i] == '\\') {
            folderpath[i] = '\0';
            break;
        }
    } 
    exe_path = folderpath;
}

void add_default_directories() {
    string exe = exe_path;
    string libs_dir = exe;
    #ifdef __WIN32__
    exe += "\\"DEFAULT_LIBRARIES;
    libs_dir += "\\..\\libs";
    #else
    exe += "/"DEFAULT_LIBRARIES;
    libs_dir += "/../libs";
    #endif
    add_include_path((char*)(libs_dir.c_str()));
    if (!file_exists(exe.c_str())) {
        //delete[] filename;
        return;
    }
    
    ifstream file(exe.c_str());
    while (file.good()) {
        char line[1025];
        readline(file, line);
        add_include_path(line);
    }
    file.close();
    //delete[] filename;
}

