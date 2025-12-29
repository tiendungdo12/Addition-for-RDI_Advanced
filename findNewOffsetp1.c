#include "ReflectiveLoader.h"
#include <stdio.h>
#include <windows.h>

// =============================================================
// CẤU HÌNH: P1 OFFSET HUNTER
// =============================================================
#define STATIC_BASE_ADDR  0x2E48FB4  // Đã chuẩn
#define OFF_MGR           0x1C       // Đã chuẩn
#define OFF_CONTAINER     0x14       // Đã chuẩn
#define OFF_P2_TARGET     0x220      // Đã chuẩn (Làm mốc đối chiếu)

#define MAX_SCAN_RANGE    0x5000     // Quét 20KB trong Manager (Quá đủ cho 1 class)
#define LOG_FILENAME      "audition_p1_hunter.txt"

extern HINSTANCE hAppInstance;

void WriteLog(const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    FILE* f = NULL;
    fopen_s(&f, LOG_FILENAME, "a");
    if (f) { 
        fprintf(f, "%s\n", buffer); 
        fclose(f); 
    }
}

BOOL IsValid(DWORD addr) {
    if (addr < 0x10000 || addr > 0x7FFFFFFF) 
        return FALSE;
    return !IsBadReadPtr((const void*)addr, 4);
}

DWORD WINAPI HunterThread(LPVOID lpParam) {
    WriteLog("=== P1 OFFSET HUNTER STARTED ===");
    WriteLog("[INFO] Base: 0x%X", STATIC_BASE_ADDR);
    WriteLog("[INST] Please START DANCING. I need P2 > 0 to start scanning.");

    static int lastScanLevel = 0;

    while (1) {
        __try {
            // 1. LẤY GIÁ TRỊ CHUẨN TỪ P2 (TARGET LIST)
            if (!IsValid(STATIC_BASE_ADDR)) { Sleep(100); continue; }
            DWORD p1 = *(DWORD*)STATIC_BASE_ADDR;
            if (!IsValid(p1)) { Sleep(100); continue; }

            DWORD pMgr = *(DWORD*)(p1 + OFF_MGR); // Đây là Base để quét tìm P1
            if (!IsValid(pMgr)) { Sleep(100); continue; }

            DWORD pCont = *(DWORD*)(pMgr + OFF_CONTAINER);
            if (IsValid(pCont)) {
                DWORD ptr_P2 = *(DWORD*)(pCont + OFF_P2_TARGET);

                if (IsValid(ptr_P2)) {
                    // Đây là "Sự thật": Số lượng mũi tên thực tế
                    int trueCount = *(int*)ptr_P2;

                    // CHỈ QUÉT KHI CÓ MŨI TÊN VÀ LEVEL THAY ĐỔI
                    if (trueCount > 0 && trueCount <= 20 && trueCount != lastScanLevel) {

                        WriteLog("------------------------------------------------");
                        WriteLog("[HUNT] Level changed to %d. Scanning Manager for matching value...", trueCount);

                        int foundCandidates = 0;

                        // 2. CHIẾN LƯỢC SCAN: QUÉT TỪNG 4 BYTE TRONG MANAGER
                        for (int offset = 0; offset < MAX_SCAN_RANGE; offset += 4) {

                            // Kiểm tra xem địa chỉ này có đọc được không (tránh crash)
                            if (IsValid(pMgr + offset)) {
                                int valInMgr = *(int*)(pMgr + offset);

                                // NẾU GIÁ TRỊ TRONG MANAGER == SỐ MŨI TÊN THỰC TẾ
                                if (valInMgr == trueCount) {
                                    WriteLog("   [FOUND] Match at Offset: +0x%X (Value: %d)", offset, valInMgr);
                                    foundCandidates++;
                                }
                            }
                        }

                        if (foundCandidates == 0) {
                            WriteLog("   [FAIL] No offset in Manager matches the value %d.", trueCount);
                        }
                        else {
                            WriteLog("   [INFO] Found %d candidates. Wait for next level to filter.", foundCandidates);
                        }

                        lastScanLevel = trueCount;
                    }
                }
            }
        }
        __except (1) {
            WriteLog("[EXCEPTION] Crash prevented during scan.");
            Sleep(1000);
        }
        Sleep(100); // Check liên tục
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        hAppInstance = hinstDLL;
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)HunterThread, NULL, 0, NULL);
    }
    return TRUE;
}