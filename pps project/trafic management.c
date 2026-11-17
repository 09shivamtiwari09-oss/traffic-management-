/*
 * Project 50: Traffic Violation and Penalty Management
 * ----------------------------------------------------
 * - Array of structures stored on the heap (malloc / realloc)
 * - Incremental penalties for repeat offenders
 * - Summary reports and total revenue
 * - File I/O (violations.txt) for persistent storage
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define DATA_FILE      "violations.txt"
#define LICENSE_LEN    20
#define OFFENSE_LEN    30
#define INITIAL_CAP    4
#define PENALTY_STEP   0.25f   /* +25% of base fine per previous offence */
#define PENALTY_CAP    1.00f   /* maximum surcharge = +100% of base fine */

#define UNPAID 0
#define PAID   1

/* ---------- Data structures ---------- */

typedef struct {
    char  license_no[LICENSE_LEN];
    char  offense_type[OFFENSE_LEN];
    float base_fine;       /* standard fine for this offence      */
    float fine_amount;     /* final fine after repeat penalty     */
    int   payment_status;  /* UNPAID or PAID                      */
    int   offense_no;      /* nth offence by this licence holder  */
} Violation;

typedef struct {
    Violation *data;       /* dynamically growing array */
    int size;
    int capacity;
} Database;

typedef struct {
    const char *name;
    float fine;
} OffenseInfo;

static const OffenseInfo OFFENSES[] = {
    {"Over Speeding",          1000.0f},
    {"Red Light Jumping",      1500.0f},
    {"No Helmet",               500.0f},
    {"No Seat Belt",            500.0f},
    {"Drunk Driving",         10000.0f},
    {"Wrong Side Driving",     1000.0f},
    {"Illegal Parking",         500.0f},
    {"Driving Without License",5000.0f},
    {"Using Mobile While Driving", 1000.0f}
};
#define NUM_OFFENSES ((int)(sizeof(OFFENSES) / sizeof(OFFENSES[0])))

/* ---------- Input helpers ---------- */

static void read_line(const char *prompt, char *buf, int len)
{
    printf("%s", prompt);
    if (fgets(buf, len, stdin) == NULL) {
        buf[0] = '\0';
        return;
    }
    buf[strcspn(buf, "\r\n")] = '\0';
}

static int read_int(const char *prompt)
{
    char line[64];
    int value;
    while (1) {
        read_line(prompt, line, sizeof(line));
        if (sscanf(line, "%d", &value) == 1)
            return value;
        printf("  Invalid input. Please enter a number.\n");
    }
}

static void to_upper_str(char *s)
{
    for (; *s; s++)
        *s = (char)toupper((unsigned char)*s);
}

/* ---------- Database (dynamic memory) ---------- */

static int db_init(Database *db)
{
    db->data = (Violation *)malloc(INITIAL_CAP * sizeof(Violation));
    if (db->data == NULL) return 0;
    db->size = 0;
    db->capacity = INITIAL_CAP;
    return 1;
}

static int db_add(Database *db, const Violation *v)
{
    if (db->size == db->capacity) {
        int new_cap = db->capacity * 2;
        Violation *tmp = (Violation *)realloc(db->data, new_cap * sizeof(Violation));
        if (tmp == NULL) {
            printf("  Error: out of memory!\n");
            return 0;
        }
        db->data = tmp;
        db->capacity = new_cap;
    }
    db->data[db->size++] = *v;
    return 1;
}

static void db_free(Database *db)
{
    free(db->data);
    db->data = NULL;
    db->size = db->capacity = 0;
}

/* ---------- Penalty algorithm (CO1, CO2) ---------- */

static int count_previous(const Database *db, const char *license)
{
    int i, count = 0;
    for (i = 0; i < db->size; i++)
        if (strcmp(db->data[i].license_no, license) == 0)
            count++;
    return count;
}

/*
 * 1st offence : base fine
 * 2nd offence : base + 25%
 * 3rd offence : base + 50%
 * 4th offence : base + 75%
 * 5th+ offence: base + 100% (capped)
 */
static float calculate_fine(float base_fine, int previous_offenses)
{
    float surcharge = 0.0f;
    int i;
    for (i = 0; i < previous_offenses; i++) {
        surcharge += PENALTY_STEP;
        if (surcharge >= PENALTY_CAP) {
            surcharge = PENALTY_CAP;
            break;
        }
    }
    return base_fine * (1.0f + surcharge);
}

/* ---------- File I/O ---------- */

static void save_to_file(const Database *db)
{
    FILE *fp = fopen(DATA_FILE, "w");
    int i;
    if (fp == NULL) {
        printf("  Error: cannot write to %s\n", DATA_FILE);
        return;
    }
    for (i = 0; i < db->size; i++) {
        const Violation *v = &db->data[i];
        fprintf(fp, "%s|%s|%.2f|%.2f|%d|%d\n",
                v->license_no, v->offense_type, v->base_fine,
                v->fine_amount, v->payment_status, v->offense_no);
    }
    fclose(fp);
}

static void load_from_file(Database *db)
{
    FILE *fp = fopen(DATA_FILE, "r");
    char line[256];
    Violation v;
    if (fp == NULL) return;   /* first run: no file yet */

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%19[^|]|%29[^|]|%f|%f|%d|%d",
                   v.license_no, v.offense_type, &v.base_fine,
                   &v.fine_amount, &v.payment_status, &v.offense_no) == 6) {
            db_add(db, &v);
        }
    }
    fclose(fp);
    printf("Loaded %d record(s) from %s\n", db->size, DATA_FILE);
}

/* ---------- Display helpers ---------- */

static void print_header(void)
{
    printf("\n%-4s %-14s %-28s %-8s %-12s %-8s\n",
           "No.", "License No", "Offense", "Offence#", "Fine (Rs)", "Status");
    printf("---------------------------------------------------------------------------------\n");
}

static void print_record(int idx, const Violation *v)
{
    printf("%-4d %-14s %-28s %-8d %-12.2f %-8s\n",
           idx + 1, v->license_no, v->offense_type, v->offense_no,
           v->fine_amount, v->payment_status == PAID ? "PAID" : "UNPAID");
}

/* ---------- Features ---------- */

static void add_violation(Database *db)
{
    Violation v;
    int choice, prev;

    read_line("Enter license number: ", v.license_no, LICENSE_LEN);
    if (strlen(v.license_no) == 0) {
        printf("  License number cannot be empty.\n");
        return;
    }
    to_upper_str(v.license_no);

    printf("\nSelect offense type:\n");
    for (choice = 0; choice < NUM_OFFENSES; choice++)
        printf("  %d. %-28s Rs %.2f\n", choice + 1,
               OFFENSES[choice].name, OFFENSES[choice].fine);

    choice = read_int("Enter choice: ");
    if (choice < 1 || choice > NUM_OFFENSES) {
        printf("  Invalid offense choice.\n");
        return;
    }

    strncpy(v.offense_type, OFFENSES[choice - 1].name, OFFENSE_LEN - 1);
    v.offense_type[OFFENSE_LEN - 1] = '\0';
    v.base_fine = OFFENSES[choice - 1].fine;

    prev = count_previous(db, v.license_no);
    v.offense_no = prev + 1;
    v.fine_amount = calculate_fine(v.base_fine, prev);
    v.payment_status = UNPAID;

    if (db_add(db, &v)) {
        save_to_file(db);
        printf("\n  Violation recorded.\n");
        if (prev > 0)
            printf("  REPEAT OFFENDER! Offence #%d. Base Rs %.2f -> Penalty Rs %.2f\n",
                   v.offense_no, v.base_fine, v.fine_amount);
        else
            printf("  First offence. Fine: Rs %.2f\n", v.fine_amount);
    }
}

static void view_all(const Database *db)
{
    int i;
    if (db->size == 0) {
        printf("\n  No records found.\n");
        return;
    }
    print_header();
    for (i = 0; i < db->size; i++)
        print_record(i, &db->data[i]);
}

static void search_by_license(const Database *db)
{
    char lic[LICENSE_LEN];
    int i, found = 0;
    float due = 0;

    read_line("Enter license number to search: ", lic, LICENSE_LEN);
    to_upper_str(lic);

    for (i = 0; i < db->size; i++) {
        if (strcmp(db->data[i].license_no, lic) == 0) {
            if (!found) print_header();
            print_record(i, &db->data[i]);
            if (db->data[i].payment_status == UNPAID)
                due += db->data[i].fine_amount;
            found++;
        }
    }
    if (!found)
        printf("\n  No violations found for %s.\n", lic);
    else
        printf("\n  Total violations: %d | Outstanding dues: Rs %.2f\n", found, due);
}

static void pay_fine(Database *db)
{
    int n;
    Violation *v;

    view_all(db);
    if (db->size == 0) return;

    n = read_int("\nEnter record number to mark as paid (0 to cancel): ");
    if (n == 0) return;
    if (n < 1 || n > db->size) {
        printf("  Invalid record number.\n");
        return;
    }
    v = &db->data[n - 1];
    if (v->payment_status == PAID) {
        printf("  This fine is already paid.\n");
        return;
    }
    v->payment_status = PAID;
    save_to_file(db);
    printf("  Payment of Rs %.2f received for %s.\n", v->fine_amount, v->license_no);
}

static void show_repeat_offenders(const Database *db)
{
    int i, j, count, printed_any = 0;
    int already;

    printf("\n--- Repeat Offenders (2 or more violations) ---\n");
    for (i = 0; i < db->size; i++) {
        /* skip if this licence was already reported earlier in the array */
        already = 0;
        for (j = 0; j < i; j++)
            if (strcmp(db->data[i].license_no, db->data[j].license_no) == 0) {
                already = 1;
                break;
            }
        if (already) continue;

        count = 0;
        {
            float total = 0;
            for (j = 0; j < db->size; j++)
                if (strcmp(db->data[i].license_no, db->data[j].license_no) == 0) {
                    count++;
                    total += db->data[j].fine_amount;
                }
            if (count >= 2) {
                printf("  %-14s  Violations: %-3d  Total fines: Rs %.2f\n",
                       db->data[i].license_no, count, total);
                printed_any = 1;
            }
        }
    }
    if (!printed_any)
        printf("  No repeat offenders yet.\n");
}

static void summary_report(const Database *db)
{
    int counts[NUM_OFFENSES] = {0};
    float amounts[NUM_OFFENSES] = {0};
    int i, j, paid = 0, unpaid = 0;
    float revenue = 0, pending = 0;

    if (db->size == 0) {
        printf("\n  No data available for report.\n");
        return;
    }

    for (i = 0; i < db->size; i++) {
        const Violation *v = &db->data[i];
        for (j = 0; j < NUM_OFFENSES; j++) {
            if (strcmp(v->offense_type, OFFENSES[j].name) == 0) {
                counts[j]++;
                amounts[j] += v->fine_amount;
                break;
            }
        }
        if (v->payment_status == PAID) { paid++;   revenue += v->fine_amount; }
        else                           { unpaid++; pending += v->fine_amount; }
    }

    printf("\n================ VIOLATION SUMMARY REPORT ================\n");
    printf("%-28s %-8s %-14s\n", "Offense Type", "Count", "Total Fines");
    printf("----------------------------------------------------------\n");
    for (j = 0; j < NUM_OFFENSES; j++)
        if (counts[j] > 0)
            printf("%-28s %-8d Rs %-12.2f\n", OFFENSES[j].name, counts[j], amounts[j]);

    printf("----------------------------------------------------------\n");
    printf("Total violations recorded : %d\n", db->size);
    printf("Fines paid                : %d\n", paid);
    printf("Fines unpaid              : %d\n", unpaid);
    printf("Total revenue collected   : Rs %.2f\n", revenue);
    printf("Pending (unpaid) amount   : Rs %.2f\n", pending);
    printf("Total fines imposed       : Rs %.2f\n", revenue + pending);
    printf("==========================================================\n");
}

/* ---------- Main ---------- */

static void print_menu(void)
{
    printf("\n====== TRAFFIC VIOLATION & PENALTY MANAGEMENT ======\n");
    printf(" 1. Add new violation\n");
    printf(" 2. View all violations\n");
    printf(" 3. Search by license number\n");
    printf(" 4. Mark fine as paid\n");
    printf(" 5. Show repeat offenders\n");
    printf(" 6. Summary report & total revenue\n");
    printf(" 7. Exit\n");
    printf("====================================================\n");
}

int main(void)
{
    Database db;
    int choice;

    if (!db_init(&db)) {
        printf("Memory allocation failed.\n");
        return 1;
    }
    load_from_file(&db);

    do {
        print_menu();
        choice = read_int("Enter your choice: ");
        switch (choice) {
            case 1: add_violation(&db);        break;
            case 2: view_all(&db);             break;
            case 3: search_by_license(&db);    break;
            case 4: pay_fine(&db);             break;
            case 5: show_repeat_offenders(&db);break;
            case 6: summary_report(&db);       break;
            case 7: printf("\nData saved. Goodbye!\n"); break;
            default: printf("  Invalid choice. Try again.\n");
        }
    } while (choice != 7);

    save_to_file(&db);
    db_free(&db);
    return 0;
}