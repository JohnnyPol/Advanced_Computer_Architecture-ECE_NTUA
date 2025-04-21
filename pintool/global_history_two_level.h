#ifndef GLOBAL_HISTORY_TWO_LEVEL_H
#define GLOBAL_HISTORY_TWO_LEVEL_H

#include <vector>  // Για std::vector
#include <cstdint> // Για uint8_t
#include <cmath>   // Για pow (αν και μπορούμε να το αποφύγουμε)
#include <sstream> // Για std::ostringstream στο getName
#include <string>  // Για std::string

class GlobalHistoryPredictor : public BranchPredictor
{
private:
    // Παράμετροι του Predictor
    const unsigned int pht_entries;    // Αριθμός εγγραφών PHT (Z)
    const unsigned int counter_length; // Μήκος μετρητή PHT (X bits)
    const unsigned int bhr_length;     // Μήκος Global BHR (N bits)

    // Πίνακας PHT
    std::vector<uint8_t> PHT; // Pattern History Table (κρατάει X-bit μετρητές)

    // Καθολικός Καταχωρητής Ιστορικού
    uint8_t BHR; // Branch History Register (κρατάει N bits ιστορικού)

    // Μάσκες και όρια
    const uint8_t counter_max;          // Μέγιστη τιμή μετρητή (2^X - 1)
    const uint8_t bhr_mask;             // Μάσκα για τον BHR (2^N - 1)
    const uint8_t prediction_threshold; // Όριο για πρόβλεψη Taken (2^(X-1))

public:
    // Constructor
    GlobalHistoryPredictor(unsigned int pht_entries_Z, unsigned int counter_length_X, unsigned int bhr_length_N) : BranchPredictor(),
                                                                                                                   pht_entries(pht_entries_Z),
                                                                                                                   counter_length(counter_length_X),
                                                                                                                   bhr_length(bhr_length_N),
                                                                                                                   BHR(0), // Αρχικοποίηση BHR σε 0
                                                                                                                   counter_max((1 << counter_length_X) - 1),
                                                                                                                   bhr_mask((1 << bhr_length_N) - 1),
                                                                                                                   prediction_threshold(1 << (counter_length_X - 1))
    {
        // Αρχικοποίηση PHT (μέγεθος Z)
        // Αρχική κατάσταση: Weakly Not Taken (η κατάσταση ακριβώς κάτω από το όριο πρόβλεψης)
        uint8_t initial_pht_state = (prediction_threshold > 0) ? prediction_threshold - 1 : 0;
        PHT.assign(pht_entries, initial_pht_state);
    }

    // Destructor (δεν χρειάζεται ειδική υλοποίηση λόγω std::vector)
    ~GlobalHistoryPredictor() override = default;

    // Μέθοδος πρόβλεψης
    bool predict(ADDRINT ip, ADDRINT target) override
    {
        // 1. Χρησιμοποίησε την τρέχουσα τιμή του BHR ως δείκτη PHT
        unsigned int pht_index = BHR;

        // Προαιρετικός έλεγχος ορίων (αν και με N=2,4 δεν θα ξεπεράσουμε τα Z=8K/16K)
        // if (pht_index >= pht_entries) { /* handle error or default */ pht_index = 0; }

        // 2. Διάβασε τον X-bit μετρητή από τον PHT
        uint8_t counter_state = PHT[pht_index];

        // 3. Κάνε την πρόβλεψη (Taken αν >= όριο)
        bool prediction = (counter_state >= prediction_threshold);
        return prediction;
    }

    // Μέθοδος ενημέρωσης
    void update(bool predicted, bool actual, ADDRINT ip, ADDRINT target) override
    {
        // 1. Βρες τον δείκτη PHT χρησιμοποιώντας το BHR ΠΡΙΝ την ενημέρωση
        unsigned int pht_index = BHR;

        // Προαιρετικός έλεγχος ορίων
        // if (pht_index >= pht_entries) { /* handle error or default */ pht_index = 0; }

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

        // 3. Ενημέρωσε τον καθολικό BHR
        // Ολίσθηση αριστερά, εισαγωγή actual στο LSB, εφαρμογή μάσκας
        BHR = ((BHR << 1) | (actual ? 1 : 0)) & bhr_mask;

        // 4. Ενημέρωσε τους γενικούς μετρητές της βασικής κλάσης
        updateCounters(predicted, actual);
    }

    // Μέθοδος για το όνομα του predictor
    std::string getName() override
    {
        std::ostringstream stream;
        stream << "Global"
               << "-N" << bhr_length                     // BHR Length (N)
               << "-X" << counter_length                 // Counter Length (X)
               << "-" << (pht_entries / 1024) << "KPHT"; // PHT Entries (Z in K)
        return stream.str();
    }
};

#endif