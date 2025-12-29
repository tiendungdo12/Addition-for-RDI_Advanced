#include "ReflectiveLoader.h"
#include <stdio.h>

// =============================================================
// CẤU HÌNH DIAGNOSTIC USER INPUT (CHẨN ĐOÁN)
// =============================================================
#define STATIC_BASE_ADDR  0x2E48FB4
#define OFF_MGR           0x1C
#define OFF_CONTAINER     0x14
#define OFF_ARROW_LIST    0x224
#define LOG_FILENAME      "audition_user_input_log.txt"

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
    case 2: return "DOWN";
    case 3: return "3";
    case 4: return "LEFT";
    case 6: return "RIGHT";
    case 7: return "7";
    case 8: return "UP";
    case 9: return "9";
    case 0: return "ZERO";
    default: return "GARBAGE";
    }
}

DWORD WINAPI HackThread(LPVOID lpParam) {
    WriteLog("=== DIAGNOSTIC MODE (CLEAN LOG) STARTED ===");
    Sleep(1000);

    // Biến lưu mã lỗi lần trước (0: OK, 1: Base lỗi, 2: Mgr lỗi...)
    static int lastErrorState = -1;

    while (1) {
        int currentErrorState = 0; // Giả định là OK (0)
        char errorMsg[256] = { 0 };

        __try {
            // --- KIỂM TRA TỪNG CẤP POINTER ---

            // LEVEL 1: BASE
            if (!IsValid(STATIC_BASE_ADDR)) {
                currentErrorState = 1;
                sprintf_s(errorMsg, 256, "[ERR] Base Address Invalid (Game Closed or Protected?)");
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

                        // LEVEL 4: ARROW LIST
                        if (!IsValid(pCont)) {
                            currentErrorState = 4;
                            sprintf_s(errorMsg, 256, "[ERR] Container Pointer is NULL/DEAD (Mgr -> 0x%X)", pMgr);
                        }
                        else {
                            DWORD pList = *(DWORD*)(pCont + OFF_ARROW_LIST);

                            if (!IsValid(pList)) {
                                currentErrorState = 5;
                                sprintf_s(errorMsg, 256, "[ERR] ArrowList Pointer is NULL/DEAD (Cont -> 0x%X)", pCont);
                            }
                            else {
                                // --- MỌI THỨ ĐỀU OK (STATE = 0) ---
                                currentErrorState = 0;

                                // Nếu trước đó đang bị lỗi mà giờ OK -> Báo lại 1 câu
                                if (lastErrorState != 0) {
                                    WriteLog("[STATUS] Pointer Chain RECOVERED (OK)");
                                }

                                // --- ĐỌC DỮ LIỆU ---
                                char rawBuffer[1024] = { 0 };
                                int hasData = 0;

                                for (int i = 0; i < 10; i++) {
                                    char temp[64];
                                    int val = *(int*)(pList + (i * 4));
                                    if (i == 0)
                                        sprintf_s(temp, 64, "[No:%d]", i, val); // Chỉ số đầu tiên là Số lượng phím đã bấm
                                    if (val != 0) {
                                        sprintf_s(temp, 64, "[%d]%d(%s) ", i, val, GetKeyName(val));
                                        strcat_s(rawBuffer, 1024, temp);
                                        hasData = 1;
                                    }
                                }

                                // In dữ liệu nếu có (Để debug thì ta cứ in ra, nhưng nếu muốn gọn thì lọc trùng hash như bài trước)
                                // Ở đây tôi để in liên tục nếu có dữ liệu để bạn check ván 2
                                if (hasData) {
                                    WriteLog("[DATA] %s", rawBuffer);
                                }
                            }
                        }
                    }
                }
            }

            // --- XỬ LÝ LOG LỖI (ANTI-SPAM) ---
            // Chỉ in lỗi nếu trạng thái thay đổi (VD: Đang OK chuyển sang Lỗi, hoặc Lỗi A sang Lỗi B)
            if (currentErrorState != 0 && currentErrorState != lastErrorState) {
                WriteLog("%s", errorMsg);
            }

            // Cập nhật trạng thái
            lastErrorState = currentErrorState;

        }
        __except (1) {
            WriteLog("[EXCEPTION] Crash prevented.");
        }

        Sleep(500);
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