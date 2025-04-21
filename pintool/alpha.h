#ifndef GLOBAL_HISTORY_TWO_LEVEL_H
#define GLOBAL_HISTORY_TWO_LEVEL_H

#include <vector>     // Για std::vector
#include <cstdint>    // Για uint8_t, uint16_t
#include <cmath>      // Για pow (ίσως δεν χρειαστεί)
#include <sstream>    // Για std::ostringstream στο getName
#include <string>     // Για std::string
#include <numeric>    // Για std::pow (αντικατάσταση του cmath::pow)

// ------- Πρόσθεσε αυτή την κλάση στο branch_predictor.h --------

class Alpha21264Predictor : public BranchPredictor
{
private:
    // --- Παράμετροι Αρχιτεκτονικής ---
    // Τοπικός Προβλεπτής (Local)
    static const unsigned int LOCAL_BHT_ENTRIES = 1024;
    static const unsigned int LOCAL_HISTORY_LENGTH = 10;
    static const unsigned int LOCAL_PHT_ENTRIES = 1024; // Πρέπει να είναι 2^LOCAL_HISTORY_LENGTH
    static const unsigned int LOCAL_COUNTER_BITS = 3;

    // Καθολικός Προβλεπτής (Global)
    static const unsigned int GLOBAL_HISTORY_LENGTH = 12;
    static const unsigned int GLOBAL_PHT_ENTRIES = 4096; // Πρέπει να είναι 2^GLOBAL_HISTORY_LENGTH
    static const unsigned int GLOBAL_COUNTER_BITS = 2;

    // Προβλεπτής Επιλογής (Choice)
    static const unsigned int CHOICE_PHT_ENTRIES = 4096;
    static const unsigned int CHOICE_COUNTER_BITS = 2;

    // --- Πίνακες Δεδομένων ---
    std::vector<uint16_t> localBHT; // 1K x 10 bits (uint16_t αρκεί)
    std::vector<uint8_t>  localPHT; // 1K x 3 bits (uint8_t αρκεί)
    std::vector<uint8_t>  globalPHT;// 4K x 2 bits (uint8_t αρκεί)
    std::vector<uint8_t>  choicePHT;// 4K x 2 bits (uint8_t αρκεί)

    // --- Καταχωρητές & Μάσκες ---
    uint16_t globalBHR; // 12 bits (uint16_t αρκεί)

    const uint16_t local_history_mask;
    const uint16_t global_history_mask;

    // --- Όρια & Κατώφλια Μετρητών ---
    const uint8_t local_counter_max;
    const uint8_t global_counter_max;
    const uint8_t choice_counter_max;
    const uint8_t local_threshold;
    const uint8_t global_threshold;
    const uint8_t choice_threshold; // Όριο για να επιλέξουμε global predictor

    // --- Βοηθητική συνάρτηση για ενημέρωση μετρητών κορεσμού ---
    inline uint8_t update_sat_counter(uint8_t counter, uint8_t max_val, bool increment) const {
        if (increment) {
            return (counter < max_val) ? counter + 1 : max_val;
        } else {
            return (counter > 0) ? counter - 1 : 0;
        }
    }


public:
    // Constructor
    Alpha21264Predictor() :
        BranchPredictor(),
        local_history_mask((1 << LOCAL_HISTORY_LENGTH) - 1),       // Μάσκα για 10 bits
        global_history_mask((1 << GLOBAL_HISTORY_LENGTH) - 1),     // Μάσκα για 12 bits
        local_counter_max((1 << LOCAL_COUNTER_BITS) - 1),          // 7
        global_counter_max((1 << GLOBAL_COUNTER_BITS) - 1),        // 3
        choice_counter_max((1 << CHOICE_COUNTER_BITS) - 1),        // 3
        local_threshold(1 << (LOCAL_COUNTER_BITS - 1)),            // 4
        global_threshold(1 << (GLOBAL_COUNTER_BITS - 1)),          // 2
        choice_threshold(1 << (CHOICE_COUNTER_BITS - 1)),          // 2 (Ευνοεί Global αν >= 2)
        globalBHR(0) // Αρχικοποίηση Global History Register
    {
        // Έλεγχος μεγεθών PHT (προαιρετικός αλλά καλός)
        if (LOCAL_PHT_ENTRIES != (1 << LOCAL_HISTORY_LENGTH)) {
             std::cerr << "Warning: Alpha Local PHT size mismatch with history length!" << std::endl;
        }
        if (GLOBAL_PHT_ENTRIES != (1 << GLOBAL_HISTORY_LENGTH)) {
             std::cerr << "Warning: Alpha Global PHT size mismatch with history length!" << std::endl;
        }

        // Αρχικοποίηση πινάκων
        localBHT.assign(LOCAL_BHT_ENTRIES, 0); // Αρχικοποίηση ιστορικών σε 0

        // Αρχικοποίηση Local PHT σε Weakly Not Taken (κατάσταση 3 για 3-bit)
        localPHT.assign(LOCAL_PHT_ENTRIES, local_threshold -1 );

        // Αρχικοποίηση Global PHT σε Weakly Not Taken (κατάσταση 1 για 2-bit)
        globalPHT.assign(GLOBAL_PHT_ENTRIES, global_threshold - 1);

        // Αρχικοποίηση Choice PHT ώστε να ευνοεί τον Local αρχικά (κατάσταση 1)
        choicePHT.assign(CHOICE_PHT_ENTRIES, choice_threshold - 1);
    }

    // Destructor
    ~Alpha21264Predictor() override = default;

    // Μέθοδος πρόβλεψης
    bool predict(ADDRINT ip, ADDRINT target) override
    {
        // --- Λήψη Πρόβλεψης από Local Predictor ---
        unsigned int local_bht_idx = ip % LOCAL_BHT_ENTRIES;
        uint16_t local_history = localBHT[local_bht_idx];
        unsigned int local_pht_idx = local_history; // Index με το 10-bit history
        uint8_t local_counter = localPHT[local_pht_idx];
        bool prediction0 = (local_counter >= local_threshold);

        // --- Λήψη Πρόβλεψης από Global Predictor ---
        // Προσοχή: Το global history (globalBHR) διαβάζεται ως έχει
        unsigned int global_pht_idx = globalBHR; // Index με το 12-bit global history
        uint8_t global_counter = globalPHT[global_pht_idx];
        bool prediction1 = (global_counter >= global_threshold);

        // --- Λήψη Επιλογής ---
        unsigned int choice_idx = ip % CHOICE_PHT_ENTRIES;
        uint8_t choice_counter = choicePHT[choice_idx];

        // --- Τελική Πρόβλεψη ---
        bool final_prediction = (choice_counter >= choice_threshold) ? prediction1 : prediction0;
        return final_prediction;
    }

    // Μέθοδος ενημέρωσης
    void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) override
    {
        // --- Λήψη τιμών ΠΡΙΝ την ενημέρωση ---
        unsigned int local_bht_idx = ip % LOCAL_BHT_ENTRIES;
        uint16_t local_history = localBHT[local_bht_idx];
        unsigned int local_pht_idx = local_history;
        uint8_t local_counter = localPHT[local_pht_idx];

        unsigned int global_pht_idx = globalBHR;
        uint8_t global_counter = globalPHT[global_pht_idx];

        unsigned int choice_idx = ip % CHOICE_PHT_ENTRIES;
        uint8_t choice_counter = choicePHT[choice_idx];

        // --- Υπολογισμός ορθότητας επιμέρους προβλέψεων ---
        bool prediction0 = (local_counter >= local_threshold);
        bool prediction1 = (global_counter >= global_threshold);
        bool pred0_correct = (prediction0 == actual);
        bool pred1_correct = (prediction1 == actual);

        // --- Ενημέρωση Local PHT (3-bit counter) ---
        uint8_t new_local_counter = update_sat_counter(local_counter, local_counter_max, actual);
        localPHT[local_pht_idx] = new_local_counter;

        // --- Ενημέρωση Local BHT (10-bit history) ---
        uint16_t new_local_history = ((local_history << 1) | (actual ? 1 : 0)) & local_history_mask;
        localBHT[local_bht_idx] = new_local_history;

        // --- Ενημέρωση Global PHT (2-bit counter) ---
        uint8_t new_global_counter = update_sat_counter(global_counter, global_counter_max, actual);
        globalPHT[global_pht_idx] = new_global_counter;

        // --- Ενημέρωση Choice PHT (2-bit counter) ---
        // Ενημερώνεται μόνο αν ο ένας είναι σωστός και ο άλλος λάθος
        uint8_t new_choice_counter = choice_counter;
        if (pred1_correct && !pred0_correct) { // Ενίσχυση Global
            new_choice_counter = update_sat_counter(choice_counter, choice_counter_max, true);
        } else if (!pred1_correct && pred0_correct) { // Ενίσχυση Local
            new_choice_counter = update_sat_counter(choice_counter, choice_counter_max, false);
        }
        choicePHT[choice_idx] = new_choice_counter;

        // --- Ενημέρωση Global BHR (12-bit history) ---
        // Η ενημέρωση γίνεται στο τέλος, με βάση το actual outcome
        globalBHR = ((globalBHR << 1) | (actual ? 1 : 0)) & global_history_mask;

        // --- Ενημέρωση γενικών μετρητών ---
        updateCounters(predicted, actual); // Χρησιμοποιούμε την τελική πρόβλεψη 'predicted'
    }

    // Μέθοδος για το όνομα του predictor
    std::string getName() override
    {
        return "Alpha 21264";
    }
};

// ------- Τέλος κλάσης Alpha21264Predictor --------
#endif