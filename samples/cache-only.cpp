/**
* @file  cache-only
*
*  Generate by M-Simulator Compiler V 1.0.0
*    __    __
*   /_/\  /_/|
*  |  \ \/  ||  High-Performance Computing Architectures and Networks (HPCAN)
*  |   \/   ||  Sharif University of Technology
*  | |\  /| ||  Version: M-Simulator 1.0.0
*  |_|/\/ |_|/
*   Simulator
* 
*/

#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cmath>
#include <signal.h>
#include <set>
#include <map>
#include <time.h>
#include <sstream>
#include <string>

#include <sys/ioctl.h>
#include <termios.h>
#include <stdio.h>

static struct termios __old_termios, __new_termios;

void initTermios(int echo) {
    tcgetattr(0, &__old_termios); 
    __new_termios = __old_termios; 
    __new_termios.c_lflag &= ~ICANON; 
    __new_termios.c_lflag &= echo ? ECHO : ~ECHO; 
    tcsetattr(0, TCSANOW, &__new_termios);
}

void resetTermios(void) {
    tcsetattr(0, TCSANOW, &__old_termios);
}

int terminal_width() {
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    return w.ws_col;
}
char getch_(int echo) {
  char ch;
  initTermios(echo);
  ch = getchar();
  resetTermios();
  return ch;
}

char getch(void) {
  return getch_(0);
}

char getche(void) {
  return getch_(1);
}
#define int8  char
#define int16 short
#define int32 long
#define int64 long long
#define uint8  unsigned char
#define uint16 unsigned short
#define uint32 unsigned long
#define uint64 unsigned long long

#define _address unsigned long long
#define byte unsigned char
#define _data byte
#define bit  bool
#define boolean  bool

using namespace std;

typedef struct {
    double start, end;
    bool valid;
} timeslot_t;

typedef struct {
    uint64 block_addr;
    double end;
    bool valid;
} att_entry_t;

bool simulation_finished = false;
bool __redirecting_cout = false;
bool __redirecting_progress = false;
bool __one_trace_performed = false;
bool __application_terminated = false;
uint64 iterations = 0; 
uint16 cpu_count = 0; 
double latency; 
double __machine_time=0; 
double* cpu_time; 
double energy = 0; 
double frequency = 0; 
uint64 __current_value = 0;
uint64 __sim_length = 0;
int access_type = 0; 
int cpu_number = 0; 
int transaction_size = 4; 
uint64 address = 0;
uint32 offset = 0; 
uint64 sim_step = 0;
uint64 total_steps = 0;
uint64* cpu_steps;
uint32 exceptions = 0; 
uint64 memory_size = 0; 
uint64 unmatched = 0; 
uint64 matched = 0; 
uint32 memory_address_align = 27;


class typical_module;
typical_module* __main_memory = NULL;
bool __tracing = true;
bool __itracing = true;
ofstream __progress_file;
string __progress_file_path, __report_file_path;

typedef enum {ttInstFetch=0, ttDataRead=1, ttDataWrite=2, ttException=3, ttAddToTLB=4, ttRemoveFromTLB=5, ttNone=6} transaction_type_t;
typedef enum {itNone=0, itALU=1, itBranch=2, itLoad=4, itStore=8, itUndefined=16} message_type_t;

typedef struct {
    unsigned char cpu_num;
    transaction_type_t type;
    uint64 virtual_address;
    uint64 physical_address;
    unsigned char size;
    union
    {
        uint64 exp_code;
    };
} transaction_t;

typedef struct {
    message_type_t type;
    uint16 data_count;   // ready to extract
    transaction_t data[9];   // for load and store instructions
    transaction_t tlb[2];   // for tlb update transactions
    uint64 target_address;   // for branch instructions
    bool ready;   // ready to extract
    uint16 size;   // number of data transactions
    uint8 tlb_size;   // number of tlb transactions
} message_t;

transaction_t __current_transaction;
message_t* cpu_message;

template <typename type, int __size>
class rrqueue_t {
private:
    type data[__size];
    int _head, _tail;
public:
    rrqueue_t() { _head = 0; _tail = 0; }
    void clear() { _head = 0; _tail = 0; }
    bool full() { return (_tail+1 == _head) || (_tail == __size-1 && _head == 0); }
    bool empty() { return _head == _tail; }
    int size() { return _tail-_head+((_tail < _head)?__size:0); }
    type& first() { return data[_head]; }
    type& last() { if(_tail > 0) return data[_tail-1]; else return data[__size-1]; }
    void push_front(type val) { if(_head > 0) data[(--_head)] = val; else data[(_head = __size-1)] = val; }
    void push_back(type val) { if(_tail < __size-1) data[(_tail++)] = val; else {data[__size-1] = val; _tail = 0;} }
    type& pop_front() { if(_head < __size-1) return data[(_head++)]; else {_head = 0; return data[__size-1];}; }
    type& pop_back() { if(_tail > 0) return data[(--_tail)]; else return data[(_tail = __size-1)]; }
    type& operator[](int index) { return data[ (_head+index)%__size ]; }
};  

template <int __q_size>
class instructions_t{
private:
    rrqueue_t<message_t, __q_size> *q;
public:
    void set_len(int cpu_count) { q = new rrqueue_t<message_t, __q_size>[cpu_count]; }
    rrqueue_t<message_t, __q_size>& operator[](int cpu_index) {
        return dynamic_cast< rrqueue_t<message_t, __q_size>& >(q[cpu_index]);
    }
};

instructions_t<10> instructions;
inline uint64 cycle() {
    return (uint64)((latency)*frequency);
}

inline double elapsed_time() {
    return latency;
}

inline double system_time() {
    return __machine_time;
}

inline double machine_time() {
    return __machine_time;
}

inline uint64 total_latency() {
    return (uint64)((__machine_time)*frequency);
}

double find_first_free_timeslot(double _min, double _max, double len, timeslot_t* btst) {
    static double one_cycle = 1/frequency;
    if(_max <= _min) return _min;
    if(_max - _min < len) return _max;
    for(int i=0; i<64; i++) 
        if(btst[i].valid)
            if((btst[i].start >= _min && btst[i].start < _min+len) || (btst[i].end > _min && btst[i].end <= _min+len))
                return find_first_free_timeslot(btst[i].end+one_cycle, _max, len, btst);
    return _min;
}

int add_busy_timeslot(double start, double end, timeslot_t* btst, int &index) {
    index = (index+1)%64;
    btst[index].valid = true;
    btst[index].start = start;
    btst[index].end = end;
    return index;
}

double time_to_availability(double time, _address block_address, att_entry_t* att) {
    double result = time;
    for(int i=0; i<64; i++) {
        if(att[i].block_addr == block_address)
            if(att[i].end > result)
                result = att[i].end;
    }
    return result;
}

void add_unavailable_timeslot(att_entry_t* att, _address block_address, double end, int &index) {
    index = (index+1)%64;
    att[index].block_addr = block_address;
    att[index].end = end;
}

void print_meaningful_section(transaction_t t, const char* buf, bool new_line=true);
int digit(char c) {
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return 10+c-'a';
    return -1;
}

uint64 ullabs(long long val) {
    if(val < 0)
        return -val;
    return val;
}

inline uint64 hex2dec(string str) {
    uint64 res=0;
    if(str.length()==0)
        return 0;
    int i=0;
    while((str[i] == '0' || str[i] == 'x') && (i < str.length())) i++;
    for(; digit(str[i])>-1; i++)
         res = res*16+digit(str[i]);
    return res;
}

inline uint64 str2dec(string str) {
    uint64 res=0;
    if(str.length()==0)
        return 0;
    for(int i=0; digit(str[i])>-1; i++)
        res = res*10+digit(str[i]);
    return res;
}

void extract_transaction_data(string &str, transaction_t &t) {
    std::size_t c_start = str.find("CPU");
    if (c_start!=std::string::npos) {
        c_start += 3;
        std::size_t c_end = str.find("<", c_start+1);
        char cpu_b[20] = {0};
        str.copy(cpu_b, c_end-c_start, c_start);
        t.cpu_num = strtol(cpu_b, NULL, 0);
    }
    std::size_t v_start = str.find("<v:");
    if (v_start!=std::string::npos) {
        v_start += 3;
        std::size_t v_end = str.find(">", v_start+1);
        char v_addr_b[20] = {0};
        str.copy(v_addr_b, v_end-v_start, v_start);
        t.virtual_address = strtoull(v_addr_b, NULL, 0);
    }
    std::size_t p_start = str.find("<p:");
    if (p_start!=std::string::npos) {
        p_start += 3;
        std::size_t p_end = str.find(">", p_start+1);
        char p_addr_b[20] = {0};
        str.copy(p_addr_b, p_end-p_start, p_start);
        t.physical_address = strtoull(p_addr_b, NULL, 0);
    }
}

void extract_tlb_transaction_data(string &str, transaction_t &t)
{
    char buff[64];
    str.copy(buff, 57, 54);
    istringstream iss(buff);
    uint64 v=0, p=0;
    int token_num = 0;
    do {
        token_num++;
        string sub;
        iss >> sub;
        if(token_num < 6 && token_num > 1) {
            v = (v<<16) + strtoull(sub.c_str(), NULL, 16);
        } else if(token_num > 9 && token_num < 14) {
            p = (p<<16) + strtoull(sub.c_str(), NULL, 16);
        }
    } while (iss);
    t.virtual_address = v;
    t.physical_address = p;
}

int __last_transaction_cpu_num=0;
transaction_t parse(string line)
{
    istream in(line);
    char tp[6];
    uint64 adr;
    int size, val;
    transaction_t t;

    in >> tp >> adr >> size;
    if(!strcmp(tp, "store")) {
        in >> hex >> val;
        t.type = ttDataWrite;
    } else {
        t.type = ttDataRead;
    }
    t.cpu_num = 0;
    t.size = (unsigned char) size;
    t.physical_address = adr;
    t.virtual_address = adr;

    return t;
}
void __remove_char(char* str, char ch) {
    for(int i=0; str[i]; i++)
        if(str[i] == ch) {
            for(int j=i; str[j]; j++)
                str[j] = str[j+1];
        }
}

void __top_level_step();

template <typename K, typename V>
class dmap: public map<K, V> {
private:
    V default_value;
public:
    dmap<K, V>() {}
    V& operator[](const K& key) {
        typename map <K, V> :: iterator it = this->find( key );
        if ( it == this->end() ) {
             pair<typename map <K, V> :: iterator, bool> __i = this->insert(pair<K, V>(key, default_value));
             (*this)[key] = default_value;
            return __i.first->second;
        } else {
            return it->second;
        }
    }
    void set_default(V def) {
        this->default_value = def;
    }
};

string __simics_checkpoint = "";
string __simics_cycles = "";
void printCopyRight()
{
 cout << "     __    __" << endl;
 cout << "    /_/\\  /_/|" << endl; 
 cout << "   |  \\ \\/  ||  High-Performance Computing Architectures and Networks (HPCAN)" << endl; 
 cout << "   |   \\/   ||  Sharif University of Technology" << endl; 
 cout << "   | |\\  /| ||  http://hpcan.ce.sharif.edu " << endl; 
 cout << "   |_|/\\/ |_|/\n    Simulator   Version: 1.0.15" << endl << endl; 
 cout << "   http://m-sim.org ( contact@m-sim.org )" << endl; 
 cout << "   Copyright (c) 2013 by HPCAN-Sharif University of Technology." << endl << endl; 
}

bool __pause_when_finished = false;
bool __show_progress = false;
bool __showing_progress = false;

time_t __starting_time;
time_t ___starting_time;
char* __show_remaining_time(time_t seconds)
{
    static char t[64] = "";
    if(__current_value == 0) { return t; }
    if(__sim_length == 0) return t;
    double ratio = ((double)__current_value)/((double)__sim_length);
    time_t remaining = (time_t)((1.0/ratio)*(1.0-ratio)*((double)seconds));
    sprintf(t, "Remaining: %d:%02d:%02d", (int)(remaining/3600), (int)(remaining/60)%60, (int)(remaining)%60);
    if(__redirecting_progress) 
        sprintf(t, "Remaining: %d:%02d:%02d\nStart: %lld", (int)(remaining/3600), (int)(remaining/60)%60, (int)(remaining)%60, (uint64)(___starting_time));
    return t;
}

char* __show_elapsed_time(time_t starting_time, bool show_remaining = true, char rem_sep = ' ')
{
    static char t[64];
    ___starting_time = starting_time;
    time_t seconds=time(0)-starting_time;
    int h = (seconds/3600);
    if(show_remaining)
        sprintf(t, "Elapsed: %d:%02d:%02d%c%s", h, (int)(seconds/60)%60, (int)(seconds)%60, rem_sep, __show_remaining_time(seconds));
    else
        sprintf(t, "%d:%02d:%02d", h, (int)(seconds/60)%60, (int)(seconds)%60);
    return t;
}


#define STDLIB
char * display_number ( double d , const char * unit ) {
    char * str = new char [ 128 ];
    char dim = ' ';
    if ( d > 1e12 ) dim = 'T';
    else if ( d > 1e9 ) dim = 'G';
    else if ( d > 1e6 ) dim = 'M';
    else if ( d > 1e3 ) dim = 'K';
    d /= 1000;
    if ( d < 1e-15 ) dim = 'f';
    else if ( d < 1e-12 ) dim = 'p';
    else if ( d < 1e-9 ) dim = 'n';
    else if ( d < 1e-6 ) dim = 'u';
    else if ( d < 1e-3 ) dim = 'm';
    switch ( dim ) {
        case 'T' : d /= 1e12;
        break;
        case 'G' : d /= 1e9;
        break;
        case 'M' : d /= 1e6;
        break;
        case 'K' : d /= 1e3;
        break;
        case 'f' : d *= 1e15;
        break;
        case 'p' : d *= 1e12;
        break;
        case 'n' : d *= 1e9;
        break;
        case 'u' : d *= 1e6;
        break;
        case 'm' : d *= 1e3;
        break;
    }
    d *= 1000;
    if ( dim == ' ' )
    sprintf ( str , "%.3f %s" , d , unit );
    else
    sprintf ( str , "%.3f %c%s" , d , dim , unit );
    return str;
}
#define CACHES_H
#define DIRECTORY_H
#define MODIFIED  4
#define OWNED     3
#define EXCLUSIVE 2
#define SHARED    1
#define INVALID   0
const char * state_to_string ( uint8 state ) {
    switch ( state ) {
        case 4 : return "\033[0;33mMODIFIED\033[0m";
        case 3 : return "\033[0;33mOWNED\033[0m";
        case 2 : return "\033[0;33mEXCLUSIVE\033[0m";
        case 1 : return "\033[0;33mSHARED\033[0m";
        case 0 : return "\033[0;31mINVALID\033[0m";
    }
    return "UNKNOWN";
}
#define MAIN_MEMORY
#define CORE
#include <fstream>
double my_latency = 0;
double avg_latency = 0;
uint64 classes [ 4 ] = {
    0 , 0 , 0 , 0
}
;
double one_cycle = 1;
void initialize ( ) {
    offset = total_steps / 2;
    frequency = 2500000000;
    one_cycle = 0.99999 / frequency;
}
class typical_system {
    public: char* name;
    bool search_enable;
    virtual void init() {
        search_enable = true;
    }
    typical_system () {
        this->name = new char[10];
        strcpy(this->name, "Unknown");
        init();
        delete[] this->name;
    }
    typical_system (const char* n) {
        this->name = new char[strlen(n)+1];
        strcpy(this->name, n);
        init();
    }
    virtual void tlb_add ( int core_id , _address v , _address p ) {} 
    virtual void tlb_remove ( int core_id , _address v , _address p ) {} 
    virtual void tlb_lookup ( int core_id , _address v ) {} 
};
class typical_module {
    public: char* name;
    bool search_enable;
    double read_block_exiting_time;
    timeslot_t access_busy_timeslots[64];
    int access_timeslot_index;
    att_entry_t updating_blocks_table[64];
    int updating_blocks_table_index;
    double write_block_exiting_time;
    uint64 read_hit;
    uint64 write_hit;
    uint64 read_miss;
    uint64 write_miss;
    virtual void init() {
        search_enable = true;
        read_hit = 0;
        write_hit = 0;
        read_miss = 0;
        write_miss = 0;
    }
    typical_module () {
        this->name = new char[10];
        strcpy(this->name, "Unknown");
        init();
        delete[] this->name;
    }
    typical_module (const char* n) {
        this->name = new char[strlen(n)+1];
        strcpy(this->name, n);
        init();
    }
    virtual bool lookup ( _address addr ) {
        return true;
    }
    virtual void read_block ( _address addr , _data * dst_block , uint32 size ) {
    }
    virtual void write_block ( _address addr , _data * src_block , uint32 size ) {
    }
    virtual void set_data ( _address addr , _data d ) {
    }
    virtual _data get_data ( _address addr ) {
    }
    virtual void replace ( _address addr ) {
    }
    virtual void print_block ( _address addr ) {
    }
    virtual _address block_address ( _address addr ) {
    }
    virtual void pump(transaction_t transaction) {
    }
};
class typical_memory : public typical_module {
    public:uint64 read_hit;
    uint64 write_hit;
    uint64 read_miss;
    uint64 write_miss;
    virtual void init() {
        search_enable = true;
    }
    typical_memory () {
        this->name = new char[10];
        strcpy(this->name, "Unknown");
        init();
        delete[] this->name;
    }
    typical_memory (const char* n) {
        this->name = new char[strlen(n)+1];
        strcpy(this->name, n);
        init();
    }
    virtual bool lookup ( _address addr ) {} 
    virtual void read_block ( _address addr , _data * dst_block , uint32 size ) {} 
    virtual void write_block ( _address addr , _data * src_block , uint32 size ) {} 
    virtual void set_data ( _address addr , _data d ) {} 
    virtual _data get_data ( _address addr ) {} 
    virtual void replace ( _address addr ) {} 
    virtual void print_block ( _address addr ) {} 
    virtual _address block_address ( _address addr ) {} 
};
class typical_cache : public typical_module {
    public:uint64 read_hit;
    uint64 write_hit;
    uint64 read_miss;
    uint64 write_miss;
    typical_module *prev_level;
    int cache_index;
    virtual void init() {
        search_enable = true;
        read_hit = 0;
        write_hit = 0;
        read_miss = 0;
        write_miss = 0;
        cache_index = -1;
    }
    typical_cache () {
        this->name = new char[10];
        strcpy(this->name, "Unknown");
        init();
        delete[] this->name;
    }
    typical_cache (const char* n) {
        this->name = new char[strlen(n)+1];
        strcpy(this->name, n);
        init();
    }
    virtual bool lookup ( _address addr ) {
        return true;
    }
    virtual void read_block ( _address addr , _data * dst_block , uint32 size ) {
    }
    virtual void write_block ( _address addr , _data * src_block , uint32 size ) {
    }
    virtual void set_data ( _address addr , _data d ) {
    }
    virtual _data get_data ( _address addr ) {
    }
    virtual void replace ( _address addr ) {
    }
    virtual void print_block ( _address addr ) {
    }
    virtual _address block_address ( _address addr ) {
    }
    virtual void pump(transaction_t transaction) {
    }
};
class set_associative : public typical_cache {
    public:double read_block_exiting_time;
    timeslot_t access_busy_timeslots[64];
    int access_timeslot_index;
    att_entry_t updating_blocks_table[64];
    int updating_blocks_table_index;
    double write_block_exiting_time;
    uint64 read_hit;
    uint64 write_hit;
    uint64 read_miss;
    uint64 write_miss;
    int block_index;
    uint32 evictions;
    virtual void init() {
        search_enable = true;
        read_block_exiting_time = 0;
        write_block_exiting_time = 0;
        access_timeslot_index = 0;
        for(int __i=0; __i<64; __i++) {
            access_busy_timeslots[__i].valid = false;
        }
        updating_blocks_table_index = 0;
        for(int __i=0; __i<64; __i++) {
            updating_blocks_table[__i].end = 0;
        }
    }
    set_associative () {
        this->name = new char[10];
        strcpy(this->name, "Unknown");
        init();
        delete[] this->name;
    }
    set_associative (const char* n) {
        this->name = new char[strlen(n)+1];
        strcpy(this->name, n);
        init();
    }
    virtual _data get_data ( _address addr ) {} 
    virtual bool lookup ( _address addr ) {} 
    virtual int search ( _address & addr ) {} 
    virtual void evict ( int set_idx , int block_idx ) {} 
    virtual void update ( _address addr , int block_idx ) {} 
    virtual void replace ( _address addr ) {} 
    virtual void read_block ( _address addr , _data * dst_block , uint32 size ) {} 
    virtual void write_block ( _address addr , _data * src_block , uint32 size ) {} 
    virtual void report ( ) {} 
    virtual void set_data ( _address addr , _data d ) {} 
    virtual _address block_address ( _address addr ) {} 
    virtual void print_block ( _address addr ) {} 
};
class main_memory : public typical_memory {
    public:double read_block_exiting_time;
    timeslot_t access_busy_timeslots[64];
    int access_timeslot_index;
    att_entry_t updating_blocks_table[64];
    int updating_blocks_table_index;
    double write_block_exiting_time;
    typedef struct {
        double write_waiting_time;
    } bank_t;
    dmap < uint64, bank_t > bank;
    typedef struct {
        _address physical_address;
        bool accessed;
    } TLB_t;
    dmap < uint64, TLB_t > TLB;
    typedef struct {
        _address virtual_address;
    } RTLB_t;
    dmap < uint64, RTLB_t > RTLB;
    uint64 read_hit;
    uint64 write_hit;
    uint64 read_miss;
    uint64 write_miss;
    _address memory_address;
    void init() {
        search_enable = true;
        read_hit = 0;
        write_hit = 0;
        read_miss = 0;
        write_miss = 0;
        bank_t bank_default_value;
        bank_default_value.write_waiting_time = 0;
        bank.set_default(bank_default_value);
        TLB_t TLB_default_value;
        TLB_default_value.accessed = 0;
        TLB.set_default(TLB_default_value);
    }
    main_memory () {
        this->name = new char[10];
        strcpy(this->name, "Unknown");
        init();
        delete[] this->name;
    }
    main_memory (const char* n) {
        this->name = new char[strlen(n)+1];
        strcpy(this->name, n);
        init();
    }
    _data get_data ( _address addr ) {
    }
    void replace ( _address addr ) {
    }
    bool lookup ( _address addr ) {
        return true;
    }
    double wait_for_resource ( _address addr ) {
        double epoch = bank [ ((addr >> 32)) ] . write_waiting_time;
        if ( elapsed_time ( ) < epoch ) {
            bank [ ((addr >> 32)) ] . write_waiting_time = 0;
            return epoch - elapsed_time ( );
        }
        return 0;
    }
    void evaluate_waiting_time ( _address addr , double write_latency ) {
        double epoch = bank [ ((addr >> 32)) ] . write_waiting_time;
        if ( elapsed_time ( ) < epoch ) {
            epoch -= elapsed_time ( );
        } else
        epoch = 0;
        bank [ ((addr >> 32)) ] . write_waiting_time = elapsed_time ( ) + epoch + write_latency;
    }
    void read_block ( _address addr , _data * dst_block , uint32 size ) {
        if(sim_step >= offset) {
            if(latency == 0) latency = read_block_exiting_time;
            else latency = max(read_block_exiting_time, latency);
            latency = time_to_availability(latency, block_address(addr), updating_blocks_table);
        }
        int saved_access_type = access_type;
        access_type = ttDataRead;
        if(search_enable) pump(__current_transaction);
        if(!lookup(addr)) {
            if(search_enable) {
                if(sim_step >= offset) read_miss++;
                search_enable = false;
                replace(addr);
                search_enable = true;
            }
        }
         else if(search_enable && sim_step >= offset) read_hit++;
        if(sim_step >= offset) {
            latency = time_to_availability(latency, block_address(addr), updating_blocks_table) + ( wait_for_resource ( memory_address ) + 1.21e-07);
            energy += 2e-12;
        }
        addr = memory_address;
        {}  if(sim_step >= offset) {
            latency = ceil(latency*frequency)/frequency;
            if(latency > read_block_exiting_time) read_block_exiting_time = latency;
        }
        access_type = saved_access_type;
    }
    void write_block ( _address addr , _data * src_block , uint32 size ) {
        if(sim_step >= offset) {
            if(latency == 0) latency = write_block_exiting_time;
            else latency = max(write_block_exiting_time, latency);
            latency = time_to_availability(latency, block_address(addr), updating_blocks_table);
        }
        int saved_access_type = access_type;
        access_type = ttDataWrite;
        if(search_enable) pump(__current_transaction);
        if(!lookup(addr)) {
            if(search_enable) {
                if(sim_step >= offset) write_miss++;
                search_enable = false;
                replace(addr);
                search_enable = true;
            }
        }
         else if(search_enable && sim_step >= offset) write_hit++;
        if(sim_step >= offset) {
            latency = time_to_availability(latency, block_address(addr), updating_blocks_table) + ( 1e-09);
            energy += 2e-12;
        }
        addr = memory_address;
        {} 
        if ( sim_step >= offset )
        evaluate_waiting_time ( addr , wait_for_resource ( addr ) + 1.21e-07 );
        if(sim_step >= offset) {
            latency = ceil(latency*frequency)/frequency;
            if(latency > write_block_exiting_time) write_block_exiting_time = latency;
        }
        access_type = saved_access_type;
    }
    void set_data ( _address addr , _data d ) {
    }
    _address block_address ( _address addr ) {
        return addr >> 7;
    }
    void print_block ( _address addr ) {
        _address vaddr = addr;
        int w_idx = ((addr) & 0x7f);
        uint64 b_idx = ((addr >> 32)) , p_idx = ((addr >> 14) & 0x3ffff) , l_idx = ((addr >> 7) & 0x7f);
        printf ( "%s at v:0x%llx p:0x%llx (bank: %lld, page: %lld, block: %lld)" , name , vaddr , addr , b_idx , p_idx , l_idx );
        cout << endl;
    }
    void report ( ) {
    }
    virtual void pump(transaction_t transaction) {
    }
};
class typical_tlb {
    public: char* name;
    bool search_enable;
    typedef struct {
        _address physical;
    } table_t;
    dmap < uint64, table_t > table;
    void init() {
        search_enable = true;
    }
    typical_tlb () {
        this->name = new char[10];
        strcpy(this->name, "Unknown");
        init();
        delete[] this->name;
    }
    typical_tlb (const char* n) {
        this->name = new char[strlen(n)+1];
        strcpy(this->name, n);
        init();
    }
    void add ( _address v , _address p ) {
        table [ ((v >> 13)) ] . physical = ((p >> 13));
    }
    void remove ( _address v , _address p ) {
        table . erase ( ((v >> 13)) );
    }
    bool lookup ( _address v ) {
        if ( table . count ( ((v >> 13)) ) ) {
            return true;
        }
        return false;
    }
    _address translate ( _address v ) {
        return ( table [ ((v >> 13)) ] . physical << 13 ) | ((v) & 0x1fff);
    }
};
class typical_core {
    public: char* name;
    bool search_enable;
    typical_tlb *tlb;
    virtual void init() {
        search_enable = true;
    }
    typical_core () {
        this->name = new char[10];
        strcpy(this->name, "Unknown");
        init();
        delete[] this->name;
    }
    typical_core (const char* n) {
        this->name = new char[strlen(n)+1];
        strcpy(this->name, n);
        init();
    }
    virtual void exec ( message_t msg ) {} 
    virtual void set_data ( _address addr , _data d ) {} 
    virtual void tlb_add ( _address v , _address p ) {} 
    virtual void tlb_remove ( _address v , _address p ) {} 
    virtual bool tlb_lookup ( _address v ) {} 
    virtual _address translate ( _address v ) {} 
};
class system_t : public typical_system {
    public:
    class Cache : public set_associative {
        public:
        typedef struct {
            typedef struct {
                uint32 tag;
                bit valid;
                bit dirty;
                uint32 counter;
            } set_line_t;
            set_line_t line[4];
        } set_t;
        set_t set[256];
        uint64 read_hit;
        uint64 write_hit;
        uint64 read_miss;
        uint64 write_miss;
        void init() {
            search_enable = true;
            read_hit = 0;
            write_hit = 0;
            read_miss = 0;
            write_miss = 0;
            cache_index = -1;
            block_index = 0;
            evictions = 0;
            for(long __r1 = 0; __r1 < 256; __r1++) {
                for(long __r2 = 0; __r2 < 4; __r2++) {
                    set[__r1].line[__r2].tag = 0;
                    set[__r1].line[__r2].valid = 0;
                    set[__r1].line[__r2].dirty = 0;
                    set[__r1].line[__r2].counter = 0;
                }
            }
        }
        Cache () {
            this->name = new char[10];
            strcpy(this->name, "Unknown");
            init();
            delete[] this->name;
        }
        Cache (const char* n) {
            this->name = new char[strlen(n)+1];
            strcpy(this->name, n);
            init();
        }
        _data get_data ( _address addr ) {
        }
        bool lookup ( _address addr ) {
            for ( int i = 0;  i < 4;  i ++ ) {
                if ( ( ((addr >> 14)) == set [ ((addr >> 6) & 0xff) ] . line [ i ] . tag ) && ( set [ ((addr >> 6) & 0xff) ] . line [ i ] . valid ) ) {
                    block_index = i;
                    {} 
                    return true;
                }
            }
            {} 
            return false;
        }
        int search ( _address & addr ) {
            int index = -1;
            for ( int j = 0;  j < 4;  j ++ ) {
                if ( ! set [ ((addr >> 6) & 0xff) ] . line [ j ] . valid ) {
                    return j;
                }
            }
            uint32 max = 0;
            for ( int j = 0;  j < 4;  j ++ ) {
                if ( set [ ((addr >> 6) & 0xff) ] . line [ j ] . counter > max ) {
                    index = j;
                    max = set [ ((addr >> 6) & 0xff) ] . line [ j ] . counter;
                }
            }
            if ( index == -1 ) {
                return 0;
            }
            return index;
        }
        void evict ( int set_idx , int block_idx ) {
            {} 
            _address a = ( set_idx << ( 6 ) ) | ( set [ set_idx ] . line [ block_idx ] . tag << ( 8 + 6 ) );
            if ( sim_step >= offset ) evictions ++;
            prev_level -> write_block ( a , 0 , 64 );
        }
        void update ( _address addr , int block_idx ) {
            _data temp [ 64 ];
            prev_level -> read_block ( addr , temp , 64 );
            write_block ( addr , temp , 64 );
            set [ ((addr >> 6) & 0xff) ] . line [ block_idx ] . dirty = false;
        }
        void replace ( _address addr ) {
            block_index = search ( addr );
            int set_idx = ((addr >> 6) & 0xff);
            addr = ( addr >> 6 ) << 6;
            if ( set [ set_idx ] . line [ block_index ] . dirty && set [ set_idx ] . line [ block_index ] . valid )
            evict ( set_idx , block_index );
            set [ set_idx ] . line [ block_index ] . tag = ((addr >> 14));
            set [ set_idx ] . line [ block_index ] . valid = true;
            set [ set_idx ] . line [ block_index ] . counter = 0;
            set [ set_idx ] . line [ block_index ] . dirty = false;
            update ( addr , block_index );
        }
        void read_block ( _address addr , _data * dst_block , uint32 size ) {
            if(sim_step >= offset) {
                if(latency == 0) latency = read_block_exiting_time;
                else {
                    double last_latency = time_to_availability(latency, block_address(addr), updating_blocks_table);
                    latency = 0;
                    if(sim_step >= offset) {
                        latency += 0;
                    }
                    if(latency > 0) {
                        double timeslot = find_first_free_timeslot(last_latency, max(write_block_exiting_time, read_block_exiting_time), latency, access_busy_timeslots);
                        add_busy_timeslot(timeslot, timeslot+latency, access_busy_timeslots, access_timeslot_index);
                        latency = timeslot;
                    } else latency += last_latency;
                }
            }
            int saved_access_type = access_type;
            access_type = ttDataRead;
            if(search_enable) pump(__current_transaction);
            if(!lookup(addr)) {
                if(search_enable) {
                    if(sim_step >= offset) read_miss++;
                    search_enable = false;
                    replace(addr);
                    search_enable = true;
                }
            }
             else if(search_enable && sim_step >= offset) read_hit++;
            if(sim_step >= offset) {
                latency += 0;
                energy += 1e-11;
            }
            {} 
            int set_idx = ((addr >> 6) & 0xff);
            for ( int k = 0;  k < 4;  k ++ ) {
                set [ set_idx ] . line [ k ] . counter ++;
            }
            set [ set_idx ] . line [ block_index ] . counter = 0;
            int start = ((addr) & 0x3f);
            for ( int i = 0;  i < size;  i ++ ) {
                if ( i + start == 64 ) {
                    {} 
                    read_block ( addr + i , dst_block + i , size - i );
                    access_type = saved_access_type;
                    if(sim_step >= offset) {
                        latency = ceil(latency*frequency)/frequency;
                        if(latency > read_block_exiting_time) read_block_exiting_time = latency;
                    }
                    return;
                }
            }
            if(sim_step >= offset) {
                latency = ceil(latency*frequency)/frequency;
                if(latency > read_block_exiting_time) read_block_exiting_time = latency;
            }
            access_type = saved_access_type;
        }
        void write_block ( _address addr , _data * src_block , uint32 size ) {
            if(sim_step >= offset) {
                if(latency == 0) latency = write_block_exiting_time;
                else {
                    double last_latency = time_to_availability(latency, block_address(addr), updating_blocks_table);
                    latency = 0;
                    if(sim_step >= offset) {
                        latency += 0;
                    }
                    if(latency > 0) {
                        double timeslot = find_first_free_timeslot(last_latency, max(write_block_exiting_time, read_block_exiting_time), latency, access_busy_timeslots);
                        add_busy_timeslot(timeslot, timeslot+latency, access_busy_timeslots, access_timeslot_index);
                        latency = timeslot;
                    } else latency += last_latency;
                }
            }
            int saved_access_type = access_type;
            access_type = ttDataWrite;
            if(search_enable) pump(__current_transaction);
            if(!lookup(addr)) {
                if(search_enable) {
                    if(sim_step >= offset) write_miss++;
                    search_enable = false;
                    replace(addr);
                    search_enable = true;
                }
            }
             else if(search_enable && sim_step >= offset) write_hit++;
            if(sim_step >= offset) {
                latency += 0;
                energy += 1e-11;
            }
            {} 
            int set_idx = ((addr >> 6) & 0xff);
            for ( int k = 0;  k < 4;  k ++ ) {
                set [ set_idx ] . line [ k ] . counter ++;
            }
            set [ set_idx ] . line [ block_index ] . counter = 0;
            set [ set_idx ] . line [ block_index ] . dirty = true;
            int start = ((addr) & 0x3f);
            for ( int i = 0;  i < size;  i ++ ) {
                if ( i + start == 64 ) {
                    write_block ( addr + i , src_block + i , size - i );
                    access_type = saved_access_type;
                    if(sim_step >= offset) {
                        latency = ceil(latency*frequency)/frequency;
                        if(latency > write_block_exiting_time) write_block_exiting_time = latency;
                        add_unavailable_timeslot(updating_blocks_table, block_address(addr), latency, updating_blocks_table_index);
                    }
                    return;
                }
            }
            if(sim_step >= offset) {
                latency = ceil(latency*frequency)/frequency;
                if(latency > write_block_exiting_time) write_block_exiting_time = latency;
                add_unavailable_timeslot(updating_blocks_table, block_address(addr), latency, updating_blocks_table_index);
            }
            access_type = saved_access_type;
        }
        void report ( ) {
            long double total = read_miss + read_hit + write_hit + write_miss;
            if ( total > 0 ) {
                printf ( "%s.total-access = %lld\n" , name , read_miss + read_hit + write_hit + write_miss );
                printf ( "%s.read-total = %lld\n" , name , read_miss + read_hit );
                printf ( "%s.read-hit = %lld\n" , name , read_hit );
                printf ( "%s.read-miss = %lld\n" , name , read_miss );
                printf ( "%s.write-total = %lld\n" , name , write_miss + write_hit );
                printf ( "%s.write-hit = %lld\n" , name , write_hit );
                printf ( "%s.write-miss = %lld\n" , name , write_miss );
                printf ( "%s.hit-rate = %.2f%%\n" , name , ( float ) ( ( long double ) ( write_hit + read_hit ) / total * 100 ) );
            }
        }
        void set_data ( _address addr , _data d ) {
        }
        _address block_address ( _address addr ) {
            return addr >> 6;
        }
        void print_block ( _address addr ) {
            int w_idx = ((addr) & 0x3f);
            _address aaa = addr;
            addr = ( addr >> 6 ) << 6;
            uint32 s_idx = ((addr >> 6) & 0xff) , b_idx = block_index;
            printf ( "%s at 0x%llx (set: %ld, block: %ld)" , name , addr , s_idx , b_idx );
            cout << endl;
        }
        virtual void pump(transaction_t transaction) {
        }
    };
    class Core : public typical_core {
        public:Cache *cache;
        typical_tlb *tlb;
        void init() {
            search_enable = true;
        }
        Core () {
            this->name = new char[10];
            strcpy(this->name, "Unknown");
            init();
            delete[] this->name;
        }
        Core (const char* n) {
            this->name = new char[strlen(n)+1];
            strcpy(this->name, n);
            init();
        }
        void exec ( message_t msg ) {
        }
        void set_data ( _address addr , _data d ) {
        }
        void tlb_add ( _address v , _address p ) {
            tlb -> add ( v , p );
        }
        void tlb_remove ( _address v , _address p ) {
            tlb -> remove ( v , p );
        }
        bool tlb_lookup ( _address v ) {
            return tlb -> lookup ( v );
        }
        _address translate ( _address v ) {
            return tlb -> translate ( v );
        }
        virtual void pump(message_t message) {
            _data *block=NULL;
            transaction_t transaction;
            for(int i=0; i<message.tlb_size; i++) {
                if(message.tlb[i].type == ttAddToTLB) {
                    tlb_add(message.tlb[i].virtual_address, message.tlb[i].physical_address);
                } else {
                    tlb_remove(message.tlb[i].virtual_address, message.tlb[i].physical_address);
                }
            }
            if ( message.type&(itLoad|itStore) ) {
                 {
                    int di=0;
                    if(cpu_message[cpu_number].type & (itStore | itLoad))di=1;
                    __current_transaction = message.data[di];
                    if(!tlb_lookup(__current_transaction.virtual_address)) tlb_add(__current_transaction.virtual_address, __current_transaction.physical_address);
                    __current_transaction.physical_address = translate(__current_transaction.virtual_address);
                    __top_level_step();
                    transaction = __current_transaction;
                    block = new _data[transaction.size];
                    if(transaction.type == ttDataWrite) {
                        cache->write_block(transaction.physical_address, block, transaction.size);
                    } else {
                        cache->read_block(transaction.physical_address, block, transaction.size);
                    }
                    delete[] block;
                }
            }
        }
    };
    Core *core;
    main_memory *ram;
    void init() {
        search_enable = true;
        this->name = new char[9];
        strcpy(this->name, "system_t");
        core = new Core("core");
        core->cache = new Cache("core->cache");
        core->tlb = new typical_tlb("core->tlb");
        ram = new main_memory("ram");
        core->cache->prev_level = ram;
    }
    system_t () {
        this->name = new char[10];
        strcpy(this->name, "Unknown");
        init();
        delete[] this->name;
    }
    system_t (const char* n) {
        this->name = new char[strlen(n)+1];
        strcpy(this->name, n);
        init();
    }
    void tlb_add ( int core_id , _address v , _address p ) {
    }
    void tlb_remove ( int core_id , _address v , _address p ) {
    }
    void tlb_lookup ( int core_id , _address v ) {
    }
    void report ( ) {
        core -> cache -> report ( );
    }
    void __step() {
    }
    virtual void pump(message_t message) {
        _data *block=NULL;
        transaction_t transaction;
        iterations++;
        sim_step++;
        latency = 0;
        if (message.type & itStore) {
            access_type = ttDataWrite;
        } else {
            access_type = ttDataRead;
        }
        core->pump(message);
        if (sim_step >= offset) {
            cpu_time[cpu_number] = latency;
            if (cpu_number == cpu_count-1) {
                __machine_time = 0;
                for(int i=0; i<cpu_count; i++) {
                    if(__machine_time < cpu_time[i]) __machine_time = cpu_time[i];
                }
            }
            cpu_steps[cpu_number]++;
        }
    }
};
system_t* top_level_module;
void __initialize_modules() {
}
void __top_level_step() {
    top_level_module->__step();
}


bool __successfull = false;

void print_report() {
    if(!__successfull) return;
    cout << "--------------------------------<< Reports >>----------------------------------" << endl;
    top_level_module->report();
}

void pump(message_t inst) {
    if((int)inst.type == itNone)
        inst.type = itALU;
    cpu_number = inst.data[0].cpu_num;
    cpu_message[cpu_number] = inst;
    top_level_module->pump(inst);
}

void execute(transaction_t t) {
    rrqueue_t<message_t, 10> *q = &instructions[t.cpu_num];
    if(!q->empty()) {
        if(t.type == ttInstFetch) {
            if(ullabs((int64)q->last().data[0].virtual_address-(int64)t.virtual_address) > q->last().size) {
                q->last().type = (message_type_t)((int)q->last().type | itBranch);
                q->last().target_address = t.virtual_address;
            } 
            q->last().ready = true;
        } else if(t.type == ttDataRead) {
            uint16 s = q->last().data_count;
            if(s>8) { cerr << "*OVERFLOW* in data transaction size at step " << sim_step << endl; throw 36;}
            q->last().type = (message_type_t)((int)q->last().type | itLoad);
            q->last().data[s] = t;
            q->last().size += t.size;
            q->last().data_count++;
        } else if(t.type == ttDataWrite) {
            uint16 s = q->last().data_count;
            if(s>8) { cerr << "*OVERFLOW* in data transaction size at step " << sim_step << endl; throw 36;}
            q->last().type = (message_type_t)((int)q->last().type | itStore);
            q->last().data[s] = t;
            q->last().size += t.size;
            q->last().data_count++;
        } else if(t.type == ttAddToTLB || t.type == ttRemoveFromTLB) {
            uint8 s = q->last().tlb_size;
            q->last().tlb[s] = t;
            q->last().tlb_size ++;
        } else if(t.type == ttException) {
            q->last().ready = true;
        }
    }
    if(t.type == ttInstFetch) {
        message_t inst;
        inst.ready = false;
        inst.type = itNone;
        inst.data_count = 1;
        inst.data[0].type = ttNone;
        inst.data[0] = t;
        inst.size = t.size;
        inst.tlb_size = 0;
        q->push_back(inst);
        if(q->first().ready)
            pump(q->pop_front());
    }
}

void process(string buffer) {
        transaction_t __trans = parse(buffer);
        execute(__trans);
}

stringstream __sig_message;
void __manual_teminat(int sig) {
    if(simulation_finished) return;
    __sig_message << "*** Terminated by user at step " << sim_step << endl;    if(!__redirecting_cout) 
        int res=system("tput cnorm");
    exit(0);
}
int main(int argc, char* argv[])
{
    signal (SIGCHLD, SIG_IGN);
    signal (SIGINT, __manual_teminat);
    signal (SIGTERM, __manual_teminat);
    __starting_time = time(0);
    printCopyRight();
    static time_t __total_starting_time = time(0);
    if(argc < 4) int res=system("tput civis");
    try {
    cpu_count = 4;
    top_level_module = new system_t();
    __main_memory = top_level_module->ram;
    if(argc > 1) 
        total_steps = str2dec(argv[1]);
    else { 
        cout << argv[0] << " <total steps> [input file] [-prog]" << endl;
        simulation_finished = true;
    }
    initialize();
    cpu_steps = new uint64[cpu_count];
    cpu_time = new double[cpu_count];
    cpu_message = new message_t[cpu_count];
    for(int i=0; i<cpu_count; i++) { cpu_time[i]=0; cpu_steps[i]=0; }
    instructions.set_len(cpu_count);
    offset *= cpu_count;
    __initialize_modules();
    if(!simulation_finished) 
    if(argc > 2) {
        if(argc > 3) {
            if(!strcmp(argv[3], "-prog")) {
                //__pause_when_finished = true;
                __show_progress = true;
            }
        }
        ifstream in(argv[2]);
        int length = 0;
        in.seekg (0, ios::end);
        length = in.tellg();
        int cnt = 0;
        __current_value = 0;
        __sim_length = length;
        int step = __sim_length/1000;
        if(__sim_length <= 10000000) step = __sim_length/100;
        if(__sim_length >= 100000000) step = __sim_length/10000;
        if(__sim_length >= 1000000000) step = __sim_length/100000;
        clog.precision(1);
        clog.setf(ios::fixed,ios::floatfield);
        __showing_progress = __show_progress;
        in.seekg (0, ios::beg);
        if (in.is_open()) {
            char buffer[4000];
            while (in.good()){
                in.getline(buffer, 4000, '\n');
                if(simulation_finished) break;
                else process(buffer);
                if(__show_progress) {
                    if(cnt>step) {
                        __current_value = in.tellg();
                        if(__current_value < __sim_length) { 
                            if(__redirecting_progress) { __progress_file.open(__progress_file_path.c_str()); __progress_file << "Running simulation...\nProgress: " << setw(5) << right << (100*((float)__current_value))/((float)__sim_length) << '%' << endl << __show_elapsed_time(__starting_time, true, '\n'); __progress_file.close();} else
                            clog << "\rProgress: " << setw(5) << right << (100.0*(float)__current_value/((float)__sim_length)) << '%' << setw(terminal_width()-16) << right << __show_elapsed_time(__starting_time);
                        } cnt = 0;
                    } else cnt++;
                }
            }
            if(__show_progress) {
                 __showing_progress = false; }
            if(__redirecting_progress) { __progress_file.open(__progress_file_path.c_str()); __progress_file << ((__application_terminated)?"Terminated by user.":"Simulation Completed.") << endl << __show_elapsed_time(__starting_time, false) << endl; __progress_file.close();} else
            clog << "\rSimulation Completed." << setw(terminal_width()-20) << right << __show_elapsed_time(__starting_time, false) << endl;
        } else
            cerr << "Unable to load file." << endl;
    } else {
        string buffer("");
        while (getline(cin,buffer)) {
            process(buffer);
            if(simulation_finished) break;
        }
    }
        __successfull = true;
    } catch(...) {
        cerr << "Simulation terminated" << endl;
        __successfull = false;
    }
    clog << "Total time taken: " << __show_elapsed_time(__total_starting_time, false) << endl;
    print_report();
    cout << __sig_message.str().c_str();
    if(argc >= 4) {
        fclose(stdout);
    }
    if(argc >= 5) {
        __progress_file.close();
    }
    if (__pause_when_finished) {
        clog << endl << "Press any key to exit..." << endl;
        getch();
    }
    int res=system("tput cnorm");
    if(__application_terminated) kill(0, SIGTERM);
    return 0;
}

