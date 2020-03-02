
/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs 
 *  and could serve as the starting point for developing your first PIN tool
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <deque>
using std::cerr;
using std::string;
using std::endl;

/* ================================================================== */
// Global variables 
/* ================================================================== */
class BBLCache {
private:
    // double ended queue (can be iterated)
    std::deque<BBL*> blockCache;
    UINT32 _max_size;

    void pop() {
        blockCache.pop_back();
    }

public:
    UINT64 bblCount = 0;        // number of dynamically executed basic blocks
    UINT64 nMisses = 0;         // number of cache misses
    UINT64 nHits = 0;           // number of cache hits

    BBLCache(UINT32 max_size) : _max_size(max_size) {}

    size_t get_size(){
        return blockCache.size();
    }
    void push(BBL* bbl){
        // if the block is present, count as a hit
        // if not present, count as miss and insert
        // if the size of the cache is bigger than the max, 
        // pop the oldest block out (round-robin??)
        bool present = false;
        for(BBL* block : blockCache)
            if ( bbl == block) {
                nHits++;
                present = true;
                break;
            }
        
        if(!present){
            nMisses++;
            blockCache.push_front(bbl);
            if (blockCache.size() > _max_size)
                this->pop();
        }
    }
};

std::ostream * out = &cerr;
BBLCache* cache;


/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,  "pintool",
    "o", "", "specify file name for MyPinTool output");

KNOB<INT32>   KnobSizeN(KNOB_MODE_WRITEONCE,       "pintool",
                     "n", "50", "size of bbl cache");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
    cerr << "This tool prints out the number of dynamically executed " << endl <<
            "instructions, basic blocks and threads in the application." << endl << endl;

    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

/*!
 * Increase counter of the executed basic blocks and instructions and update the cache
 * This function is called for every basic block when it is about to be executed.
 * @param[in]   numInstInBbl    number of instructions in the basic block
 * @note use atomic operations for multi-threaded applications
 */
VOID CountBbl(BBL* bbl_addr)
{
    cache->bblCount++;
    cache->push(bbl_addr);
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

/*!
 * Insert call to the CountBbl() analysis routine before every basic block 
 * of the trace.
 * This function is called every time a new trace is encountered.
 * @param[in]   trace    trace to be instrumented
 * @param[in]   v        value specified by the tool in the TRACE_AddInstrumentFunction
 *                       function call
 */
VOID Trace(TRACE trace, VOID *v)
{
    // Visit every basic block in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        // Insert a call to CountBbl() before every basic bloc, passing the address of the basic block
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)CountBbl, IARG_PTR, BBL_Address(bbl), IARG_END);
    }
}

/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the 
 *                              PIN_AddFiniFunction function call
 */
VOID Fini(INT32 code, VOID *v)
{
    *out <<  "===============================================" << endl;
    *out <<  "MyPinTool analysis results: " << endl;
    *out <<  "Number of basic blocks: " << cache->bblCount  << endl;
    *out <<  "Number of basic block hits: " << cache->nHits  << endl;
    *out <<  "Number of basic block misses: " << cache->nMisses  << endl;
    *out <<  "Size of cache: " << cache->get_size()  << endl;
    *out <<  "===============================================" << endl;
}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments, 
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char *argv[])
{
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid 
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }
    
    string fileName = KnobOutputFile.Value();

    // Set cache size (default is 50 basic blocks)
    cache = new BBLCache(KnobSizeN.Value());

    if (!fileName.empty()) { out = new std::ofstream(fileName.c_str());}

    // Register function to be called to instrument traces
    TRACE_AddInstrumentFunction(Trace, 0);

    // Register function to be called for every thread before it starts running
    // PIN_AddThreadStartFunction(ThreadStart, 0);

    // Register function to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    
    cerr <<  "===============================================" << endl;
    cerr <<  "This application is instrumented by MyPinTool" << endl;
    if (!KnobOutputFile.Value().empty()) 
    {
        cerr << "See file " << KnobOutputFile.Value() << " for analysis results" << endl;
    }
    cerr <<  "===============================================" << endl;

    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
