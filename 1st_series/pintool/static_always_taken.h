#ifndef STATIC_ALWAYS_TAKEN_H
#define STATIC_ALWAYS_TAKEN_H

// Add this class definition within branch_predictor.h

class StaticAlwaysTakenPredictor : public BranchPredictor
{
public:
    // Constructor
    StaticAlwaysTakenPredictor() : BranchPredictor() {};

    // Destructor
    ~StaticAlwaysTakenPredictor() {};

    // predict(): Always predicts Taken (true)
    virtual bool predict(ADDRINT ip, ADDRINT target)
    {
        return true;
    }

    // update(): Updates counters but doesn't change predictor state
    virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target)
    {
        // Update the prediction counters in the base class
        updateCounters(predicted, actual);

        // Static predictor, state does not change.
        // ip and target arguments are ignored.
    }

    // getName(): Returns the name of the predictor
    virtual string getName()
    {
        return "Static-AlwaysTaken";
    }
};

#endif
