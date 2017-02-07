#include "compile.h"
#include "fixedcodes.h"
#include <cmath>

bool capture_code_string = false;
bool capture_exception_string = false;
bool capture_data = false;
bool propagate_data = false;
bool capture_opcode = false;
bool capture_memory_content = false;
bool automatic_simulation = false;
bool show_window = true;
bool show_progress = true;
bool run_simulation = true;
bool multithreaded = false;
bool allow_exceptions = false;
bool count_exceptions = false;
int number_of_threads = 16;
int instruction_queue_size = 10;
int max_data_blocks = 9;
int max_time_slots=64;
int max_replacing_blocks=64;
int memory_dump_size=131072;
int capture_memory_type=CAPTURE_AT_ONCE;

int number_of_bytes_per_instruction = 4;

using namespace std;

int extract_lines_count(string str) {
    int cnt = 1;
    for (unsigned int i=0; i<str.length(); i++)
        if (str[i] == '\n')
            cnt++;
    return cnt;
}

void generate_transaction_class(string& str) {
    str += "#include <iostream>\n";
    str += "#include <iomanip>\n";
    str += "#include <fstream>\n";
    str += "#include <cstring>\n";
    str += "#include <cstdio>\n";
    str += "#include <cstdlib>\n";
    str += "#include <cstdarg>\n";
    str += "#include <cmath>\n";
    str += "#include <signal.h>\n";
    str += "#include <set>\n";
    /*
    str += "#include <sys/types.h>\n";
    str += "#include <sys/stat.h>\n";

    str += "\ntime_t get_mtime(const char *path) {\n";
    str += "    struct stat statbuf;\n";
    str += "    if (stat(path, &statbuf) == -1) {\n";
    str += "        perror(path);\n";
    str += "        exit(1);\n";
    str += "    }\n";
    str += "    return statbuf.st_mtime;\n";
    str += "}\n";
    */

    if(multithreaded) {
        str += "#include <pthread.h>\n";
        str += "#include <cstdlib>\n";
    }
    str += "#include <map>\n";
    str += "#include <time.h>\n";
    str += "#include <sstream>\n";
    str += "#include <string>\n\n";
#ifdef __WIN32__
    str += "#include <conio.h>\n";
    str += "#include <windows.h>\n";
#else
    str += "#include <sys/ioctl.h>\n";
    str += "#include <termios.h>\n";
    str += "#include <stdio.h>\n";
    str += "\n";

    if(!macros["STEP"].empty())
        str += "#define STEP\n";
    if(!macros["VERBOSE"].empty())
        str += "#define VERBOSE\n";
    if(automatic_simulation)
        str += "#define AUTOMATIC_SIMULATION\n";
    if(allow_exceptions)
        str += "#define ALLOW_EXCEPTIONS\n";
    if(show_progress)
        str += "#define SHOW_PROGRESS\n";
    if(use_virtual_address)
        str += "#define USE_VIRTUAL_ADDRESS\n";
    str += "#define CAPTURE_AT_ONCE 0\n";
    str += "#define CAPTURE_FROM_FILE 1\n";
    str += "#define CAPTURE_FROM_PIPE 2\n";
    str += "#define CAPTURE_MEMORY_TYPE ";
    switch(capture_memory_type) {
        case CAPTURE_AT_ONCE: str += "CAPTURE_AT_ONCE\n"; break;
        case CAPTURE_FROM_FILE: str += "CAPTURE_FROM_FILE\n"; break;
        case CAPTURE_FROM_PIPE: str += "CAPTURE_FROM_PIPE\n"; break;
    }
    str += "#define INSTRUCTION_QUEUE_SIZE ";
    str += itoa(instruction_queue_size);
    str += "\n";
    str += "#define MAX_DATA_BLOCKS ";
    str += itoa(max_data_blocks);
    str += "\n";

    str += "\n";
    str += "static struct termios __old_termios, __new_termios;\n";
    str += "\n";
    str += "void initTermios(int echo) {\n";
    str += "    tcgetattr(0, &__old_termios); \n";
    str += "    __new_termios = __old_termios; \n";
    str += "    __new_termios.c_lflag &= ~ICANON; \n";
    str += "    __new_termios.c_lflag &= echo ? ECHO : ~ECHO; \n";
    str += "    tcsetattr(0, TCSANOW, &__new_termios);\n";
    str += "}\n";
    str += "\n";
    str += "void resetTermios(void) {\n";
    str += "    tcsetattr(0, TCSANOW, &__old_termios);\n";
    str += "}\n\n";
    str += "int terminal_width() {\n";
    str += "    struct winsize w;\n";
    str += "    ioctl(0, TIOCGWINSZ, &w);\n";
    str += "    return w.ws_col;\n";
    str += "}\n";
    str += "char getch_(int echo) {\n";
    str += "  char ch;\n";
    str += "  initTermios(echo);\n";
    str += "  ch = getchar();\n";
    str += "  resetTermios();\n";
    str += "  return ch;\n";
    str += "}\n";
    str += "\n";
    str += "char getch(void) {\n";
    str += "  return getch_(0);\n";
    str += "}\n";
    str += "\n";
    str += "char getche(void) {\n";
    str += "  return getch_(1);\n";
    str += "}\n";
#endif
    str += "#define int8  char\n";
    str += "#define int16 short\n";
    str += "#define int32 long\n";
    str += "#define int64 long long\n";
    str += "#define uint8  unsigned char\n";
    str += "#define uint16 unsigned short\n";
    str += "#define uint32 unsigned long\n";
    str += "#define uint64 unsigned long long\n\n";
    str += "#define _address unsigned long long\n";
    str += "#define byte unsigned char\n";
    str += "#define _data byte\n";
    str += "#define bit  bool\n";
    str += "#define boolean  bool\n";
    str += "\n";
    str += "using namespace std;\n";
    str += "\n";
    str += "typedef struct {\n";
    str += "    double start, end;\n";
    str += "    bool valid;\n";
    str += "} timeslot_t;\n";
    str += "\n";
    str += "typedef struct {\n";
    str += "    uint64 block_addr;\n";
    str += "    double end;\n";
    str += "    bool valid;\n";
    str += "} att_entry_t;\n";
    str += "\n";
    str += "bool simulation_finished = false;\n";
    str += "bool __redirecting_cout = false;\n";
    str += "bool __redirecting_progress = false;\n";
    str += "bool __one_trace_performed = false;\n";
    str += "bool __application_terminated = false;\n";
    str += "uint64 iterations = 0; \n";
    //    str += "uint64* cycles = NULL; \n";
    str += "uint16 cpu_count = 0; \n";
    str += "double latency; \n";
    str += "double __machine_time=0; \n";
    str += "double* cpu_time; \n";
    //    str += "double elapsed_time = 0; \n";
    str += "double energy = 0; \n";
    str += "double frequency = 0; \n";
    str += "uint64 __current_value = 0;\n";
    str += "uint64 __sim_length = 0;\n";
    str += "int access_type = 0; \n";
    str += "int cpu_number = 0; \n";
    str += "int transaction_size = 4; \n";
    str += "uint64 address = 0;\n";
    str += "uint32 offset = 0; \n";
    str += "uint64 sim_step = 0;\n";
    str += "uint64 total_steps = 0;\n";
    str += "uint64* cpu_steps;\n";
    str += "uint32 exceptions = 0; \n";
    str += "uint64 memory_size = 0; \n";
    str += "uint64 unmatched = 0; \n";
    str += "uint64 matched = 0; \n";
    str += "uint32 memory_address_align = ";
    str += itoa((int)(log2(memory_dump_size)+10));
    str += ";\n";
    if(capture_memory_type == CAPTURE_FROM_FILE)
        str += "set<uint64> __added_memory_dumps;\n";
    str += "\n\n";
    str += "class typical_module;\n";
    str += "typical_module* __main_memory = NULL;\n";
    //    str += "double* instruction_injecting_time=NULL;\n";
    if(!macros["STEP"].empty() || !macros["VERBOSE"].empty() || allow_exceptions) {
        str += "string __transaction_string;\n";
    }
    str += "bool __tracing = true;\n";
    str += "bool __itracing = true;\n";
    if(!macros["DISPLAY_PIPELINE"].empty())
        str += "ofstream __pipeline_file;\n";
    str += "ofstream __progress_file;\n";
    str += "string __progress_file_path, __report_file_path;\n";
    str += "\n";
    str += "typedef enum {ttInstFetch=0, ttDataRead=1, ttDataWrite=2, ttException=3, ttAddToTLB=4, ttRemoveFromTLB=5, ttNone=6} transaction_type_t;\n";
    str += "typedef enum {itNone=0, itALU=1, itBranch=2, itLoad=4, itStore=8, itUndefined=16} message_type_t;\n";
    str += "\n";
    str += "typedef struct {\n";
    str += "    unsigned char cpu_num;\n";
    str += "    transaction_type_t type;\n";
    str += "    uint64 virtual_address;\n";
    str += "    uint64 physical_address;\n";
    if (capture_code_string) {
        str += "    string code;\n";
    }
    if (capture_exception_string) {
        str += "    string exp_str;\n";
    }
    str += "    unsigned char size;\n";
    str += "    union\n";
    str += "    {\n";
    if (capture_data || capture_memory_content) {
        str += "        uint64 data;\n";
    }
    if (capture_opcode) {
        str += "        uint64 opcode;\n";
    }
    str += "        uint64 exp_code;\n";
    str += "    };\n";
    str += "} transaction_t;\n";
    str += "\n";
    str += "typedef struct {\n";
    str += "    message_type_t type;\n";
    //str += "    transaction_t inst;\n";
    str += "    uint16 data_count;   // ready to extract\n";
    str += "    transaction_t data[";
    str += itoa(max_data_blocks);
    str += "];   // for load and store instructions\n";
    str += "    transaction_t tlb[2];   // for tlb update transactions\n";
    str += "    uint64 target_address;   // for branch instructions\n";
    str += "    bool ready;   // ready to extract\n";
    str += "    uint16 size;   // number of data transactions\n";
    str += "    uint8 tlb_size;   // number of tlb transactions\n";
    if(!macros["STEP"].empty() || !macros["VERBOSE"].empty() || allow_exceptions) {
        str += "    string __data_inst_string[";
        str += itoa(max_data_blocks);
        str += "];\n";
        str += "    string __inst_inst_string;\n";
    }
    str += "} message_t;\n\n";
    str += "transaction_t __current_transaction;\n";
    str += "message_t* cpu_message;\n\n";
    str += "template <typename type, int __size>\n";
    str += "class rrqueue_t {\n";
    str += "private:\n";
    str += "    type data[__size];\n";
    str += "    int _head, _tail;\n";
    str += "public:\n";
    str += "    rrqueue_t() { _head = 0; _tail = 0; }\n";
    str += "    void clear() { _head = 0; _tail = 0; }\n";
    str += "    bool full() { return (_tail+1 == _head) || (_tail == __size-1 && _head == 0); }\n";
    str += "    bool empty() { return _head == _tail; }\n";
    str += "    int size() { return _tail-_head+((_tail < _head)?__size:0); }\n";
    str += "    type& first() { return data[_head]; }\n";
    str += "    type& last() { if(_tail > 0) return data[_tail-1]; else return data[__size-1]; }\n";
    str += "    void push_front(type val) { if(_head > 0) data[(--_head)] = val; else data[(_head = __size-1)] = val; }\n";
    str += "    void push_back(type val) { if(_tail < __size-1) data[(_tail++)] = val; else {data[__size-1] = val; _tail = 0;} }\n";
    str += "    type& pop_front() { if(_head < __size-1) return data[(_head++)]; else {_head = 0; return data[__size-1];}; }\n";
    str += "    type& pop_back() { if(_tail > 0) return data[(--_tail)]; else return data[(_tail = __size-1)]; }\n";
    str += "    type& operator[](int index) { return data[ (_head+index)%__size ]; }\n";
    str += "};  \n\n";
    
    str += "template <int __q_size>\n";
    str += "class instructions_t{\n";
    str += "private:\n";
    str += "    rrqueue_t<message_t, __q_size> *q;\n";
    str += "public:\n";
    str += "    void set_len(int cpu_count) { q = new rrqueue_t<message_t, __q_size>[cpu_count]; }\n";
    str += "    rrqueue_t<message_t, __q_size>& operator[](int cpu_index) {\n";
    str += "        return dynamic_cast< rrqueue_t<message_t, __q_size>& >(q[cpu_index]);\n";
    str += "    }\n";
    str += "};\n\n";
    str += "instructions_t<";
    str += itoa(instruction_queue_size);
    str += "> instructions;\n";
    
    str += "inline uint64 cycle() {\n";
    str += "    return (uint64)((latency)*frequency);\n";
    str += "}\n\n";
    
    str += "inline double elapsed_time() {\n";
    str += "    return latency;\n";
    str += "}\n\n";
    
    str += "inline double system_time() {\n";
    str += "    return __machine_time;\n";
    str += "}\n\n";
    
    str += "inline double machine_time() {\n";
    str += "    return __machine_time;\n";
    str += "}\n\n";
    
    str += "inline uint64 total_latency() {\n";
    str += "    return (uint64)((__machine_time)*frequency);\n";
    str += "}\n\n";
    
    str += "double find_first_free_timeslot(double _min, double _max, double len, timeslot_t* btst) {\n";
    str += "    static double one_cycle = 1/frequency;\n";
    str += "    if(_max <= _min) return _min;\n";
    str += "    if(_max - _min < len) return _max;\n";
    str += "    for(int i=0; i<";
    str += itoa(max_time_slots);
    str += "; i++) \n";
    str += "        if(btst[i].valid)\n";
    str += "            if((btst[i].start >= _min && btst[i].start < _min+len) || (btst[i].end > _min && btst[i].end <= _min+len))\n";
    str += "                return find_first_free_timeslot(btst[i].end+one_cycle, _max, len, btst);\n";
    str += "    return _min;\n";
    str += "}\n\n";

    str += "int add_busy_timeslot(double start, double end, timeslot_t* btst, int &index) {\n";
    str += "    index = (index+1)%";
    str += itoa(max_time_slots);
    str += ";\n";
    str += "    btst[index].valid = true;\n";
    str += "    btst[index].start = start;\n";
    str += "    btst[index].end = end;\n";
    str += "    return index;\n";
    str += "}\n\n";
    
    str += "double time_to_availability(double time, _address block_address, att_entry_t* att) {\n";
    str += "    double result = time;\n";
    str += "    for(int i=0; i<";
    str += itoa(max_replacing_blocks);
    str += "; i++) {\n";
    str += "        if(att[i].block_addr == block_address)\n";
    str += "            if(att[i].end > result)\n";
    str += "                result = att[i].end;\n";
    str += "    }\n";
    str += "    return result;\n";
    str += "}\n\n";

    str += "void add_unavailable_timeslot(att_entry_t* att, _address block_address, double end, int &index) {\n";
    str += "    index = (index+1)%";
    str += itoa(max_replacing_blocks);
    str += ";\n";
    str += "    att[index].block_addr = block_address;\n";
    str += "    att[index].end = end;\n";
    str += "}\n\n";
    if(automatic_simulation)
        str += "void __load_memory_content(uint64 addr, uint64 last_addr=0);\n";
    str += "void print_meaningful_section(transaction_t t, const char* buf, bool new_line=true);\n";

/*    str += "int digit(char c) {\n";
    str += "    if(c>='0'&&c<='9') return c-'0';\n";
    str += "    if(c>='a'&&c<='f') return 10+c-'a';\n";
    str += "    return -1;\n";
    str += "}\n\n";

    str += "uint64 ullabs(long long val) {\n";
    str += "    if(val < 0)\n";
    str += "        return -val;\n";
    str += "    return val;\n";
    str += "}\n\n";
    
    str += "inline uint64 hex2dec(string str) {\n";
    str += "    uint64 res=0;\n";
    str += "    if(str.length()==0)\n";
    str += "        return 0;\n";
    str += "    int i=0;\n";
    str += "    while((str[i] == '0' || str[i] == 'x') && (i < str.length())) i++;\n";
    str += "    for(; digit(str[i])>-1; i++)\n";
    //    str += "        if(str[i] != ' ')\n";
    str += "         res = res*16+digit(str[i]);\n";
    str += "    return res;\n";
    str += "}\n\n";
    
    str += "inline uint64 str2dec(string str) {\n";
    str += "    uint64 res=0;\n";
    str += "    if(str.length()==0)\n";
    str += "        return 0;\n";
    str += "    for(int i=0; digit(str[i])>-1; i++)\n";
    //    str += "        if(str[i] != ' ')\n";
    str += "        res = res*10+digit(str[i]);\n";
    str += "    return res;\n";
    str += "}\n\n";
    
    str += "void extract_transaction_data(string &str, transaction_t &t) {\n";
    str += "    std::size_t c_start = str.find(\"CPU\");\n";
    str += "    if (c_start!=std::string::npos) {\n";
    str += "        c_start += 3;\n";
    str += "        std::size_t c_end = str.find(\"<\", c_start+1);\n";
    str += "        char cpu_b[20] = {0};\n";
    str += "        str.copy(cpu_b, c_end-c_start, c_start);\n";
    str += "        t.cpu_num = strtol(cpu_b, NULL, 0);\n";
    str += "    }\n";
    str += "    std::size_t v_start = str.find(\"<v:\");\n";
    str += "    if (v_start!=std::string::npos) {\n";
    str += "        v_start += 3;\n";
    str += "        std::size_t v_end = str.find(\">\", v_start+1);\n";
    str += "        char v_addr_b[20] = {0};\n";
    str += "        str.copy(v_addr_b, v_end-v_start, v_start);\n";
    str += "        t.virtual_address = strtoull(v_addr_b, NULL, 0);\n";
    str += "    }\n";

    str += "    std::size_t p_start = str.find(\"<p:\");\n";
    str += "    if (p_start!=std::string::npos) {\n";
    str += "        p_start += 3;\n";
    str += "        std::size_t p_end = str.find(\">\", p_start+1);\n";
    str += "        char p_addr_b[20] = {0};\n";
    str += "        str.copy(p_addr_b, p_end-p_start, p_start);\n";
    str += "        t.physical_address = strtoull(p_addr_b, NULL, 0);\n";
    str += "    }\n";
    str += "}\n\n";
    str += "void extract_tlb_transaction_data(string &str, transaction_t &t)\n";
    str += "{\n";
    str += "    char buff[64];\n";
    str += "    str.copy(buff, 57, 54);\n";
    str += "    istringstream iss(buff);\n";
    str += "    uint64 v=0, p=0;\n";
    str += "    int token_num = 0;\n";
    str += "    do {\n";
    str += "        token_num++;\n";
    str += "        string sub;\n";
    str += "        iss >> sub;\n";
    str += "        if(token_num < 6 && token_num > 1) {\n";
    str += "            v = (v<<16) + strtoull(sub.c_str(), NULL, 16);\n";
    str += "        } else if(token_num > 9 && token_num < 14) {\n";
    str += "            p = (p<<16) + strtoull(sub.c_str(), NULL, 16);\n";
    str += "        }\n";
    str += "    } while (iss);\n";
    str += "    t.virtual_address = v;\n";
    str += "    t.physical_address = p;\n";
    str += "}\n\n";

    str += "int __last_transaction_cpu_num=0;\n";
    str += "transaction_t parse(string line)\n";
    str += "{\n";
    if(!macros["STEP"].empty() || !macros["VERBOSE"].empty() || allow_exceptions)
        str += "    __transaction_string = line;\n";
    str += "    transaction_t t;\n";
    str += "    t.type = ttNone;\n";
    str += "    if(line.length() < 10) return t;\n";
    str += "    std::size_t found = line.find(\"inst:\");\n";
    str += "    if (found!=std::string::npos) {\n";
    str += "        t.type = ttInstFetch;\n";
    str += "        extract_transaction_data(line, t);\n";
    str += "        __last_transaction_cpu_num = t.cpu_num;\n";
    str += "        if(t.cpu_num == 0) __current_value++;\n";
    str += "    } else {\n";
    str += "        found = line.find(\"data:\");\n";
    str += "        if (found!=std::string::npos) {\n";
    str += "            t.type = ttDataRead;\n";
    str += "            extract_transaction_data(line, t);\n";
    str += "            std::size_t t_start = line.find(\"Read\");\n";
    str += "            if (t_start==std::string::npos) {\n";
    str += "                t.type = ttDataWrite;\n";
    str += "                t_start = line.find(\"Write\");\n";
    str += "                t_start += 5;\n";
    str += "            } else\n";
    str += "                t_start += 4;\n";
    str += "            std::size_t t_end = line.find(\"bytes\", t_start+1);\n";
    str += "            char tag_b[20] = {0};\n";
    str += "            line.copy(tag_b, t_end-t_start, t_start);\n";
    str += "            t.size = strtol(tag_b, NULL, 0);\n";
    str += "        } else {\n";
    str += "            found = line.find(\"cheetah-mmu\");\n";
    str += "            if (found!=std::string::npos) {\n";
    str += "                t.cpu_num = __last_transaction_cpu_num;\n";
    str += "                extract_tlb_transaction_data(line, t);\n";
    str += "                found = line.find(\"adding\", found+11);\n";
    str += "                if (found!=std::string::npos) {\n";
    str += "                    t.type = ttAddToTLB;\n";
    str += "                } else {\n";
    str += "                    found = line.find(\"remove\", found+11);\n";
    str += "                    if (found!=std::string::npos) {\n";
    str += "                        t.type = ttRemoveFromTLB;\n";
    str += "                    }\n";
    str += "                }\n";
    str += "            } else {\n";
    str += "                found = line.find(\"exce:\");\n";
    str += "                if (found!=std::string::npos) {\n";
    str += "                    t.type = ttException;\n";
    str += "                } else\n";
    str += "                    return t;\n";
    str += "            }\n";
    str += "        }\n";
    str += "    }\n";
    str += "    return t;\n";
    str += "}\n";

*/

    /*
    str += "transaction_t parse(string line)\n";
    str += "{\n";
    if(!macros["STEP"].empty() || !macros["VERBOSE"].empty() || allow_exceptions)
        str += "    __transaction_string = line;\n";
    str += "    transaction_t t;\n";
    str += "    t.type = ttNone;\n";
    str += "    if(line.empty()) {\n";
    if(!automatic_simulation)
    str += "        simulation_finished = true;\n";
    str += "        return t;\n";
    str += "    }\n";
    str += "    if(line.length() < 10) return t;\n";
    str += "    char idx = 1;\n";
    str += "    char c = line[1];\n";
    str += "    if(c != 'i' && c != 'd' && c != 'e') c = line[(idx=0)];\n";
    str += "    if(line[idx+4] != ':') return t;\n";
    if(!automatic_simulation)
    str += "    if(c == '[') simulation_finished = true;\n";
    str += "    if(c != 'i' && c != 'd' && c != 'e') {\n";
    if(!automatic_simulation)
    str += "        simulation_finished = true;\n";
    str += "        return t;\n";
    str += "    }\n";
    str += "    string s = \"\", l = \"1\";\n";
    str += "    t.type = ttDataRead;\n";
    str += "    int i=0,m=0;\n";
    str += "    if(c == 'i')\n";
    str += "        t.type = ttInstFetch;\n";
    str += "    else if(c == 'e')\n";
    str += "        t.type = ttException;\n";
    str += "    COPY_TAG(line, 7, ']');\n";
    str += "    \n";
    str += "    if(t.type == ttException)\n";
    str += "        COPY_TAG(line, m+5, 'e')\n";
    str += "    else\n";
    str += "        COPY_TAG(line, m+5, '<');\n";
    str += "    t.cpu_num=(unsigned char) str2dec(s);\n";
    str += "    cpu_number = t.cpu_num;\n";
    str += "    if(t.cpu_num == 0 && c == 'i') __current_value++;\n";
    str += "\n";
    str += "    if(t.type == ttException)\n";
    str += "    {\n";
    str += "        nextStr(line,m+1);\n";
    str += "        nextStr(line,m+1);\n";
    str += "        t.exp_code = (unsigned int) str2dec(s);\n";
    if (capture_exception_string) {
        str += "        COPY(line, m+1);\n";
        str += "        t.exp_str = s;\n";
    }
    str += "        return t;\n";
    str += "    }\n";
    str += "\n";
    str += "    COPY_TAG(line, m+4, '>');\n";
    str += "    t.virtual_address = hex2dec(s);\n";
    str += "\n";
    str += "    COPY_TAG(line, m+7, '>');\n";
    str += "    t.physical_address = hex2dec(s);\n";
    str += "\n";
    str += "    if(t.type != ttInstFetch && t.type != ttException)\n";
    str += "    {\n";
    str += "        while(1)\n";
    str += "        {\n";
    str += "            nextStr(line,m+1);\n";
    str += "            if(!s.compare(\"Read\"))\n";
    str += "                t.type = ttDataRead;\n";
    str += "            if(!s.compare(\"Write\"))\n";
    str += "                t.type = ttDataWrite;\n";
    str += "            if(!s.compare(\"bytes\"))\n";
    str += "            {\n";
    str += "                t.size = (unsigned char) str2dec(l);\n";
    str += "                break;\n";
    str += "            }\n";
    str += "            l = s;\n";
    str += "        }\n";
    if (capture_data || capture_memory_content)  {
        str += "        COPY_TAG(line, m+4, ' ');\n";
        str += "        t.data = hex2dec(s);\n";
    }
    str += "    }\n";
    str += "    else if(t.type == ttInstFetch)\n";
    str += "    {\n";
    str += "        t.size = ";
    str += itoa(number_of_bytes_per_instruction);
    str += ";\n";
    str += "        COPY_TAG(line, m+2, ' ');\n";
    if (capture_opcode) {
        str += "        t.opcode = (uint64) hex2dec(s);\n";
        if(capture_data || capture_memory_content) {
            str += "        t.data = t.opcode;\n";
        }
    } else if (capture_data || capture_memory_content) {
        str += "        t.data = (uint64) hex2dec(s);\n";
    }
    if (capture_code_string) {
        str += "        COPY(line, m+1);\n";
        str += "        t.code = s;\n";
    }
    str += "    }\n";
    str += "    return t;\n";
    str += "}\n\n";


*/

    str += "void __remove_char(char* str, char ch) {\n";
    str += "    for(int i=0; str[i]; i++)\n";
    str += "        if(str[i] == ch) {\n";
    str += "            for(int j=i; str[j]; j++)\n";
    str += "                str[j] = str[j+1];\n";
    str += "        }\n";
    str += "}\n\n";

    str += "void __top_level_step();\n\n";
    
    str += "template <typename K, typename V>\n";
    str += "class dmap: public map<K, V> {\n";
    str += "private:\n";
    str += "    V default_value;\n";
    str += "public:\n";
    str += "    dmap<K, V>() {}\n";
    str += "    V& operator[](const K& key) {\n";
    str += "        typename map <K, V> :: iterator it = this->find( key );\n";
    str += "        if ( it == this->end() ) {\n";
    str += "             pair<typename map <K, V> :: iterator, bool> __i = this->insert(pair<K, V>(key, default_value));\n";
    str += "             (*this)[key] = default_value;\n";
    str += "            return __i.first->second;\n";
    str += "        } else {\n";
    str += "            return it->second;\n";
    str += "        }\n";
    str += "    }\n";
    str += "    void set_default(V def) {\n";
    str += "        this->default_value = def;\n";
    str += "    }\n";
    str += "};\n\n";
#ifdef __WIN32__
    str += "#include <windows.h>\n";
    str += "HANDLE hstdout;\n";
    str += "CONSOLE_CURSOR_INFO CCI;\n";
    str += "CONSOLE_SCREEN_BUFFER_INFO csbi;\n";
    str += "int terminal_width() {\n";
    str += "    return csbi.dwSize.X;\n";
    str += "}\n";
#endif
    if(!macros["VERBOSE"].empty() || !macros["STEP"].empty() || allow_exceptions) {
        str += "#include <set>\n";
#ifdef __WIN32__
        str += "char __convert_c(unsigned char c) {\n";
        str += "    if(c == 4) return c-3;\n";
        str += "    if(c == 1) return c+3;\n";
        str += "    if(c == 3) return c+3;\n";
        str += "    if(c == 6) return c-3;\n";
        str += "    return c;\n";
        str += "}\n";
        str += "void __print_linux_escape_char_sequence(const char* msg) {\n";
        str += "    int j = 0;\n";
        str += "    for(int i = 0; msg[i]; i++) {\n";
        str += "        if(msg[i] == '\\e') {\n";
        str += "            int color = 0;\n";
        str += "            unsigned char ccolor = 0;\n";
        str += "            int color_idx = 0;\n";
        str += "            bool bright = true;\n";
        str += "            bool cursor_info = false;\n";
        str += "            for(i++; (msg[i] <= '9' && msg[i] >= '0') || msg[i] == '[' || msg[i] == ';' || msg[i] == '?'; i++) {\n";
        str += "                if(msg[i] == '[' || msg[i] == ';' || msg[i] == '?') {\n";
        str += "                    if(color >= 30 && color <= 37) {\n";
        str += "                        ccolor |= __convert_c(color-30);\n";
        str += "                    } else if(color >= 40 && color <= 47) {\n";
        str += "                        ccolor |= 16*__convert_c(color-40);\n";
        str += "                    } else if(color == 2) {\n";
        str += "                        bright = false;\n";
        str += "                    }\n";
        str += "                    if(msg[i] == '?')\n";
        str += "                        cursor_info = true;\n";
        str += "                    color = 0;\n";
        str += "                    color_idx++;\n";
        str += "                } else {\n";
        str += "                    color = color*10+(msg[i]-'0');\n";
        str += "                }\n";
        str += "            }\n";
        str += "            if(cursor_info) {\n";
        str += "                if(msg[i] == 'l') {\n";
        str += "                    CCI.bVisible = false; \n";
        str += "                    SetConsoleCursorInfo(hstdout, &CCI); \n";
        str += "                } else if(msg[i] == 'h') {\n";
        str += "                    CCI.bVisible = true; \n";
        str += "                    SetConsoleCursorInfo(hstdout, &CCI); \n";
        str += "                }\n";
        str += "            } else {\n";
        str += "                if(color >= 30 && color <= 37) {\n";
        str += "                    ccolor |= __convert_c(color-30);\n";
        str += "                } else if(color >= 40 && color <= 47) {\n";
        str += "                    ccolor |= 16*__convert_c(color-40);\n";
        str += "                } else if(color == 2) {\n";
        str += "                    bright = true;\n";
        str += "                }\n";
        str += "                if(ccolor == 0) \n";
        str += "                    SetConsoleTextAttribute( hstdout, csbi.wAttributes );\n";
        str += "                else {\n";
        str += "                    if(bright) ccolor += 8;\n";
        str += "                    SetConsoleTextAttribute( hstdout, ccolor );\n";
        str += "                }\n";
        str += "            }\n";
        str += "        } else {\n";
        str += "            cout << msg[i];\n";
        str += "        }\n";
        str += "     } \n";
        str += "}\n";
#endif
        str += "void trace(const char* fmt, ...) {\n";
        str += "    if(!__tracing) return;\n";
        str += "    if(sim_step < offset) return;\n";
        str += "    __one_trace_performed = true;\n";
        str += "    cout << latency << \" s:\\t\";\n";
        str += "    char msg[1024];\n";
        str += "    va_list argptr;\n";
        str += "    va_start(argptr, fmt);\n";
        str += "    vsprintf(msg, fmt, argptr);\n";
        str += "    va_end(argptr);\n";
        str += "    if(__redirecting_cout) {\n";
        str += "        char msg2[512];\n";
        str += "        int j = 0;\n";
        str += "        for(int i = 0; msg[i]; i++) {\n";
        str += "            if(msg[i] == '\\e') {\n";
        str += "                for(i++; (msg[i] <= '9' && msg[i] >= '0') || msg[i] == '[' || msg[i] == ';' || msg[i] == '?'; i++);\n";
        str += "            } else {\n";
        str += "                msg2[j++] = msg[i];\n";
        str += "            }\n";
        str += "        } msg2[j++] = '\\0';\n";
        str += "        if(sim_step >= offset) {\n";
        str += "            cout << msg2 << endl;\n";
        str += "        }\n";
        str += "    } else\n";
#ifdef __WIN32__
        str += "        if(!__redirecting_cout) {\n";
        str += "            __print_linux_escape_char_sequence(msg);\n";
        str += "            cout << endl;\n";
        str += "        } else\n";
#endif
        str += "        cout << msg << endl;\n";
        str += "}\n\n";
#ifndef __WIN32__
        str += "char __convert_c(unsigned char c) {\n";
        str += "    if((c%8) == 4) return c-3;\n";
        str += "    if((c%8) == 1) return c+3;\n";
        str += "    if((c%8) == 3) return c+3;\n";
        str += "    if((c%8) == 6) return c-3;\n";
        str += "    return c;\n";
        str += "}\n";
        str += "char __convert_color_out[24];\n";
        str += "char* __convert_color(unsigned char c) {\n";
        str += "    char fore = __convert_c(c & 0x0f);\n";
        str += "    char back = __convert_c((c >> 4)%8);\n";
        str += "    if(c == 0xf0) {\n";
        str += "        sprintf(__convert_color_out, \"\\e[0;7m\");\n";
        str += "    } else if(c == 0x00) {\n";
        str += "        sprintf(__convert_color_out, \"\\e[1m\");\n";
        str += "    } else {\n";
        str += "        if(back == 0) {\n";
        str += "            sprintf(__convert_color_out, \"\\e[%c;%dm\",  (fore > 8)?'0':'2',(fore%8)+30);\n";
        str += "        } else if(fore == 15) {\n";
        str += "            sprintf(__convert_color_out, \"\\e[7;%dm\", (back%8)+30);\n";
        str += "         } else \n";
        str += "            sprintf(__convert_color_out, \"\\e[%c;%d;%dm\", (fore > 8)?'0':'2', (fore%8)+30, back+40);\n";
        str += "    }\n";
        str += "    return __convert_color_out;\n";
        str += "}\n";
#endif
        str += "void trace(uint16 color, const char* fmt, ...) {\n";
        str += "    if(!__tracing) return;\n";
        str += "    if(sim_step < offset) return;\n";
        str += "    __one_trace_performed = true;\n";
        str += "    #ifdef __WIN32__\n";
        str += "    SetConsoleTextAttribute( hstdout, color );\n";
        str += "    #else\n";
        str += "    if(!__redirecting_cout)\n";
        str += "        cout << __convert_color(color);\n";
        str += "    #endif\n";
        str += "    char msg[1024];\n";
        str += "    va_list argptr;\n";
        str += "    va_start(argptr, fmt);\n";
        str += "    vsprintf(msg, fmt, argptr);\n";
        str += "    va_end(argptr);\n";
        str += "    trace(msg);\n";
        str += "    va_end(argptr);\n";
        str += "    #ifdef __WIN32__\n";
        str += "    SetConsoleTextAttribute( hstdout, csbi.wAttributes );\n";
        str += "    #else\n";
        str += "    if(!__redirecting_cout)\n";
        str += "        cout << \"\\e[0m\";\n";
        str += "    #endif\n";
        str += "}\n\n";
        str += "set<int> skipped_traces;\n\n";
        str += "void trace_off() {\n";
        str += "    __tracing = false;\n";
        str += "    __itracing = false;\n";
        str += "}\n\n";
        str += "void trace_on() {\n";
        str += "    __tracing = true;\n";
        str += "    __itracing = true;\n";
        str += "}\n\n";
        str += "void trace_off(int id) {\n";
        str += "    skipped_traces.insert(id);\n";
        str += "}\n\n";
        str += "void trace_on(int id) {\n";
        str += "    skipped_traces.erase(id);\n";
        str += "    __itracing = true;\n";
        str += "}\n\n";
        str += "void itrace(int id, const char* fmt, ...) {\n";
        str += "    if(!__tracing && !__itracing) return;\n";
        str += "    if(sim_step < offset) return;\n";
        str += "    if(skipped_traces.count(id)) return;\n";
        str += "    char msg[1024];\n";
        str += "    va_list argptr;\n";
        str += "    va_start(argptr, fmt);\n";
        str += "    vsprintf(msg, fmt, argptr);\n";
        str += "    va_end(argptr);\n";
        str += "    bool tracing_bk = __tracing;\n";
        str += "    __tracing = true;\n";
        str += "    trace(msg);\n";
        str += "    __tracing = tracing_bk;\n";
        str += "}\n\n";
        str += "void itrace(int id, uint16 color, const char* fmt, ...) {\n";
        str += "    if(!__tracing && !__itracing) return;\n";
        str += "    if(sim_step < offset) return;\n";
        str += "    if(skipped_traces.count(id)) return;\n";
        str += "    char msg[1024];\n";
        str += "    va_list argptr;\n";
        str += "    va_start(argptr, fmt);\n";
        str += "    vsprintf(msg, fmt, argptr);\n";
        str += "    va_end(argptr);\n";
        str += "    bool tracing_bk = __tracing;\n";
        str += "    __tracing = true;\n";
        str += "    trace(color, msg);\n";
        str += "    __tracing = tracing_bk;\n";
        str += "}\n\n";
        str += "set<int> skipped_breakpoints;\n";
        str += "bool all_breakpoints_skipped = false;\n";
        str += "void breakpoint(int id, const char* fmt, ...) {\n";
        str += "    if(all_breakpoints_skipped) return;\n";
        str += "    if(skipped_breakpoints.count(id)) return;\n";
        str += "    char msg[1024];\n";
        str += "    va_list argptr;\n";
        str += "    va_start(argptr, fmt);\n";
        str += "    vsprintf(msg, fmt, argptr);\n";
        str += "    va_end(argptr);\n";
        str += "    if(__redirecting_cout)\n";
        str += "        cout << \"Breakpoint: \" << msg << endl;\n";
#ifdef __WIN32__
        str += "    clog << \"Breakpoint: \";\n";
        str += "    __print_linux_escape_char_sequence(msg);\n";
        str += "    clog << endl;\n";
        str += "    int w = csbi.dwSize.X;\n";
#else
        str += "    clog << \"Breakpoint: \" << msg << endl;\n";
        str += "    int w = terminal_width();\n";
#endif
        str += "    clog << setw(w) << \"[1: Skip this] [2: Skip all] [Any: Resume] \";\n";
        str += "    switch(getch()) {\n";
        str += "        case '1': skipped_breakpoints.insert(id); break;\n";
        str += "        case '2': all_breakpoints_skipped = true; break;\n";
        str += "    }\n";
        str += "    __tracing = true;\n";
        str += "    clog << endl;";
        str += "}\n\n";
    }
    str += "string __simics_checkpoint = \"\";\n";
    str += "string __simics_cycles = \"\";\n";
    str += "void printCopyRight()\n";
    str += "{\n";
    
    str += " cout << \"     __    __\" << endl;\n";
    str += " cout << \"    /_/\\\\  /_/|\" << endl; \n";
    str += " cout << \"   |  \\\\ \\\\/  ||  High-Performance Computing Architectures and Networks (HPCAN)\" << endl; \n";
    str += " cout << \"   |   \\\\/   ||  Sharif University of Technology\" << endl; \n";
    str += " cout << \"   | |\\\\  /| ||  http://hpcan.ce.sharif.edu \" << endl; \n";
    str += " cout << \"   |_|/\\\\/ |_|/\\n    Simulator   Version: " MSIM_VERSION "\" << endl << endl; \n";
    str += " cout << \"   http://m-sim.org ( contact@m-sim.org )\" << endl; \n";
    str += " cout << \"   Copyright (c) 2013 by HPCAN-Sharif University of Technology.\" << endl << endl; \n";
    str += "}\n\n";
    if(automatic_simulation) {
        str += "void print_checkpoint_info() {\n";
        str += "    cout << \"Checkpoint: \" << __simics_checkpoint.c_str() << endl;\n";
        str += "    cout << \"Total simulation steps: \" << __simics_cycles.c_str() << endl << endl;\n";
        str += "}\n\n";
    }
    if(macros["STEP"].empty() && !automatic_simulation)
        str += "bool __pause_when_finished = false;\n";
    else
        str += "bool __pause_when_finished = true;\n";
    if(!automatic_simulation || !show_progress || !macros["STEP"].empty() || !macros["VERBOSE"].empty())
        str += "bool __show_progress = false;\n";
    else
        str += "bool __show_progress = true;\n\n";
    str += "bool __showing_progress = false;\n\n";
    str += "time_t __starting_time;\n";
    str += "time_t ___starting_time;\n";
    str += "char* __show_remaining_time(time_t seconds)\n";
    str += "{\n";
    str += "    static char t[64] = \"\";\n";
    str += "    if(__current_value == 0) { return t; }\n";
    str += "    if(__sim_length == 0) return t;\n";
    str += "    double ratio = ((double)__current_value)/((double)__sim_length);\n";
    str += "    time_t remaining = (time_t)((1.0/ratio)*(1.0-ratio)*((double)seconds));\n";
    str += "    sprintf(t, \"Remaining: %d:%02d:%02d\", (int)(remaining/3600), (int)(remaining/60)%60, (int)(remaining)%60);\n";
    str += "    if(__redirecting_progress) \n";
    str += "        sprintf(t, \"Remaining: %d:%02d:%02d\\nStart: %lld\",";
    str +=          " (int)(remaining/3600), (int)(remaining/60)%60, (int)(remaining)%60,";
    str +=          " (uint64)(___starting_time));\n";
    //str += "    sprintf(t, \"Remaining: %lld/%lld\", __current_value, __sim_length);\n";
    str += "    return t;\n";
    str += "}\n\n";
    str += "char* __show_elapsed_time(time_t starting_time, bool show_remaining = true, char rem_sep = ' ')\n";
    str += "{\n";
    str += "    static char t[64];\n";
    str += "    ___starting_time = starting_time;\n";
    str += "    time_t seconds=time(0)-starting_time;\n";
    str += "    int h = (seconds/3600);\n";
    str += "    if(show_remaining)\n";
    str += "        sprintf(t, \"Elapsed: %d:%02d:%02d%c%s\", h, (int)(seconds/60)%60, (int)(seconds)%60, rem_sep, __show_remaining_time(seconds));\n";
    str += "    else\n";
    str += "        sprintf(t, \"%d:%02d:%02d\", h, (int)(seconds/60)%60, (int)(seconds)%60);\n";
    str += "    return t;\n";
    str += "}\n\n";
    if(multithreaded) {
        str += "bool* __thread_started;\n";
        str += "pthread_t* __threads;\n";
        str += "bool* __thread_finished;\n";
        str += "int __thread_counter=0;\n";
        str += "typedef struct {\n";
        str += "    void* owner;\n";
        str += "    int thread_id;\n";
        str += "    transaction_t trans;\n";
        str += "} callback_t;\n";
        str += "\n";
        str += "template<class T, void(T::*mem_fn)(int tid, transaction_t t), void(T::*mem_fn2)(int tid)>\n";
        str += "void* __thunk(void* p) {\n";
        str += "    callback_t* cb = (callback_t*)p;\n";
        str += "    __thread_finished[cb->thread_id] = false;\n";
        if(use_virtual_address)
            str += "    _address addr = cb->trans.virtual_address;\n";
        else
            str += "    _address addr = cb->trans.physical_address;\n";
        if(capture_data || capture_memory_content)
            str += "    _data d = cb->trans.data;\n";
        str += "    (static_cast<T*>(cb->owner)->*mem_fn2)(cb->thread_id);\n";
        str += "    for(int i=0; i < cb->trans.size; i++)\n";
        str += "    {\n";
        if(use_virtual_address)
            str += "        cb->trans.virtual_address = addr+i;\n";
        else
            str += "        cb->trans.physical_address = addr+i;\n";
        if(capture_data || capture_memory_content)
            str += "        cb->trans.data = (d >> (8*i)) & 0x0ff;\n";
        str += "        (static_cast<T*>(cb->owner)->*mem_fn)(cb->thread_id, cb->trans);\n";
        str += "    }\n";
        str += "    __thread_finished[cb->thread_id] = true;\n";
        str += "    (static_cast<T*>(cb->owner)->*mem_fn2)(-1);\n";
        str += "    delete cb;\n";
        str += "    return 0;\n";
        str += "}\n\n";
    }
}

void generate_license_notes(string& str) {
    int slash = 0;
    for(unsigned int i=0; i<compile_output_filename.length(); i++)
        if(compile_output_filename[i] == '\\' || compile_output_filename[i] == '/')
            slash = i+1;
    str += "/**\n";
    str += "* @file ";
    str += compile_output_filename.substr(slash, compile_output_filename.length()-slash);
    str += "\n*\n";
    str += "*  Generate by M-Simulator Compiler V 1.0.0\n";
    str += "*    __    __\n";
    str += "*   /_/\\  /_/|\n";
    str += "*  |  \\ \\/  ||  High-Performance Computing Architectures and Networks (HPCAN)\n";
    str += "*  |   \\/   ||  Sharif University of Technology\n";
    str += "*  | |\\  /| ||  Version: M-Simulator 1.0.0\n";
    str += "*  |_|/\\/ |_|/\n";
    str += "*   Simulator\n";
    str += "* \n";
    str += "*/\n\n";
}

void generate_header_codes(string& str, int& line) {
    line = 1;
    generate_license_notes(str);
    generate_transaction_class(str);
    line += extract_lines_count(str);
}

void generate_footer_codes(string& str) {
    /*if(capture_memory_content) {
        str += "void __extract_memory_content(char* line)\n";
        str += "{\n";
        str += "    if(!line) return;\n";
        str += "    char c = line[0];\n";
        str += "    if(c != 'p' || c == 0) return;\n";
        str += "    if(strlen(line) < 54) return;\n";
        str += "    if(line[1] != ':') return;\n";
        str += "    if(line[2] != '0') return;\n";
        str += "    if(line[3] != 'x') return;\n";
        str += "    char addr[17] = {0};\n";
        str += "    int i=4;\n";
        str += "    for(; line[i] != ' '; i++) {\n";
        str += "        addr[i-4] = line[i];\n";
        str += "    }\n";
        str += "    i+=2;\n";
        str += "    uint64 base=hex2dec(addr);\n";
        str += "    for(int b=0; b<8; b++) {\n";
        str += "        int A = digit(line[i+0+b*5])*16+digit(line[i+1+b*5]);\n";
        str += "        int B = digit(line[i+2+b*5])*16+digit(line[i+3+b*5]);\n";
        str += "        __main_memory->set_data(base+b*2, A);\n";
        str += "        __main_memory->set_data(base+b*2+1, B);\n";
        str += "    }\n";
        str += "}\n";
        str += "\n";
        str += "void __update_memory_content(const char* cmd, uint64 size) {\n";
        str += "    FILE* pipe = popen(cmd, \"r\");\n";
        str += "    if (!pipe)\n";
        str += "        return;\n";
        str += "    char buffer[128];\n";
        str += "    int i = 0;\n";
        str += "    while (!feof(pipe)) {\n";
        str += "        if (fgets(buffer, 128, pipe) != NULL) {\n";
        str += "            __extract_memory_content(buffer);\n";
        str += "        }\n";
        str += "    }\n";
        str += "    pclose(pipe);\n";
        str += "}\n";
    }*/
    if(automatic_simulation) {
        str += "string __image_files;\n";
        str += "uint64 __image_files_size;\n";
        str += "void __load_memory_content(uint64 addr, uint64 last_addr) {\n";
        str += "    addr >>= ";
        str += itoa((int)(log2(memory_dump_size)+10));
        str += ";\n";
        if(capture_memory_type == CAPTURE_FROM_FILE) {
            str += "    if(!__added_memory_dumps.count(addr))\n";
            str += "        __added_memory_dumps.insert(addr);\n";
            str += "    else return;\n";
        } else {
            str += "    static bool loading_completed = false;\n";
            str += "    if(loading_completed) return;\n";
            str += "    loading_completed = true;\n";
        }
        str += "    uint64 min_addr = ((last_addr==0)?(addr << ";
        str += itoa((int)(log2(memory_dump_size)+10));
        str += "):addr), max_addr = ((last_addr==0)?((addr+1) << ";
        str += itoa((int)(log2(memory_dump_size)+10));
        str += "):last_addr);\n";
        str += "    string cmd = __image_files;\n";
        str += "    if(max_addr > __image_files_size) max_addr=__image_files_size;\n";
        str += "    if(max_addr < min_addr) return;\n";
        str += "    clog << \"\\nLoading memory content (\" << hex << min_addr << dec << \")...\" << endl;\n";
        str += "    char len_str[32];\n";
        str += "    sprintf(len_str, \" -e %lld -w %lld \", min_addr, max_addr-min_addr);\n";
        str += "    cmd += len_str;\n";

#ifdef __WIN32__
        str += "    int w = csbi.dwSize.X;\n";
#else
        str += "    int w = terminal_width();\n";
#endif

        str += "#ifdef __WIN32__\n";
        str += "    FILE* pipe = popen(cmd.c_str(), \"rb\");\n";
        str += "#else\n";
        str += "    FILE* pipe = popen(cmd.c_str(), \"r\");\n";
        str += "#endif\n";
        str += "    if (!pipe) {\n";
        str += "        cerr << \"Unable to run craff.\" << endl;\n";
        str += "        return;\n";
        str += "    }\n";
        str += "    uint64 cnt = 0;\n";
        str += "    uint64 total = max_addr-min_addr;\n";
        str += "    uint64 step = total/100;\n";
        str += "    uint8 buffer[17];\n";
        str += "    long where = 0;\n";
        str += "    size_t result=16;\n";
        //str += "    time_t start_time=time(0);\n";
        str += "    uint64 size = min_addr;\n";
        str += "    uint64 saved_current_value = __current_value;\n";
        str += "    __sim_length += step/10;\n";
        str += "    int curr_step = step/1000;\n";
#ifdef __WIN32__
        str += "    uint8 loan = 0;\n";
        str += "    int owed = 0;\n";
#endif
        if(show_progress || !macros["STEP"].empty() || !macros["VERBOSE"].empty())
            str += "    bool display_progress = true;\n";
        else
            str += "    bool display_progress = false;\n";
        str += "    while(result > 0) {\n";
#ifdef __WIN32__
        str += "        result = fread (buffer+owed,1,16,pipe);\n";
        str += "        if(owed) {\n";
        str += "            result ++;\n";
        str += "            owed = 0;\n";
        str += "            buffer[0] = loan;\n";
        str += "        }\n";
#else
        str += "        result = fread (buffer,1,16,pipe);\n";
#endif
        str += "        for(int i=0; i<result; i++) { \n";
#ifdef __WIN32__
        str += "            if(buffer[i] == 0x0d) {\n";
        str += "                if(i < result-1) {\n";
        str += "                    if(buffer[i+1] == 0x0a) {\n";
        str += "                        continue;\n";
        str += "                    }\n";
        str += "                } else { \n";
        str += "                    if(fread(&loan,1,1,pipe)) {\n";
        str += "                        if(loan == 0x0a) {\n";
        str += "                            buffer[i] = 0x0a;\n";
        str += "                        } else {\n";
        str += "                            owed = 1;\n";
        str += "                        }\n";
        str += "                    } \n";
        str += "                }\n";
        str += "            }\n";
#endif
        str += "            if(buffer[i] != 0) __main_memory->set_data(size, buffer[i]);\n";
        str += "            size++;\n";
        str += "            if(display_progress) {\n";
        str += "                cnt++;\n";
        str += "                if(cnt == step) {\n";
        str += "                    __current_value += curr_step;\n";
        str += "                    if(__redirecting_progress) { __progress_file.open(__progress_file_path.c_str()); __progress_file << \"Loading memory content...\\nStatus: \" << setw(5) << right << (int)(100*(double)(size-min_addr)/(double)total) << endl << __show_elapsed_time(__starting_time, true, '\\n'); __progress_file.close();} else\n";
        str += "                    clog << \"\\rProgress: \" << setw(4) << (int)(100*(double)(size-min_addr)/(double)total) << '%' << setw(w-15) << right << __show_elapsed_time(__starting_time);\n";
        str += "                    cnt = 0;\n";
        str += "                }\n";
        str += "            }\n";
        str += "        }\n";
        str += "    }\n";
        str += "    clog << \"\\rMemory loading completed: \" << (total) << \" Bytes.\" << endl;\n";
        str += "    pclose (pipe);\n";
        //str += "    __starting_time += (time(0) - start_time);\n";
        str += "}\n";
        str += "void __read_checkpoint_configuration(string filename) {\n";
        str += "    struct __path_t {\n";
        str += "        string path[64];\n";
        str += "        int idx;\n";
        str += "        bool is_capturing;\n";
        str += "        __path_t() {\n";
        str += "            idx = 0;\n";
        str += "            is_capturing = false;\n";
        str += "        }\n";
        str += "        static bool __is_dec_or_hex(const char* s) {\n";
        str += "            bool is_hex = false;\n";
        str += "            for(int i=0; s[i]; i++) {\n";
        str += "                if(s[i] < '0' || s[i] > '9') {\n";
        str += "                    if(s[i] == 'x' && i == 1) {\n";
        str += "                        is_hex = true;\n";
        str += "                        continue;\n";
        str += "                    } else if(is_hex && ((s[i] >= 'a' && s[i] <= 'f') || (s[i] >= 'A' && s[i] <= 'F')))\n";
        str += "                        continue;\n";
        str += "                    else\n";
        str += "                        return false;\n";
        str += "                }\n";
        str += "            }\n";
        str += "            return true;\n";
        str += "        }\n";
        str += "        static string refine_path(string base, string bases[], string path) {\n";
        str += "            int j = 0;\n";
        str += "            int lst = 0;\n";
        str += "            char srch[10];\n";
        str += "            string tmp = \"\";\n";
        str += "            bool found = false;\n";
        str += "            while(lst != string::npos) {\n";
        str += "                sprintf(srch, \"%%%d%%\", j);\n";
        str += "                lst = path.find(srch);\n";
        str += "                if (lst != string::npos) {\n";
        str += "                    found = true;\n";
        str += "                    tmp = path.substr(0, lst);\n";
        str += "                    if(j==1) tmp += base; else tmp += bases[j];\n";
        str += "                    tmp += path.substr(lst+3);\n";
        str += "                    path = tmp;\n";
        str += "                } else {\n";
        str += "                    j++;\n";
        str += "                    sprintf(srch, \"%%%d%%\", j);\n";
        str += "                    lst = path.find(srch);\n";
        str += "                }\n";
        str += "            }\n";
        str += "            if(!found) {\n";
        str += "                return base+path;\n";
        str += "            }\n";
        str += "            return path;\n";
        str += "        }\n";
        str += "    } simics, checkpoint, memory_image, sd0_image, cpu_list;\n";
        str += "    int idx = filename.rfind(\"/\");\n";
        str += "    if(idx == string::npos) idx = filename.rfind(\"\\\\\");\n";
        str += "    string base = filename.substr(0, idx+1);\n";
        str += "    FILE* file=fopen(filename.c_str(), \"r\");\n";
        str += "    if(!file){\n";
        str += "        cerr << \"*FATAL Error*: Could not open checkpoint file.\" << endl;\n";
        str += "        //if(__application_terminated) kill(0, SIGTERM);\n";
#if defined (__WIN32__)
        str += "        FlushConsoleInputBuffer( hstdin );\n";
        str += "        SetConsoleTextAttribute( hstdout, csbi.wAttributes );\n";
#else
        str += "        int res=system(\"tput cnorm\");\n";
#endif
        str += "        if(__redirecting_progress) {\n";
        str += "            int res2=system((\"rm \"+__progress_file_path).c_str());\n";
        str += "            res2=system((\"rm \"+__report_file_path).c_str());\n";
        str += "        }\n";
        str += "        exit(-1);\n";
        str += "    }\n";
        str += "    char ch;\n";
        str += "    int paran=0;\n";
        str += "    string str, last_str;\n";
        str += "    bool is_string=false, capturing_memory_image=false, capturing_sd0_image=false;\n";
        str += "    char buff[1024];\n";
        str += "    while(!feof(file)) {\n";
        str += "        char* res=fgets(buff, 1024, file);\n";
        str += "        for(int i=0; buff[i]; i++) {\n";
        str += "            ch = buff[i];\n";
        str += "            if(!is_string && ch == '\"') {\n";
        str += "                is_string = true;\n";
        str += "            } else if(ch=='\"') {\n";
        str += "                is_string = false;\n";
        str += "                if(simics.is_capturing)\n";
        str += "                    simics.path[simics.idx++] = str;\n";
        str += "                if(checkpoint.is_capturing)\n";
        str += "                    checkpoint.path[checkpoint.idx++] = str;\n";
        str += "                if(memory_image.is_capturing)\n";
        str += "                    memory_image.path[memory_image.idx++] = str;\n";
        str += "                if(sd0_image.is_capturing)\n";
        str += "                    sd0_image.path[sd0_image.idx++] = str;\n";
        str += "                str = \"\";\n";
        str += "            } else if(is_string) {\n";
        str += "                str += ch;\n";
        str += "            } else if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_') {\n";
        str += "                simics.is_capturing = false;\n";
        str += "                checkpoint.is_capturing = false;\n";
        str += "                str += ch;\n";
        str += "            } else {\n";
        str += "                if(!str.empty()) {\n";
        str += "                    if(ch == ',') {\n";
        str += "                        if(cpu_list.is_capturing) {\n";
        str += "                           cpu_list.idx++;\n";
        str += "                        }\n";
        str += "                    } else if(ch == ')')\n";
        str += "                        cpu_list.is_capturing = false;\n";
        str += "                    if(__path_t::__is_dec_or_hex(str.c_str())) {\n";
        str += "                        if(memory_image.is_capturing) {\n";
        str += "                            memory_image.path[memory_image.idx++] = str;\n";
        str += "                        } else if(sd0_image.is_capturing)\n";
        str += "                            sd0_image.path[sd0_image.idx++] = str;\n";
        str += "                    } else\n";
        str += "                        if(memory_image.is_capturing) {\n";
        str += "                            memory_image.is_capturing = false;\n";
        str += "                        } else if(sd0_image.is_capturing)\n";
        str += "                            sd0_image.is_capturing = false;\n";
        str += "                }\n";
        str += "                if(str == \"cpu_list\") {\n";
        str += "                    cpu_list.is_capturing = true;\n";
        str += "                } else if(str == \"simics_path\") {\n";
        str += "                    simics.is_capturing = true;\n";
        str += "                } else if(str == \"checkpoint_path\") {\n";
        str += "                    checkpoint.is_capturing = true;\n";
        str += "                } else if(str == \"files\" && capturing_memory_image) {\n";
        str += "                    memory_image.is_capturing = true;\n";
        str += "                } else if(str == \"files\" && capturing_sd0_image) {\n";
        str += "                    sd0_image.is_capturing = true;\n";
        str += "                } else if(last_str == \"OBJECT\") {\n";
        str += "                    if(str == \"memory_image\") {\n";
        str += "                        capturing_memory_image = true;\n";
        str += "                    } else\n";
        str += "                        capturing_memory_image = false;\n";
        str += "                    if(str == \"sd0_image\") {\n";
        str += "                        capturing_sd0_image = true;\n";
        str += "                    } else\n";
        str += "                        capturing_sd0_image = false;\n";
        str += "                }\n";
        str += "                last_str = str;\n";
        str += "                str = \"\";\n";
        str += "            }\n";
        str += "        }\n";
        str += "    }\n";
        str += "    fclose(file);\n";
        str += "    cpu_count = cpu_list.idx+1;\n";
        str += "    clog << \"Number of cores: \" << cpu_count << endl;\n";
        str += "    instructions.set_len(cpu_count);\n";
        /*
        if(capture_memory_content) {
            str += "    clog << \"Loading memory contents...\" << endl;\n";
            str += "    string cmd = \"simics -no-win \";\n";
            str += "    cmd += __simics_checkpoint;\n";
            str += "    cmd += \" -e \\\"x p:0 \"+memory_image.path[3]+\"\\\"\";\n";
            str += "    cmd += \" -e q\";\n";
            str += "    cout << cmd.c_str() << endl;\n";
            str += "    __update_memory_content(cmd.c_str(), hex2dec(memory_image.path[3]));\n";
        } */

        if(capture_memory_content) {
            str += "    string cmd = \"craff -o -\";\n";
            str += "    cout << \"Memory image files:\" << endl;\n";
            str += "    unsigned long long max_addr = 0, tmp = 0;\n";
            //        	str += "    for(int i = 0; i < sd0_image.idx / 5; i++) {\n";
            //        	str += "        cmd += \" \\\"\"+ __path_t::refine_path(base, simics.path, sd0_image.path[i*5])+\"\\\"\";\n";
            //        	str += "        cout << \"    \" << __path_t::refine_path(base, simics.path, sd0_image.path[i*5]) << endl;\n";
            //        	str += "    }\n";
            str += "    max_addr = 0;\n";
            str += "    uint64 min_addr = 0x1000000000000000LL;\n";
            str += "    for(int i = 0; i < memory_image.idx / 5; i++) {\n";
            //                str += "        if(memory_image.path[i*5].find(\".memory_image\")==std::string::npos) continue;\n";
            str += "        tmp = hex2dec(memory_image.path[i*5+3]);\n";
            str += "        if(tmp>max_addr) max_addr = tmp;\n";
            str += "        tmp = hex2dec(memory_image.path[i*5+2]);\n";
            str += "        if(tmp<min_addr) min_addr = tmp;\n";
            //        	str += "        cmd += \" \\\"\"+__path_t::refine_path(base, simics.path, memory_image.path[i*5])+\"\\\"\";\n";
            //        	str += "        cout << \"   \" << __path_t::refine_path(base, simics.path, memory_image.path[i*5]) << endl;\n";
            str += "    }\n";

            str += "    string ls_cmd = \"ls \";\n";
            str += "    ls_cmd += __simics_checkpoint;\n";
            str += "    ls_cmd += \"*.memory_image\";\n";
            str += "    FILE* ls_pipe = popen(ls_cmd.c_str(), \"r\");\n";
            str += "    if (!ls_pipe) {\n";
            str += "        cerr << \"Unable to run simics.\" << endl;\n";
            str += "        return;\n";
            str += "    }\n";
            str += "    char buffer[4000];\n";
            str += "    while (!feof(ls_pipe) && !__application_terminated) {\n";
            str += "        if (fgets(buffer, 4000, ls_pipe) != NULL) {\n";
            str += "            string s = \" \\\"\";\n";
            str += "            s += buffer;\n";
            str += "            s.replace(s.find(\"\\n\"), 1, \"\\\"\");\n";
            str += "            cmd += s;\n";
            str += "            cout << \"    \" << buffer;\n";
            str += "        }\n";
            str += "    }\n";
            str += "    pclose(ls_pipe);\n";

            str += "    __image_files = cmd;\n";
            //                str += "    cout << __image_files.c_str() << endl;\n";
            str += "    __image_files_size = max_addr;\n";
            str += "    memory_size = max_addr;\n";
            if(capture_memory_type == CAPTURE_AT_ONCE)
                str += "    __load_memory_content(min_addr, max_addr);\n";
        }
        str += "}\n";
    }
    str += "\n";
    str += "\nbool __successfull = false;\n\n";
    str += "void print_report() {\n";
    if(has_report_function) {
        str += "    if(!__successfull) return;\n";
        str += "    cout << \"--------------------------------<< Reports >>----------------------------------\" << endl;\n";
        str += "    top_level_module->report();\n";
    } else {
        str += "    cout << \"No report function is declared.\" << endl;\n";
    }
    str += "}\n\n";
    if(!macros["STEP"].empty() || !macros["VERBOSE"].empty() || allow_exceptions) {
        str += "void print_meaningful_section(transaction_t t, const char* buf, bool new_line) {\n";
        str += "    if((__one_trace_performed || __showing_progress) && new_line) cout << endl;\n";
        str += "    cout << \"<< \";\n";
        str += "    printf(\"%10lld \", sim_step);\n";
        str += "    bool ignore = true;\n";
#if defined (__WIN32__)
        str += "    char line_[64];\n";
        str += "    sprintf(line_, \"\\e[1;31mCore [%d]\\e[0m \", t.cpu_num);\n";
        str += "    __print_linux_escape_char_sequence(line_);\n";
#else
        str += "    if(__redirecting_cout)\n";
        str += "        cout << \"Core [\" << (int)t.cpu_num << \"] \";\n";
        str += "    else\n";
        str += "        cout << \"\\e[1;31mCore [\" << (int)t.cpu_num << \"]\\e[0m \";\n";
#endif

#if defined (__WIN32__)
        str += "    int w = csbi.dwSize.X-32;\n";
#else
        str += "    int w = terminal_width()-32;\n";
#endif

        str += "    switch(t.type) {\n";
        str += "        case ttInstFetch: cout << \"inst:\"; break;\n";
        str += "        case ttDataRead: cout << \"data:\"; break;\n";
        str += "        case ttDataWrite: cout << \"data:\"; break;\n";
        str += "        case ttException: ignore = false; w += 5;\n";
        str += "    }\n";
#if defined (__WIN32__)
        str += "    __print_linux_escape_char_sequence(\"\\e[0;36m\");\n";
#else
        str += "    if(!__redirecting_cout)\n";
        str += "        cout << \"\\e[0;36m\";\n";
#endif
        str += "    int cnt = 0;\n";
        str += "    for(int i=0, j = 0; buf[i]; i++) {\n";
        str += "        if(!ignore && buf[i] != '\\n' && j < w) {\n";
        str += "            if(j == w-3 && (buf[i] != '\\0') && (buf[i+1] != '\\0') && (buf[i+2] != '\\0') && (buf[i+3] != '\\0')) {\n";
        str += "                cout << \"...\";\n";
        str += "                j = w;\n";
        str += "            } else\n";
        str += "                cout << buf[i];\n";
        str += "            j++;\n";
        str += "        }\n";
        str += "        if(buf[i] == '>') {\n";
        str += "            cnt++;\n";
        str += "            if(cnt == 2)\n";
        str += "                ignore = false;\n";
        str += "        }\n";
        str += "    }\n";
#if defined (__WIN32__)
        str += "    SetConsoleTextAttribute( hstdout, csbi.wAttributes );\n";
#else
        str += "    if(!__redirecting_cout)\n";
        str += "        cout << \"\\e[0m >>\";\n";
        str += "    else\n";
#endif
        str += "        cout << \" >>\";\n";
        str += "       cout << endl;\n";
        str += "}\n";
    }

#if defined (__WIN32__)
    if(!automatic_simulation) {
        str += "void run(istream &in) {\n";
        str += "    cout << \"Running simulation...  \" << endl;\n";
        str += "    char buffer[4000];\n";
        str += "    in.seekg (0, ios::end);\n";
        str += "    __sim_length = in.tellg();\n";
        str += "    __current_value = 0;\n";
        str += "    int step = 100;\n";
        str += "    int cnt = 0;\n";
        str += "    int w = csbi.dwSize.X-17;\n";
        str += "    clog.precision(1);\n";
        str += "    clog.setf(ios::fixed,ios::floatfield);\n";
        str += "    in.seekg (0, ios::beg);\n";
        str += "    __showing_progress = __show_progress;\n";
        str += "    while (in.good()){\n";
        str += "        in.getline(buffer, 4000, '\\n');\n";
        str += "        transaction_t t = parse(buffer);\n";
        str += "        execute(t);\n";
        str += "        if(__show_progress) {\n";
        str += "            if(cnt>step || cnt == 10) {\n";
        str += "                __current_value = in.tellg();\n";
        str += "                if(__redirecting_progress) { __progress_file.open(__progress_file_path.c_str()); __progress_file << \"Running simulation...\\nProgress: \" << setw(5) << right << (100*((float)__current_value))/((float)__sim_length) << '%' << endl << __show_elapsed_time(__starting_time, true, '\\n'); __progress_file.close();} else\n";
        str += "                clog << \"\\rProgress: \" << setw(4) << right << (100*((float)__current_value))/((float)__sim_length) << '%' << setw(w) << right << __show_elapsed_time(__starting_time);\n";
        str += "                cnt = 0;\n";
        str += "            } else if(t.cpu_num == 0) cnt++;\n";
        str += "        }\n";
        str += "        if(simulation_finished) break;\n";
        str += "    }\n";
        str += "    if(__show_progress) { \n";
        str += "        if(__redirecting_progress) { __progress_file.open(__progress_file_path.c_str()); __progress_file << \"\\r\" << ((__application_terminated)?\"Terminated by user.\":\"Simulation Completed.\") << setw(58) << right << __show_elapsed_time(__starting_time, false) << endl; __progress_file.close();} else\n";
        str += "        clog << \"\\rSimulation Completed.\" << setw(w-8) << right << __show_elapsed_time(__starting_time, false) << endl;\n";
        str += "        __showing_progress = false; }\n";
        str += "        __successfull = true;\n";
        str += "}\n\n";
    }
#endif
    if(automatic_simulation) {

        str += "void __run_simulation(const char* cmd) {\n";
        str += "    clog << \"Running simulation...\" << endl;\n";
        str += "    FILE* pipe = popen(cmd, \"r\");\n";
        str += "    if (!pipe) {\n";
        str += "        cerr << \"Unable to run simics.\" << endl;\n";
        str += "        return;\n";
        str += "    }\n";
        str += "    char buffer[4000];\n";
        str += "    __current_value = 0;\n";
        str += "    __sim_length = total_steps;\n";
        str += "    uint64 step = __sim_length/1000, curr = 0;\n";
        str += "    if(__sim_length <= 10000000) step = __sim_length/100;\n";
        str += "    if(__sim_length >= 100000000) step = __sim_length/10000;\n";
        //        str += "    if(__sim_length >= 1000000000) step = __sim_length/100000;\n";
        str += "    int i = 0;\n";
        str += "    clog.precision(1);\n";
        str += "    clog.setf(ios::fixed,ios::floatfield);\n";
        str += "    if(__show_progress) {\n";
        str += "        __showing_progress = true;}\n";
        str += "    while (!feof(pipe) && !__application_terminated) {\n";
        str += "        if (fgets(buffer, 4000, pipe) != NULL) {\n";
        str += "            transaction_t t = parse(buffer);\n";
        str += "            if(t.type != ttNone) {\n";
        str += "                execute(t);\n";
        str += "                if(__show_progress) {\n";
        str += "                    if(curr > step) {\n";
        str += "                        curr = 0;\n";
        str += "                        if(__current_value < __sim_length) {\n";
        str += "                            if(__redirecting_progress) {__progress_file.open(__progress_file_path.c_str()); __progress_file << \"Running simulation...\\nProgress: \" << setw(5) << right << (100*((float)__current_value))/((float)__sim_length) << '%' << endl << __show_elapsed_time(__starting_time, true, '\\n'); __progress_file.close();} else\n";
        str += "                            clog << \"\\rProgress: \" << setw(5) << right << (100.0*(float)__current_value/((float)__sim_length)) << '%' << setw(terminal_width()-16) << right << __show_elapsed_time(__starting_time);\n";
        str += "                        } else break;\n";
        str += "                    } else if(t.cpu_num == 0) curr++;\n";
        str += "                }\n";
        str += "            }\n";
        str += "        }\n";
        str += "        if(simulation_finished) __application_terminated = true;\n";
        str += "    }\n";
        str += "    if(__show_progress) {\n";
        str += "        if(__redirecting_progress) { __progress_file.open(__progress_file_path.c_str()); __progress_file << ((__application_terminated)?\"Terminated by user.\":\"Simulation Completed.\") << endl << right << __show_elapsed_time(__starting_time, false) << endl; __progress_file.close();} else\n";
        str += "        clog << \"\\rProgress: \" << setw(5) << right << \"100%\" << setw(terminal_width()-15) << right << __show_elapsed_time(__starting_time, false) << endl;\n";
        str += "        __showing_progress = false;\n";
        str += "    }\n";
        str += "    if(!__application_terminated) pclose(pipe);\n";
        str += "    __successfull = true;\n";
        str += "}\n";
        str += "void __run_simulation() {\n";
        if(run_simulation) {
            str += "    string cmd = \"simics \";\n";
            str += "    cmd += __simics_checkpoint;\n";
            str += "    cmd += \" -e \\\"instruction-profile-mode instruction-fetch-trace\\\"\";\n";
            str += "    cmd += \" -e new-tracer -e trace0.start\";\n";
            str += "    char b[60];\n";
            str += "    for(int i=0; i<cpu_count; i++) {\n";
            str += "        sprintf(b, \" -e mmu%d.trace\", i);\n";
            str += "        cmd += b;\n";
            str += "    }\n";
            str += "    cmd += \" -e \\\"c \";\n";
            str += "    cmd += __simics_cycles;\n";
            str += "    cmd += \"\\\" -e q\";\n";
            str += "    __run_simulation(cmd.c_str());\n";

            /*
            str += "    string cmd = \"simics \";\n";
            if(!show_window)
                str += "    cmd += \"-no-win \";\n";
            str += "    cmd += __simics_checkpoint;\n";
            str += "    cmd += \" -e \\\"instruction-profile-mode instruction-fetch-trace\\\"\";\n";
            str += "    cmd += \" -e new-tracer -e trace0.start -e \\\"c \";\n";
            str += "    cmd += __simics_cycles;\n";
            str += "    cmd += \"\\\" -e q\";\n";
            str += "    __run_simulation(cmd.c_str());\n";
            */
        }
        str += "}\n";
    }
    str += "stringstream __sig_message;\n";
    str += "void __manual_teminat(int sig) {\n";
    str += "    if(simulation_finished) return;\n";
    str += "    __sig_message << \"*** Terminated by user at step \" << sim_step << endl;";

#if defined (__WIN32__)
    str += "    CCI.bVisible = true;\n";
    str += "    SetConsoleCursorInfo(hstdout, &CCI); \n";
#else
    str += "    if(!__redirecting_cout) \n";
    str += "        int res=system(\"tput cnorm\");\n";
#endif
    //    str += "    if(__application_terminated) kill(0, SIGTERM);\n";
    if(automatic_simulation) {
        //	str += "    kill(0, SIGTERM);\n";
        str += "    simulation_finished=true;\n";
    } else
        str += "    exit(0);\n";
    str += "}\n";
    str += "int main(int argc, char* argv[])\n";
    str += "{\n";
    str += "    signal (SIGCHLD, SIG_IGN);\n";
    str += "    signal (SIGINT, __manual_teminat);\n";
    str += "    signal (SIGTERM, __manual_teminat);\n";
    //if(automatic_simulation)
        str += "    __starting_time = time(0);\n";
    str += "    printCopyRight();\n";
    if(!macros["DISPLAY_PIPELINE"].empty())
        str += "    __pipeline_file.open(\"pipeline.txt\");\n";
    if(multithreaded) {
        char cstr[256];
        sprintf(cstr, "    __threads = new pthread_t[%d];\n", number_of_threads);
        str += cstr;
        sprintf(cstr, "    __thread_started = new bool[%d];\n", number_of_threads);
        str += cstr;
        sprintf(cstr, "    __thread_finished = new bool[%d];\n", number_of_threads);
        str += cstr;
        sprintf(cstr, "    for(int i=0; i<%d; i++) __thread_started[i] = !(__thread_finished[i] = true);\n", number_of_threads);
        str += cstr;
    }
    str += "    static time_t __total_starting_time = time(0);\n";
#if defined (__WIN32__)
    str += "    HANDLE hstdin  = GetStdHandle( STD_INPUT_HANDLE  );\n";
    str += "    hstdout = GetStdHandle( STD_OUTPUT_HANDLE );\n";
    str += "    GetConsoleScreenBufferInfo( hstdout, &csbi );\n";
    str += "    GetConsoleCursorInfo(hstdout, &CCI);\n";
    str += "    CCI.bVisible = false;\n";
    str += "    SetConsoleCursorInfo(hstdout, &CCI); \n";
#else
    str += "    if(argc < 4) ";
    str += "int res=system(\"tput civis\");\n";
#endif
    str += "    try {\n";
    if(automatic_simulation) {
        str += "    if(argc < 3) {\n";
        str += "        clog << \"usage: \" << argv[0] << \" <checkpoint> <total steps> [redirect file (optional)] [progress file (optional)]\" << endl;";
#if defined (__WIN32__)
        str += "        FlushConsoleInputBuffer( hstdin );\n";
        str += "        SetConsoleTextAttribute( hstdout, csbi.wAttributes );\n";
#else
        str += "        int res=system(\"tput cnorm\");\n";
#endif
        str += "        return 0;";
        str += "    }\n";
        str += "    __simics_checkpoint = \"\\\"\";\n";
        str += "    __simics_checkpoint += argv[1];\n";
        str += "    __simics_checkpoint += \"\\\"\";\n";
        str += "    __remove_char(argv[2], '`');\n";
        str += "    __remove_char(argv[2], ',');\n";
        str += "    __remove_char(argv[2], '_');\n";
        str += "    __remove_char(argv[2], '/');\n";
        str += "    __simics_cycles = argv[2];\n";
        str += "    total_steps = str2dec(__simics_cycles);\n";
        str += "    __sim_length = total_steps;\n";
        str += "    print_checkpoint_info();\n";
        str += "    if(argc >= 4) {\n";
        str += "        FILE* f = freopen (argv[3],\"w\",stdout);\n";
        str += "        __report_file_path = argv[3];\n";
        str += "        __redirecting_cout = true;\n";
        str += "        printCopyRight();\n";
        str += "        print_checkpoint_info();\n";
        str += "    }\n";
        str += "    if(argc >= 5) {\n";
        str += "        __redirecting_progress = true;\n";
        str += "        __progress_file_path = argv[4];\n";
        str += "        __progress_file.precision(1);\n";
        str += "        __progress_file.setf(ios::fixed,ios::floatfield);\n";
        str += "        clog << \"Standard output is redirected to file `\" << argv[4] << \"'.\" << endl;\n";
        str += "    }\n";
        str += "    top_level_module = new "+top_level_module()->name+"();\n";
        str += "    __main_memory = top_level_module->"+main_memory_module()->name+";\n";
        str += "    initialize();\n";
        str += "    __read_checkpoint_configuration(argv[1]);\n";
        str += "    instructions.set_len(cpu_count);\n";
        //        str += "    instruction_injecting_time = new double[cpu_count];\n";
        //        str += "    for(int i=0; i<cpu_count; i++) instruction_injecting_time[i] = 0;\n";
        //        str += "    latency = new double[cpu_count];\n";
        //        str += "    for(int i=0; i<cpu_count; i++) latency[i] = 0;\n";
        str += "    cpu_time = new double[cpu_count];\n";
        str += "    cpu_message = new message_t[cpu_count];\n";
        str += "    cpu_steps = new uint64[cpu_count];\n";
        str += "    for(int i=0; i<cpu_count; i++) { cpu_time[i]=0; cpu_steps[i]=0; }\n";
        str += "    offset *= cpu_count;\n";
        str += "    __initialize_modules();\n";
        str += "    __run_simulation();\n";
    } else {
        str += "    cpu_count = 4;\n";
        str += "    top_level_module = new "+top_level_module()->name+"();\n";
        str += "    __main_memory = top_level_module->"+main_memory_module()->name+";\n";
        str += "    if(argc > 1) \n";
        str += "        total_steps = str2dec(argv[1]);\n";
        str += "    else { \n";
        str += "        cout << argv[0] << \" <total steps> [input file] [-prog]\" << endl;\n";
        str += "        simulation_finished = true;\n";
        str += "    }\n";
        str += "    initialize();\n";
        //		str += "    cycles = new uint64[cpu_count];\n";
        //        str += "    for(int k=0; k<cpu_count; k++) cycles[k] = 0;\n";
        //        str += "    instruction_injecting_time = new double[cpu_count];\n";
        //        str += "    for(int i=0; i<cpu_count; i++) instruction_injecting_time[i] = 0;\n";
        //        str += "    latency = new double[cpu_count];\n";
        //        str += "    for(int i=0; i<cpu_count; i++) latency[i] = 0;\n";
        str += "    cpu_steps = new uint64[cpu_count];\n";
        str += "    cpu_time = new double[cpu_count];\n";
        str += "    cpu_message = new message_t[cpu_count];\n";
        str += "    for(int i=0; i<cpu_count; i++) { cpu_time[i]=0; cpu_steps[i]=0; }\n";
        str += "    instructions.set_len(cpu_count);\n";
        str += "    offset *= cpu_count;\n";
        str += "    __initialize_modules();\n";
        str += "    if(!simulation_finished) \n";
        str += "    if(argc > 2) {\n";
        str += "        if(argc > 3) {\n";
        str += "            if(!strcmp(argv[3], \"-prog\")) {\n";
        str += "                //__pause_when_finished = true;\n";
        if(macros["STEP"].empty() && macros["VERBOSE"].empty())
            str += "                __show_progress = true;\n";
        str += "            }\n";
        str += "        }\n";
        str += "        ifstream in(argv[2]);\n";
#if defined (__WIN32__)
        str += "        if (in.is_open()) {\n";
        str += "           run(in);\n";
        str += "           in.close();\n";
        str += "        } else\n";
        str += "           cerr << \"Unable to load file.\" << endl;\n";
        str += "    } else\n";
        str += "        run(cin);\n";
#else
        str += "        int length = 0;\n";
        str += "        in.seekg (0, ios::end);\n";
        str += "        length = in.tellg();\n";
        str += "        int cnt = 0;\n";
        str += "        __current_value = 0;\n";
        str += "        __sim_length = length;\n";
        str += "        int step = __sim_length/1000;\n";
        str += "        if(__sim_length <= 10000000) step = __sim_length/100;\n";
        str += "        if(__sim_length >= 100000000) step = __sim_length/10000;\n";
        str += "        if(__sim_length >= 1000000000) step = __sim_length/100000;\n";
        str += "        clog.precision(1);\n";
        str += "        clog.setf(ios::fixed,ios::floatfield);\n";
        str += "        __showing_progress = __show_progress;\n";
        str += "        in.seekg (0, ios::beg);\n";
        str += "        if (in.is_open()) {\n";
        str += "            char buffer[4000];\n";
        //str += "            time_t __starting_time = time(0);\n";
        str += "            while (in.good()){\n";
        str += "                in.getline(buffer, 4000, '\\n');\n";
        str += "                if(simulation_finished) break;\n";
        str += "                else process(buffer);\n";
        str += "                if(__show_progress) {\n";
        str += "                    if(cnt>step) {\n";
        str += "                        __current_value = in.tellg();\n";
        str += "                        if(__current_value < __sim_length) { \n";
        str += "                            if(__redirecting_progress) { __progress_file.open(__progress_file_path.c_str()); __progress_file << \"Running simulation...\\nProgress: \" << setw(5) << right << (100*((float)__current_value))/((float)__sim_length) << '%' << endl << __show_elapsed_time(__starting_time, true, '\\n'); __progress_file.close();} else\n";
        str += "                            clog << \"\\rProgress: \" << setw(5) << right << (100.0*(float)__current_value/((float)__sim_length)) << '%' << setw(terminal_width()-16) << right << __show_elapsed_time(__starting_time);\n";
        str += "                        } cnt = 0;\n";
        str += "                    } else cnt++;\n";
        str += "                }\n";
        str += "            }\n";
        str += "            if(__show_progress) {\n";
        str += "                 __showing_progress = false; }\n";
        str += "            if(__redirecting_progress) { __progress_file.open(__progress_file_path.c_str()); __progress_file << ((__application_terminated)?\"Terminated by user.\":\"Simulation Completed.\") << endl << __show_elapsed_time(__starting_time, false) << endl; __progress_file.close();} else\n";
        str += "            clog << \"\\rSimulation Completed.\" << setw(terminal_width()-20) << right << __show_elapsed_time(__starting_time, false) << endl;\n";
        str += "        } else\n";
        str += "            cerr << \"Unable to load file.\" << endl;\n";
        str += "    } else {\n";
        str += "        string buffer(\"\");\n";
        str += "        while (getline(cin,buffer)) {\n";
        str += "            process(buffer);\n";
        str += "            if(simulation_finished) break;\n";
        str += "        }\n";
        str += "    }\n";
#endif
    }
    if(multithreaded) {
        char cstr[54];
        str += "    void *ret_join;\n";
        sprintf(cstr, "    for(int i=0; i<%d; i++) {\n", number_of_threads);
        str += cstr;
        str += "        if(__thread_started[i]) {\n";
        str += "            if(pthread_join(__threads[i], &ret_join) != 0) {\n";
        str += "                int res=system(\"tput cnorm\");\n";
        str += "                if(__application_terminated) kill(0, SIGTERM);\n";
        str += "                exit(EXIT_FAILURE);\n";
        str += "            }\n";
        str += "        }\n";
        str += "    }\n";
    }
    str += "        __successfull = true;\n";
    str += "    } catch(...) {\n";
    str += "        cerr << \"Simulation terminated\" << endl;\n";
    str += "        __successfull = false;\n";
    str += "    }\n";
    if(!macros["DISPLAY_PIPELINE"].empty())
        str += "    __pipeline_file.close();\n";
    str += "    clog << \"Total time taken: \" << __show_elapsed_time(__total_starting_time, false) << endl;\n";
    str += "    print_report();\n";

    str += "    cout << __sig_message.str().c_str();\n";

    str += "    if(argc >= 4) {\n";
    str += "        fclose(stdout);\n";
    str += "    }\n";
    str += "    if(argc >= 5) {\n";
    str += "        __progress_file.close();\n";
    str += "    }\n";
    if(!automatic_simulation) {
        str += "    if (__pause_when_finished) {\n";
        str += "        clog << endl << \"Press any key to exit...\" << endl;\n";
        str += "        getch();\n";
        str += "    }\n";
    }
#if defined (__WIN32__)
    str += "    FlushConsoleInputBuffer( hstdin );\n";
    str += "    SetConsoleTextAttribute( hstdout, csbi.wAttributes );\n";
#else
    str += "    int res=system(\"tput cnorm\");\n";
#endif
    str += "    if(__application_terminated) kill(0, SIGTERM);\n";
    str += "    return 0;\n";
    str += "}\n";
}

