#ifndef LOCAL_HISTORY_TWO_LEVEL_H
#define LOCAL_HISTORY_TWO_LEVEL_H

#include <vector>  
#include <cstdint> 
#include <cmath>   
#include <sstream> 
#include <string>  
#include <numeric> 

class LocalHistoryPredictor : public BranchPredictor
{

public:
    // Constructor
    LocalHistoryPredictor(unsigned int bht_entries_X, unsigned int history_length_Z) : BranchPredictor(),
                                                                                       bht_entries(bht_entries_X),
                                                                                       history_length(history_length_Z),
                                                                                       pht_entries(8192),                       
                                                                                       pht_counter_bits(2),                      
                                                                                       history_mask((1 << history_length_Z) - 1) // Υπολογισμός μάσκας 
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

        // Χρησιμοποίησε το ιστορικό ως δείκτη PHT
        unsigned int pht_index = local_history;
        // FIXME: is this correct?
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

        // Υπολόγισε δείκτη PHT FIXME: Is this correct?
        unsigned int pht_index = local_history;

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

        // Ενημέρωσε το Z-bit ιστορικό στον BHT
        // Ολίσθηση αριστερά, εισαγωγή actual στο LSB, εφαρμογή μάσκας
        uint8_t new_history = ((local_history << 1) | (actual ? 1 : 0)) & history_mask;
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

private:
    // Παράμετροι του Predictor
    const unsigned int bht_entries;      // Αριθμός εγγραφών BHT (X)
    const unsigned int history_length;   // Μήκος ιστορικού ανά εγγραφή BHT (Z)
    const unsigned int pht_entries;      // Αριθμός εγγραφών PHT (σταθερό 8192)
    const unsigned int pht_counter_bits; // Bits ανά μετρητή PHT (σταθερό 2)

    // Πίνακες
    std::vector<uint8_t> BHT; // Branch History Table (κρατάει Z bits ιστορικού)
    std::vector<uint8_t> PHT; // Pattern History Table (κρατάει 2-bit μετρητές)

    // Μάσκα για το ιστορικό (για να κρατάμε μόνο Z bits)
    const uint8_t history_mask;

    // Όριο για τον 2-bit μετρητή
    const uint8_t counter_max = 3; // (1 << pht_counter_bits) - 1;
};

// ------- Τέλος κλάσης LocalHistoryPredictor --------

#endif