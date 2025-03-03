#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "resource.h"
#include <richedit.h>
#include <gdiplus.h>
#pragma comment(lib, "msimg32.lib")  // 用于 AlphaBlend 函数

// 前向声明窗口过程函数
LRESULT CALLBACK RoundedButtonProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK RoundedStaticProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

#define MAX_TICKETS 1000
#define MAX_NAME_LENGTH 50
#define BTN_DRAW 101
#define BTN_ADD_NAME 102
#define BTN_DELETE_NAME 103
#define BTN_TOGGLE_LIST 104
#define BTN_SET_WINNERS 105
#define BTN_IMPORT 107
#define BTN_CLEAR_LIST 108
#define ID_LISTBOX 106
#define TIMER_ROLL 1
#define ROLL_INTERVAL 100
#define MAX_WINNERS 10
#define MIN_WINDOW_WIDTH 800
#define MIN_WINDOW_HEIGHT 800
#define BTN_CLASS L"RoundedButton"
#define STATIC_CLASS L"RoundedStatic"
#define CORNER_RADIUS 10  // 圆角半径
#define WM_CUSTOM_PAINT (WM_USER + 1)
#define ECO_TRANSPARENT 0x00000800L

// 抽签系统结构体
typedef struct
{
    wchar_t names[MAX_TICKETS][MAX_NAME_LENGTH]; // 存储名单
    int count;                                   // 当前名单人数
} LotterySystem;

LotterySystem lottery = {0};       // 初始化为0
HWND hOutput;                      // 显示结果的文本框
HWND hNameInput;                   // 输入名字的文本框
HWND hNameList;                    // 名单列表框（隐藏）
BOOL isRolling = FALSE;            // 是否正在滚动
int currentRollIndex = 0;          // 当前显示的索引
HWND hRollText;                    // 显示滚动名字的文本框
int numWinners = 1;                // 默认抽取1人
wchar_t displayBuffer[4096] = L""; // 用于存储滚动显示的文本
int drawCount = 0;                 // 添加全局变量来记录抽取次数

// 添加全局变量用于存储字体
HFONT g_hFont = NULL;

// 添加按钮状态结构体
typedef struct {
    BOOL isHovered;
    BOOL isPressed;
} ButtonState;

// 添加全局变量用于存储背景图片
HBITMAP g_hBackgroundBitmap = NULL;

// 添加创建字体的函数
HFONT CreateScaledFont(int height) {
    return CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
}

// 抽签函数
int drawName(LotterySystem *lottery, wchar_t *drawnName)
{
    if (lottery->count <= 0)
    {
        return -1;
    }

    int index = rand() % lottery->count;
    wcscpy(drawnName, lottery->names[index]);

    // 将最后一个名字移到被抽位置
    if (index < lottery->count - 1)
    {
        wcscpy(lottery->names[index], lottery->names[lottery->count - 1]);
    }
    lottery->count--;

    return index;
}

// 添加名字到名单
void addNameToList(HWND hWnd)
{
    wchar_t name[MAX_NAME_LENGTH];
    GetWindowTextW(hNameInput, name, MAX_NAME_LENGTH);

    if (name[0] == L'\0')
    {
        MessageBoxW(hWnd, L"请输入有效的名字！", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    if (lottery.count >= MAX_TICKETS)
    {
        MessageBoxW(hWnd, L"名单已满！", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    // 添加到列表框和数组
    SendMessageW(hNameList, LB_ADDSTRING, 0, (LPARAM)name);
    wcscpy(lottery.names[lottery.count], name);
    lottery.count++;

    // 清空输入框
    SetWindowTextW(hNameInput, L"");
}

// 从名单中删除选中的名字
void deleteSelectedName(HWND hWnd)
{
    int sel = SendMessageW(hNameList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR)
    {
        MessageBoxW(hWnd, L"请先选择要删除的名字！", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    // 从列表框中删除
    SendMessageW(hNameList, LB_DELETESTRING, sel, 0);

    // 更新名单数组
    for (int i = sel; i < lottery.count - 1; i++)
    {
        wcscpy(lottery.names[i], lottery.names[i + 1]);
    }
    lottery.count--;
}

// 添加 CSV 导入函数
void ImportFromCSV(HWND hDlg, HWND hListBox) {
    wchar_t filename[MAX_PATH];
    
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hDlg;
    ofn.lpstrFilter = L"CSV Files\0*.csv\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"csv";
    
    filename[0] = '\0';
    
    if (GetOpenFileNameW(&ofn)) {
        FILE* file = _wfopen(filename, L"r");
        if (file) {
            char line[MAX_NAME_LENGTH];
            wchar_t wline[MAX_NAME_LENGTH];
            int oldCount = lottery.count;
            
            // 读取每一行
            while (fgets(line, sizeof(line), file) && lottery.count < MAX_TICKETS) {
                // 移除换行符
                size_t len = strlen(line);
                if (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
                    line[len-1] = '\0';
                }
                if (len > 1 && (line[len-2] == '\r')) {
                    line[len-2] = '\0';
                }
                
                // 转换为宽字符
                MultiByteToWideChar(CP_UTF8, 0, line, -1, wline, MAX_NAME_LENGTH);
                
                // 添加到列表框和数组
                if (wline[0] != L'\0') {
                    SendMessageW(hListBox, LB_ADDSTRING, 0, (LPARAM)wline);
                    wcscpy(lottery.names[lottery.count], wline);
                    lottery.count++;
                }
            }
            
            fclose(file);
            
            // 显示导入结果
            wchar_t msg[128];
            swprintf(msg, 128, L"成功导入 %d 个名字", lottery.count - oldCount);
            MessageBoxW(hDlg, msg, L"导入完成", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxW(hDlg, L"无法打开文件！", L"错误", MB_OK | MB_ICONERROR);
        }
    }
}

// 修改 DialogProc 函数，添加 WM_NCCREATE 处理
LRESULT CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HWND hListBox;
    static HWND hInputName;

    switch (message)
    {
    case WM_NCCREATE:
    {
        // 设置窗口的默认字体为微软雅黑
        NONCLIENTMETRICSW ncm = { sizeof(NONCLIENTMETRICSW) };
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);
        wcscpy(ncm.lfCaptionFont.lfFaceName, L"Microsoft YaHei UI");
        HFONT hCaptionFont = CreateFontIndirectW(&ncm.lfCaptionFont);
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)hCaptionFont);
        return DefWindowProcW(hDlg, message, wParam, lParam);
    }

    case WM_INITDIALOG:
    {
        // 创建字体
        HFONT hDialogFont = CreateFontW(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");

        // 创建名字输入框和标签
        HWND hLabel = CreateWindowW(L"STATIC", L"输入名字：",
                      WS_CHILD | WS_VISIBLE,
                      20, 25, 120, 40, hDlg, NULL, NULL, NULL);
        SendMessage(hLabel, WM_SETFONT, (WPARAM)hDialogFont, TRUE);

        hInputName = CreateWindowExW(
            0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            150, 20, 400, 40,  // 调整高度
            hDlg, NULL, NULL, NULL);
        SendMessage(hInputName, WM_SETFONT, (WPARAM)hDialogFont, TRUE);

        // 创建添加按钮
        HWND hAddBtn = CreateWindowExW(
            0, L"BUTTON", L"添加",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            570, 20, 100, 40,  // 调整高度
            hDlg, (HMENU)BTN_ADD_NAME, NULL, NULL);
        SendMessage(hAddBtn, WM_SETFONT, (WPARAM)hDialogFont, TRUE);

        // 创建删除按钮
        HWND hDelBtn = CreateWindowExW(
            0, L"BUTTON", L"删除",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            680, 20, 100, 40,  // 调整高度
            hDlg, (HMENU)BTN_DELETE_NAME, NULL, NULL);
        SendMessage(hDelBtn, WM_SETFONT, (WPARAM)hDialogFont, TRUE);

        // 创建列表框标签
        HWND hListLabel = CreateWindowW(L"STATIC", L"当前名单：",
                      WS_CHILD | WS_VISIBLE,
                      20, 80, 120, 40, hDlg, NULL, NULL, NULL);
        SendMessage(hListLabel, WM_SETFONT, (WPARAM)hDialogFont, TRUE);

        // 创建列表框
        hListBox = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
            20, 120, 760, 400,  // 调整位置和宽度
            hDlg, (HMENU)IDC_NAMELIST, NULL, NULL);
        SendMessage(hListBox, WM_SETFONT, (WPARAM)hDialogFont, TRUE);

        // 创建底部按钮
        HWND hOkBtn = CreateWindowExW(
            0, L"BUTTON", L"确定",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            260, 540, 140, 40,  // 调整大小和位置
            hDlg, (HMENU)IDOK, NULL, NULL);
        SendMessage(hOkBtn, WM_SETFONT, (WPARAM)hDialogFont, TRUE);

        HWND hClearBtn = CreateWindowExW(
            0, L"BUTTON", L"清空名单",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            420, 540, 140, 40,  // 调整大小和位置
            hDlg, (HMENU)BTN_CLEAR_LIST, NULL, NULL);
        SendMessage(hClearBtn, WM_SETFONT, (WPARAM)hDialogFont, TRUE);

        HWND hImportBtn = CreateWindowExW(
            0, L"BUTTON", L"导入CSV",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            580, 540, 140, 40,  // 调整大小和位置
            hDlg, (HMENU)BTN_IMPORT, NULL, NULL);
        SendMessage(hImportBtn, WM_SETFONT, (WPARAM)hDialogFont, TRUE);

        // 填充列表框
        for (int i = 0; i < lottery.count; i++)
        {
            SendMessageW(hListBox, LB_ADDSTRING, 0, (LPARAM)lottery.names[i]);
        }

        // 设置焦点到输入框
        SetFocus(hInputName);
        return TRUE;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case BTN_ADD_NAME:
        {
            wchar_t name[MAX_NAME_LENGTH];
            GetWindowTextW(hInputName, name, MAX_NAME_LENGTH);

            if (name[0] == L'\0')
            {
                MessageBoxW(hDlg, L"请输入有效的名字！", L"提示", MB_OK | MB_ICONWARNING);
                return TRUE;
            }

            if (lottery.count >= MAX_TICKETS)
            {
                MessageBoxW(hDlg, L"名单已满！", L"提示", MB_OK | MB_ICONWARNING);
                return TRUE;
            }

            SendMessageW(hListBox, LB_ADDSTRING, 0, (LPARAM)name);
            wcscpy(lottery.names[lottery.count], name);
            lottery.count++;
            SetWindowTextW(hInputName, L"");
            SetFocus(hInputName); // 将焦点设回输入框
            return TRUE;
        }

        case BTN_DELETE_NAME:
        {
            int sel = SendMessageW(hListBox, LB_GETCURSEL, 0, 0);
            if (sel == LB_ERR)
            {
                MessageBoxW(hDlg, L"请先选择要删除的名字！", L"提示", MB_OK | MB_ICONWARNING);
                return TRUE;
            }

            SendMessageW(hListBox, LB_DELETESTRING, sel, 0);
            for (int i = sel; i < lottery.count - 1; i++)
            {
                wcscpy(lottery.names[i], lottery.names[i + 1]);
            }
            lottery.count--;
            return TRUE;
        }

        case BTN_IMPORT:
            ImportFromCSV(hDlg, hListBox);
            return TRUE;

        case BTN_CLEAR_LIST: {
            // 弹出确认对话框
            if (MessageBoxW(hDlg, L"确定要清空整个名单吗？", L"确认",
                MB_YESNO | MB_ICONQUESTION) == IDYES) {
                // 清空列表框
                SendMessageW(hListBox, LB_RESETCONTENT, 0, 0);
                // 清空名单数组
                lottery.count = 0;
                // 禁用抽签按钮（因为没有参与者了）
                EnableWindow(GetDlgItem(GetParent(hDlg), BTN_DRAW), FALSE);
            }
            return TRUE;
        }

        case IDOK:
        case IDCANCEL:
            DestroyWindow(hDlg);
            return TRUE;
        }
        break;
    }

    case WM_CLOSE:
        DestroyWindow(hDlg);
        return TRUE;
    }
    return DefWindowProc(hDlg, message, wParam, lParam);
}

// 修改 ShowNameListDialog 函数
void ShowNameListDialog(HWND hWnd)
{
    // 检查窗口类是否已注册
    WNDCLASSEXW wc = {0};
    if (!GetClassInfoExW(GetModuleHandle(NULL), L"LotteryDialog", &wc)) {
        // 如果没有注册，则注册窗口类
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = DialogProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"LotteryDialog";
        // 添加以下行来设置默认字体和图标
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        
        if (!RegisterClassExW(&wc)) {
            MessageBoxW(hWnd, L"窗口类注册失败！", L"错误", MB_OK | MB_ICONERROR);
            return;
        }
    }

    // 创建对话框
    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"LotteryDialog",
        L"参与者名单",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 
        800, 670,  // 增加窗口大小
        hWnd, NULL, GetModuleHandle(NULL), NULL);

    if (hDlg == NULL) {
        MessageBoxW(hWnd, L"创建对话框失败！", L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    // 发送初始化消息
    SendMessageW(hDlg, WM_INITDIALOG, 0, 0);

    // 设置对话框为模态
    EnableWindow(hWnd, FALSE);
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!IsWindow(hDlg)) {
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(hWnd, TRUE);
    SetForegroundWindow(hWnd);
}

// 添加输入对话框的窗口过程
LRESULT CALLBACK InputWndProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static HWND hEdit;
    static wchar_t* pInput;
    
    switch (message) {
        case WM_CREATE: {
            // 创建字体
            HFONT hInputFont = CreateFontW(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");

            // 创建说明文本
            wchar_t prompt[64];
            swprintf(prompt, 64, L"请输入中奖人数(1-%d)：", lottery.count);
            HWND hPrompt = CreateWindowW(L"STATIC", prompt,
                WS_CHILD | WS_VISIBLE,
                20, 30, 360, 35,  // 调整高度
                hDlg, NULL, NULL, NULL);
            SendMessage(hPrompt, WM_SETFONT, (WPARAM)hInputFont, TRUE);

            // 创建输入框
            hEdit = CreateWindowW(L"EDIT", L"1",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                20, 80, 360, 35,  // 调整高度
                hDlg, NULL, NULL, NULL);
            SendMessage(hEdit, WM_SETFONT, (WPARAM)hInputFont, TRUE);

            // 创建确定按钮
            HWND hOkBtn = CreateWindowW(L"BUTTON", L"确定",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                140, 140, 120, 35,  // 调整位置和高度
                hDlg, (HMENU)IDOK, NULL, NULL);
            SendMessage(hOkBtn, WM_SETFONT, (WPARAM)hInputFont, TRUE);

            return 0;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                wchar_t input[16];
                GetWindowTextW(hEdit, input, 16);
                int newCount = _wtoi(input);
                
                if (newCount > 0 && newCount <= lottery.count) {
                    numWinners = newCount;
                    // 如果新的中奖人数小于等于剩余名单人数，启用抽签按钮
                    EnableWindow(GetDlgItem(GetParent(hDlg), BTN_DRAW), TRUE);
                    
                    wchar_t msg[64];
                    swprintf(msg, 64, L"已设置中奖人数为: %d", numWinners);
                    MessageBoxW(GetParent(hDlg), msg, L"提示", MB_OK | MB_ICONINFORMATION);
                    DestroyWindow(hDlg);
            } else {
                    wchar_t errorMsg[64];
                    swprintf(errorMsg, 64, L"请输入1到%d之间的数字！", lottery.count);
                    MessageBoxW(hDlg, errorMsg, L"提示", MB_OK | MB_ICONWARNING);
                }
                return 0;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hDlg);
            return 0;

        case WM_DESTROY:
            EnableWindow(GetParent(hDlg), TRUE);
            SetForegroundWindow(GetParent(hDlg));
            return 0;
    }
    return DefWindowProcW(hDlg, message, wParam, lParam);
}

// 修改 SetWinnersCount 函数
void SetWinnersCount(HWND hWnd) {
    // 注册窗口类
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = InputWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"InputWindow";
    RegisterClassExW(&wc);

    // 创建输入窗口
    HWND hInputWnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"InputWindow",
        L"设置中奖人数",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        400, 220,  // 增加窗口高度到220
        hWnd, NULL, GetModuleHandle(NULL), NULL);

    if (!hInputWnd) {
        MessageBoxW(hWnd, L"创建输入窗口失败！", L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    // 禁用主窗口
    EnableWindow(hWnd, FALSE);

    // 显示输入窗口
    ShowWindow(hInputWnd, SW_SHOW);
    UpdateWindow(hInputWnd);
}

// 修改 DrawWinners 函数
void DrawWinners(HWND hWnd)
{
    if (lottery.count < numWinners) {
        MessageBoxW(hWnd, L"参与人数少于中奖人数！", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    // 创建临时数组存储所有可用索引
    int *availableIndices = (int *)malloc(lottery.count * sizeof(int));
    if (!availableIndices) {
        MessageBoxW(hWnd, L"内存分配失败！", L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    // 创建临时数组存储中奖者
    wchar_t (*winners)[MAX_NAME_LENGTH] = malloc(numWinners * sizeof(wchar_t[MAX_NAME_LENGTH]));
    int *winnerIndices = (int *)malloc(numWinners * sizeof(int));
    
    if (!winners || !winnerIndices) {
        free(availableIndices);
        if (winners) free(winners);
        if (winnerIndices) free(winnerIndices);
        MessageBoxW(hWnd, L"内存分配失败！", L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    // 初始化可用索引数组
    for (int i = 0; i < lottery.count; i++) {
        availableIndices[i] = i;
    }
    int availableCount = lottery.count;

    // 随机抽取指定数量的中奖者
    for (int i = 0; i < numWinners && availableCount > 0; i++) {
        int randomPos = rand() % availableCount;
        int selectedIndex = availableIndices[randomPos];

        // 保存中奖者
        wcscpy_s(winners[i], MAX_NAME_LENGTH, lottery.names[selectedIndex]);
        winnerIndices[i] = selectedIndex;

        // 将最后一个可用索引移到当前位置
        availableIndices[randomPos] = availableIndices[availableCount - 1];
        availableCount--;
    }

    // 增加抽取次数
    drawCount++;

    // 构建显示文本
    wchar_t resultBuffer[4096];
    swprintf(resultBuffer, 4096, L"第 %d 次抽取结果：\r\n", drawCount);
    size_t currentLen = wcslen(resultBuffer);
    
    for (int i = 0; i < numWinners; i++) {
        if (currentLen + wcslen(winners[i]) + 4 < 4096) {
            wcscat(resultBuffer, winners[i]);
            if (i < numWinners - 1) {
                wcscat(resultBuffer, L"、");
            }
            currentLen = wcslen(resultBuffer);
        }
    }
    wcscat(resultBuffer, L"\r\n\r\n");  // 添加额外的空行分隔不同次数的结果

    // 显示结果
    wchar_t currentText[8192] = L"";
    GetWindowTextW(hOutput, currentText, 8192);
    if (wcslen(currentText) + wcslen(resultBuffer) < 8192) {
        wcscat(currentText, resultBuffer);
        SetWindowTextW(hOutput, currentText);
        
        // 滚动到最新结果
        SendMessage(hOutput, EM_SETSEL, 0, -1);  // 选中所有文本
        SendMessage(hOutput, EM_SCROLLCARET, 0, 0);  // 滚动到光标位置
        SendMessage(hOutput, EM_SETSEL, -1, -1);  // 取消选中
    }

    // 从名单中移除中奖者（从后向前移除）
    // 先对索引进行排序，从大到小
    for (int i = 0; i < numWinners - 1; i++) {
        for (int j = 0; j < numWinners - i - 1; j++) {
            if (winnerIndices[j] < winnerIndices[j + 1]) {
                int temp = winnerIndices[j];
                winnerIndices[j] = winnerIndices[j + 1];
                winnerIndices[j + 1] = temp;
            }
        }
    }

    // 从后向前移除
    for (int i = 0; i < numWinners; i++) {
        int index = winnerIndices[i];
        if (index < lottery.count - 1) {
            wcscpy_s(lottery.names[index], MAX_NAME_LENGTH, 
                    lottery.names[lottery.count - 1]);
        }
        lottery.count--;
    }

    // 清理内存
    free(availableIndices);
    free(winners);
    free(winnerIndices);

    // 如果剩余人数少于设定的中奖人数，禁用抽签按钮
    if (lottery.count < numWinners) {
        EnableWindow(GetDlgItem(hWnd, BTN_DRAW), FALSE);
    }

    // 更新显示
    SetWindowTextW(hRollText, resultBuffer);
}

// 添加初始化测试名单的函数
void InitializeTestList()
{
    // 测试名单
    const wchar_t *testNames[] = {
        L"张三", L"李四", L"王五", L"赵六", L"钱七",
        L"孙八", L"周九", L"吴十", L"郑十一", L"王十二",
        L"刘一", L"陈二", L"杨三", L"黄四", L"周五"};
    int numTestNames = sizeof(testNames) / sizeof(testNames[0]);

    // 添加测试名单
    for (int i = 0; i < numTestNames; i++)
    {
        wcscpy(lottery.names[i], testNames[i]);
    }
    lottery.count = numTestNames;
}

// 修改 RegisterRoundedControls 函数
BOOL RegisterRoundedControls(HINSTANCE hInstance) {
    // 只注册圆角按钮类
    WNDCLASSEXW wcButton = {0};
    wcButton.cbSize = sizeof(WNDCLASSEXW);
    wcButton.style = CS_HREDRAW | CS_VREDRAW;
    wcButton.lpfnWndProc = RoundedButtonProc;
    wcButton.hInstance = hInstance;
    wcButton.hCursor = LoadCursor(NULL, IDC_HAND);  // 使用手型光标
    wcButton.lpszClassName = BTN_CLASS;
    if (!RegisterClassExW(&wcButton)) return FALSE;

    return TRUE;  // 删除静态控件的注册
}

// 修改圆角按钮的窗口过程
LRESULT CALLBACK RoundedButtonProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ButtonState* state = (ButtonState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    
    switch (msg) {
        case WM_CREATE: {
            // 为每个按钮分配状态
            state = (ButtonState*)malloc(sizeof(ButtonState));
            state->isHovered = FALSE;
            state->isPressed = FALSE;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
            return 0;
        }

        case WM_DESTROY: {
            if (state) {
                free(state);
            }
            return 0;
        }

        case WM_PAINT: {
            if (!state) return 0;
            
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
            SelectObject(memDC, memBitmap);
            
            // 计算更平滑的圆角半径
            int cornerRadius = min((rect.bottom - rect.top) / 2, (rect.right - rect.left) / 4);
            
            // 创建路径
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
            SelectObject(memDC, hPen);
            
            // 修改按钮状态的背景颜色为白色系
            HBRUSH hBrush;
            if (state->isPressed)
                hBrush = CreateSolidBrush(RGB(240, 240, 240));  // 按下状态变为浅灰
            else if (state->isHovered)
                hBrush = CreateSolidBrush(RGB(248, 248, 248));  // 悬停状态变为近白
            else
                hBrush = CreateSolidBrush(RGB(255, 255, 255));  // 正常状态为纯白
            
            // 先填充整个背景为白色
            HBRUSH hBgBrush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(memDC, &rect, hBgBrush);
            DeleteObject(hBgBrush);
            
            SelectObject(memDC, hBrush);
            RoundRect(memDC, 0, 0, rect.right, rect.bottom, cornerRadius * 2, cornerRadius * 2);
            
            // 绘制文本
            SetBkMode(memDC, TRANSPARENT);
            wchar_t text[256];
            GetWindowTextW(hwnd, text, 256);
            HFONT hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
            SelectObject(memDC, hFont);
            SetTextColor(memDC, RGB(0, 0, 0));
            DrawTextW(memDC, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            // 将内存 DC 的内容复制到实际的 DC
            BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);
            
            // 清理资源
            DeleteObject(hPen);
            DeleteObject(hBrush);
            DeleteObject(memBitmap);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (!state) return 0;
            if (!state->isHovered) {
                state->isHovered = TRUE;
                TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (!state) return 0;
            state->isHovered = FALSE;
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (!state) return 0;
            state->isPressed = TRUE;
            SetCapture(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        case WM_LBUTTONUP: {
            if (!state) return 0;
            if (state->isPressed) {
                ReleaseCapture();
                state->isPressed = FALSE;
                InvalidateRect(hwnd, NULL, TRUE);
                
                // 检查鼠标是否在按钮内
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);
                RECT rect;
                GetClientRect(hwnd, &rect);
                
                if (PtInRect(&rect, pt)) {
                    // 发送点击消息
                    HWND parent = GetParent(hwnd);
                    if (parent) {
                        SendMessage(parent, WM_COMMAND, 
                            MAKEWPARAM(GetDlgCtrlID(hwnd), BN_CLICKED), 
                            (LPARAM)hwnd);
                    }
                }
            }
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// 添加圆角静态控件的窗口过程
LRESULT CALLBACK RoundedStaticProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
            SelectObject(memDC, memBitmap);
            
            // 填充白色背景
            HBRUSH hBgBrush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(memDC, &rect, hBgBrush);
            DeleteObject(hBgBrush);
            
            // 绘制圆角边框
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
            SelectObject(memDC, hPen);
            HBRUSH hBrush = GetStockObject(NULL_BRUSH);
            SelectObject(memDC, hBrush);
            RoundRect(memDC, 0, 0, rect.right, rect.bottom, 20, 20);
            DeleteObject(hPen);
            
            // 绘制文本
            wchar_t text[4096];
            GetWindowTextW(hwnd, text, 4096);
            HFONT hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
            SelectObject(memDC, hFont);
            SetTextColor(memDC, RGB(0, 0, 0));
            SetBkMode(memDC, TRANSPARENT);
            DrawTextW(memDC, text, -1, &rect, DT_CENTER | DT_VCENTER);
            
            BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);
            
            DeleteObject(memBitmap);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// 窗口过程函数
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_NCCREATE:
    {
        // 设置窗口的默认字体为微软雅黑
        NONCLIENTMETRICSW ncm = { sizeof(NONCLIENTMETRICSW) };
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);
        wcscpy(ncm.lfCaptionFont.lfFaceName, L"Microsoft YaHei UI");
        HFONT hCaptionFont = CreateFontIndirectW(&ncm.lfCaptionFont);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)hCaptionFont);
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }

    case WM_CREATE:
    {
        // 加载背景图片
        g_hBackgroundBitmap = (HBITMAP)LoadImageW(
            NULL,                   // 实例句柄
            L"backgroundpage.bmp",  // 图片文件名
            IMAGE_BITMAP,           // 图片类型
            0, 0,                  // 使用图片原始尺寸
            LR_LOADFROMFILE        // 从文件加载
        );

        if (!g_hBackgroundBitmap)
        {
            MessageBoxW(hWnd, L"背景图片加载失败！", L"错误", MB_OK | MB_ICONERROR);
        }

        // 加载 RichEdit 库，使用 LoadLibraryW 而不是 LoadLibrary
        LoadLibraryW(L"Msftedit.dll");
        
        // 创建白色背景画刷
        SetClassLongPtr(hWnd, GCLP_HBRBACKGROUND, (LONG_PTR)GetStockObject(WHITE_BRUSH));
        
        // 创建三种大小的字体
        g_hFont = CreateScaledFont(24);         // 普通字体
        HFONT g_hLargeFont = CreateScaledFont(48);  // 更大的字体，用于动态显示
        HFONT g_hSmallFont = CreateScaledFont(20);  // 小一号的字体，用于历史记录

        // 创建按钮，使用 BS_OWNERDRAW 样式
        HWND hButton1 = CreateWindowW(L"BUTTON", L"管理名单",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            MIN_WINDOW_WIDTH - 150, 20, 120, 30, hWnd, (HMENU)BTN_TOGGLE_LIST, NULL, NULL);
        SendMessage(hButton1, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        HWND hButton2 = CreateWindowW(L"BUTTON", L"设置人数",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            MIN_WINDOW_WIDTH - 150, 60, 120, 30, hWnd, (HMENU)BTN_SET_WINNERS, NULL, NULL);
        SendMessage(hButton2, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        HWND hButton3 = CreateWindowW(L"BUTTON", L"开始抽签",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            20, 20, MIN_WINDOW_WIDTH - 200, 70, hWnd, (HMENU)BTN_DRAW, NULL, NULL);
        SendMessage(hButton3, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        // 进一步增大动态显示框的大小和字体
        hRollText = CreateWindowExW(
            WS_EX_TRANSPARENT,  // 只使用透明扩展样式
            L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            20, 110, MIN_WINDOW_WIDTH - 40, 200,
            hWnd, NULL, NULL, NULL);
        SendMessage(hRollText, WM_SETFONT, (WPARAM)g_hLargeFont, TRUE);

        // 修改 RichEdit 控件的创建
        hOutput = CreateWindowExW(
            0,  // 移除透明样式
            MSFTEDIT_CLASS,
            L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | 
            WS_VSCROLL | ES_AUTOVSCROLL,
            20, 330,
            MIN_WINDOW_WIDTH - 40, MIN_WINDOW_HEIGHT - 420,
            hWnd, NULL, NULL, NULL);
        SendMessage(hOutput, WM_SETFONT, (WPARAM)g_hSmallFont, TRUE);

        // 修改 RichEdit 控件的背景色设置
        SendMessage(hOutput, EM_SETBKGNDCOLOR, 0, (LPARAM)0x80FFFFFF);  // 设置半透明白色背景
        
        // 设置文本颜色
        CHARFORMAT2 cf;
        ZeroMemory(&cf, sizeof(cf));
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = RGB(0, 0, 0);
        SendMessage(hOutput, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case BTN_DRAW:
        {
            if (lottery.count <= 0)
            {
                MessageBoxW(hWnd, L"请先添加参与者！", L"提示", MB_OK | MB_ICONWARNING);
                break;
            }

            if (lottery.count < numWinners)
            {
                MessageBoxW(hWnd, L"参与人数少于中奖人数！", L"提示", MB_OK | MB_ICONWARNING);
                break;
            }

            HWND hButton = GetDlgItem(hWnd, BTN_DRAW);

            if (!isRolling)
            {
                // 开始滚动
                isRolling = TRUE;
                SetWindowTextW(hButton, L"停止抽签");
                SetTimer(hWnd, TIMER_ROLL, ROLL_INTERVAL, NULL);
            }
            else
            {
                // 停止滚动并抽取指定数量的中奖者
                isRolling = FALSE;
                KillTimer(hWnd, TIMER_ROLL);
                SetWindowTextW(hButton, L"开始抽签");
                DrawWinners(hWnd);
            }
            break;
        }

        case BTN_TOGGLE_LIST:
            ShowNameListDialog(hWnd);
            break;

        case BTN_SET_WINNERS:
            SetWinnersCount(hWnd);
            break;
        }
        break;
    }

    case WM_TIMER:
    {
        if (wParam == TIMER_ROLL && isRolling) {
            // 创建临时数组存储所有可用索引
            int* availableIndices = (int*)malloc(lottery.count * sizeof(int));
            if (!availableIndices) {
                MessageBoxW(hWnd, L"内存分配失败！", L"错误", MB_OK | MB_ICONERROR);
                return 0;
            }

            for (int i = 0; i < lottery.count; i++) {
                availableIndices[i] = i;
            }
            int availableCount = lottery.count;

            // 构建显示文本，随机选择k个不重复的名字
            wcscpy(displayBuffer, L"");
            int namesPerLine = 5;  // 每行显示的名字数量
            
            for (int i = 0; i < numWinners && availableCount > 0; i++) {
                // 添加名字分隔符
                if (i > 0) {
                    if (i % namesPerLine == 0) {
                        wcscat(displayBuffer, L"\r\n");  // 换行
                    } else {
                        wcscat(displayBuffer, L"、");    // 同一行名字之间用顿号分隔
                    }
                }
                
                // 从剩余索引中随机选择一个
                int randomPos = rand() % availableCount;
                int selectedIndex = availableIndices[randomPos];
                
                // 添加选中的名字
                wcscat(displayBuffer, lottery.names[selectedIndex]);
                
                // 将最后一个可用索引移到当前位置，并减少可用数量
                availableIndices[randomPos] = availableIndices[availableCount - 1];
                availableCount--;
            }

            free(availableIndices);
            SetWindowTextW(hRollText, displayBuffer);
        }
        break;
    }

    case WM_DESTROY:
    {
        // 删除背景图片
        if (g_hBackgroundBitmap)
        {
            DeleteObject(g_hBackgroundBitmap);
            g_hBackgroundBitmap = NULL;
        }

        // 清理字体资源
        HFONT hCaptionFont = (HFONT)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        if (hCaptionFont) DeleteObject(hCaptionFont);
        
        if (isRolling)
        {
            KillTimer(hWnd, TIMER_ROLL);
        }
        if (g_hFont) DeleteObject(g_hFont);
        PostQuitMessage(0);
        break;
    }

    case WM_SIZE:
    {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);

        // 计算新的字体大小
        int newFontHeight = (height * 24) / MIN_WINDOW_HEIGHT;
        newFontHeight = max(24, min(newFontHeight, 48));

        // 更新字体
        if (g_hFont) DeleteObject(g_hFont);
        g_hFont = CreateScaledFont(newFontHeight);

        // 更新所有控件的字体
        SendMessage(GetDlgItem(hWnd, BTN_TOGGLE_LIST), WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessage(GetDlgItem(hWnd, BTN_SET_WINNERS), WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessage(GetDlgItem(hWnd, BTN_DRAW), WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessage(hRollText, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessage(hOutput, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        // 使用比例计算各个控件的位置和大小
        int spacing = height * 0.02;          // 间距占2%
        int buttonHeight = height * 0.06;     // 按钮高度占6%
        int manageWidth = width * 0.2;        // 管理按钮宽度占20%

        // 计算管理区域的总高度（两个按钮加间距）
        int managementAreaHeight = buttonHeight * 2 + spacing;

        // 调整管理按钮
        SetWindowPos(GetDlgItem(hWnd, BTN_TOGGLE_LIST), NULL,
            width - manageWidth - spacing, spacing,
            manageWidth, buttonHeight,
            SWP_NOZORDER);

        SetWindowPos(GetDlgItem(hWnd, BTN_SET_WINNERS), NULL,
            width - manageWidth - spacing, spacing * 2 + buttonHeight,
            manageWidth, buttonHeight,
            SWP_NOZORDER);

        // 调整抽签按钮（与管理按钮区域等高）
        int drawBtnWidth = width - manageWidth - spacing * 3;
        SetWindowPos(GetDlgItem(hWnd, BTN_DRAW), NULL,
            spacing, spacing,
            drawBtnWidth, managementAreaHeight,  // 高度设为管理区域的总高度
            SWP_NOZORDER);

        // 调整滚动显示文本框（进一步增加高度）
        int rollHeight = height * 0.35;  // 增加到35%的高度
        int contentTop = spacing * 2 + managementAreaHeight;
        SetWindowPos(hRollText, NULL,
            spacing, contentTop,
            width - spacing * 2, rollHeight,
            SWP_NOZORDER);

        // 调整结果显示区域
        int outputTop = contentTop + rollHeight + spacing;
        SetWindowPos(hOutput, NULL,
            spacing, outputTop,
            width - spacing * 2, height - outputTop - spacing * 2,
            SWP_NOZORDER);

        break;
    }

    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        lpMMI->ptMinTrackSize.x = MIN_WINDOW_WIDTH;
        lpMMI->ptMinTrackSize.y = MIN_WINDOW_HEIGHT;
        break;
    }

    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT lpDIS = (LPDRAWITEMSTRUCT)lParam;
        if (lpDIS->CtlType == ODT_BUTTON) 
        {
            RECT rect = lpDIS->rcItem;
            
            // 获取按钮在父窗口中的位置
            POINT pt = {rect.left, rect.top};
            MapWindowPoints(lpDIS->hwndItem, GetParent(lpDIS->hwndItem), &pt, 1);

            // 创建内存DC
            HDC hdcMem = CreateCompatibleDC(lpDIS->hDC);
            HBITMAP hBitmap = CreateCompatibleBitmap(lpDIS->hDC, 
                rect.right - rect.left, rect.bottom - rect.top);
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

            // 如果有背景图片，先绘制背景
            if (g_hBackgroundBitmap)
            {
                HDC hdcBg = CreateCompatibleDC(lpDIS->hDC);
                SelectObject(hdcBg, g_hBackgroundBitmap);
                
                // 获取背景图片信息
                BITMAP bm;
                GetObject(g_hBackgroundBitmap, sizeof(BITMAP), &bm);
                
                // 计算缩放比例
                RECT parentRect;
                GetClientRect(GetParent(lpDIS->hwndItem), &parentRect);
                float scaleX = (float)parentRect.right / bm.bmWidth;
                float scaleY = (float)parentRect.bottom / bm.bmHeight;
                
                // 计算背景图片在按钮区域的对应部分
                StretchBlt(hdcMem, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                          hdcBg, (int)(pt.x / scaleX), (int)(pt.y / scaleY),
                          (int)((rect.right - rect.left) / scaleX),
                          (int)((rect.bottom - rect.top) / scaleY),
                          SRCCOPY);
                
                DeleteDC(hdcBg);
            }

            // 创建半透明效果
            HBRUSH hBrush;
            if (lpDIS->itemState & ODS_SELECTED)
                hBrush = CreateSolidBrush(RGB(180, 180, 180));
            else if (lpDIS->itemState & ODS_HOTLIGHT)
                hBrush = CreateSolidBrush(RGB(220, 220, 220));
            else
                hBrush = CreateSolidBrush(RGB(240, 240, 240));

            // 设置混合模式
            BLENDFUNCTION bf = {AC_SRC_OVER, 0, 100, 0};  // 调整透明度

            // 创建临时DC用于绘制按钮
            HDC hdcTemp = CreateCompatibleDC(lpDIS->hDC);
            HBITMAP hBitmapTemp = CreateCompatibleBitmap(lpDIS->hDC,
                rect.right - rect.left, rect.bottom - rect.top);
            HBITMAP hOldBitmapTemp = (HBITMAP)SelectObject(hdcTemp, hBitmapTemp);

            // 绘制圆角矩形
            SelectObject(hdcTemp, hBrush);
            RoundRect(hdcTemp, 0, 0, rect.right - rect.left,
                rect.bottom - rect.top, 20, 20);

            // 将按钮与背景混合
            AlphaBlend(hdcMem, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                      hdcTemp, 0, 0, rect.right - rect.left, rect.bottom - rect.top, bf);

            // 将最终结果复制到按钮DC
            BitBlt(lpDIS->hDC, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                   hdcMem, 0, 0, SRCCOPY);

            // 绘制文本
            wchar_t text[256];
            GetWindowTextW(lpDIS->hwndItem, text, 256);
            SetBkMode(lpDIS->hDC, TRANSPARENT);
            SelectObject(lpDIS->hDC, g_hFont);
            SetTextColor(lpDIS->hDC, RGB(0, 0, 0));
            DrawTextW(lpDIS->hDC, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // 清理资源
            DeleteObject(hBrush);
            SelectObject(hdcTemp, hOldBitmapTemp);
            DeleteObject(hBitmapTemp);
            DeleteDC(hdcTemp);
            SelectObject(hdcMem, hOldBitmap);
            DeleteObject(hBitmap);
            DeleteDC(hdcMem);

            return TRUE;
        }
        break;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        HWND hwndStatic = (HWND)lParam;
        
        if (hwndStatic == hRollText)
        {
            SetTextColor(hdcStatic, RGB(0, 0, 0));
            SetBkMode(hdcStatic, TRANSPARENT);
            
            // 创建半透明画刷
            static HBRUSH hBrush = NULL;
            if (!hBrush)
            {
                LOGBRUSH lb;
                lb.lbStyle = BS_SOLID;
                lb.lbColor = RGB(255, 255, 255);
                lb.lbHatch = 0;
                hBrush = CreateBrushIndirect(&lb);
            }
            
            // 设置画刷的透明度
            BLENDFUNCTION bf = {AC_SRC_OVER, 0, 128, 0};  // 128 是 50% 的透明度
            HDC hdcScreen = GetDC(NULL);
            AlphaBlend(hdcStatic, 0, 0, 1, 1, hdcScreen, 0, 0, 1, 1, bf);
            ReleaseDC(NULL, hdcScreen);
            
            return (LRESULT)hBrush;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    case WM_ERASEBKGND:
    {
        if (g_hBackgroundBitmap)
        {
            HDC hdc = (HDC)wParam;
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, g_hBackgroundBitmap);

            RECT rect;
            GetClientRect(hWnd, &rect);
            
            BITMAP bm;
            GetObject(g_hBackgroundBitmap, sizeof(BITMAP), &bm);

            // 拉伸绘制背景图片
            SetStretchBltMode(hdc, HALFTONE);
            StretchBlt(hdc, 0, 0, rect.right, rect.bottom,
                      hdcMem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

            SelectObject(hdcMem, hOldBitmap);
            DeleteDC(hdcMem);
            return TRUE;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        
        // 如果需要，在这里添加其他绘制代码
        
        EndPaint(hWnd, &ps);
        return 0;
    }

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
} 

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    // 设置字符集
    SetProcessDPIAware();
    SetThreadUILanguage(MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED));
    
    // 添加这一行
    SetThreadLocale(MAKELCID(MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED), SORT_CHINESE_PRC));
    
    srand((unsigned int)time(NULL));
    InitializeTestList();

    // 注册自定义控件类
    if (!RegisterRoundedControls(hInstance)) {
        MessageBoxW(NULL, L"注册控件类失败！", L"错误", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"LotteryWindow";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);  // 添加默认图标
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);  // 添加默认小图标
    wc.style = CS_HREDRAW | CS_VREDRAW;  // 添加窗口样式
    RegisterClassExW(&wc);

    // 修改这里，使用 CreateWindowW 而不是 CreateWindow
    HWND hWnd = CreateWindowW(
        L"LotteryWindow",  // 窗口类名使用宽字符
        L"抽签系统",       // 标题使用宽字符
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1500, 1200,        // 设置初始窗口大小为1024x900
        NULL, NULL, hInstance, NULL);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
} 