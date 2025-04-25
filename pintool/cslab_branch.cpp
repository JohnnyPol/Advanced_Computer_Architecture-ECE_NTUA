#include "pin.H"

#include <iostream>
#include <fstream>
#include <cassert>

using namespace std;

#include "branch_predictor.h"
#include "pentium_m_predictor/pentium_m_branch_predictor.h"
#include "ras.h"

/* ===================================================================== */
/* Commandline Switches                                                  */
/* ===================================================================== */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
                            "o", "cslab_branch.out", "specify output file name");
/* ===================================================================== */

/* ===================================================================== */
/* Global Variables                                                      */
/* ===================================================================== */
std::vector<BranchPredictor *> branch_predictors;
typedef std::vector<BranchPredictor *>::iterator bp_iterator_t;

//> BTBs have slightly different interface (they also have target predictions)
//  so we need to have different vector for them.
std::vector<BTBPredictor *> btb_predictors;
typedef std::vector<BTBPredictor *>::iterator btb_iterator_t;

std::vector<RAS *> ras_vec;
typedef std::vector<RAS *>::iterator ras_vec_iterator_t;

UINT64 total_instructions;
std::ofstream outFile;

/* ===================================================================== */

INT32 Usage()
{
    cerr << "This tool simulates various branch predictors.\n\n";
    cerr << KNOB_BASE::StringKnobSummary();
    cerr << endl;
    return -1;
}

/* ===================================================================== */

VOID count_instruction()
{
    total_instructions++;
}

VOID call_instruction(ADDRINT ip, ADDRINT target, UINT32 ins_size)
{
    ras_vec_iterator_t ras_it;

    for (ras_it = ras_vec.begin(); ras_it != ras_vec.end(); ++ras_it)
    {
        RAS *ras = *ras_it;
        ras->push_addr(ip + ins_size);
    }
}

VOID ret_instruction(ADDRINT ip, ADDRINT target)
{
    ras_vec_iterator_t ras_it;

    for (ras_it = ras_vec.begin(); ras_it != ras_vec.end(); ++ras_it)
    {
        RAS *ras = *ras_it;
        ras->pop_addr(target);
    }
}

VOID cond_branch_instruction(ADDRINT ip, ADDRINT target, BOOL taken)
{
    bp_iterator_t bp_it;
    BOOL pred;

    for (bp_it = branch_predictors.begin(); bp_it != branch_predictors.end(); ++bp_it)
    {
        BranchPredictor *curr_predictor = *bp_it;
        pred = curr_predictor->predict(ip, target);
        curr_predictor->update(pred, taken, ip, target);
    }
}

VOID branch_instruction(ADDRINT ip, ADDRINT target, BOOL taken)
{
    btb_iterator_t btb_it;
    BOOL pred;

    for (btb_it = btb_predictors.begin(); btb_it != btb_predictors.end(); ++btb_it)
    {
        BTBPredictor *curr_predictor = *btb_it;
        pred = curr_predictor->predict(ip, target);
        curr_predictor->update(pred, taken, ip, target);
    }
}

VOID Instruction(INS ins, void *v)
{
    if (INS_Category(ins) == XED_CATEGORY_COND_BR)
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)cond_branch_instruction,
                       IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN,
                       IARG_END);
    else if (INS_IsCall(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)call_instruction,
                       IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR,
                       IARG_UINT32, INS_Size(ins), IARG_END);
    else if (INS_IsRet(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ret_instruction,
                       IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_END);

    // For BTB we instrument all branches except returns
    if (INS_IsBranch(ins) && !INS_IsRet(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)branch_instruction,
                       IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN,
                       IARG_END);

    // Count each and every instruction
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)count_instruction, IARG_END);
}

/* ===================================================================== */

VOID Fini(int code, VOID *v)
{
    bp_iterator_t bp_it;
    btb_iterator_t btb_it;
    ras_vec_iterator_t ras_it;

    // Report total instructions and total cycles
    outFile << "Total Instructions: " << total_instructions << "\n";
    outFile << "\n";

    outFile << "RAS: (Correct - Incorrect)\n";
    for (ras_it = ras_vec.begin(); ras_it != ras_vec.end(); ++ras_it)
    {
        RAS *ras = *ras_it;
        outFile << ras->getNameAndStats() << "\n";
    }
    outFile << "\n";

    outFile << "Branch Predictors: (Name - Correct - Incorrect)\n";
    for (bp_it = branch_predictors.begin(); bp_it != branch_predictors.end(); ++bp_it)
    {
        BranchPredictor *curr_predictor = *bp_it;
        outFile << "  " << curr_predictor->getName() << ": "
                << curr_predictor->getNumCorrectPredictions() << " "
                << curr_predictor->getNumIncorrectPredictions() << "\n";
    }
    outFile << "\n";

    outFile << "BTB Predictors: (Name - Correct - Incorrect - TargetCorrect)\n";
    for (btb_it = btb_predictors.begin(); btb_it != btb_predictors.end(); ++btb_it)
    {
        BTBPredictor *curr_predictor = *btb_it;
        outFile << "  " << curr_predictor->getName() << ": "
                << curr_predictor->getNumCorrectPredictions() << " "
                << curr_predictor->getNumIncorrectPredictions() << " "
                << curr_predictor->getNumCorrectTargetPredictions() << "\n";
    }

    outFile.close();
}

/* ===================================================================== */

VOID InitPredictors()
{
    /* Question 5.3 (i)
    // N-bit predictors

    for (int i=1; i <= 4; i++) {
        NbitPredictor *nbitPred = new NbitPredictor(14, i); // 2^14 = 16k (16384)
        branch_predictors.push_back(nbitPred);
    }

    */
    /* Question 5.3 (ii)
    // Row 1
    NbitPredictor *nbitPred = new NbitPredictor(15, 2); // 2-bit saturating counter
    branch_predictors.push_back(nbitPred);
    for (int row = 2; row <= 5; row++)
    {
        branch_predictors.push_back(new FSMPredictor(row));
    }
    */
    /* Question 5.3 (iii)

    // — N-bit predictors with constant hardware 32K bits —
    // N=1bit → index_bits=15
    branch_predictors.push_back(new NbitPredictor(15, 1));
    // N=2bit → index_bits=14
    branch_predictors.push_back(new NbitPredictor(14, 2));
    // N=4bit → index_bits=13
    branch_predictors.push_back(new NbitPredictor(13, 4));

    // — For N=2 also the alternative FSMs (rows 2–5) —
    for (unsigned r = 2; r <= 5; ++r) {
        branch_predictors.push_back(new FSMPredictor(r));
    }
    */
    /* Question 5.4

    btb_predictors.push_back(new BTBPredictor(512, 1)); // 512 lines, 1-way
    btb_predictors.push_back(new BTBPredictor(512, 2)); // 512 lines, 2-way
    btb_predictors.push_back(new BTBPredictor(256, 2)); // 256 lines, 2-way
    btb_predictors.push_back(new BTBPredictor(256, 4)); // 256 lines, 4-way
    btb_predictors.push_back(new BTBPredictor(128, 2)); // 128 lines, 2-way
    btb_predictors.push_back(new BTBPredictor(128, 4)); // 128 lines, 4-way
    btb_predictors.push_back(new BTBPredictor(64, 4)); // 64 lines, 4-way
    btb_predictors.push_back(new BTBPredictor(64, 8)); // 64 lines, 8-way

    */

    /* Question 5.5 */

    /* Question 5.6 */
    // 1. Static Always Taken Predictor
    branch_predictors.push_back(new StaticAlwaysTakenPredictor());

    // 2. Static BTFNT (BackwardTaken-ForwardNotTaken) Predictor
    branch_predictors.push_back(new StaticBTFNTPredictor());

    // 3. N-Bit Predictor (FSM from Row 3)
    branch_predictors.push_back(new FSMPredictor(3)); // Row 3

    // 4. Pentium M Predictor
    branch_predictors.push_back(new PentiumMBranchPredictor());

    // 5-7. Local History Predictors (32K budget)
    branch_predictors.push_back(new LocalHistoryPredictor(2048, 8, 8192, 2)); // X=2048, Z=8
    branch_predictors.push_back(new LocalHistoryPredictor(4096, 4, 8192, 2)); // X=4096, Z=4
    branch_predictors.push_back(new LocalHistoryPredictor(8192, 2, 8192, 2)); // X=8192, Z=2

    // 8-11. Global History Predictors (32K budget)
    // X=2 => Z=16384
    branch_predictors.push_back(new GlobalHistoryPredictor(16384, 2, 2)); // Z=16K, X=2, N=2
    branch_predictors.push_back(new GlobalHistoryPredictor(16384, 2, 4)); // Z=16K, X=2, N=4

    // X=4 => Z=8192
    branch_predictors.push_back(new GlobalHistoryPredictor(8192, 4, 2)); // Z=8K,  X=4, N=2
    branch_predictors.push_back(new GlobalHistoryPredictor(8192, 4, 4)); // Z=8K,  X=4, N=4

    // 12. Alpha 21264 Predictor
    branch_predictors.push_back(new Alpha21264Predictor());

    // 13-16. Tournament Hybrid Predictors

    branch_predictors.push_back(
        new TournamentHybridPredictor(
            10,
            new NbitPredictor(13, 2),
            new GlobalHistoryPredictor(8192, 2, 2)));

    branch_predictors.push_back(
        new TournamentHybridPredictor(
            10,
            new GlobalHistoryPredictor(8192, 2, 2),
            new LocalHistoryPredictor(8192, 2, 8192, 2)));

    branch_predictors.push_back(
        new TournamentHybridPredictor(
            10,
            new NbitPredictor(13, 2),
            new LocalHistoryPredictor(8192, 2, 8192, 2)));

    branch_predictors.push_back(
        new TournamentHybridPredictor(
            11,
            new NbitPredictor(13, 2),
            new GlobalHistoryPredictor(8192, 2, 2)));

    // branch_predictors.push_back(
    //     new TournamentHybridPredictor(
    //         11,
    //         new GlobalHistoryPredictor(8192, 2, 2),
    //         new LocalHistoryPredictor(8192, 2, 8192, 2)));

    // branch_predictors.push_back(
    //     new TournamentHybridPredictor(
    //         11,
    //         new NbitPredictor(13, 2),
    //         new LocalHistoryPredictor(8192, 2, 8192, 2)));
}

VOID InitRas()
{
    /* Question 5.5
    ras_vec.push_back(new RAS(4));
    ras_vec.push_back(new RAS(8));
    ras_vec.push_back(new RAS(16));
    ras_vec.push_back(new RAS(32));
    ras_vec.push_back(new RAS(48));
    ras_vec.push_back(new RAS(64));
    */
}

int main(int argc, char *argv[])
{
    PIN_InitSymbols();

    if (PIN_Init(argc, argv))
        return Usage();

    // Open output file
    outFile.open(KnobOutputFile.Value().c_str());

    // Initialize predictors and RAS vector
    InitPredictors();
    // InitRas();

    // Instrument function calls in order to catch __parsec_roi_{begin,end}
    INS_AddInstrumentFunction(Instruction, 0);

    // Called when the instrumented application finishes its execution
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
