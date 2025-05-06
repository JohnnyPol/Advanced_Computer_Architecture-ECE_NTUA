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

class StaticBTFNTPredictor : public BranchPredictor
{
public:
	StaticBTFNTPredictor() : BranchPredictor() {}
	~StaticBTFNTPredictor() {}

	virtual bool predict(ADDRINT ip, ADDRINT target) {
		return target < ip;
	}

        virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) {
		updateCounters(predicted, actual);
	}

    	virtual string getName() { 
        	std::ostringstream stream;
		stream << "BTFNT";
		return stream.str();
	}
};

#endif
