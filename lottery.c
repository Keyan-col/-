#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "resource.h"

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
void ImportFromCSV(HWND hDlg, HWND hListBox)
{
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

    if (GetOpenFileNameW(&ofn))
    {
        FILE *file = _wfopen(filename, L"r");
        if (file)
        {
            char line[MAX_NAME_LENGTH];
            wchar_t wline[MAX_NAME_LENGTH];
            int oldCount = lottery.count;

            // 读取每一行
            while (fgets(line, sizeof(line), file) && lottery.count < MAX_TICKETS)
            {
                // 移除换行符
                size_t len = strlen(line);
                if (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
                {
                    line[len - 1] = '\0';
                }
                if (len > 1 && (line[len - 2] == '\r'))
                {
                    line[len - 2] = '\0';
                }

                // 转换为宽字符
                MultiByteToWideChar(CP_UTF8, 0, line, -1, wline, MAX_NAME_LENGTH);

                // 添加到列表框和数组
                if (wline[0] != L'\0')
                {
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
        }
        else
        {
            MessageBoxW(hDlg, L"无法打开文件！", L"错误", MB_OK | MB_ICONERROR);
        }
    }
}

// 修改 DialogProc 函数
LRESULT CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HWND hListBox;
    static HWND hInputName;

    switch (message)
    {
    case WM_INITDIALOG:
    {
        // 创建名字输入框和标签
        CreateWindowW(L"STATIC", L"输入名字：",
                      WS_CHILD | WS_VISIBLE,
                      10, 15, 80, 25, hDlg, NULL, NULL, NULL);

        hInputName = CreateWindowExW(
            0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            90, 10, 200, 25,
            hDlg, NULL, NULL, NULL);

        // 创建添加按钮
        CreateWindowExW(
            0, L"BUTTON", L"添加",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            300, 10, 80, 25,
            hDlg, (HMENU)BTN_ADD_NAME, NULL, NULL);

        // 创建删除按钮
        CreateWindowExW(
            0, L"BUTTON", L"删除",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            390, 10, 80, 25,
            hDlg, (HMENU)BTN_DELETE_NAME, NULL, NULL);

        // 创建列表框标签
        CreateWindowW(L"STATIC", L"当前名单：",
                      WS_CHILD | WS_VISIBLE,
                      10, 45, 80, 25, hDlg, NULL, NULL, NULL);

        // 创建列表框
        hListBox = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
            10, 70, 460, 180,
            hDlg, (HMENU)IDC_NAMELIST, NULL, NULL);

        // 创建确定按钮
        CreateWindowExW(
            0, L"BUTTON", L"确定",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            200, 260, 80, 25,
            hDlg, (HMENU)IDOK, NULL, NULL);

        // 创建导入按钮
        CreateWindowExW(
            0, L"BUTTON", L"导入CSV",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            390, 260, 80, 25,
            hDlg, (HMENU)BTN_IMPORT, NULL, NULL);

        // 创建清空按钮（在导入按钮旁边）
        CreateWindowExW(
            0, L"BUTTON", L"清空名单",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            290, 260, 80, 25, // 位置在确定按钮和导入按钮之间
            hDlg, (HMENU)BTN_CLEAR_LIST, NULL, NULL);

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

        case BTN_CLEAR_LIST:
        {
            // 弹出确认对话框
            if (MessageBoxW(hDlg, L"确定要清空整个名单吗？", L"确认",
                            MB_YESNO | MB_ICONQUESTION) == IDYES)
            {
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
    if (!GetClassInfoExW(GetModuleHandle(NULL), L"LotteryDialog", &wc))
    {
        // 如果没有注册，则注册窗口类
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = DialogProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"LotteryDialog";

        if (!RegisterClassExW(&wc))
        {
            MessageBoxW(hWnd, L"窗口类注册失败！", L"错误", MB_OK | MB_ICONERROR);
            return;
        }
    }

    // 创建对话框
    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"LotteryDialog",
        L"参与者名单",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, // 修改窗口样式
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 350,
        hWnd, NULL, GetModuleHandle(NULL), NULL);

    if (hDlg == NULL)
    {
        MessageBoxW(hWnd, L"创建对话框失败！", L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    // 发送初始化消息
    SendMessage(hDlg, WM_INITDIALOG, 0, 0);

    // 设置对话框为模态
    EnableWindow(hWnd, FALSE);
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!IsWindow(hDlg))
        {
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(hWnd, TRUE);
    SetForegroundWindow(hWnd);
}

// 添加输入对话框的窗口过程
LRESULT CALLBACK InputWndProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HWND hEdit;
    static wchar_t *pInput;

    switch (message)
    {
    case WM_CREATE:
    {
        // 创建说明文本
        wchar_t prompt[64];
        swprintf(prompt, 64, L"请输入中奖人数(1-%d)：", lottery.count);
        CreateWindowW(L"STATIC", prompt,
                      WS_CHILD | WS_VISIBLE,
                      10, 10, 200, 20, hDlg, NULL, NULL, NULL);

        // 创建输入框
        hEdit = CreateWindowW(L"EDIT", L"1",
                              WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                              10, 35, 220, 25, hDlg, NULL, NULL, NULL);

        // 创建确定按钮
        CreateWindowW(L"BUTTON", L"确定",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      85, 70, 80, 25, hDlg, (HMENU)IDOK, NULL, NULL);

        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            wchar_t input[16];
            GetWindowTextW(hEdit, input, 16);
            int newCount = _wtoi(input);

            if (newCount > 0 && newCount <= lottery.count)
            {
                numWinners = newCount;
                // 如果新的中奖人数小于等于剩余名单人数，启用抽签按钮
                EnableWindow(GetDlgItem(GetParent(hDlg), BTN_DRAW), TRUE);

                wchar_t msg[64];
                swprintf(msg, 64, L"已设置中奖人数为: %d", numWinners);
                MessageBoxW(GetParent(hDlg), msg, L"提示", MB_OK | MB_ICONINFORMATION);
                DestroyWindow(hDlg);
            }
            else
            {
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
void SetWinnersCount(HWND hWnd)
{
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
        250, 150,
        hWnd, NULL, GetModuleHandle(NULL), NULL);

    if (!hInputWnd)
    {
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
    if (lottery.count < numWinners)
    {
        MessageBoxW(hWnd, L"参与人数少于中奖人数！", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    // 创建临时数组存储所有可用索引
    int *availableIndices = (int *)malloc(lottery.count * sizeof(int));
    if (!availableIndices)
    {
        MessageBoxW(hWnd, L"内存分配失败！", L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    // 创建临时数组存储中奖者
    wchar_t(*winners)[MAX_NAME_LENGTH] = malloc(numWinners * sizeof(wchar_t[MAX_NAME_LENGTH]));
    int *winnerIndices = (int *)malloc(numWinners * sizeof(int));

    if (!winners || !winnerIndices)
    {
        free(availableIndices);
        if (winners)
            free(winners);
        if (winnerIndices)
            free(winnerIndices);
        MessageBoxW(hWnd, L"内存分配失败！", L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    // 初始化可用索引数组
    for (int i = 0; i < lottery.count; i++)
    {
        availableIndices[i] = i;
    }
    int availableCount = lottery.count;

    // 随机抽取指定数量的中奖者
    for (int i = 0; i < numWinners && availableCount > 0; i++)
    {
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

    for (int i = 0; i < numWinners; i++)
    {
        if (currentLen + wcslen(winners[i]) + 4 < 4096)
        {
            wcscat(resultBuffer, winners[i]);
            if (i < numWinners - 1)
            {
                wcscat(resultBuffer, L"、");
            }
            currentLen = wcslen(resultBuffer);
        }
    }
    wcscat(resultBuffer, L"\r\n\r\n"); // 添加额外的空行分隔不同次数的结果

    // 显示结果
    wchar_t currentText[8192] = L"";
    GetWindowTextW(hOutput, currentText, 8192);
    if (wcslen(currentText) + wcslen(resultBuffer) < 8192)
    {
        wcscat(currentText, resultBuffer);
        SetWindowTextW(hOutput, currentText);

        // 滚动到最新结果
        SendMessage(hOutput, EM_SETSEL, 0, -1);     // 选中所有文本
        SendMessage(hOutput, EM_SCROLLCARET, 0, 0); // 滚动到光标位置
        SendMessage(hOutput, EM_SETSEL, -1, -1);    // 取消选中
    }

    // 从名单中移除中奖者（从后向前移除）
    // 先对索引进行排序，从大到小
    for (int i = 0; i < numWinners - 1; i++)
    {
        for (int j = 0; j < numWinners - i - 1; j++)
        {
            if (winnerIndices[j] < winnerIndices[j + 1])
            {
                int temp = winnerIndices[j];
                winnerIndices[j] = winnerIndices[j + 1];
                winnerIndices[j + 1] = temp;
            }
        }
    }

    // 从后向前移除
    for (int i = 0; i < numWinners; i++)
    {
        int index = winnerIndices[i];
        if (index < lottery.count - 1)
        {
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
    if (lottery.count < numWinners)
    {
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

// 窗口过程函数
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        // 创建查看名单按钮
        CreateWindowW(L"BUTTON", L"管理名单",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      20, 20, 100, 25, hWnd, (HMENU)BTN_TOGGLE_LIST, NULL, NULL);

        // 创建设置中奖人数按钮
        CreateWindowW(L"BUTTON", L"设置中奖人数",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      130, 20, 100, 25, hWnd, (HMENU)BTN_SET_WINNERS, NULL, NULL);

        // 创建抽签按钮
        CreateWindowW(L"BUTTON", L"开始抽签",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      240, 20, 100, 25, hWnd, (HMENU)BTN_DRAW, NULL, NULL);

        // 创建滚动显示文本框
        hRollText = CreateWindowW(L"STATIC", L"",
                                  WS_CHILD | WS_VISIBLE | SS_CENTER | WS_BORDER | SS_EDITCONTROL,
                                  20, 60, 530, 100, // 增加高度到100像素
                                  hWnd, NULL, NULL, NULL);

        // 创建结果显示区域（添加垂直滚动条）
        hOutput = CreateWindowW(L"EDIT", L"",
                                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY |
                                    WS_VSCROLL | ES_AUTOVSCROLL, // 添加垂直滚动条
                                20, 180, 530, 380,
                                hWnd, NULL, NULL, NULL);

        // 设置编辑框字体
        HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
        SendMessage(hOutput, WM_SETFONT, (WPARAM)hFont, TRUE);

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
        if (wParam == TIMER_ROLL && isRolling)
        {
            // 创建临时数组存储所有可用索引
            int *availableIndices = (int *)malloc(lottery.count * sizeof(int));
            if (!availableIndices)
            {
                MessageBoxW(hWnd, L"内存分配失败！", L"错误", MB_OK | MB_ICONERROR);
                return 0;
            }

            for (int i = 0; i < lottery.count; i++)
            {
                availableIndices[i] = i;
            }
            int availableCount = lottery.count;

            // 构建显示文本，随机选择k个不重复的名字
            wcscpy(displayBuffer, L"");
            int namesPerLine = 5; // 每行显示的名字数量

            for (int i = 0; i < numWinners && availableCount > 0; i++)
            {
                // 添加名字分隔符
                if (i > 0)
                {
                    if (i % namesPerLine == 0)
                    {
                        wcscat(displayBuffer, L"\r\n"); // 换行
                    }
                    else
                    {
                        wcscat(displayBuffer, L"、"); // 同一行名字之间用顿号分隔
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
        if (isRolling)
        {
            KillTimer(hWnd, TIMER_ROLL);
        }
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{

    srand((unsigned int)time(NULL));
    InitializeTestList(); // 初始化测试名单

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"LotteryWindow";
    RegisterClassExW(&wc);

    HWND hWnd = CreateWindowW(
        L"LotteryWindow", L"Lottery",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 620, // 增加窗口高度
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