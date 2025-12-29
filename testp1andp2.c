#include "ReflectiveLoader.h"
#include <stdio.h>

// =============================================================
// CẤU HÌNH: SO SÁNH CHÉO (MANAGER vs CONTAINER)
// =============================================================
#define STATIC_BASE_ADDR  0x2E48FB4
#define OFF_MGR           0x1C
#define OFF_CONTAINER     0x14

#define OFF_TARGET_LIST   0x220  // (Trong Container)
#define OFF_COUNT_PTR     0x40   // (Trong Manager)

#define LOG_FILENAME      "audition_compare_diag.txt"

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

DWORD WINAPI HackThread(LPVOID lpParam) {
    WriteLog("=== CROSS-CHECK TOOL: MGR(0x40) vs CONT(0x220) ===");
    Sleep(1000);

    // Biến lưu trạng thái cũ
    static int lastCountHeader = -999;
    static int lastCountV4 = -999;

    while (1) {
        __try {
            // --- 1. BASE ---
            if (IsValid(STATIC_BASE_ADDR)) {
                DWORD p1 = *(DWORD*)STATIC_BASE_ADDR;

                // --- 2. MANAGER ---
                if (IsValid(p1)) {
                    DWORD pMgr = *(DWORD*)(p1 + OFF_MGR);

                    if (IsValid(pMgr)) {

                        // ====================================================
                        // NHÁNH 1: LẤY TỪ MANAGER (+0x40)
                        // ====================================================
                        int countV4 = -2;
                        DWORD ptrV3 = *(DWORD*)(pMgr + OFF_COUNT_PTR); // Lấy pointer tại Mgr + 0x40

                        // Kiểm tra ptrV3 có hợp lệ không
                        if (IsValid(ptrV3)) {
                            countV4 = *(int*)ptrV3; // Dereference để lấy giá trị thực (v4)
                        }
                        else {
                            // Nếu nó không phải pointer mà là giá trị luôn? (Debug case)
                            countV4 = (int)ptrV3;
                        }

                        // ====================================================
                        // NHÁNH 2: LẤY TỪ CONTAINER -> TARGET LIST (+0x220)
                        // ====================================================
                        int countHeader = -1;
                        DWORD pCont = *(DWORD*)(pMgr + OFF_CONTAINER); // Lấy Container tại Mgr + 0x14

                        if (IsValid(pCont)) {
                            DWORD pTarget = *(DWORD*)(pCont + OFF_TARGET_LIST); // Lấy Target tại Cont + 0x220

                            if (IsValid(pTarget)) {
                                countHeader = *(int*)pTarget; // Lấy Header (Count) tại Target[0]
                            }
                        }

                        // ====================================================
                        // SO SÁNH VÀ GHI LOG
                        // ====================================================
                        // Chỉ ghi log khi có sự thay đổi giá trị của 1 trong 2
                        if (countHeader != lastCountHeader || countV4 != lastCountV4) {

                            // Logic xác định MATCH:
                            // Cả 2 phải > 0 và bằng nhau
                            char* matchStatus = "NO";
                            if (countHeader > 0 && countV4 > 0 && countHeader == countV4) {
                                matchStatus = "YES !!!";
                            }

                            WriteLog("[COMPARE] Header(0x220): %d | Mgr_Offset40: %d | Match: %s",
                                countHeader,
                                countV4,
                                matchStatus);

                            // Cập nhật
                            lastCountHeader = countHeader;
                            lastCountV4 = countV4;
                        }
                    }
                }
            }
        }
        __except (1) {}

        Sleep(100);
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        hAppInstance = hinstDLL;
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)HackThread, NULL, 0, NULL);
    }
    return TRUE;
}