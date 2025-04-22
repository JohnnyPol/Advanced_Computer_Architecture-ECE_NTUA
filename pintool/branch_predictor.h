#ifndef BRANCH_PREDICTOR_H
#define BRANCH_PREDICTOR_H

#include <sstream> // std::ostringstream
#include <cmath>   // pow(), floor
#include <cstring> // memset()
#include <cstdint> // UINT64

/**
 * A generic BranchPredictor base class.
 * All predictors can be subclasses with overloaded predict() and update()
 * methods.
 **/
class BranchPredictor
{
public:
    BranchPredictor() : correct_predictions(0), incorrect_predictions(0) {};
    ~BranchPredictor() {};

    virtual bool predict(ADDRINT ip, ADDRINT target) = 0;
    virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) = 0;
    virtual string getName() = 0;

    UINT64 getNumCorrectPredictions() { return correct_predictions; }
    UINT64 getNumIncorrectPredictions() { return incorrect_predictions; }

    void resetCounters() { correct_predictions = incorrect_predictions = 0; };

protected:
    void updateCounters(bool predicted, bool actual)
    {
        if (predicted == actual)
            correct_predictions++;
        else
            incorrect_predictions++;
    };

private:
    UINT64 correct_predictions;
    UINT64 incorrect_predictions;
};

class NbitPredictor : public BranchPredictor
{
public:
    NbitPredictor(unsigned index_bits_, unsigned cntr_bits_)
        : BranchPredictor(), index_bits(index_bits_), cntr_bits(cntr_bits_)
    {
        table_entries = 1 << index_bits;
        TABLE = new unsigned long long[table_entries];
        memset(TABLE, 0, table_entries * sizeof(*TABLE));

        COUNTER_MAX = (1 << cntr_bits) - 1;
    };
    ~NbitPredictor() { delete TABLE; };

    virtual bool predict(ADDRINT ip, ADDRINT target)
    {
        unsigned int ip_table_index = ip % table_entries;
        unsigned long long ip_table_value = TABLE[ip_table_index];
        unsigned long long prediction = ip_table_value >> (cntr_bits - 1);
        return (prediction != 0);
    };

    virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target)
    {
        unsigned int ip_table_index = ip % table_entries;
        if (actual)
        {
            if (TABLE[ip_table_index] < COUNTER_MAX)
                TABLE[ip_table_index]++;
        }
        else
        {
            if (TABLE[ip_table_index] > 0)
                TABLE[ip_table_index]--;
        }

        updateCounters(predicted, actual);
    };

    virtual string getName()
    {
        std::ostringstream stream;
        stream << "Nbit-" << pow(2.0, double(index_bits)) / 1024.0 << "K-" << cntr_bits;
        return stream.str();
    }

private:
    unsigned int index_bits, cntr_bits;
    unsigned int COUNTER_MAX;

    /* Make this unsigned long long so as to support big numbers of cntr_bits. */
    unsigned long long *TABLE;
    unsigned int table_entries;
};

class FSMPredictor : public BranchPredictor
{
public:
    FSMPredictor(unsigned row_) : row(row_), index_bits(14), cntr_bits(2)
    {
        if (row < 2 || row > 5)
        {
            std::cerr << "Error: FSMPredictor row must be between 2 and 5." << std::endl;
            exit(1);
        }
        table_entries = 1 << index_bits;
        TABLE = new uint8_t[table_entries];
        memset(TABLE, 0, table_entries * sizeof(*TABLE));
    }

    ~FSMPredictor()
    {
        delete[] TABLE;
    }

    bool predict(ADDRINT ip, ADDRINT target) override
    {
        unsigned int ip_table_index = ip % table_entries;
        uint8_t ip_table_value = TABLE[ip_table_index];
        uint8_t prediction = ip_table_value >> (cntr_bits - 1);
        return (prediction != 0);
    }

    void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) override
    {
        unsigned int idx = ip % table_entries;
        uint8_t state = TABLE[idx];
        
        unsigned int row_idx = row - 2;
        unsigned int outcome_idx = actual ? 1 : 0;
        uint8_t next_state = transitions[row_idx][outcome_idx][state];
        
        // Update the state in the table
        TABLE[idx] = next_state;

        // Update the counters
        updateCounters(predicted, actual);
    }

    std::string getName() override
    {
        std::ostringstream stream;
        stream << "FSM-Row-" << row;
        return stream.str();
    }

private:
    static const uint8_t transitions[4][2][4];
    unsigned int row;
    unsigned int table_entries;
    const unsigned index_bits, cntr_bits;
    uint8_t *TABLE;
};
const uint8_t FSMPredictor::transitions[4][2][4] = {
    // Row 2 (row_idx=0) - [outcome][state]
    { {0, 0, 0, 2},  // Not Taken transitions (state 0->0, 1->0, 2->0, 3->2)
      {1, 2, 3, 3} },// Taken transitions     (state 0->1, 1->2, 2->3, 3->3)
    // Row 3 (row_idx=1) - [outcome][state]
    { {0, 0, 1, 2},  // Not Taken transitions (state 0->0, 1->0, 2->1, 3->2)
      {1, 3, 3, 3} },// Taken transitions     (state 0->1, 1->3, 2->3, 3->3)
    // Row 4 (row_idx=2) - [outcome][state]
    { {0, 0, 0, 2},  // Not Taken transitions (state 0->0, 1->0, 2->0, 3->2)
      {1, 3, 3, 3} },// Taken transitions     (state 0->1, 1->3, 2->3, 3->3)
    // Row 5 (row_idx=3) - [outcome][state]
    { {0, 0, 1, 2},  // Not Taken transitions (state 0->0, 1->0, 2->1, 3->2)
      {1, 3, 3, 2} } // Taken transitions     (state 0->1, 1->3, 2->3, 3->2)
};

// Fill in the BTB implementation ...
class BTBPredictor : public BranchPredictor
{
public:
    BTBPredictor(int btb_lines, int btb_assoc)
        : table_lines(btb_lines), table_assoc(btb_assoc)
    {
        /* ... fill me ... */
    }

    ~BTBPredictor()
    {
        /* ... fill me ... */
    }

    virtual bool predict(ADDRINT ip, ADDRINT target)
    {
        /* ... fill me ... */
        return false;
    }

    virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target)
    {
        /* ... fill me ... */
    }

    virtual string getName()
    {
        std::ostringstream stream;
        stream << "BTB-" << table_lines << "-" << table_assoc;
        return stream.str();
    }

    UINT64 getNumCorrectTargetPredictions()
    {
        /* ... fill me ... */
        return 0;
    }

private:
    int table_lines, table_assoc;
};




#endif
