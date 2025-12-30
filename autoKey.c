// =============================================================
// FILE: ReflectiveDll.c (V4 REFINED - FIX GARBAGE & START)
// =============================================================
#include "ReflectiveLoader.h"
#include <stdio.h>
#include <windows.h>

// --- CẤU HÌNH ---
#define STATIC_BASE_ADDR  0x2E48FB4
#define OFF_MGR           0x1C
#define OFF_CONTAINER     0x14
#define OFF_P2_TARGET     0x220
#define OFF_P3_USER       0x224

#define GAME_WINDOW_NAME  "Audition"

// Cấu hình độ trễ
#define START_DELAY       500   // Chờ 0.5s sau khi vào level mới (để game load xong)
#define TYPING_DELAY      100   // Tốc độ gõ giữa các phím (Càng chậm càng an toàn)

extern HINSTANCE hAppInstance;

typedef int BOOL;
#define TRUE 1
#define FALSE 0

// Hàm kiểm tra tính hợp lệ của địa chỉ bộ nhớ
BOOL IsValid(DWORD addr) {
    if (addr < 0x10000 || addr > 0x7FFFFFFF) return FALSE;
    return !IsBadReadPtr((const void*)addr, 4);
}

BOOL IsKeyDown(int key) {
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

const char* GetKeyName(int val) {
    switch (val) {
    case 8: return "UP";
    case 2: return "DOWN";
    case 4: return "LEFT";
    case 6: return "RIGHT";
    case 1: return "SW (1)";
    case 3: return "SE (3)";
    case 7: return "NW (7)";
    case 9: return "NE (9)";
    default: return "???";
    }
}

void SendKeyToGame(int audKeyVal) {
    BYTE vkCode = 0;
    DWORD dwFlags = 0;

    switch (audKeyVal) {
    case 8: vkCode = VK_UP;    dwFlags = KEYEVENTF_EXTENDEDKEY; break;
    case 2: vkCode = VK_DOWN;  dwFlags = KEYEVENTF_EXTENDEDKEY; break;
    case 4: vkCode = VK_LEFT;  dwFlags = KEYEVENTF_EXTENDEDKEY; break;
    case 6: vkCode = VK_RIGHT; dwFlags = KEYEVENTF_EXTENDEDKEY; break;
    case 1: vkCode = VK_NUMPAD1; break;
    case 3: vkCode = VK_NUMPAD3; break;
    case 7: vkCode = VK_NUMPAD7; break;
    case 9: vkCode = VK_NUMPAD9; break;
    default: return;
    }

    UINT scanCode = MapVirtualKey(vkCode, 0);
    keybd_event(vkCode, scanCode, dwFlags, 0);
    Sleep(40); // Giữ phím 40ms
    keybd_event(vkCode, scanCode, dwFlags | KEYEVENTF_KEYUP, 0);
}

DWORD WINAPI AutoKeyThread(LPVOID lpParam) {
    // Biến quản lý trạng thái CỤC BỘ (Quan trọng để fix spam)
    int myCurrentStep = 1;       // Tool đang muốn gõ phím thứ mấy? (Bắt đầu là 1)
    int lastObservedTarget = -1; // Để phát hiện level mới

    BOOL bEnabled = FALSE;
    FILE* f = NULL;

    AllocConsole();
    freopen_s(&f, "CONOUT$", "w", stdout);

    printf("=============================================\n");
    printf("   AUDITION V4 REFINED - AUTO START FIX      \n");
    printf("   [F8]: ON/OFF                              \n");
    printf("=============================================\n");

    while (1) {
        // 1. Xử lý Toggle F8
        if (IsKeyDown(VK_F8)) {
            bEnabled = !bEnabled;
            if (bEnabled) {
                printf("\n>>> [ON] AUTO ENABLED. WAITING... <<<\n");
                // Reset trạng thái khi bật
                myCurrentStep = 1;
                lastObservedTarget = -1;
            }
            else {
                printf("\n>>> [OFF] AUTO PAUSED <<<\n");
            }
            Sleep(400);
        }

        if (bEnabled) {
            __try {
                if (IsValid(STATIC_BASE_ADDR)) {
                    DWORD p1 = *(DWORD*)STATIC_BASE_ADDR;
                    // Kiểm tra nhiều lớp để tránh đọc rác
                    if (IsValid(p1)) {
                        DWORD pMgr = *(DWORD*)(p1 + OFF_MGR);
                        if (IsValid(pMgr)) {
                            DWORD pCont = *(DWORD*)(pMgr + OFF_CONTAINER);
                            if (IsValid(pCont)) {
                                DWORD ptr_P2 = *(DWORD*)(pCont + OFF_P2_TARGET);
                                DWORD ptr_P3 = *(DWORD*)(pCont + OFF_P3_USER);

                                if (IsValid(ptr_P2) && IsValid(ptr_P3)) {
                                    int targetCount = *(int*)ptr_P2;
                                    int userCount = *(int*)ptr_P3;

                                    // --- LỌC DỮ LIỆU RÁC (FIX LỖI ẢNH 2) ---
                                    // Số lượng mũi tên Audition thường chỉ từ 1 đến 20. 
                                    // Nếu lớn hơn (ví dụ 225633383), đó là rác -> Bỏ qua ngay.
                                    if (targetCount < 0 || targetCount > 30) {
                                        Sleep(10);
                                        continue;
                                    }

                                    // --- LOGIC RESET TRẠNG THÁI ---
                                    // Nếu game báo UserCount về 0 (đầu ván hoặc miss), Reset bước đi của tool về 1
                                    if (userCount == 0 && myCurrentStep != 1) {
                                        myCurrentStep = 1;
                                        printf("[RESET] New sequence detected (Target: %d). Wait 0.5s...\n", targetCount);

                                        // DELAY QUAN TRỌNG: Chờ game hiện mũi tên xong mới gõ phím đầu
                                        Sleep(START_DELAY);
                                    }

                                    // --- LOGIC GÕ PHÍM (LOCAL STATE) ---
                                    // Điều kiện:
                                    // 1. Có mũi tên (Target > 0)
                                    // 2. Game chưa nhận đủ (UserCount < Target)
                                    // 3. QUAN TRỌNG: UserCount thực tế phải == (Bước tool đang muốn gõ - 1)
                                    //    Ví dụ: Muốn gõ phím 1 (myCurrentStep=1), thì UserCount phải là 0.
                                    //           Muốn gõ phím 2 (myCurrentStep=2), thì UserCount phải là 1.

                                    if (targetCount > 0 && userCount < targetCount) {

                                        if (userCount == (myCurrentStep - 1)) {
                                            // Đã đến lúc gõ phím 'myCurrentStep'
                                            int nextKeyVal = *(int*)(ptr_P2 + (myCurrentStep * 4));

                                            // Log ngắn gọn
                                            printf("[ACT] Step %d/%d: %d -> %s\n",
                                                myCurrentStep, targetCount, nextKeyVal, GetKeyName(nextKeyVal));

                                            // Focus & Gõ
                                            HWND hGame = FindWindowA(NULL, GAME_WINDOW_NAME);
                                            if (hGame && GetForegroundWindow() != hGame) {
                                                SetForegroundWindow(hGame);
                                            }
                                            SendKeyToGame(nextKeyVal);

                                            // Tăng bước nhảy của Tool lên ngay lập tức
                                            // -> Vòng lặp sau sẽ KHÔNG gõ lại nữa vì (userCount != myCurrentStep - 1)
                                            myCurrentStep++;

                                            // Nghỉ ngơi giữa các phím
                                            Sleep(TYPING_DELAY);
                                        }
                                        else if (userCount >= myCurrentStep) {
                                            // Trường hợp bạn tự gõ tay nhanh hơn tool -> Tool tự đuổi theo
                                            myCurrentStep = userCount + 1;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            __except (1) { Sleep(100); }
        }
        Sleep(10);
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        hAppInstance = hinstDLL;
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AutoKeyThread, NULL, 0, NULL);
    }
    return TRUE;
}