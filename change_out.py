import os
import re

# --- Configuration ---
# !! ΠΡΟΣΑΡΜΟΣΤΕ ΤΟΥΣ ΚΑΤΑΛΟΓΟΥΣ !!
input_directory = "train"  # Κατάλογος με τα αρχικά .out αρχεία
output_directory = "train" # Κατάλογος για τα τροποποιημένα .out αρχεία

# Έλεγχος αν ο κατάλογος εξόδου υπάρχει, αλλιώς δημιουργία του
if not os.path.exists(output_directory):
    os.makedirs(output_directory)
    print(f"Δημιουργήθηκε ο κατάλογος εξόδου: {output_directory}")

# Ορισμός των αντιστοιχίσεων ονομάτων (παλιό -> νέο)
# Προσθέστε ή αφαιρέστε αντιστοιχίσεις ανάλογα με τις ανάγκες σας
name_mapping = {
    # Local Predictors
    "Local-2048ent-8hist": "Local-2K-8bit",
    "Local-4096ent-4hist": "Local-4K-4bit",
    "Local-8192ent-2hist": "Local-8K-2bit",
    # Global Predictors (Βάσει των παραδειγμάτων - ελέγξτε αν χρειάζονται κι άλλες ή διαφορετικές αντιστοιχίσεις)
    "Global-N2-X2-16KPHT": "Global-16K-2-2",
    "Global-N4-X2-16KPHT": "Global-16k-4-2", # Σημείωση: Το 'k' είναι πεζό στο παράδειγμα εξόδου
    "T-Nbit-8K-2-FSM-1-Global-N2-X2-8KPHT": "T-Nbit-Global-8K-1",
    "T-Global-N2-X2-8KPHT-Local-8K-2bit": "T-Global-8K-Local-8K",
    "T-Nbit-8K-2-FSM-1-Local-8K-2bit": "T-Nbit-Local-8K",
    "T-Nbit-8K-2-FSM-1-Global-N2-X2-8KPHT": "T-Nbit-Global-8K-2",

    # Tournament Predictors (Φαίνεται να παραμένουν ίδια στα παραδείγματα, αλλά προσθέστε αν χρειάζεται)
    # "T-Old-Name": "T-New-Name",
}

# --- Script Logic ---
files_processed = 0
files_skipped = 0

print(f"Έναρξη επεξεργασίας αρχείων από: {input_directory}")

# Λίστα όλων των στοιχείων στον κατάλογο εισόδου
try:
    all_files = os.listdir(input_directory)
except FileNotFoundError:
    print(f"Σφάλμα: Ο κατάλογος εισόδου '{input_directory}' δεν βρέθηκε.")
    exit()
except Exception as e:
    print(f"Προέκυψε σφάλμα κατά την ανάγνωση του καταλόγου '{input_directory}': {e}")
    exit()


# Επανάληψη μόνο στα αρχεία .out
for filename in all_files:
    if filename.endswith(".out"):
        input_filepath = os.path.join(input_directory, filename)
        output_filepath = os.path.join(output_directory, filename)

        try:
            # Ανάγνωση του περιεχομένου του αρχικού αρχείου
            with open(input_filepath, 'r', encoding='utf-8') as f_in:
                content = f_in.read()

            # Αντικατάσταση των ονομάτων βάσει του mapping
            modified_content = content
            for old_name, new_name in name_mapping.items():
                # Χρησιμοποιούμε απλή αντικατάσταση string.
                # Αν χρειάζεται πιο αυστηρός έλεγχος (π.χ., να μην αντικαταστήσει υπο-string),
                # θα μπορούσε να χρησιμοποιηθεί regex με όρια λέξεων (\b).
                # Για τις συγκεκριμένες αλλαγές, η απλή αντικατάσταση είναι πιθανόν αρκετή.
                modified_content = modified_content.replace(old_name, new_name)

            # Εγγραφή του τροποποιημένου περιεχομένου στο νέο αρχείο
            with open(output_filepath, 'w', encoding='utf-8') as f_out:
                f_out.write(modified_content)

            files_processed += 1
            # print(f"Επεξεργάστηκε: {filename} -> Αποθηκεύτηκε στο {output_directory}")

        except Exception as e:
            print(f"Σφάλμα κατά την επεξεργασία του αρχείου {filename}: {e}")
            files_skipped += 1
    else:
         # Προαιρετικά: μπορείς να καταγράψεις ή να αγνοήσεις αρχεία που δεν είναι .out
         # print(f"Παραλείφθηκε (όχι .out): {filename}")
         pass


print("\n--- Ολοκλήρωση Επεξεργασίας ---")
print(f"Συνολικά αρχεία που επεξεργάστηκαν: {files_processed}")
if files_skipped > 0:
    print(f"Συνολικά αρχεία που παραλείφθηκαν λόγω σφάλματος: {files_skipped}")
print(f"Τα τροποποιημένα αρχεία βρίσκονται στον κατάλογο: {output_directory}")