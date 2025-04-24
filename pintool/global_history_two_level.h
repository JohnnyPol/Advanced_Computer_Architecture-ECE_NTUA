#ifndef GLOBAL_HISTORY_TWO_LEVEL_H
#define GLOBAL_HISTORY_TWO_LEVEL_H

#include <vector>  // Για std::vector
#include <cstdint> // Για uint8_t
#include <cmath>   // Για log2
#include <sstream> // Για std::ostringstream στο getName
#include <string>  // Για std::string

class GlobalHistoryPredictor : public BranchPredictor
{
private:
    const unsigned int pht_entries;    // Αριθμός εγγραφών PHT (Z)
    const unsigned int cntr_bits; // Μήκος μετρητή PHT (X bits)
    const unsigned int bhr_length;     // Μήκος Global BHR (N bits)

    // Πίνακας PHT
    std::vector<uint8_t> PHT; // Pattern History Table (κρατάει X-bit μετρητές)

    // Καθολικός Καταχωρητής Ιστορικού
    uint8_t BHR; // Branch History Register (κρατάει N bits ιστορικού)

    // Μάσκες και όρια
    const uint8_t counter_max;         // Μέγιστη τιμή μετρητή (2^X - 1)
    const uint8_t bhr_mask;            // Μάσκα για τον BHR (2^N - 1)
    const uint8_t prediction_threshold; // Όριο πρόβλεψης (2^(X-1))
    const unsigned int pht_index_mask; // Μάσκα για τον δείκτη PHT (Z-1)
    const unsigned int pht_index_bits; // Αριθμός bits για τον δείκτη PHT

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
        BHR = ((BHR >> 1) | ((actual) ? bhr_update_mask : 0))& bhr_mask;


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

#endif