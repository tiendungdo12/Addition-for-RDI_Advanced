#include "ReflectiveLoader.h"
#include <stdio.h>

// =============================================================
// CẤU HÌNH: HYBRID DIAGNOSTIC + OPTIMIZED READER TARGET
// =============================================================
#define STATIC_BASE_ADDR  0x2E48FB4
#define OFF_MGR           0x1C   // 28
#define OFF_CONTAINER     0x14   // 20
#define OFF_TARGET_LIST   0x220  // (Decimal 544) - ĐỀ BÀI

#define LOG_FILENAME      "audition_target_log.txt"

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

const char* GetKeyName(int val) {
    switch (val) {
    case 1: return "1";
    case 2: return "DW"; // Down
    case 3: return "3";
    case 4: return "LF"; // Left
    case 6: return "RT"; // Right
    case 7: return "7";
    case 8: return "UP"; // Up
    case 9: return "9";
    case 0: return "__";
    default: return "??";
    }
}

DWORD WINAPI HackThread(LPVOID lpParam) {
    WriteLog("=== HYBRID READER: DIAGNOSTIC + HEADER COUNT STARTED ===");
    Sleep(1000);

    // Biến lưu trạng thái lỗi Pointer (Anti-Spam)
    static int lastErrorState = -1;

    // Biến lưu Hash dữ liệu mũi tên (Để chỉ in khi Level thay đổi)
    static int lastDataHash = 0;

    while (1) {
        int currentErrorState = 0; // 0 = OK
        char errorMsg[256] = { 0 };

        __try {
            // --- PHẦN 1: KIỂM TRA POINTER CHAIN CHI TIẾT ---

            // LEVEL 1: BASE
            if (!IsValid(STATIC_BASE_ADDR)) {
                currentErrorState = 1;
                sprintf_s(errorMsg, 256, "[ERR] Base Address Invalid (Game Closed/Protected?)");
            }
            else {
                DWORD p1 = *(DWORD*)STATIC_BASE_ADDR;

                // LEVEL 2: MANAGER
                if (!IsValid(p1)) {
                    currentErrorState = 2;
                    sprintf_s(errorMsg, 256, "[ERR] Base Pointer is NULL/DEAD (0x%X -> ???)", STATIC_BASE_ADDR);
                }
                else {
                    DWORD pMgr = *(DWORD*)(p1 + OFF_MGR);

                    // LEVEL 3: CONTAINER
                    if (!IsValid(pMgr)) {
                        currentErrorState = 3;
                        sprintf_s(errorMsg, 256, "[ERR] Manager Pointer is NULL/DEAD (Base -> 0x%X)", p1);
                    }
                    else {
                        DWORD pCont = *(DWORD*)(pMgr + OFF_CONTAINER);

                        // LEVEL 4: TARGET LIST
                        if (!IsValid(pCont)) {
                            currentErrorState = 4;
                            sprintf_s(errorMsg, 256, "[ERR] Container Pointer is NULL/DEAD (Mgr -> 0x%X)", pMgr);
                        }
                        else {
                            DWORD pTarget = *(DWORD*)(pCont + OFF_TARGET_LIST);

                            if (!IsValid(pTarget)) {
                                currentErrorState = 5;
                                sprintf_s(errorMsg, 256, "[ERR] Target Pointer is NULL/DEAD (Cont -> 0x%X)", pCont);
                            }
                            else {
                                // --- PHẦN 2: ĐỌC DỮ LIỆU KHI POINTER NGON (STATE = 0) ---
                                currentErrorState = 0;

                                // Báo cáo nếu Pointer vừa hồi phục
                                if (lastErrorState != 0) {
                                    WriteLog("[STATUS] Pointer Chain RECOVERED (OK) -> Ready to read");
                                }

                                // 1. ĐỌC BIẾN COUNT (OFFSET +0)
                                int count = *(int*)pTarget;

                                // Chỉ xử lý nếu Count hợp lý (1-20)
                                if (count > 0 && count <= 20) {
                                    char buffer[1024] = { 0 };
                                    int currentHash = count;

                                    // Format Header
                                    sprintf_s(buffer, 1024, "[CNT:%d] ", count);

                                    // 2. VÒNG LẶP ĐỌC MŨI TÊN (TỪ 1 ĐẾN COUNT)
                                    for (int i = 1; i <= count; i++) {
                                        int val = *(int*)(pTarget + (i * 4));

                                        currentHash += val * (i + 1); // Tính Hash

                                        char temp[32];
                                        sprintf_s(temp, 32, "[%s] ", GetKeyName(val));
                                        strcat_s(buffer, 1024, temp);
                                    }

                                    // 3. GHI LOG DỮ LIỆU (NẾU THAY ĐỔI)
                                    if (currentHash != lastDataHash) {
                                        WriteLog("Ptr: 0x%X | %s", pTarget, buffer);
                                        lastDataHash = currentHash;
                                    }
                                }
                                else {
                                    // Count = 0 hoặc rác -> Reset Hash chờ ván mới
                                    if (lastDataHash != 0) lastDataHash = 0;
                                }
                            }
                        }
                    }
                }
            }

            // --- PHẦN 3: XỬ LÝ LOG LỖI POINTER (ANTI-SPAM) ---
            // Chỉ in lỗi Pointer khi trạng thái thay đổi
            if (currentErrorState != 0 && currentErrorState != lastErrorState) {
                WriteLog("%s", errorMsg);
                // Khi pointer lỗi, reset luôn hash dữ liệu
                lastDataHash = 0;
            }

            // Cập nhật trạng thái
            lastErrorState = currentErrorState;

        }
        __except (1) {
            WriteLog("[EXCEPTION] Crash prevented.");
        }

        Sleep(100); // Quét 100ms/lần
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