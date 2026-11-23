#include <windows.h> // Windows API header for GUI windows, controls, fonts, and drawing
#include <stdio.h>   // Standard I/O header for file handling and pipes
#include <stdlib.h>  // Standard library header for atoi() string-to-integer conversion
#include <string.h>  // String header for strstr(), strlen(), and snprintf()

// Unique numeric IDs for the 6 left-sidebar navigation buttons
#define NAV_ENTRY   201
#define NAV_EXIT    202
#define NAV_PARKED  203
#define NAV_REV     204
#define NAV_TARIFF  205
#define NAV_QUIT    206

// Unique numeric IDs for form inputs, radio buttons, action buttons, and output box
#define ID_VNO        101
#define ID_RB_BIKE    111
#define ID_RB_CAR     112
#define ID_RB_HEAVY   113
#define ID_RB_EX_VNO  114
#define ID_RB_EX_RID  115
#define ID_ACT_IN     103
#define ID_ACT_OUT    104
#define ID_FB         105
#define ID_FC         106
#define ID_FH         107
#define ID_ACT_RATE   108
#define ID_OUT        109

// Global window handles (HWND) for all UI controls so any function can read/update them
HWND hLblTitle, hLblVno, hVno, hLblType, hRbBike, hRbCar, hRbHeavy, hBtnIn, hBtnOut;
HWND hLblExitBy, hRbExVno, hRbExRid;
HWND hLblBike, hLblCar, hLblHeavy, hFb, hFc, hFh, hBtnRate, hLblRateStatus, hOut;
HWND hNav[6];

// Global handles for smooth ClearType fonts and custom background color brushes
HFONT hTitleFont, hUiFont, hBoldFont, hBtnFont, hMonoFont;
HBRUSH hBgBrush, hSideBrush, hCardBrush;
int activeTab = NAV_ENTRY; // Tracks which sidebar tab is currently selected

// Callback function used by EnumChildWindows to apply a smooth font to every child control
BOOL CALLBACK SetChildFont(HWND child, LPARAM font) {
    SendMessageA(child, WM_SETFONT, (WPARAM)font, TRUE);
    return TRUE;
}

// Finds the matching Vehicle Number for a given Receipt ID inside parked_vehicles.txt
int resolveVehicleByReceiptId(const char *rawInput, char *outVno, size_t outSize) {
    const char *numStr = (rawInput[0] == '#') ? rawInput + 1 : rawInput; // Skip '#' if typed
    int targetId = atoi(numStr);                                         // Convert text ID to integer
    if (targetId <= 0) return 0;                                         // Reject invalid or non-numeric IDs

    FILE *fp = fopen("parked_vehicles.txt", "r");                        // Open active vehicles data file
    if (!fp) return 0;

    int rid = 0, vtype = 0;
    char vno[32] = {0};
    float fee = 0.0f;
    while (fscanf(fp, "%d %31s %d %f", &rid, vno, &vtype, &fee) == 4) {  // Read each parked record
        if (rid == targetId) {                                           // Match found for Receipt ID
            snprintf(outVno, outSize, "%s", vno);                        // Copy matching vehicle number
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0; // Receipt ID not found
}

// Passes simulated keyboard inputs into parking.exe silently and captures its console output
void executeParkingCLI(const char *inputs, const char *trimMarker, char *outBuf, size_t outSize) {
    FILE *in = fopen("gui_in.tmp", "w"); // Create temporary input file for CLI redirection
    if (!in) return;
    fprintf(in, "%s\n6\n", inputs);      // Write menu inputs followed by '6' to exit CLI cleanly
    fclose(in);

    FILE *pipe = _popen("parking.exe < gui_in.tmp", "r"); // Run backend exe and read stdout pipe
    if (!pipe) {
        snprintf(outBuf, outSize, "Error: parking.exe not found in current folder!");
        return;
    }

    char raw[8192] = {0};
    fread(raw, 1, sizeof(raw) - 1, pipe); // Read entire console output from parking.exe
    _pclose(pipe);                        // Close process pipe
    remove("gui_in.tmp");                 // Delete temporary input file

    char *start = strstr(raw, "Enter choice (1-6): "); // Skip initial menu banner
    start = start ? start + 20 : raw;
    char *end = strstr(start, "=== PARKING FEE MANAGEMENT SYSTEM ==="); // Cut off trailing exit menu
    if (end) *end = '\0';

    if (trimMarker && strstr(start, trimMarker)) { // Jump directly to clean receipt/table start marker
        start = strstr(start, trimMarker);
    }
    while (*start == '\n' || *start == '\r' || *start == ' ') start++; // Trim leading whitespace

    size_t j = 0;
    for (int i = 0; start[i] && j < outSize - 2; i++) { // Convert Unix '\n' to Windows '\r\n' line breaks
        if (start[i] == '\n') outBuf[j++] = '\r';
        outBuf[j++] = start[i];
    }
    outBuf[j] = '\0';
}

// Helper wrapper that runs executeParkingCLI and displays the result directly in the output card
void runParkingCLI(const char *inputs, const char *trimMarker) {
    char formatted[16384] = {0};
    executeParkingCLI(inputs, trimMarker, formatted, sizeof(formatted));
    SetWindowTextA(hOut, formatted);
}

// Refreshes the Vehicle Exit screen with optional status message plus all currently parked vehicles
void refreshExitScreen(const char *statusMsg) {
    char parkedList[12288] = {0};
    char combined[16384] = {0};
    executeParkingCLI("3", "--- Active Vehicles", parkedList, sizeof(parkedList)); // Fetch active vehicles

    if (statusMsg && strlen(statusMsg) > 0) {
        snprintf(combined, sizeof(combined), "%s\r\n\r\n%s", statusMsg, parkedList);
    } else {
        snprintf(combined, sizeof(combined), "%s", parkedList);
    }
    SetWindowTextA(hOut, combined);
}

// Switches active screen view by hiding all controls first and showing only the selected tab's widgets
void switchTab(int tab) {
    activeTab = tab;
    for (int i = 0; i < 6; i++) InvalidateRect(hNav[i], NULL, TRUE); // Redraw sidebar buttons for active highlight

    // Hide every form control before revealing only the chosen tab's elements
    ShowWindow(hLblVno, SW_HIDE);        ShowWindow(hVno, SW_HIDE);
    ShowWindow(hLblType, SW_HIDE);       ShowWindow(hRbBike, SW_HIDE);
    ShowWindow(hRbCar, SW_HIDE);         ShowWindow(hRbHeavy, SW_HIDE);
    ShowWindow(hLblExitBy, SW_HIDE);     ShowWindow(hRbExVno, SW_HIDE);
    ShowWindow(hRbExRid, SW_HIDE);
    ShowWindow(hBtnIn, SW_HIDE);         ShowWindow(hBtnOut, SW_HIDE);
    ShowWindow(hLblBike, SW_HIDE);       ShowWindow(hFb, SW_HIDE);
    ShowWindow(hLblCar, SW_HIDE);        ShowWindow(hFc, SW_HIDE);
    ShowWindow(hLblHeavy, SW_HIDE);      ShowWindow(hFh, SW_HIDE);
    ShowWindow(hBtnRate, SW_HIDE);       ShowWindow(hLblRateStatus, SW_HIDE);
    ShowWindow(hOut, SW_HIDE);

    SetWindowTextA(hVno, "");            // Clear previous text input
    SetWindowTextA(hOut, "");            // Clear previous output card text
    SetWindowTextA(hLblRateStatus, "");  // Clear previous tariff save message

    if (tab == NAV_ENTRY) { // Show Vehicle Entry form + receipt card
        SetWindowTextA(hLblTitle, "VEHICLE ENTRY & RECEIPT GENERATION");
        SetWindowTextA(hLblVno, "Vehicle Number:");
        ShowWindow(hLblVno, SW_SHOW);  ShowWindow(hVno, SW_SHOW);
        ShowWindow(hLblType, SW_SHOW); ShowWindow(hRbBike, SW_SHOW);
        ShowWindow(hRbCar, SW_SHOW);   ShowWindow(hRbHeavy, SW_SHOW);
        ShowWindow(hBtnIn, SW_SHOW);
        MoveWindow(hOut, 230, 165, 620, 355, TRUE);
        ShowWindow(hOut, SW_SHOW);
        SetWindowTextA(hOut, "Enter Vehicle Number, pick a category, and click 'Generate Receipt'.");
    } else if (tab == NAV_EXIT) { // Show Vehicle Exit form (Vehicle No / Receipt ID) + live parked list
        SetWindowTextA(hLblTitle, "VEHICLE EXIT & RECEIPT VERIFICATION");
        BOOL byRid = (SendMessageA(hRbExRid, BM_GETCHECK, 0, 0) == BST_CHECKED);
        SetWindowTextA(hLblVno, byRid ? "Receipt ID:" : "Vehicle Number:");
        ShowWindow(hLblExitBy, SW_SHOW); ShowWindow(hRbExVno, SW_SHOW);
        ShowWindow(hRbExRid, SW_SHOW);
        ShowWindow(hLblVno, SW_SHOW);    ShowWindow(hVno, SW_SHOW);
        ShowWindow(hBtnOut, SW_SHOW);
        MoveWindow(hOut, 230, 165, 620, 355, TRUE);
        ShowWindow(hOut, SW_SHOW);
        refreshExitScreen(NULL);
    } else if (tab == NAV_PARKED) { // Show full-height type-wise parked vehicles report
        SetWindowTextA(hLblTitle, "VEHICLES LEFT IN PARKING (TYPE-WISE)");
        MoveWindow(hOut, 230, 72, 620, 448, TRUE);
        ShowWindow(hOut, SW_SHOW);
        runParkingCLI("3", "--- Active Vehicles");
    } else if (tab == NAV_REV) { // Show full-height daily revenue report
        SetWindowTextA(hLblTitle, "DAILY PARKING FEE REVENUE REPORT");
        MoveWindow(hOut, 230, 72, 620, 448, TRUE);
        ShowWindow(hOut, SW_SHOW);
        runParkingCLI("4", "=======");
    } else if (tab == NAV_TARIFF) { // Show stacked tariff configuration inputs without output card
        SetWindowTextA(hLblTitle, "CONFIGURE FIXED ENTRY TARIFF RATES");
        ShowWindow(hLblBike, SW_SHOW);       ShowWindow(hFb, SW_SHOW);
        ShowWindow(hLblCar, SW_SHOW);        ShowWindow(hFc, SW_SHOW);
        ShowWindow(hLblHeavy, SW_SHOW);      ShowWindow(hFh, SW_SHOW);
        ShowWindow(hBtnRate, SW_SHOW);
        ShowWindow(hLblRateStatus, SW_SHOW);
    }
}

// Main Windows Message Loop Handler (creates controls, draws custom buttons/colors, handles clicks)
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CREATE) { // Runs once when window starts: creates brushes, fonts, and child controls
        hBgBrush   = CreateSolidBrush(RGB(244, 247, 251)); // Light slate background brush
        hSideBrush = CreateSolidBrush(RGB(24, 32, 44));    // Dark navy sidebar brush
        hCardBrush = CreateSolidBrush(RGB(255, 255, 255)); // Pure white output card brush

        // Create smooth anti-aliased ClearType fonts for headers, labels, buttons, and monospace tables
        hTitleFont = CreateFontA(23, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        hUiFont    = CreateFontA(19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        hBoldFont  = CreateFontA(18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        hBtnFont   = CreateFontA(19, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        hMonoFont  = CreateFontA(19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");

        // Create the 6 custom-drawn sidebar menu buttons
        const char *labels[6] = {
            "1. Vehicle Entry", "2. Vehicle Exit", "3. Parked Vehicles",
            "4. Today's Revenue", "5. Update Tariff", "6. Exit Program"
        };
        for (int i = 0; i < 6; i++) {
            hNav[i] = CreateWindowA("BUTTON", labels[i], WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                                    14, 22 + i * 58, 184, 46, hwnd, (HMENU)(INT_PTR)(NAV_ENTRY + i), NULL, NULL);
        }

        hLblTitle = CreateWindowA("STATIC", "", WS_VISIBLE | WS_CHILD, 230, 24, 620, 32, hwnd, NULL, NULL, NULL); // Page heading

        // Create Vehicle Number / Receipt ID uppercase input box
        hLblVno  = CreateWindowA("STATIC", "Vehicle Number:", WS_CHILD, 230, 76, 140, 28, hwnd, NULL, NULL, NULL);
        hVno     = CreateWindowA("EDIT", "", WS_CHILD | WS_BORDER | ES_UPPERCASE, 375, 73, 240, 32, hwnd, (HMENU)ID_VNO, NULL, NULL);

        // Create Vehicle Category radio button group (Bike, Car, Heavy)
        hLblType = CreateWindowA("STATIC", "Vehicle Category:", WS_CHILD, 230, 120, 140, 28, hwnd, NULL, NULL, NULL);
        hRbBike  = CreateWindowA("BUTTON", "Bike", WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP, 375, 118, 70, 30, hwnd, (HMENU)ID_RB_BIKE, NULL, NULL);
        hRbCar   = CreateWindowA("BUTTON", "Car", WS_CHILD | BS_AUTORADIOBUTTON, 452, 118, 65, 30, hwnd, (HMENU)ID_RB_CAR, NULL, NULL);
        hRbHeavy = CreateWindowA("BUTTON", "Heavy", WS_CHILD | BS_AUTORADIOBUTTON, 524, 118, 90, 30, hwnd, (HMENU)ID_RB_HEAVY, NULL, NULL);
        SendMessageA(hRbBike, BM_SETCHECK, BST_CHECKED, 0); // Select 'Bike' by default

        // Create Exit Search Mode radio button group (Vehicle No vs Receipt ID)
        hLblExitBy = CreateWindowA("STATIC", "Exit Search By:", WS_CHILD, 230, 120, 140, 28, hwnd, NULL, NULL, NULL);
        hRbExVno   = CreateWindowA("BUTTON", "Vehicle No", WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP, 375, 118, 118, 30, hwnd, (HMENU)ID_RB_EX_VNO, NULL, NULL);
        hRbExRid   = CreateWindowA("BUTTON", "Receipt ID", WS_CHILD | BS_AUTORADIOBUTTON, 500, 118, 115, 30, hwnd, (HMENU)ID_RB_EX_RID, NULL, NULL);
        SendMessageA(hRbExVno, BM_SETCHECK, BST_CHECKED, 0); // Select 'Vehicle No' by default

        // Create rounded primary action buttons for Entry and Exit
        hBtnIn  = CreateWindowA("BUTTON", "Generate Receipt", WS_CHILD | BS_OWNERDRAW, 632, 72, 218, 76, hwnd, (HMENU)ID_ACT_IN, NULL, NULL);
        hBtnOut = CreateWindowA("BUTTON", "Verify & Clear Exit", WS_CHILD | BS_OWNERDRAW, 632, 72, 218, 76, hwnd, (HMENU)ID_ACT_OUT, NULL, NULL);

        // Create stacked Tariff configuration labels, input fields, and single Save button
        hLblBike  = CreateWindowA("STATIC", "1. Bike Fee (Rs.):", WS_CHILD, 230, 86, 200, 30, hwnd, NULL, NULL, NULL);
        hFb       = CreateWindowA("EDIT", "20", WS_CHILD | WS_BORDER, 440, 83, 160, 34, hwnd, (HMENU)ID_FB, NULL, NULL);

        hLblCar   = CreateWindowA("STATIC", "2. Car Fee (Rs.):", WS_CHILD, 230, 138, 200, 30, hwnd, NULL, NULL, NULL);
        hFc       = CreateWindowA("EDIT", "50", WS_CHILD | WS_BORDER, 440, 135, 160, 34, hwnd, (HMENU)ID_FC, NULL, NULL);

        hLblHeavy = CreateWindowA("STATIC", "3. Heavy Vehicle (Rs.):", WS_CHILD, 230, 190, 200, 30, hwnd, NULL, NULL, NULL);
        hFh       = CreateWindowA("EDIT", "100", WS_CHILD | WS_BORDER, 440, 187, 160, 34, hwnd, (HMENU)ID_FH, NULL, NULL);

        hBtnRate  = CreateWindowA("BUTTON", "Save All Rates", WS_CHILD | BS_OWNERDRAW, 230, 248, 370, 44, hwnd, (HMENU)ID_ACT_RATE, NULL, NULL);
        hLblRateStatus = CreateWindowA("STATIC", "", WS_CHILD, 230, 310, 580, 32, hwnd, NULL, NULL, NULL);

        // Create read-only scrollable output card for receipts, tables, and revenue reports
        hOut = CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                             230, 165, 620, 355, hwnd, (HMENU)ID_OUT, NULL, NULL);

        // Apply smooth fonts to every child widget
        EnumChildWindows(hwnd, SetChildFont, (LPARAM)hUiFont);
        SendMessageA(hLblTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);
        SendMessageA(hLblRateStatus, WM_SETFONT, (WPARAM)hBoldFont, TRUE);
        SendMessageA(hOut, WM_SETFONT, (WPARAM)hMonoFont, TRUE);

        switchTab(NAV_ENTRY); // Open Vehicle Entry tab on startup
    } else if (msg == WM_DRAWITEM) { // Custom paints rounded buttons with drop shadows and press animation
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lp;
        RECT rc = dis->rcItem;
        BOOL isSidebar = (dis->CtlID >= NAV_ENTRY && dis->CtlID <= NAV_QUIT);
        BOOL isPressed = (dis->itemState & ODS_SELECTED);

        FillRect(dis->hDC, &rc, isSidebar ? hSideBrush : hBgBrush); // Match surrounding background

        COLORREF bg = RGB(37, 99, 235), border = RGB(29, 78, 216), shadow = RGB(195, 208, 226);
        if (isSidebar) {
            shadow = RGB(14, 19, 27);
            if ((int)dis->CtlID == activeTab) { bg = RGB(37, 99, 235); border = RGB(96, 165, 250); }
            else if (dis->CtlID == NAV_QUIT)  { bg = RGB(185, 28, 28); border = RGB(220, 38, 38); }
            else                              { bg = RGB(38, 49, 66);  border = RGB(55, 70, 92); }
        } else if (dis->CtlID == ID_ACT_IN) {
            bg     = isPressed ? RGB(15, 118, 110) : RGB(13, 148, 136);
            border = RGB(15, 118, 110);
            shadow = RGB(180, 210, 208);
        } else if (dis->CtlID == ID_ACT_OUT) {
            bg     = isPressed ? RGB(180, 83, 9) : RGB(217, 119, 6);
            border = RGB(180, 83, 9);
        }

        if (!isPressed) { // Paint subtle bottom shadow when button is unpressed
            HBRUSH shBrush = CreateSolidBrush(shadow);
            HPEN shPen = CreatePen(PS_SOLID, 1, shadow);
            SelectObject(dis->hDC, shBrush);
            SelectObject(dis->hDC, shPen);
            RoundRect(dis->hDC, rc.left + 1, rc.top + 3, rc.right, rc.bottom, 14, 14);
            DeleteObject(shBrush);
            DeleteObject(shPen);
        }

        int offset = isPressed ? 2 : 0; // Shift button down 2px when clicked
        HBRUSH btnBrush = CreateSolidBrush(bg);
        HPEN btnPen = CreatePen(PS_SOLID, 1, border);
        SelectObject(dis->hDC, btnBrush);
        SelectObject(dis->hDC, btnPen);
        RoundRect(dis->hDC, rc.left, rc.top + offset, rc.right - 1, rc.bottom - 2 + offset, 14, 14);
        DeleteObject(btnBrush);
        DeleteObject(btnPen);

        RECT textRc = { rc.left, rc.top + offset, rc.right - 1, rc.bottom - 2 + offset };
        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, RGB(255, 255, 255));
        SelectObject(dis->hDC, isSidebar ? hBoldFont : hBtnFont);
        char text[64];
        GetWindowTextA(dis->hwndItem, text, sizeof(text));
        DrawTextA(dis->hDC, text, -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE); // Draw centered button label
        return TRUE;
    } else if (msg == WM_CTLCOLORSTATIC) { // Sets text and background colors for labels and read-only output card
        HDC hdc = (HDC)wp;
        if ((HWND)lp == hOut) {
            SetTextColor(hdc, RGB(15, 23, 42));  // Deep black text inside output card
            SetBkColor(hdc, RGB(255, 255, 255)); // Pure white background inside output card
            return (LRESULT)hCardBrush;
        }
        if ((HWND)lp == hLblRateStatus) {
            SetTextColor(hdc, RGB(13, 148, 136));
        } else {
            SetTextColor(hdc, (HWND)lp == hLblTitle ? RGB(15, 23, 42) : RGB(51, 65, 85));
        }
        SetBkColor(hdc, RGB(244, 247, 251));
        return (LRESULT)hBgBrush;
    } else if (msg == WM_PAINT) { // Paints the dark navy left sidebar panel
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT side = {0, 0, 212, 600};
        FillRect(hdc, &side, hSideBrush);
        EndPaint(hwnd, &ps);
    } else if (msg == WM_COMMAND) { // Handles all sidebar clicks, radio toggles, and form submissions
        int id = LOWORD(wp);
        if (id >= NAV_ENTRY && id <= NAV_TARIFF) {
            switchTab(id); // Switch to clicked menu screen
        } else if (id == NAV_QUIT) {
            PostQuitMessage(0); // Close application
        } else if (id == ID_RB_EX_VNO) {
            SetWindowTextA(hLblVno, "Vehicle Number:"); // Update label for Vehicle Number exit
            SetWindowTextA(hVno, "");
        } else if (id == ID_RB_EX_RID) {
            SetWindowTextA(hLblVno, "Receipt ID:");     // Update label for Receipt ID exit
            SetWindowTextA(hVno, "");
        } else if (id == ID_ACT_IN) { // Handle Generate Receipt button click
            char vno[32] = {0}, cmd[128] = {0};
            GetWindowTextA(hVno, vno, sizeof(vno));
            if (strlen(vno) == 0) {
                SetWindowTextA(hOut, "[!] Please enter a Vehicle Number first.");
                return 0;
            }
            int typeIdx = 1;
            if (SendMessageA(hRbCar, BM_GETCHECK, 0, 0) == BST_CHECKED) typeIdx = 2;
            else if (SendMessageA(hRbHeavy, BM_GETCHECK, 0, 0) == BST_CHECKED) typeIdx = 3;

            sprintf(cmd, "1\n%s\n%d", vno, typeIdx);
            runParkingCLI(cmd, "======="); // Execute entry in backend and print receipt
            SetWindowTextA(hVno, "");
        } else if (id == ID_ACT_OUT) { // Handle Verify & Clear Exit button click
            char inputVal[32] = {0}, targetVno[32] = {0}, cmd[128] = {0}, exitStatus[512] = {0};
            GetWindowTextA(hVno, inputVal, sizeof(inputVal));
            BOOL byRid = (SendMessageA(hRbExRid, BM_GETCHECK, 0, 0) == BST_CHECKED);

            if (strlen(inputVal) == 0) {
                refreshExitScreen(byRid ? "[!] Please enter a Receipt ID first."
                                        : "[!] Please enter an Exiting Vehicle Number first.");
                return 0;
            }

            if (byRid) { // Convert Receipt ID to Vehicle Number before calling backend
                if (!resolveVehicleByReceiptId(inputVal, targetVno, sizeof(targetVno))) {
                    char errMsg[128];
                    snprintf(errMsg, sizeof(errMsg), "[!] Receipt ID #%s not found in active parking!",
                             (inputVal[0] == '#') ? inputVal + 1 : inputVal);
                    refreshExitScreen(errMsg);
                    return 0;
                }
            } else {
                snprintf(targetVno, sizeof(targetVno), "%s", inputVal);
            }

            sprintf(cmd, "2\n%s", targetVno);
            executeParkingCLI(cmd, "[", exitStatus, sizeof(exitStatus)); // Clear exit via parking.exe
            refreshExitScreen(exitStatus);                                // Refresh active list + show status
            SetWindowTextA(hVno, "");
        } else if (id == ID_ACT_RATE) { // Handle Save All Rates button click
            char b[16], c[16], h[16], cmd[128], rateResp[256] = {0};
            GetWindowTextA(hFb, b, sizeof(b));
            GetWindowTextA(hFc, c, sizeof(c));
            GetWindowTextA(hFh, h, sizeof(h));
            sprintf(cmd, "5\n%s\n%s\n%s", b, c, h);
            executeParkingCLI(cmd, "[", rateResp, sizeof(rateResp));
            SetWindowTextA(hLblRateStatus, rateResp);
        }
    } else if (msg == WM_DESTROY) { // Free GDI fonts and brushes cleanly before program exits
        DeleteObject(hTitleFont); DeleteObject(hUiFont);
        DeleteObject(hBoldFont);  DeleteObject(hBtnFont); DeleteObject(hMonoFont);
        DeleteObject(hBgBrush);   DeleteObject(hSideBrush); DeleteObject(hCardBrush);
        PostQuitMessage(0);
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

// Windows GUI entry point: enables sharp DPI scaling, registers window class, and runs message loop
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR args, int show) {
    SetProcessDPIAware(); // Prevents Windows display scaling blur

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hbrBackground = CreateSolidBrush(RGB(244, 247, 251));
    wc.lpszClassName = "ParkingModernGUI";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA("ParkingModernGUI", "Parking Fee Management System",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
                              150, 80, 885, 575, NULL, NULL, hInst, NULL);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) { // Keep GUI responsive until user closes window
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}