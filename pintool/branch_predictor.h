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
        table_entries = 1 << index_bits;
        TABLE = new unsigned long long[table_entries];
        memset(TABLE, 0, table_entries * sizeof(*TABLE));
    }

    ~FSMPredictor()
    {
        delete[] TABLE;
    }

    bool predict(ADDRINT ip, ADDRINT target) override
    {
        unsigned int ip_table_index = ip % table_entries;
        unsigned long long ip_table_value = TABLE[ip_table_index];
        unsigned long long prediction = ip_table_value >> (cntr_bits - 1);
        return (prediction != 0);
    }

    void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) override
    {
        unsigned int idx = ip % table_entries;
        unsigned long long state = TABLE[idx];

        switch (row)
        {
        case 2:
            if (actual)
            { // Taken
                switch (state)
                {
                case 0:
                    TABLE[idx] = 1;
                    break; // 0->1
                case 1:
                    TABLE[idx] = 2;
                    break; // 1->2
                case 2:
                    TABLE[idx] = 3;
                    break; // 2->3
                case 3:
                    TABLE[idx] = 3;
                    break; // 3->3
                }
            }
            else
            { // Not Taken
                switch (state)
                {
                case 0:
                    TABLE[idx] = 0;
                    break; // 0->0
                case 1:
                    TABLE[idx] = 0;
                    break; // 1->0
                case 2:
                    TABLE[idx] = 0;
                    break; // 2->0
                case 3:
                    TABLE[idx] = 2;
                    break; // 3->2
                }
            }
            break;
        case 3:
            if (actual)
            { // Taken
                switch (state)
                {
                case 0:
                    TABLE[idx] = 1;
                    break; // 0->1
                case 1:
                    TABLE[idx] = 3;
                    break; // 1->3
                case 2:
                    TABLE[idx] = 3;
                    break; // 2->3
                case 3:
                    TABLE[idx] = 3;
                    break; // 3->3
                }
            }
            else
            { // Not Taken
                switch (state)
                {
                case 0:
                    TABLE[idx] = 0;
                    break; // 0->0
                case 1:
                    TABLE[idx] = 0;
                    break; // 1->0
                case 2:
                    TABLE[idx] = 1;
                    break; // 2->1
                case 3:
                    TABLE[idx] = 2;
                    break; // 3->2
                }
            }
            break;
        case 4:
            if (actual)
            { // Taken
                switch (state)
                {
                case 0:
                    TABLE[idx] = 1;
                    break; // 1→2
                case 1:
                    TABLE[idx] = 3;
                    break; // 1→3
                case 2:
                    TABLE[idx] = 3;
                    break; // 2→3
                case 3:
                    TABLE[idx] = 3;
                    break; // 3->3
                }
            }
            else
            { // Not Taken
                switch (state)
                {
                case 0:
                    TABLE[idx] = 0;
                    break; // 1→1
                case 1:
                    TABLE[idx] = 0;
                    break; // 2→1
                case 2:
                    TABLE[idx] = 0;
                    break; // 3→1
                case 3:
                    TABLE[idx] = 2;
                    break; // 4→3
                }
            }
            break;
        case 5:
            if (actual)
            { // Taken
                switch (state)
                {
                case 0:
                    TABLE[idx] = 1;
                    break; // 0->1
                case 1:
                    TABLE[idx] = 3;
                    break; // 1->3
                case 2:
                    TABLE[idx] = 3;
                    break; // 2->3
                case 3:
                    TABLE[idx] = 2;
                    break; // 3->2
                }
            }
            else
            { // Not Taken
                switch (state)
                {
                case 0:
                    TABLE[idx] = 0;
                    break; // 0->0
                case 1:
                    TABLE[idx] = 0;
                    break; // 1->0
                case 2:
                    TABLE[idx] = 1;
                    break; // 2->1
                case 3:
                    TABLE[idx] = 2;
                    break; // 3->2
                }
            }
            break;
        default:
            std::cerr << "Invalid row number" << std::endl;
            break;
        }
        updateCounters(predicted, actual);
    }

    std::string getName() override
    {
        std::ostringstream stream;
        stream << "FSM-Row" << row;
        return stream.str();
    }

private:
    unsigned int row;
    unsigned int table_entries;
    unsigned int index_bits, cntr_bits;
    /* Make this unsigned long long so as to support big numbers of cntr_bits. */
    unsigned long long *TABLE;
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
