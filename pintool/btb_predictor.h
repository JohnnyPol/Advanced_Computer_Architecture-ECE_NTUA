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

#endif
