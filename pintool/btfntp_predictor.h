#ifndef BRANCH_PREDICTOR_H
#define BRANCH_PREDICTOR_H

#include <sstream> // std::ostringstream
#include <cmath>   // pow(), floor
#include <cstring> // memset()

using namespace std;

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
    void updateCounters(bool predicted, bool actual) {
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
    NbitPredictor(unsigned index_bits_, unsigned cntr_bits_, std::function<int(bool actual, unsigned long long &counter, unsigned int counter_max)> f)
        : BranchPredictor(), index_bits(index_bits_), cntr_bits(cntr_bits_), fsmUpdate(f) {
        table_entries = 1 << index_bits;
        TABLE = new unsigned long long[table_entries];
        memset(TABLE, 0, table_entries * sizeof(*TABLE));
        
        COUNTER_MAX = (1 << cntr_bits) - 1;
    };
    ~NbitPredictor() { delete TABLE; };

    virtual bool predict(ADDRINT ip, ADDRINT target) {
        unsigned int ip_table_index = ip % table_entries;
        unsigned long long ip_table_value = TABLE[ip_table_index];
        unsigned long long prediction = ip_table_value >> (cntr_bits - 1);
        return (prediction != 0);
    };

    virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) {
        unsigned int ip_table_index = ip % table_entries;
	fsm_id = fsmUpdate(actual, TABLE[ip_table_index], COUNTER_MAX);
        updateCounters(predicted, actual);
    };

    virtual string getName() {
        std::ostringstream stream;
        stream << "Nbit-" << pow(2.0,double(index_bits)) / 1024.0 << "K-" << cntr_bits << "-FSM-" << fsm_id;
        return stream.str();
    }

private:
    unsigned int index_bits, cntr_bits;
    int fsm_id;
    std::function<int(bool actual, unsigned long long &counter, unsigned int counter_max)> fsmUpdate;
    unsigned int COUNTER_MAX;
    
    /* Make this unsigned long long so as to support big numbers of cntr_bits. */
    unsigned long long *TABLE;
    unsigned int table_entries;
};

// Fill in the BTB implementation ...
class BTBPredictor : public BranchPredictor
{
public:
	BTBPredictor(int btb_lines, int btb_assoc)
	     : BranchPredictor(), table_lines(btb_lines), table_assoc(btb_assoc), numSets(table_lines / table_assoc), sets(numSets, std::vector<BTBEntry>(table_assoc)), 
	       current_time(0), NumCorrectTargetPredictions(0) {}

	~BTBPredictor() {}

    virtual bool predict(ADDRINT ip, ADDRINT target) {
       	        int index = ip & (numSets - 1);
		for (auto& entry : sets[index]) {
			if (entry.valid && entry.ip == ip)
				return true;
		}
		return false;
	}

    virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) {
	    	int index = ip & (numSets - 1);
		bool invalid_found = false;
		BTBEntry* lru_entry = nullptr;
		
		if (predicted) {
			for (auto& entry : sets[index]) {
				if (entry.ip == ip) {
					if (actual) {
						entry.timestamp = current_time++;
						if (entry.target != target)
							entry.target = target;
						else
							NumCorrectTargetPredictions++;
						break;
					}

					else {
						entry.valid = false;
						break;
					}
				}
			}
		}

		else {
			if (actual) {
				// Insert new entry
				for (auto& entry : sets[index]) {
					if (!entry.valid) {
						entry.valid = true;
						entry.ip = ip;
						entry.target = target;
						entry.timestamp = current_time++;
						invalid_found = true;
						break;
					}

					if (!lru_entry || entry.timestamp < lru_entry->timestamp)
						lru_entry = &entry;
				}

				if (!invalid_found) {
					lru_entry->valid = true;
					lru_entry->ip = ip;
					lru_entry->target = target;
					lru_entry->timestamp = current_time++;
				}
			}
		}

	        updateCounters(predicted, actual);
    }

    virtual string getName() { 
        std::ostringstream stream;
		stream << "BTB-" << table_lines << "-" << table_assoc;
		return stream.str();
	}

    UINT64 getNumCorrectTargetPredictions() { 
		return NumCorrectTargetPredictions;
	}

private:
	int table_lines, table_assoc, numSets;
	struct BTBEntry {
		bool valid = false;
		ADDRINT ip = 0;
		ADDRINT target = 0;
		uint64_t timestamp = 0;
	};
	std::vector<std::vector<BTBEntry>> sets;
	uint64_t current_time;
	UINT64 NumCorrectTargetPredictions;
};


class StaticBTFNTPredictor : public BranchPredictor
{
public:
	StaticBTFNTPredictor() : BranchPredictor() {}
	~StaticBTFNTPredictor() {}

	virtual bool predict(ADDRINT ip, ADDRINT target)
		return target < ip;

        virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target)
		updateCounters(predicter, actual);

    	virtual string getName() { 
        	std::ostringstream stream;
		stream << "BTFNT";
		return stream.str();
	}
}

#endif
