// =============================================================
// FILE: ReflectiveDll.c
// =============================================================
#include "ReflectiveLoader.h" // Giữ nguyên header của project bạn
#include <stdio.h>
#include <windows.h>

// =============================================================
// CẤU HÌNH: THỢ SĂN THẦM LẶNG (SILENT HUNTER)
// =============================================================
#define OLD_BASE_ADDR     0x2E3BC3C  // mỗi khi game cập nhật bản mới thì paste vào đây để dò tìm
#define SCAN_LIMIT        0x20000    // 128KB - Quét rộng để chắc ăn
#define LOG_FILENAME      "audition_silent_log.txt"

// Các Offset cũ
#define OFF_MGR           0x1C       
#define OFF_CONTAINER     0x14       
#define OFF_TARGET_LIST   0x220      

extern HINSTANCE hAppInstance;

// --- HÀM GHI LOG ---
void WriteLog(const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    // SỬA LỖI: Thêm sizeof(buffer)
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    FILE* f = NULL;
    fopen_s(&f, LOG_FILENAME, "a");
    if (f) { 
        fprintf(f, "%s\n", buffer); 
        fclose(f); 
    }
}

// --- KIỂM TRA POINTER ---
BOOL IsValid(DWORD addr) {
    if (addr < 0x10000 || addr > 0x7FFFFFFF) 
        return FALSE;
    return !IsBadReadPtr((const void*)addr, 4);
}

// --- LẤY TÊN PHÍM ---
const char* GetKeyName(int val) {
    switch (val) {
    case 1: return "1";
    case 2: return "DW";
    case 3: return "3";
    case 4: return "LF";
    case 6: return "RT";
    case 7: return "7";
    case 8: return "UP";
    case 9: return "9";
    case 0: return "__";
    default: return "??";
    }
}

// --- KIỂM TRA CẤU TRÚC MŨI TÊN ---
BOOL IsTargetStruct(DWORD baseAddr) {
    // Dùng __try để tránh crash khi đọc vào vùng nhớ cấm
    __try {
        if (!IsValid(baseAddr)) return FALSE;
        DWORD p1 = *(DWORD*)baseAddr;
        if (!IsValid(p1)) return FALSE;

        DWORD pMgr = *(DWORD*)(p1 + OFF_MGR);
        if (!IsValid(pMgr)) return FALSE;

        DWORD pCont = *(DWORD*)(pMgr + OFF_CONTAINER);
        if (!IsValid(pCont)) return FALSE;

        DWORD pTarget = *(DWORD*)(pCont + OFF_TARGET_LIST);
        if (!IsValid(pTarget)) return FALSE;

        // KIỂM TRA DỮ LIỆU SỐNG
        int count = *(int*)pTarget;

        // Chỉ chấp nhận khi đang nhảy (Count từ 1 đến 20)
        if (count <= 0 || count > 20) return FALSE;

        int firstArrow = *(int*)(pTarget + 4);
        // Kiểm tra mũi tên đầu tiên có hợp lệ không (1,2,3,4,6,7,8,9)
        if (firstArrow == 1 || firstArrow == 2 || firstArrow == 3 || firstArrow == 4 ||
            firstArrow == 6 || firstArrow == 7 || firstArrow == 8 || firstArrow == 9) {
            return TRUE;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }
    return FALSE;
}

// --- LUỒNG CHÍNH ---
DWORD WINAPI SilentHunterThread(LPVOID lpParam) {
    WriteLog("[START] Silent Hunter loaded. Waiting for game data...");

    DWORD FOUND_ADDRESS = 0;

    // --- GIAI ĐOẠN 1: QUÉT LIÊN TỤC CHO ĐẾN KHI THẤY DỮ LIỆU ---
    while (FOUND_ADDRESS == 0) {
        // Quét vùng xung quanh địa chỉ cũ
        for (int offset = 0; offset < SCAN_LIMIT; offset += 4) {
            if (IsTargetStruct(OLD_BASE_ADDR + offset)) {
                FOUND_ADDRESS = OLD_BASE_ADDR + offset;
                WriteLog("[SUCCESS] Target LOCKED at: 0x%X (Offset +0x%X)", FOUND_ADDRESS, offset);
                break; // Tìm thấy thì thoát vòng lặp for
            }
        }

        if (FOUND_ADDRESS == 0) {
            // Chưa tìm thấy (do chưa nhảy), ngủ 1 giây rồi quét lại
            Sleep(1000);
        }
    }

    // --- GIAI ĐOẠN 2: GHI LOG MŨI TÊN ---
    WriteLog("[MODE] Active monitoring started.");

    static int lastHash = 0;
    while (1) {
        __try {
            // Đọc lại Pointer Chain từ địa chỉ mới tìm được
            DWORD p1 = *(DWORD*)FOUND_ADDRESS;
            // Vì đã verify ở trên nên đoạn này ta đi tắt đón đầu cho nhanh
            DWORD pMgr = *(DWORD*)(p1 + OFF_MGR);
            DWORD pCont = *(DWORD*)(pMgr + OFF_CONTAINER);
            DWORD pTarget = *(DWORD*)(pCont + OFF_TARGET_LIST);

            int count = *(int*)pTarget;

            if (count > 0 && count <= 20) {
                // Tính Hash đơn giản để kiểm tra thay đổi
                int currentHash = count;
                char msg[1024] = { 0 };

                // SỬA LỖI: Thêm sizeof(msg)
                sprintf_s(msg, sizeof(msg), "[Cnt:%d] ", count);

                for (int i = 1; i <= count; i++) {
                    int val = *(int*)(pTarget + i * 4);
                    currentHash += val * (i + 1); // Hash có trọng số vị trí

                    char tmp[10];
                    // SỬA LỖI: Thêm sizeof(tmp)
                    sprintf_s(tmp, sizeof(tmp), "%s ", GetKeyName(val));

                    // SỬA LỖI: Thêm sizeof(msg)
                    strcat_s(msg, sizeof(msg), tmp);
                }

                if (currentHash != lastHash) {
                    WriteLog(msg);
                    lastHash = currentHash;
                }
            }
            else {
                if (lastHash != 0) lastHash = 0;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            // Nếu game crash hoặc pointer chết, ghi log và thoát hoặc chờ
            WriteLog("[ERR] Pointer Access Failed.");
            Sleep(1000);
        }
        Sleep(50); // Tốc độ cập nhật 50ms
    }
    return 0;
}

// --- DLL ENTRY POINT ---
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        hAppInstance = hinstDLL;
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SilentHunterThread, NULL, 0, NULL);
    }
    return TRUE;
}