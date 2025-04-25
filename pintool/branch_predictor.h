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
    FSMPredictor(unsigned row_) : BranchPredictor(), row(row_), index_bits(14), cntr_bits(2)
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
    {{0, 0, 0, 2},  // Not Taken transitions (state 0->0, 1->0, 2->0, 3->2)
     {1, 2, 3, 3}}, // Taken transitions     (state 0->1, 1->2, 2->3, 3->3)
    // Row 3 (row_idx=1) - [outcome][state]
    {{0, 0, 1, 2},  // Not Taken transitions (state 0->0, 1->0, 2->1, 3->2)
     {1, 3, 3, 3}}, // Taken transitions     (state 0->1, 1->3, 2->3, 3->3)
    // Row 4 (row_idx=2) - [outcome][state]
    {{0, 0, 0, 2},  // Not Taken transitions (state 0->0, 1->0, 2->0, 3->2)
     {1, 3, 3, 3}}, // Taken transitions     (state 0->1, 1->3, 2->3, 3->3)
    // Row 5 (row_idx=3) - [outcome][state]
    {{0, 0, 1, 2}, // Not Taken transitions (state 0->0, 1->0, 2->1, 3->2)
     {1, 3, 3, 2}} // Taken transitions     (state 0->1, 1->3, 2->3, 3->2)
};

// Fill in the BTB implementation ...
class BTBPredictor : public BranchPredictor
{
public:
    BTBPredictor(int btb_lines, int btb_assoc)
        : BranchPredictor(), table_lines(btb_lines), table_assoc(btb_assoc), numSets(table_lines / table_assoc), sets(numSets, std::vector<BTBEntry>(table_assoc)),
          current_time(0), NumCorrectTargetPredictions(0) {}

    ~BTBPredictor() {}

    virtual bool predict(ADDRINT ip, ADDRINT target)
    {
        int index = ip & (numSets - 1);
        for (auto &entry : sets[index])
        {
            if (entry.valid && entry.ip == ip)
                return true;
        }
        return false;
    }

    virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target)
    {
        int index = ip & (numSets - 1);
        bool invalid_found = false;
        BTBEntry *lru_entry = nullptr;

        if (predicted)
        {
            for (auto &entry : sets[index])
            {
                if (entry.ip == ip)
                {
                    if (actual)
                    {
                        entry.timestamp = current_time++;
                        if (entry.target != target)
                            entry.target = target;
                        else
                            NumCorrectTargetPredictions++;
                        break;
                    }

                    else
                    {
                        entry.valid = false;
                        break;
                    }
                }
            }
        }

        else
        {
            if (actual)
            {
                // Insert new entry
                for (auto &entry : sets[index])
                {
                    if (!entry.valid)
                    {
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

                if (!invalid_found)
                {
                    lru_entry->valid = true;
                    lru_entry->ip = ip;
                    lru_entry->target = target;
                    lru_entry->timestamp = current_time++;
                }
            }
        }

        updateCounters(predicted, actual);
    }

    virtual string getName()
    {
        std::ostringstream stream;
        stream << "BTB-" << table_lines << "-" << table_assoc;
        return stream.str();
    }

    UINT64 getNumCorrectTargetPredictions()
    {
        return NumCorrectTargetPredictions;
    }

private:
    int table_lines, table_assoc, numSets;
    struct BTBEntry
    {
        bool valid = false;
        ADDRINT ip = 0;
        ADDRINT target = 0;
        uint64_t timestamp = 0;
    };
    std::vector<std::vector<BTBEntry>> sets;
    uint64_t current_time;
    UINT64 NumCorrectTargetPredictions;
};

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

class StaticBTFNTPredictor : public BranchPredictor
{
public:
    StaticBTFNTPredictor() : BranchPredictor() {}
    ~StaticBTFNTPredictor() {}

    virtual bool predict(ADDRINT ip, ADDRINT target)
    {
        return target < ip;
    }

    virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target)
    {
        updateCounters(predicted, actual);
    }

    virtual string getName()
    {
        std::ostringstream stream;
        stream << "BTFNT";
        return stream.str();
    }
};

class GlobalHistoryPredictor : public BranchPredictor
{
private:
    const unsigned int pht_entries; // Αριθμός εγγραφών PHT (Z)
    const unsigned int cntr_bits;   // Μήκος μετρητή PHT (X bits)
    const unsigned int bhr_length;  // Μήκος Global BHR (N bits)

    // Πίνακας PHT
    std::vector<uint8_t> PHT; // Pattern History Table (κρατάει X-bit μετρητές)

    // Καθολικός Καταχωρητής Ιστορικού
    uint8_t BHR; // Branch History Register (κρατάει N bits ιστορικού)

    // Μάσκες και όρια
    const uint8_t counter_max;          // Μέγιστη τιμή μετρητή (2^X - 1)
    const uint8_t bhr_mask;             // Μάσκα για τον BHR (2^N - 1)
    const uint8_t prediction_threshold; // Όριο πρόβλεψης (2^(X-1))
    const unsigned int pht_index_mask;  // Μάσκα για τον δείκτη PHT (Z-1)
    const unsigned int pht_index_bits;  // Αριθμός bits για τον δείκτη PHT

public:
    // Constructor
    GlobalHistoryPredictor(unsigned int pht_entries_Z, unsigned int counter_length_X, unsigned int bhr_length_N) : BranchPredictor(),
                                                                                                                   pht_entries(pht_entries_Z),
                                                                                                                   cntr_bits(counter_length_X),
                                                                                                                   bhr_length(bhr_length_N),
                                                                                                                   BHR(0), // Αρχικοποίηση BHR σε 0
                                                                                                                   counter_max((1 << counter_length_X) - 1),
                                                                                                                   bhr_mask((1 << bhr_length_N) - 1),
                                                                                                                   prediction_threshold(1 << (counter_length_X - 1)),
                                                                                                                   pht_index_mask(pht_entries_Z - 1),
                                                                                                                   pht_index_bits(static_cast<unsigned int>(std::round(std::log2(pht_entries_Z))))
    {
        // Αρχικοποίηση PHT (μέγεθος Z)
        // Αρχική κατάσταση: Weakly Not Taken (η κατάσταση ακριβώς κάτω από το όριο πρόβλεψης)
        uint8_t initial_pht_state = (prediction_threshold > 0) ? prediction_threshold - 1 : 0;
        PHT.assign(pht_entries, initial_pht_state);
    }

    ~GlobalHistoryPredictor() override = default;

    // Μέθοδος πρόβλεψης
    bool predict(ADDRINT ip, ADDRINT target) override
    {
        // Ολίσθηση του PC component για να κάνει χώρο για τα BHR bits
        unsigned int shifted_pc_component = ip << bhr_length;

        // Υπολογισμός του δείκτη PHT χρησιμοποιώντας το BHR
        unsigned int pht_index = (shifted_pc_component | BHR) & pht_index_mask;

        // Διάβασε τον X-bit μετρητή από τον PHT
        uint8_t counter_state = PHT[pht_index];

        // Κάνε την πρόβλεψη
        uint8_t prediction = counter_state >> (cntr_bits - 1);
        return (prediction != 0);
    }

    // Μέθοδος ενημέρωσης
    void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) override
    {
        // Ολίσθηση του PC component για να κάνει χώρο για τα BHR bits
        unsigned int shifted_pc_component = ip << bhr_length;

        // Υπολογισμός του δείκτη PHT χρησιμοποιώντας το BHR
        unsigned int pht_index = (shifted_pc_component | BHR) & pht_index_mask;

        // 2. Ενημέρωσε τον X-bit μετρητή στον PHT
        uint8_t old_counter_state = PHT[pht_index];
        uint8_t new_counter_state;
        if (actual) // Branch Taken
        {
            new_counter_state = (old_counter_state < counter_max) ? old_counter_state + 1 : counter_max;
        }
        else // Branch Not Taken
        {
            new_counter_state = (old_counter_state > 0) ? old_counter_state - 1 : 0;
        }
        PHT[pht_index] = new_counter_state;

        unsigned int bhr_update_mask = (1 << (bhr_length - 1)); // Μάσκα για την ενημέρωση του BHR
        // Ενημέρωσε τον καθολικό BHR
        BHR = ((BHR >> 1) | ((actual) ? bhr_update_mask : 0)) & bhr_mask;

        // Ενημέρωσε τους γενικούς μετρητές της βασικής κλάσης
        updateCounters(predicted, actual);
    }

    // Μέθοδος για το όνομα του predictor
    std::string getName() override
    {
        std::ostringstream stream;
        stream << "Global"
               << "-N" << bhr_length                     // BHR Length (N)
               << "-X" << cntr_bits                      // Counter Bits (X)
               << "-" << (pht_entries / 1024) << "KPHT"; // PHT Entries (Z in K)
        return stream.str();
    }
};

class LocalHistoryPredictor : public BranchPredictor
{
private:
    // Παράμετροι του Predictor
    const unsigned int bht_entries;      // Αριθμός εγγραφών BHT (X)
    const unsigned int history_length;   // Μήκος ιστορικού ανά εγγραφή BHT (Z)
    const unsigned int pht_entries;      // Αριθμός εγγραφών PHT (σταθερό 8192)
    const unsigned int pht_counter_bits; // Bits ανά μετρητή PHT (σταθερό 2)
    const unsigned int bht_length;       // Μήκος BHT (log2(X))
    const unsigned int pht_index_mask;   // Μάσκα για τον δείκτη PHT (8192 - 1)

    // Πίνακες
    std::vector<uint8_t> BHT; // Branch History Table (κρατάει Z bits ιστορικού)
    std::vector<uint8_t> PHT; // Pattern History Table (κρατάει 2-bit μετρητές)

    // Μάσκα για το ιστορικό (για να κρατάμε μόνο Z bits)
    const uint8_t history_mask;

    // Όριο για τον 2-bit μετρητή
    const uint8_t counter_max = 3; // (1 << pht_counter_bits) - 1;

public:
    // Constructor
    LocalHistoryPredictor(unsigned int bht_entries_X, unsigned int history_length_Z) : BranchPredictor(),
                                                                                       bht_entries(bht_entries_X),
                                                                                       history_length(history_length_Z),
                                                                                       pht_entries(8192),
                                                                                       pht_counter_bits(2),
                                                                                       pht_index_mask(8192 - 1),
                                                                                       history_mask((1 << history_length_Z) - 1), // Υπολογισμός μάσκας
                                                                                       bht_length(static_cast<unsigned int>(std::round(std::log2(bht_entries_X)))),
    {
        // Αρχικοποίηση BHT με μηδενικά (μέγεθος Χ)
        BHT.assign(bht_entries, 0);

        // Αρχικοποίηση PHT με 0 ή 1 (μέγεθος 8192)
        // Ας πούμε Weakly Not Taken (1) ως αρχική κατάσταση
        PHT.assign(pht_entries, 1);
    }

    ~LocalHistoryPredictor() override = default;

    bool predict(ADDRINT ip, ADDRINT target) override
    {
        unsigned int bht_index = ip % bht_entries;
        uint8_t local_history = BHT[bht_index];

        // Ολίσθηση του PC component για να κάνει χώρο για τα BHR bits
        unsigned int shifted_pc_component = ip << bht_length;

        // Υπολογισμός του δείκτη PHT χρησιμοποιώντας το BHR
        unsigned int pht_index = (shifted_pc_component | BHR) & pht_index_mask;

        // Διάβασε τον 2-bit μετρητή από τον PHT
        uint8_t counter_state = PHT[pht_index];

        // Κάνε την πρόβλεψη (Taken αν >= 2)
        bool prediction = (counter_state >= 2);
        return prediction;
    }

    // Μέθοδος ενημέρωσης
    void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) override
    {
        // Υπολόγισε δείκτη BHT
        unsigned int bht_index = ip % bht_entries;

        // Διάβασε το τοπικό ιστορικό (που χρησιμοποιήθηκε για την πρόβλεψη)
        uint8_t local_history = BHT[bht_index];

        // Ολίσθηση του PC component για να κάνει χώρο για τα BHR bits
        unsigned int shifted_pc_component = ip << bht_length;

        // Υπολογισμός του δείκτη PHT χρησιμοποιώντας το BHR
        unsigned int pht_index = (shifted_pc_component | BHR) & pht_index_mask;

        // Ενημέρωσε τον 2-bit μετρητή στον PHT
        uint8_t old_counter_state = PHT[pht_index];
        uint8_t new_counter_state;
        if (actual)
        {
            new_counter_state = (old_counter_state < counter_max) ? old_counter_state + 1 : counter_max;
        }
        else
        {
            new_counter_state = (old_counter_state > 0) ? old_counter_state - 1 : 0;
        }
        PHT[pht_index] = new_counter_state;

        unsigned int bht_update_mask = (1 << (bht_length - 1));
        uint8_t new_history = ((local_history >> 1) | (actual ? bht_update_mask : 0)) & history_mask;
        BHT[bht_index] = new_history;

        updateCounters(predicted, actual);
    }

    // Μέθοδος για το όνομα του predictor
    std::string getName() override
    {
        std::ostringstream stream;
        stream << "Local-" << bht_entries << "ent-" << history_length << "hist";
        return stream.str();
    }
};

class Alpha21264Predictor : public TournamentHybridPredictor 
{
public:
	Alpha21264Predictor() : TournamentHybridPredictor(12, new LocalHistoryPredictor(/* ? */), new GlobalHistoryPredictor(/* ? */)) {}
	~Alpha21264Predictor() {}

	virtual string getName() {
		std::ostringstream stream;
		stream << "Alpha21264";
		return stream.str();
	}
};

class TournamentHybridPredictor : public BranchPredictor
{
public:
    TournamentHybridPredictor(unsigned int index_bits_, BranchPredictor *p1, BranchPredictor *p2) : BranchPredictor(), index_bits(index_bits_), predictor1(p1), predictor2(p2), table_entries(1 << index_bits)
    {
        TABLE = new unsigned long long[table_entries];
        COUNTER_MAX = 3;
        memset(TABLE, 0, table_entries * sizeof(*TABLE));
    }

    ~TournamentHybridPredictor()
    {
        delete[] TABLE;
        delete predictor1;
        delete predictor2;
    }

    virtual bool predict(ADDRINT ip, ADDRINT target)
    {
        unsigned int ip_table_index = ip % table_entries;
        unsigned long long ip_table_value = TABLE[ip_table_index];
        unsigned long long prediction_bit = (ip_table_value >> 1) & 1;

        if (prediction_bit)
            return (predictor2->predict(ip, target));
        return (predictor1->predict(ip, target));
    }

    virtual void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target)
    {
        unsigned int ip_table_index = ip % table_entries;
        bool prediction1 = predictor1->predict(ip, target);
        bool prediction2 = predictor2->predict(ip, target);

        if (prediction1 != prediction2)
        {
            if (prediction1 == actual && (TABLE[ip_table_index] > 0))
                TABLE[ip_table_index]--;
            else if (TABLE[ip_table_index] < COUNTER_MAX)
                TABLE[ip_table_index]++;
        }
        predictor1->update(prediction1, actual, ip, target);
        predictor2->update(prediction2, actual, ip, target);
        updateCounters(predicted, actual);
    }

    virtual string getName()
    {
        std::ostringstream stream;
        stream << "Tournament-" << predictor1->getName() << "-" << predictor2->getName();
        return stream.str();
    }

private:
    unsigned int index_bits;
    BranchPredictor *predictor1;
    BranchPredictor *predictor2;
    unsigned int table_entries;
    unsigned long long *TABLE;
    unsigned int COUNTER_MAX;
};

#endif
