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

class TournamentHybridPredictor : public BranchPredictor
{
public:
	TournamentHybridPredictor(unsigned int index_bits_, BranchPredictor* p1, BranchPredictor* p2) : BranchPredictor(), index_bits(index_bits_), predictor1(p1), predictor2(p2), table_entries(1 << index_bits) {
		TABLE = new unsigned long long[table_entries];
		COUNTER_MAX = 3;
		memset(TABLE, 0, table_entries * sizeof(*TABLE));
	}

	~TournamentHybridPredictor() {delete[] TABLE; delete predictor1; delete predictor2;}

	virtual bool predict(ADDRINT ip, ADDRINT target) {
        	unsigned int ip_table_index = ip % table_entries;
	        unsigned long long ip_table_value = TABLE[ip_table_index];
        	unsigned long long prediction_bit = (ip_table_value >> 1) & 1;

		if (prediction_bit)
			return (predictor2->predict(ip, target));
	        return (predictor1->predict(ip, target));
	}

	virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) {
		unsigned int ip_table_index = ip % table_entries;
		bool prediction1 = predictor1->predict(ip, target);
		bool prediction2 = predictor2->predict(ip, target);
		
		if (prediction1 != prediction2) {
			if (prediction1 == actual && (TABLE[ip_table_index] > 0))
				TABLE[ip_table_index]--;
			else if (TABLE[ip_table_index] < COUNTER_MAX)
				TABLE[ip_table_index]++;
		}
		predictor1->update(prediction1, actual, ip, target);
		predictor2->update(prediction2, actual, ip, target);
		updateCounters(predicted, actual);
	}

    	virtual string getName() { 
        	std::ostringstream stream;
		stream << "Tournament-" << predictor1->getName() << "-" << predictor2->getName();
		return stream.str();
	}

private:
	unsigned int index_bits;
	BranchPredictor* predictor1;
	BranchPredictor* predictor2;
	unsigned int table_entries;
	unsigned long long *TABLE;
	unsigned int COUNTER_MAX;
};

class Alpha21264Predictor : public TournamentHybridPredictor 
{
public:
	Alpha21264Predictor() : TournamentHybridPredictor(12, new LocalHistoryPredictor(/* ? */) new GlobalHistoryPredictor(/* ? */)) {}
	~Alpha21264Predictor() {}

	virtual string getName() {
		std::ostringstream stream;
		stream << "Alpha21264";
		return stream.str();
	}
};
#endif
