#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define PARKED_FILE "parked_vehicles.txt"
#define TEMP_FILE   "temp_parked.txt"
#define LOG_FILE    "parking_fee_records.txt"

// Maximum capacity limits per vehicle type
#define MAX_BIKES 30
#define MAX_CARS  20
#define MAX_HEAVY 10

// Default Fixed Fee Tariff per Entry (in Rs.)
float feeBike  = 20.0;
float feeCar   = 50.0;
float feeHeavy = 100.0;

struct Vehicle {
    int receiptId;
    char vehicleNo[20];
    int type;      // 1: Bike, 2: Car, 3: Heavy
    float feePaid; // Fee collected at entry
};

void toUpperStr(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = toupper((unsigned char)str[i]);
    }
}

const char* getTypeName(int type) {
    switch (type) {
        case 1: return "Bike";
        case 2: return "Car";
        case 3: return "Heavy Vehicle";
        default: return "Unknown";
    }
}

float getTariffFee(int type) {
    switch (type) {
        case 1: return feeBike;
        case 2: return feeCar;
        case 3: return feeHeavy;
        default: return 0.0;
    }
}

int getMaxLimit(int type) {
    switch (type) {
        case 1: return MAX_BIKES;
        case 2: return MAX_CARS;
        case 3: return MAX_HEAVY;
        default: return 0;
    }
}

void getCurrentTimestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", t);
}

void getCurrentDate(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d", t);
}

// Auto-generate next sequential Receipt ID by checking existing logs and active vehicles
int getNextReceiptId() {
    int maxId = 1000; // Starts receipts from 1001
    FILE *fp = fopen(LOG_FILE, "r");
    if (fp != NULL) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            char *rPtr = strstr(line, "Receipt: #");
            if (rPtr != NULL) {
                int id = 0;
                if (sscanf(rPtr + 10, "%d", &id) == 1 && id > maxId) {
                    maxId = id;
                }
            }
        }
        fclose(fp);
    }

    // Also check active parked file as backup
    fp = fopen(PARKED_FILE, "r");
    if (fp != NULL) {
        struct Vehicle v;
        while (fscanf(fp, "%d %19s %d %f", &v.receiptId, v.vehicleNo, &v.type, &v.feePaid) == 4) {
            if (v.receiptId > maxId) {
                maxId = v.receiptId;
            }
        }
        fclose(fp);
    }

    return maxId + 1;
}

// Record Entry and Exit into log file with Receipt ID
void logFeeEvent(const char *eventType, int receiptId, const char *vNo, const char *vTypeStr, float fee) {
    FILE *fp = fopen(LOG_FILE, "a");
    if (fp == NULL) {
        printf("[!] Warning: Could not write to %s\n", LOG_FILE);
        return;
    }

    char timestamp[30];
    getCurrentTimestamp(timestamp, sizeof(timestamp));

    fprintf(fp, "[%s] | %-6s | Receipt: #%-5d | Vehicle: %-15s | Type: %-13s | Fee: Rs. %.2f\n",
            timestamp, eventType, receiptId, vNo, vTypeStr, fee);
    fclose(fp);
}

// Get current parked count for a specific vehicle type
int getCountByType(int targetType) {
    FILE *fp = fopen(PARKED_FILE, "r");
    if (fp == NULL) return 0;

    struct Vehicle v;
    int count = 0;
    while (fscanf(fp, "%d %19s %d %f", &v.receiptId, v.vehicleNo, &v.type, &v.feePaid) == 4) {
        if (v.type == targetType) {
            count++;
        }
    }
    fclose(fp);
    return count;
}

int isVehicleParked(const char *vNo) {
    FILE *fp = fopen(PARKED_FILE, "r");
    if (fp == NULL) return 0;

    struct Vehicle v;
    while (fscanf(fp, "%d %19s %d %f", &v.receiptId, v.vehicleNo, &v.type, &v.feePaid) == 4) {
        if (strcmp(v.vehicleNo, vNo) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

// 1. Vehicle Entry & Single Receipt Generation
void vehicleEntry() {
    char vNo[20];
    int vType;

    int curBikes = getCountByType(1);
    int curCars  = getCountByType(2);
    int curHeavy = getCountByType(3);

    printf("\n--- Vehicle Entry & Receipt Generation ---\n");
    printf("Available Space -> Bikes: %d/%d | Cars: %d/%d | Heavy: %d/%d\n",
           MAX_BIKES - curBikes, MAX_BIKES,
           MAX_CARS - curCars, MAX_CARS,
           MAX_HEAVY - curHeavy, MAX_HEAVY);

    printf("Enter Vehicle Number (e.g., MH15AB1234): ");
    scanf("%19s", vNo);
    toUpperStr(vNo);

    if (isVehicleParked(vNo)) {
        printf("[!] Rejected: Vehicle %s is already inside parking!\n", vNo);
        return;
    }

    printf("Select Vehicle Category:\n");
    printf("1. Bike          - Rs. %.2f (Free: %d)\n", feeBike, MAX_BIKES - curBikes);
    printf("2. Car           - Rs. %.2f (Free: %d)\n", feeCar, MAX_CARS - curCars);
    printf("3. Heavy Vehicle - Rs. %.2f (Free: %d)\n", feeHeavy, MAX_HEAVY - curHeavy);
    printf("4. Reject Vehicle / Cancel Entry\n");
    printf("Enter choice (1-4): ");

    if (scanf("%d", &vType) != 1) {
        while (getchar() != '\n');
        printf("[!] Invalid input! Vehicle entry rejected.\n");
        return;
    }

    if (vType == 4) {
        printf("[X] Vehicle %s entry REJECTED by operator.\n", vNo);
        return;
    }

    if (vType < 1 || vType > 3) {
        printf("[!] Invalid vehicle type (%d)! Vehicle %s rejected.\n", vType, vNo);
        return;
    }

    int currentCount = getCountByType(vType);
    int maxLimit = getMaxLimit(vType);
    if (currentCount >= maxLimit) {
        printf("[!] FULL: %s parking capacity (%d/%d) is full! Vehicle %s rejected.\n",
               getTypeName(vType), currentCount, maxLimit, vNo);
        return;
    }

    int receiptId = getNextReceiptId();
    float paidFee = getTariffFee(vType);
    char entryTime[30];
    getCurrentTimestamp(entryTime, sizeof(entryTime));

    FILE *fp = fopen(PARKED_FILE, "a");
    if (fp == NULL) {
        printf("[!] Error opening %s!\n", PARKED_FILE);
        return;
    }

    fprintf(fp, "%d %s %d %.2f\n", receiptId, vNo, vType, paidFee);
    fclose(fp);

    logFeeEvent("ENTRY", receiptId, vNo, getTypeName(vType), paidFee);

    // Single Parking Receipt generated at Entry
    printf("\n=====================================\n");
    printf("        PARKING ENTRY RECEIPT        \n");
    printf("=====================================\n");
    printf("Receipt ID      : #%d\n", receiptId);
    printf("Entry Time      : %s\n", entryTime);
    printf("Vehicle Number  : %s\n", vNo);
    printf("Vehicle Type    : %s (%d/%d)\n", getTypeName(vType), currentCount + 1, maxLimit);
    printf("Total Fee Paid  : Rs. %.2f\n", paidFee);
    printf("Payment Status  : PAID (Valid for Exit)\n");
    printf("=====================================\n");
}

// 2. Vehicle Exit (Receipt Verification & Gate Clearance)
void vehicleExit() {
    FILE *fp = fopen(PARKED_FILE, "r");
    if (fp == NULL) {
        printf("\n[!] Parking lot is currently empty.\n");
        return;
    }

    char vNo[20];
    printf("\n--- Vehicle Exit & Receipt Check ---\n");
    printf("Enter Exiting Vehicle Number: ");
    scanf("%19s", vNo);
    toUpperStr(vNo);

    FILE *tempFp = fopen(TEMP_FILE, "w");
    if (tempFp == NULL) {
        printf("[!] Error creating temporary file!\n");
        fclose(fp);
        return;
    }

    struct Vehicle v;
    int found = 0;
    int exitedReceiptId = 0;
    int exitedType = 0;
    float paidAtEntry = 0.0;

    while (fscanf(fp, "%d %19s %d %f", &v.receiptId, v.vehicleNo, &v.type, &v.feePaid) == 4) {
        if (strcmp(v.vehicleNo, vNo) == 0 && !found) {
            found = 1;
            exitedReceiptId = v.receiptId;
            exitedType = v.type;
            paidAtEntry = v.feePaid;
        } else {
            fprintf(tempFp, "%d %s %d %.2f\n", v.receiptId, v.vehicleNo, v.type, v.feePaid);
        }
    }

    fclose(fp);
    fclose(tempFp);

    if (!found) {
        remove(TEMP_FILE);
        printf("[!] Vehicle %s not found in active parking!\n", vNo);
        return;
    }

    remove(PARKED_FILE);
    rename(TEMP_FILE, PARKED_FILE);

    char exitTime[30];
    getCurrentTimestamp(exitTime, sizeof(exitTime));
    logFeeEvent("EXIT", exitedReceiptId, vNo, getTypeName(exitedType), 0.0);

    printf("[OK] Exit Cleared at %s | Receipt #%d | Vehicle: %s (%s) | Paid Fee: Rs. %.2f Verified.\n",
           exitTime, exitedReceiptId, vNo, getTypeName(exitedType), paidAtEntry);
}

// 3. View Active Vehicles Type-Wise & Capacity
void showParkedVehiclesTypeWise() {
    FILE *fp = fopen(PARKED_FILE, "r");
    int bikes = 0, cars = 0, heavy = 0, total = 0;

    printf("\n--- Active Vehicles in Parking (Type-Wise) ---\n");
    if (fp == NULL) {
        printf("Bikes: 0/%d | Cars: 0/%d | Heavy: 0/%d | Total: 0/%d\n",
               MAX_BIKES, MAX_CARS, MAX_HEAVY, MAX_BIKES + MAX_CARS + MAX_HEAVY);
        return;
    }

    struct Vehicle v;
    printf("%-11s %-16s %-15s %-12s\n", "Receipt ID", "Vehicle Number", "Type", "Paid Fee");
    printf("---------------------------------------------------------\n");

    while (fscanf(fp, "%d %19s %d %f", &v.receiptId, v.vehicleNo, &v.type, &v.feePaid) == 4) {
        printf("#%-10d %-16s %-15s Rs. %-8.2f\n", v.receiptId, v.vehicleNo, getTypeName(v.type), v.feePaid);
        if (v.type == 1) bikes++;
        else if (v.type == 2) cars++;
        else if (v.type == 3) heavy++;
        total++;
    }
    fclose(fp);

    if (total == 0) {
        printf("No vehicles currently parked.\n");
    }

    printf("---------------------------------------------------------\n");
    printf("Occupied  -> Bikes: %d/%d | Cars: %d/%d | Heavy: %d/%d | Total: %d/%d\n",
           bikes, MAX_BIKES, cars, MAX_CARS, heavy, MAX_HEAVY,
           total, MAX_BIKES + MAX_CARS + MAX_HEAVY);
    printf("Available -> Bikes: %d | Cars: %d | Heavy: %d\n",
           MAX_BIKES - bikes, MAX_CARS - cars, MAX_HEAVY - heavy);
}

// 4. Daily Revenue Report (Calculated from Entry Receipts Issued Today)
void showTodayRevenue() {
    FILE *fp = fopen(LOG_FILE, "r");
    if (fp == NULL) {
        printf("\n[!] No fee records found yet. Today's Revenue: Rs. 0.00\n");
        return;
    }

    char today[15];
    getCurrentDate(today, sizeof(today));

    char line[256];
    float revBike = 0.0, revCar = 0.0, revHeavy = 0.0, totalRevenue = 0.0;
    int countBike = 0, countCar = 0, countHeavy = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, today) != NULL && strstr(line, "ENTRY") != NULL) {
            char *feePtr = strstr(line, "Fee: Rs. ");
            float fee = 0.0;
            if (feePtr != NULL && sscanf(feePtr + 9, "%f", &fee) == 1) {
                totalRevenue += fee;
                if (strstr(line, "Type: Bike") != NULL) {
                    revBike += fee;
                    countBike++;
                } else if (strstr(line, "Type: Car") != NULL) {
                    revCar += fee;
                    countCar++;
                } else if (strstr(line, "Type: Heavy Vehicle") != NULL) {
                    revHeavy += fee;
                    countHeavy++;
                }
            }
        }
    }
    fclose(fp);

    printf("\n==============================================\n");
    printf("    DAILY PARKING FEE REVENUE (%s)    \n", today);
    printf("==============================================\n");
    printf("Bike          (%d receipts) : Rs. %.2f\n", countBike, revBike);
    printf("Car           (%d receipts) : Rs. %.2f\n", countCar, revCar);
    printf("Heavy Vehicle (%d receipts) : Rs. %.2f\n", countHeavy, revHeavy);
    printf("----------------------------------------------\n");
    printf("Total Receipts Issued      : %d\n", countBike + countCar + countHeavy);
    printf("TOTAL COLLECTED REVENUE    : Rs. %.2f\n", totalRevenue);
    printf("==============================================\n");
}

// 5. Update Parking Fee Tariff Rates
void updateFeeRates() {
    printf("\n--- Update Parking Fee Tariff (Fixed Per Entry) ---\n");
    printf("Current Rates -> Bike: Rs. %.2f | Car: Rs. %.2f | Heavy: Rs. %.2f\n",
           feeBike, feeCar, feeHeavy);

    float newBike, newCar, newHeavy;
    printf("Enter new Bike fee (Rs.): ");
    if (scanf("%f", &newBike) != 1 || newBike < 0) {
        while (getchar() != '\n');
        printf("[!] Invalid rate. Update cancelled.\n");
        return;
    }

    printf("Enter new Car fee (Rs.): ");
    if (scanf("%f", &newCar) != 1 || newCar < 0) {
        while (getchar() != '\n');
        printf("[!] Invalid rate. Update cancelled.\n");
        return;
    }

    printf("Enter new Heavy Vehicle fee (Rs.): ");
    if (scanf("%f", &newHeavy) != 1 || newHeavy < 0) {
        while (getchar() != '\n');
        printf("[!] Invalid rate. Update cancelled.\n");
        return;
    }

    feeBike = newBike;
    feeCar = newCar;
    feeHeavy = newHeavy;
    printf("[+] Tariff updated successfully for new entries!\n");
}

int main() {
    int choice;

    while (1) {
        printf("\n=== PARKING FEE MANAGEMENT SYSTEM ===\n");
        printf("1. Vehicle Entry & Generate Single Fee Receipt\n");
        printf("2. Vehicle Exit (Verify Receipt & Clear Gate)\n");
        printf("3. Vehicles Left in Parking (Type-Wise & Capacity)\n");
        printf("4. Daily Fee Revenue Report (Category-Wise)\n");
        printf("5. Configure / Update Fee Tariff Rates\n");
        printf("6. Exit System\n");
        printf("Enter choice (1-6): ");

        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            printf("[!] Invalid input. Enter a number (1-6).\n");
            continue;
        }

        switch (choice) {
            case 1: vehicleEntry(); break;
            case 2: vehicleExit(); break;
            case 3: showParkedVehiclesTypeWise(); break;
            case 4: showTodayRevenue(); break;
            case 5: updateFeeRates(); break;
            case 6: printf("All fee records saved in %s. Exiting...\n", LOG_FILE); return 0;
            default: printf("[!] Invalid choice! Try again.\n");
        }
    }
}