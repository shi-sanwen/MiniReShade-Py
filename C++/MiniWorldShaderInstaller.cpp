#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <winreg.h>
#include <tlhelp32.h>
#include <urlmon.h>
#include <psapi.h>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <memory>
#include <iostream>
#include <gdiplus.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")

// ��Դ ID
#define IDC_OUTPUT_TEXT 101
#define IDC_PROGRESS_BAR 102
#define IDC_START_SCAN 103
#define IDC_PAUSE_SCAN 104
#define IDC_STOP_SCAN 105
#define IDC_INSTALL_BUTTON 106
#define IDC_ADD_CUSTOM 107
#define IDC_DOWNLOAD_PRESET 108
#define IDC_YIRIXC_CHECKBOX 109
#define IDC_TIANXINGDASHE_CHECKBOX 110
#define IDC_VERSION_GROUP 111
#define IDC_STATUS_BAR 112

// �Զ�����Ϣ
#define WM_UPDATE_PROGRESS (WM_USER + 1)
#define WM_LOG_MESSAGE (WM_USER + 2)

// ��ɫ����
#define COLOR_BG RGB(240, 248, 255)  // ����ɫ����
#define COLOR_TEXT RGB(0, 0, 128)            // ����ɫ����
#define COLOR_BUTTON_NORMAL RGB(230, 243, 255)  // ��ť������ɫ
#define COLOR_BUTTON_HOVER RGB(208, 232, 255)   // ��ť��ͣ��ɫ
#define COLOR_BUTTON_PRESSED RGB(184, 220, 255) // ��ť������ɫ
#define COLOR_PROGRESS_BAR RGB(70, 130, 180)    // ��������ɫ
#define COLOR_BORDER RGB(70, 130, 180)          // �߿���ɫ
#define COLOR_TITLE_BAR RGB(25, 25, 112)        // ��������ɫ
#define COLOR_TITLE_TEXT RGB(255, 255, 255)     // ����������ɫ

// ȫ�ֱ���
HWND g_hWnd = NULL;
HWND g_hOutputText = NULL;
HWND g_hProgressBar = NULL;
HWND g_hVersionGroup = NULL;
HWND g_hStatusBar = NULL;
std::map<std::wstring, std::wstring> g_detectedVersions;
bool g_scanning = false;
bool g_scanPaused = false;
std::chrono::steady_clock::time_point g_scanStartTime;
std::vector<std::wstring> g_failedInstallations;
int g_retryCount = 0;
std::wstring g_applicationPath;
HANDLE g_scanThread = NULL;
HANDLE g_installThread = NULL;
bool g_yirixcChecked = false;
bool g_tianxingdasheChecked = false;
HFONT g_hFontNormal = NULL;
HFONT g_hFontBold = NULL;
HFONT g_hFontTitle = NULL;
HBRUSH g_hBackgroundBrush = NULL;
ULONG_PTR g_gdiplusToken;
std::map<HWND, bool> g_buttonHoverState;
std::map<HWND, bool> g_buttonPressedState;

// �޸�ȫ�ֱ������֣��Ż���������
bool g_animationActive = true;
int g_animationStep = 0;
int g_animationMaxSteps = 120; // ���Ӷ���������ʹ������ƽ��
int g_windowStartX = 0;
int g_windowTargetX = 0;
int g_windowY = 0;
int g_windowWidth = 1200;
int g_windowHeight = 700;
UINT_PTR g_animationTimerId = 1;
COLORREF g_currentGradientColors[4] = {
    RGB(70, 130, 180),   // ��ɫ
    RGB(255, 182, 193),  // ��ɫ
    RGB(255, 255, 0),    // ��ɫ
    RGB(255, 255, 255)   // ��ɫ
};
float g_colorBlendFactor = 0.0f;
HDC g_memDC = NULL;      // ����ڴ�DC����˫����
HBITMAP g_memBitmap = NULL; // ����ڴ�λͼ����˫����
HBITMAP g_oldBitmap = NULL; // ����ԭʼλͼ

// ��־�ļ�·��
std::wofstream g_logFile;

// ��������
void SetupUI(HWND hWnd);
void LogOutput(const std::wstring& message);
void StartScan();
void PauseScan();
void StopScan();
void AddCustomPath();
void DownloadPresets();
void InstallAction();
void PopulateOptions();
DWORD WINAPI ScanThreadProc(LPVOID lpParameter);
DWORD WINAPI InstallThreadProc(LPVOID lpParameter);
bool ExtractZipWith7z(const std::wstring& zipPath, const std::wstring& extractPath);
bool IsAdmin();
void CheckAllVersions();
std::wstring GetWindowsVersion();
std::vector<std::wstring> CheckRunningProcesses(const std::wstring& installPath);
bool TerminateProcessByPath(const std::wstring& processPath);
std::wstring CheckRegistryAndExtract(const std::wstring& registryKeyPath);
void ScanForMicroMiniNew();
bool WindowsIndexSearch();
void FullDiskScan();
void AddDetectedVersion(const std::wstring& version, const std::wstring& path);
void DownloadAndExtractPreset(const std::wstring& url, const std::wstring& installPath, const std::wstring& presetName);
void StartInstallation(const std::wstring& installPath);
void OnInstallationComplete(bool success);
void ScheduleRebootAndInstall();
void SetupLogging();
void CreateCustomFonts();
void CleanupResources();
void DrawRoundedRectangle(HDC hdc, RECT rect, int radius, COLORREF color, bool filled);
void SetStatusText(const std::wstring& text);
HWND CreateCustomButton(HWND hParent, LPCWSTR text, int x, int y, int width, int height, HMENU hMenu);
LRESULT CALLBACK CustomButtonProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK CustomGroupBoxProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK CustomProgressProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

// ����º�������
COLORREF BlendColors(COLORREF color1, COLORREF color2, float factor);
void DrawGradientRect(HDC hdc, RECT rect, COLORREF colorStart, COLORREF colorEnd);
double EaseInOutQuad(double t);

// ���ڹ���
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBrushBackground = CreateSolidBrush(COLOR_BG);
    static HBRUSH hBrushEdit = CreateSolidBrush(RGB(255, 255, 255));
    static HBRUSH hBrushButton = CreateSolidBrush(COLOR_BUTTON_NORMAL);
    static HBRUSH hBrushGroupBox = CreateSolidBrush(COLOR_BG);

    switch (message) {
    case WM_CREATE:
    {
        // ��ʼ��GDI+
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

        // �����Զ�������
        CreateCustomFonts();

        // ����UI
        SetupUI(hWnd);

        // �����Ϸ�汾
        CheckAllVersions();
    }
    break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_START_SCAN:
            StartScan();
            break;
        case IDC_PAUSE_SCAN:
            PauseScan();
            break;
        case IDC_STOP_SCAN:
            StopScan();
            break;
        case IDC_ADD_CUSTOM:
            AddCustomPath();
            break;
        case IDC_DOWNLOAD_PRESET:
            DownloadPresets();
            break;
        case IDC_INSTALL_BUTTON:
            InstallAction();
            break;
        case IDC_YIRIXC_CHECKBOX:
            g_yirixcChecked = !g_yirixcChecked;
            CheckDlgButton(hWnd, IDC_YIRIXC_CHECKBOX, g_yirixcChecked ? BST_CHECKED : BST_UNCHECKED);
            break;
        case IDC_TIANXINGDASHE_CHECKBOX:
            g_tianxingdasheChecked = !g_tianxingdasheChecked;
            CheckDlgButton(hWnd, IDC_TIANXINGDASHE_CHECKBOX, g_tianxingdasheChecked ? BST_CHECKED : BST_UNCHECKED);
            break;
        }
        break;

    case WM_UPDATE_PROGRESS:
        SendMessage(g_hProgressBar, PBM_SETPOS, wParam, 0);
        break;

    case WM_LOG_MESSAGE:
        if (lParam) {
            std::wstring* pMessage = reinterpret_cast<std::wstring*>(lParam);
            LogOutput(*pMessage);
            delete pMessage;
        }
        break;

    case WM_CTLCOLOREDIT:
        SetTextColor((HDC)wParam, COLOR_TEXT);
        SetBkColor((HDC)wParam, RGB(255, 255, 255));
        return (LRESULT)hBrushEdit;

    case WM_CTLCOLORSTATIC:
        // Ϊ��̬�ؼ�������ɫ
        SetTextColor((HDC)wParam, COLOR_TEXT);
        SetBkColor((HDC)wParam, COLOR_BG);
        return (LRESULT)hBrushBackground;

    case WM_CTLCOLORBTN:
        // Ϊ��ť������ɫ
        SetTextColor((HDC)wParam, COLOR_TEXT);
        SetBkColor((HDC)wParam, COLOR_BUTTON_NORMAL);
        return (LRESULT)hBrushButton;

        // ��WndProc���������WM_TIMER��Ϣ����
    case WM_TIMER:
        if (wParam == g_animationTimerId && g_animationActive) {
            g_animationStep++;

            if (g_animationStep <= g_animationMaxSteps) {
                // ʹ�û����������㴰��λ�ã�ʹ������ƽ��
                double progress = (double)g_animationStep / g_animationMaxSteps;
                double easedProgress = EaseInOutQuad(progress);

                // ���㴰��λ��
                int newX = g_windowStartX + (int)((g_windowTargetX - g_windowStartX) * easedProgress);

                // ����͸����
                BYTE alpha = (BYTE)(255 * easedProgress);

                // ������ɫ�������
                g_colorBlendFactor = (float)easedProgress;

                // ���ô���λ��
                SetWindowPos(hWnd, NULL, newX, g_windowY, g_windowWidth, g_windowHeight, SWP_NOZORDER);

                // ���ô���͸����
                SetLayeredWindowAttributes(hWnd, 0, alpha, LWA_ALPHA);

                // ǿ���ػ洰�ڣ������ٲ���Ҫ���ػ�
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else {
                // ��������
                g_animationActive = false;
                KillTimer(hWnd, g_animationTimerId);

                // ȷ��������ȫ��͸��
                SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA);

                // ����˫������Դ
                if (g_memDC) {
                    SelectObject(g_memDC, g_oldBitmap);
                    DeleteObject(g_memBitmap);
                    DeleteDC(g_memDC);
                    g_memDC = NULL;
                    g_memBitmap = NULL;
                    g_oldBitmap = NULL;
                }
            }
        }
        break;

        // �޸�WM_PAINT��Ϣ������ӽ��䱳��
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // ��ȡ�ͻ�����С
        RECT clientRect;
        GetClientRect(hWnd, &clientRect);

        // ���������ü���DC��λͼ����˫����
        if (!g_memDC) {
            g_memDC = CreateCompatibleDC(hdc);
            g_memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
            g_oldBitmap = (HBITMAP)SelectObject(g_memDC, g_memBitmap);
        }

        // ���㵱ǰ����ɫ
        COLORREF gradientStart, gradientEnd;

        // ֱ�Ӵ���ɫ����ɫ
        gradientStart = BlendColors(g_currentGradientColors[0], g_currentGradientColors[3], g_colorBlendFactor);
        gradientEnd = g_currentGradientColors[3];

        // ʹ��GDI+���Ƹ�ƽ���Ľ��䱳��
        Gdiplus::Graphics graphics(g_memDC);
        Gdiplus::Rect rect(0, 0, clientRect.right, clientRect.bottom);

        Gdiplus::LinearGradientBrush brush(
            Gdiplus::Point(0, 0),
            Gdiplus::Point(0, clientRect.bottom),
            Gdiplus::Color(255, GetRValue(gradientStart), GetGValue(gradientStart), GetBValue(gradientStart)),
            Gdiplus::Color(255, GetRValue(gradientEnd), GetGValue(gradientEnd), GetBValue(gradientEnd))
        );

        graphics.FillRectangle(&brush, rect);

        // ���Ʊ�����
        RECT titleRect = clientRect;
        titleRect.bottom = 40;
        HBRUSH hTitleBrush = CreateSolidBrush(COLOR_TITLE_BAR);
        FillRect(g_memDC, &titleRect, hTitleBrush);
        DeleteObject(hTitleBrush);

        // ���Ʊ�������
        SetTextColor(g_memDC, COLOR_TITLE_TEXT);
        SetBkMode(g_memDC, TRANSPARENT);
        SelectObject(g_memDC, g_hFontTitle);
        DrawText(g_memDC, L"���������Ӱ����װ�� V2.7.42", -1, &titleRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // ���ڴ�DC�����ݸ��Ƶ���Ļ
        BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, g_memDC, 0, 0, SRCCOPY);

        EndPaint(hWnd, &ps);
    }
    break;

    case WM_CLOSE:
        if (g_scanThread != NULL) {
            g_scanning = false;
            WaitForSingleObject(g_scanThread, 1000);
            CloseHandle(g_scanThread);
        }
        if (g_installThread != NULL) {
            WaitForSingleObject(g_installThread, 1000);
            CloseHandle(g_installThread);
        }
        DestroyWindow(hWnd);
        break;

        // ��WM_DESTROY��Ϣ����������������
    case WM_DESTROY:
        CleanupResources();

        // ����˫������Դ
        if (g_memDC) {
            SelectObject(g_memDC, g_oldBitmap);
            DeleteObject(g_memBitmap);
            DeleteDC(g_memDC);
        }

        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        DeleteObject(hBrushBackground);
        DeleteObject(hBrushEdit);
        DeleteObject(hBrushButton);
        DeleteObject(hBrushGroupBox);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// �����Զ�������
void CreateCustomFonts() {
    g_hFontNormal = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"΢���ź�");

    g_hFontBold = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"΢���ź�");

    g_hFontTitle = CreateFont(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"΢���ź�");

    g_hBackgroundBrush = CreateSolidBrush(COLOR_BG);
}

// ������Դ
void CleanupResources() {
    if (g_hFontNormal) DeleteObject(g_hFontNormal);
    if (g_hFontBold) DeleteObject(g_hFontBold);
    if (g_hFontTitle) DeleteObject(g_hFontTitle);
    if (g_hBackgroundBrush) DeleteObject(g_hBackgroundBrush);
}

// ����Բ�Ǿ���
void DrawRoundedRectangle(HDC hdc, RECT rect, int radius, COLORREF color, bool filled) {
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    Gdiplus::Color gdipColor(GetRValue(color), GetGValue(color), GetBValue(color));
    Gdiplus::Pen pen(gdipColor, 1);
    Gdiplus::SolidBrush brush(gdipColor);

    Gdiplus::GraphicsPath path;
    int diameter = radius * 2;

    // ���Ͻ�
    path.AddArc(rect.left, rect.top, diameter, diameter, 180, 90);
    // ���Ͻ�
    path.AddArc(rect.right - diameter, rect.top, diameter, diameter, 270, 90);
    // ���½�
    path.AddArc(rect.right - diameter, rect.bottom - diameter, diameter, diameter, 0, 90);
    // ���½�
    path.AddArc(rect.left, rect.bottom - diameter, diameter, diameter, 90, 90);
    path.CloseFigure();

    if (filled) {
        graphics.FillPath(&brush, &path);
    }
    else {
        graphics.DrawPath(&pen, &path);
    }
}

// ����״̬���ı�
void SetStatusText(const std::wstring& text) {
    SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)text.c_str());
}

// �Զ��尴ť���ڹ���
LRESULT CALLBACK CustomButtonProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    static bool hover = false;
    static bool pressed = false;

    switch (message) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rect;
        GetClientRect(hWnd, &rect);

        // ȷ����ť״̬����ɫ
        COLORREF bgColor = COLOR_BUTTON_NORMAL;
        if (pressed) {
            bgColor = COLOR_BUTTON_PRESSED;
        }
        else if (hover) {
            bgColor = COLOR_BUTTON_HOVER;
        }

        // ����Բ�ǰ�ť����
        DrawRoundedRectangle(hdc, rect, 5, bgColor, true);

        // ���Ʊ߿�
        DrawRoundedRectangle(hdc, rect, 5, COLOR_BORDER, false);

        // ��ȡ��ť�ı�
        wchar_t text[256] = { 0 };
        GetWindowText(hWnd, text, 256);

        // �����ı�
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COLOR_TEXT);
        SelectObject(hdc, g_hFontNormal);
        DrawText(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE:
        if (!hover) {
            hover = true;
            g_buttonHoverState[hWnd] = true;

            // ��������뿪�¼�
            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            tme.dwHoverTime = HOVER_DEFAULT;
            TrackMouseEvent(&tme);

            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;

    case WM_MOUSELEAVE:
        hover = false;
        g_buttonHoverState[hWnd] = false;
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case WM_LBUTTONDOWN:
        pressed = true;
        g_buttonPressedState[hWnd] = true;
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case WM_LBUTTONUP:
        if (pressed) {
            pressed = false;
            g_buttonPressedState[hWnd] = false;
            InvalidateRect(hWnd, NULL, TRUE);

            // ���͵����Ϣ��������
            RECT rect;
            GetClientRect(hWnd, &rect);
            POINT pt = { LOWORD(lParam), HIWORD(lParam) };
            if (PtInRect(&rect, pt)) {
                DWORD id = GetDlgCtrlID(hWnd);
                SendMessage(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)hWnd);
            }
        }
        break;
    }

    return DefSubclassProc(hWnd, message, wParam, lParam);
}

// �Զ������򴰿ڹ���
LRESULT CALLBACK CustomGroupBoxProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (message) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rect;
        GetClientRect(hWnd, &rect);

        // ��䱳��
        HBRUSH hBrush = CreateSolidBrush(COLOR_BG);
        FillRect(hdc, &rect, hBrush);
        DeleteObject(hBrush);

        // ���Ʊ߿�
        DrawRoundedRectangle(hdc, rect, 8, COLOR_BORDER, false);

        // ��ȡ������ı�
        wchar_t text[256] = { 0 };
        GetWindowText(hWnd, text, 256);

        // ������ı��������ı�
        if (text[0] != L'\0') {
            // �����ı����
            SIZE textSize;
            SelectObject(hdc, g_hFontBold);
            GetTextExtentPoint32(hdc, text, wcslen(text), &textSize);

            // �����ı�����
            RECT textRect = { rect.left + 10, rect.top, rect.left + 20 + textSize.cx, rect.top + textSize.cy };
            hBrush = CreateSolidBrush(COLOR_BG);
            FillRect(hdc, &textRect, hBrush);
            DeleteObject(hBrush);

            // �����ı�
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, COLOR_TEXT);
            TextOut(hdc, rect.left + 15, rect.top, text, wcslen(text));
        }

        EndPaint(hWnd, &ps);
        return 0;
    }
    }

    return DefSubclassProc(hWnd, message, wParam, lParam);
}

// �Զ�����������ڹ���
LRESULT CALLBACK CustomProgressProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (message) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rect;
        GetClientRect(hWnd, &rect);

        // ���ƽ���������
        HBRUSH hBackBrush = CreateSolidBrush(RGB(240, 240, 240));
        FillRect(hdc, &rect, hBackBrush);
        DeleteObject(hBackBrush);

        // ���Ʊ߿�
        DrawRoundedRectangle(hdc, rect, 5, RGB(200, 200, 200), false);

        // ��ȡ��ǰ����
        int pos = (int)SendMessage(hWnd, PBM_GETPOS, 0, 0);
        int range = (int)SendMessage(hWnd, PBM_GETRANGE, FALSE, 0);

        if (pos > 0 && range > 0) {
            // ��������������
            int width = (rect.right - rect.left) * pos / range;
            RECT fillRect = rect;
            fillRect.right = fillRect.left + width;

            // ���ƽ��������
            RECT progressRect = { rect.left + 2, rect.top + 2, rect.left + width - 2, rect.bottom - 2 };
            DrawRoundedRectangle(hdc, progressRect, 3, COLOR_PROGRESS_BAR, true);

            // ���ƽ����ı�
            wchar_t progressText[32];
            swprintf_s(progressText, L"%d%%", (pos * 100) / range);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            SelectObject(hdc, g_hFontNormal);
            DrawText(hdc, progressText, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        EndPaint(hWnd, &ps);
        return 0;
    }
    }

    return DefSubclassProc(hWnd, message, wParam, lParam);
}

// �����Զ��尴ť
HWND CreateCustomButton(HWND hParent, LPCWSTR text, int x, int y, int width, int height, HMENU hMenu) {
    HWND hButton = CreateWindow(
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, width, height,
        hParent,
        hMenu,
        GetModuleHandle(NULL),
        NULL
    );

    // �������໯����
    SetWindowSubclass(hButton, CustomButtonProc, 0, 0);

    // ��������
    SendMessage(hButton, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    return hButton;
}

// ������
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // ��ʼ��ͨ�ÿؼ�
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    // ������־
    SetupLogging();

    // ��ȡӦ�ó���·��
    wchar_t path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    g_applicationPath = std::wstring(path);
    size_t lastSlash = g_applicationPath.find_last_of(L"\\");
    if (lastSlash != std::wstring::npos) {
        g_applicationPath = g_applicationPath.substr(0, lastSlash);
    }

    // ������ԱȨ��
    if (!IsAdmin()) {
        LogOutput(L"���ǹ���Ա�������Թ���Ա�����������...");

        // �Թ���Ա�����������
        SHELLEXECUTEINFO sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = path;
        sei.hwnd = NULL;
        sei.nShow = SW_NORMAL;

        if (ShellExecuteEx(&sei)) {
            return 0;
        }
        else {
            MessageBox(NULL, L"�޷��Թ���Ա�����������", L"����", MB_ICONERROR);
            return 1;
        }
    }

    // ע�ᴰ����
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"MiniWorldShaderInstallerClass";
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex)) {
        MessageBox(NULL, L"����ע��ʧ�ܣ�", L"����", MB_ICONERROR);
        return 1;
    }

    // ��WinMain�������޸Ĵ��ڴ�������ʾ����
    // ��������
    DWORD exStyle = WS_EX_LAYERED; // ��ӷֲ㴰����ʽ��֧��͸����
    g_hWnd = CreateWindowEx(
        exStyle,
        L"MiniWorldShaderInstallerClass",
        L"���������Ӱ����װ��V2.6.31",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        g_windowWidth, g_windowHeight,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!g_hWnd) {
        MessageBox(NULL, L"���ڴ���ʧ�ܣ�", L"����", MB_ICONERROR);
        return 1;
    }

    // ���㴰����ʼλ�ú�Ŀ��λ��
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    g_windowStartX = -g_windowWidth; // ��ʼλ������Ļ�����
    g_windowTargetX = (screenWidth - g_windowWidth) / 2; // Ŀ��λ������Ļ����
    g_windowY = (screenHeight - g_windowHeight) / 2; // ��ֱ����

    // ���ó�ʼλ�ú�͸����
    SetWindowPos(g_hWnd, NULL, g_windowStartX, g_windowY, g_windowWidth, g_windowHeight, SWP_NOZORDER);
    SetLayeredWindowAttributes(g_hWnd, 0, 0, LWA_ALPHA); // ��ʼ͸����Ϊ0

    // ��WinMain�������޸Ķ�ʱ������
    // ����������ʱ����ʹ�ø�С��ʱ����
    SetTimer(g_hWnd, g_animationTimerId, 8, NULL); // Լ120fps���������Ķ���

    // ��ʾ����
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // ��Ϣѭ��
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

// ����UI
void SetupUI(HWND hWnd) {
    // ����״̬��
    g_hStatusBar = CreateWindowEx(
        0,
        STATUSCLASSNAME,
        NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        hWnd,
        (HMENU)IDC_STATUS_BAR,
        GetModuleHandle(NULL),
        NULL
    );

    // ����״̬�����
    int statwidths[] = { -1 };
    SendMessage(g_hStatusBar, SB_SETPARTS, 1, (LPARAM)statwidths);
    SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)L"׼������");

    // ��������ı���
    g_hOutputText = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
        420, 50,
        760, 500,
        hWnd,
        (HMENU)IDC_OUTPUT_TEXT,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(g_hOutputText, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    // ����������
    g_hProgressBar = CreateWindowEx(
        0,
        PROGRESS_CLASS,
        L"",
        WS_CHILD | WS_VISIBLE,
        420, 560,
        760, 30,
        hWnd,
        (HMENU)IDC_PROGRESS_BAR,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(g_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SetWindowSubclass(g_hProgressBar, CustomProgressProc, 0, 0);

    // ������Ȩ������ǩ
    HWND hCopyrightLabel = CreateWindowEx(
        0,
        L"STATIC",
        L"��Ȩ����",
        WS_CHILD | WS_VISIBLE,
        20, 50,
        380, 20,
        hWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(hCopyrightLabel, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    // ������Ȩ�����ı���
    HWND hCopyrightText = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"��Ȩ������ \r\n \r\n����Ʒ�����������������֡�ͼƬ����Ƶ����Ƶ�����Ԫ�صȣ��İ�Ȩ������\"��Ӱ�����������ҡ�Qichee\"��\"�����Ǽ�\"��ͬ���С�δ�����߼���������ȷ������Ȩ���κθ��ˡ���֯������������κ���ʽ���ơ��ַ���������չʾ���޸ġ��ı���������κη�ʽʹ�ñ���Ʒ��ȫ���򲿷����ݡ����߼������ұ����Ա���Ʒ��ȫ��Ȩ��������������������Ȩ���̱�Ȩ��ר��Ȩ�ȡ��κ�δ����Ȩ��ʹ����Ϊ��������Ȩ�����߼������ҽ�����׷����Ȩ�ߵķ������Σ���Ҫ����Ȩ�߳е���Ӧ���⳥���Ρ������ȡ����Ʒ����Ȩʹ�ã�����\"��Ӱ�����������ҡ�Qichee�������Ǽ�\"��������ϵ�������ҽ����ݾ�����������Ƿ���Ȩ�Լ���Ȩ�������ͷ�Χ�����߼������ұ�����ʱ�޸ġ����»���ֹ����Ȩ������Ȩ��������������֪ͨ�κε����������߼������ҶԱ���Ȩ����ӵ�����ս���Ȩ��\r\n \r\n��ϵ��ʽ��\r\n���������ƣ������Ǽ�\r\n��ַ��https://www.scmgzs.top/\r\n��ϵ���䣺ssw2196634956@outlook.com",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
        20, 75,
        380, 100,
        hWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(hCopyrightText, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    // ����������־��ǩ
    HWND hUpdateLabel = CreateWindowEx(
        0,
        L"STATIC",
        L"������־",
        WS_CHILD | WS_VISIBLE,
        20, 185,
        380, 20,
        hWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(hUpdateLabel, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    // ����������־�ı���
    HWND hUpdateText = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"## ������־\r\n  \r\n### �汾 2.7.42 (2025��5��) \r\n- **�޸�**:\r\n -�޸���������1.46.0.0���º���ȾЧ��ʧЧ���� \r\n -ʹ��C++�ع����� \r\n \r\n### �汾 2.6.32��2025��4�£�\r\n- **�޸�**:\r\n -�޸���������1.45.90���º���ȾЧ��ʧЧ���� \r\n \r\n### �汾 2.6.31��2025��1�£�\r\n- **�Ż�**:\r\n - �Ż�����ܹ������õļ���win7ϵͳ���С�\r\n - �Ż�UI���棬ʹ��pyqt5������\r\n- **�޸�**��\r\n - �޸���ACE��ʹ��������ϧ����������\r\n - �޸�ɨ��Ѱ�����⡣\r\n### �汾 2.6.24 (2023��10��)\r\n- **��������**:\r\n - ����˶�����Ϸ�汾��֧�֡�\r\n  - �������û��������ܣ��û�����ֱ����Ӧ�����ύ������\r\n \r\n- **�Ż�**:\r\n  - �Ż��˰�װ�����е��ļ���ѹ�ٶȡ�\r\n  - �Ľ���UI���棬ʹ������Ѻú�ֱ�ۡ�\r\n \r\n- **�޸�**:\r\n  - �޸�����ĳЩ����¿�ݷ�ʽ����ʧ�ܵ����⡣\r\n  - �޸��˰�װ�����п��ܳ��ֵı������⡣",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
        20, 210,
        380, 100,
        hWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(hUpdateText, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    // ������⵽����Ϸ�汾��ǩ
    HWND hVersionLabel = CreateWindowEx(
        0,
        L"STATIC",
        L"��⵽����Ϸ�汾",
        WS_CHILD | WS_VISIBLE,
        20, 320,
        380, 20,
        hWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(hVersionLabel, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    // �����汾���
    g_hVersionGroup = CreateWindowEx(
        0,
        L"BUTTON",
        L"��Ϸ�汾",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        20, 345,
        380, 120,
        hWnd,
        (HMENU)IDC_VERSION_GROUP,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(g_hVersionGroup, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    SetWindowSubclass(g_hVersionGroup, CustomGroupBoxProc, 0, 0);

    // ��������Զ��尲װ��ť
    HWND hAddCustomButton = CreateCustomButton(
        hWnd,
        L"����Զ��尲װ",
        20, 475,
        380, 30,
        (HMENU)IDC_ADD_CUSTOM
    );

    // ����Ԥ������ѡ���ǩ
    HWND hPresetLabel = CreateWindowEx(
        0,
        L"STATIC",
        L"Ԥ������ѡ��",
        WS_CHILD | WS_VISIBLE,
        20, 515,
        380, 20,
        hWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(hPresetLabel, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    // ����Ԥ�踴ѡ��
    HWND hYirixcCheckbox = CreateWindowEx(
        0,
        L"BUTTON",
        L"Ԥ��������YiRixc",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        20, 540,
        180, 20,
        hWnd,
        (HMENU)IDC_YIRIXC_CHECKBOX,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(hYirixcCheckbox, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    HWND hTianxingdasheCheckbox = CreateWindowEx(
        0,
        L"BUTTON",
        L"Ԥ�����������Ǵ���",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        210, 540,
        190, 20,
        hWnd,
        (HMENU)IDC_TIANXINGDASHE_CHECKBOX,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(hTianxingdasheCheckbox, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    // ��������Ԥ�谴ť
    HWND hDownloadPresetButton = CreateCustomButton(
        hWnd,
        L"����Ԥ��",
        20, 570,
        380, 30,
        (HMENU)IDC_DOWNLOAD_PRESET
    );

    // ����ɨ����ư�ť
    HWND hStartScanButton = CreateCustomButton(
        hWnd,
        L"��ʼɨ��",
        20, 610,
        120, 30,
        (HMENU)IDC_START_SCAN
    );

    HWND hPauseScanButton = CreateCustomButton(
        hWnd,
        L"��ͣɨ��",
        150, 610,
        120, 30,
        (HMENU)IDC_PAUSE_SCAN
    );
    EnableWindow(hPauseScanButton, FALSE);

    HWND hStopScanButton = CreateCustomButton(
        hWnd,
        L"ֹͣɨ��",
        280, 610,
        120, 30,
        (HMENU)IDC_STOP_SCAN
    );
    EnableWindow(hStopScanButton, FALSE);

    // ������װ��ť
    HWND hInstallButton = CreateCustomButton(
        hWnd,
        L"��װ",
        420, 610,
        760, 30,
        (HMENU)IDC_INSTALL_BUTTON
    );
}

// �����־
void LogOutput(const std::wstring& message) {
    // ��ӵ�����ı���
    int length = GetWindowTextLength(g_hOutputText);
    SendMessage(g_hOutputText, EM_SETSEL, (WPARAM)length, (LPARAM)length);
    SendMessageW(g_hOutputText, EM_REPLACESEL, FALSE, (LPARAM)message.c_str());
    SendMessageW(g_hOutputText, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");

    // �������ײ�
    SendMessage(g_hOutputText, EM_SCROLLCARET, 0, 0);

    // д����־�ļ�
    if (g_logFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        std::wstring timeStr = std::to_wstring(now_time_t);
        g_logFile << L"[" << timeStr << L"] " << message << std::endl;
    }

    // ����״̬��
    SetStatusText(message);
}

// ������־
void SetupLogging() {
    // ��ȡ����·��
    wchar_t desktopPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_DESKTOP, NULL, 0, desktopPath))) {
        std::wstring logPath = std::wstring(desktopPath) + L"\\installer_log.txt";
        g_logFile.open(logPath, std::ios::out | std::ios::app);
        if (g_logFile.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto now_time_t = std::chrono::system_clock::to_time_t(now);
            std::wstring timeStr = std::to_wstring(now_time_t);
            g_logFile << L"[" << timeStr << L"] ��־��ʼ��¼" << std::endl;
        }
    }
}

// ����Ƿ�Ϊ����Ա
bool IsAdmin() {
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup;

    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &AdministratorsGroup)) {
        if (!CheckTokenMembership(NULL, AdministratorsGroup, &isAdmin)) {
            isAdmin = FALSE;
        }
        FreeSid(AdministratorsGroup);
    }

    return isAdmin != FALSE;
}

// ��ȡWindows�汾
std::wstring GetWindowsVersion() {
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

#pragma warning(disable:4996)
    GetVersionEx((OSVERSIONINFO*)&osvi);
#pragma warning(default:4996)

    return std::to_wstring(osvi.dwMajorVersion) + L"." + std::to_wstring(osvi.dwMinorVersion);
}

// ������а汾
void CheckAllVersions() {
    g_detectedVersions.clear();
    LogOutput(L"����Ƿ��Թ���Ա�������...");
    LogOutput(L"��ʼ�����Ϸ�汾...");

    // ��ȡ�û��ļ���
    wchar_t usersFolderPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, 0, usersFolderPath))) {
        std::wstring usersFolder = usersFolderPath;
        size_t lastSlash = usersFolder.find_last_of(L"\\");
        if (lastSlash != std::wstring::npos) {
            usersFolder = usersFolder.substr(0, lastSlash);
        }

        // ���ע���
        std::wstring officialPath = CheckRegistryAndExtract(L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\�ٷ�����������");
        if (!officialPath.empty()) {
            g_detectedVersions[L"�ٷ�����������"] = officialPath;
            LogOutput(L"��⵽: �ٷ�����������");
        }

        // ����û��ļ���
        WIN32_FIND_DATA findData;
        HANDLE hFind = FindFirstFile((usersFolder + L"\\*").c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0) {
                        std::wstring userFolder = usersFolder + L"\\" + findData.cFileName;

                        std::map<std::wstring, std::wstring> paths = {
                            {L"4399����������", userFolder + L"\\AppData\\Roaming\\miniworldgame4399"},
                            {L"7k7k����������", userFolder + L"\\AppData\\Roaming\\miniworldgame7k7k"},
                            {L"����������ʰ�", userFolder + L"\\AppData\\Roaming\\miniworldOverseasgame"}
                        };

                        for (const auto& pair : paths) {
                            if (PathFileExists(pair.second.c_str())) {
                                std::wstring version = pair.first + L" (�û�: " + findData.cFileName + L")";
                                g_detectedVersions[version] = pair.second;
                                LogOutput(L"��⵽: " + version);
                            }
                        }
                    }
                }
            } while (FindNextFile(hFind, &findData));
            FindClose(hFind);
        }
    }

    if (g_detectedVersions.empty()) {
        int result = MessageBox(g_hWnd, L"δ��⵽��װ·�����Ƿ����ȫ��ɨ��Ѱ���������磿", L"δ��⵽��װ·��", MB_YESNO | MB_ICONQUESTION);
        if (result == IDYES) {
            StartScan();
        }
    }

    LogOutput(L"��⵽�İ汾����: " + std::to_wstring(g_detectedVersions.size()));
    PopulateOptions();
}

// ��ע����鲢��ȡ·��
std::wstring CheckRegistryAndExtract(const std::wstring& registryKeyPath) {
    HKEY hKey;
    DWORD accessFlag = KEY_READ;

    std::wstring windowsVersion = GetWindowsVersion();
    double version = _wtof(windowsVersion.c_str());
    if (version <= 6.1) {
        accessFlag = KEY_READ;
    }
    else {
        accessFlag = KEY_READ | KEY_WOW64_32KEY;
    }

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, registryKeyPath.c_str(), 0, accessFlag, &hKey) == ERROR_SUCCESS) {
        wchar_t publisher[MAX_PATH];
        DWORD bufferSize = sizeof(publisher);
        DWORD type;

        if (RegQueryValueEx(hKey, L"Publisher", 0, &type, (LPBYTE)publisher, &bufferSize) == ERROR_SUCCESS) {
            std::wstring publisherStr = publisher;
            std::wstring folderPath = publisherStr;

            // �滻"������Ƽ����޹�˾"Ϊ��
            size_t pos = folderPath.find(L"������Ƽ����޹�˾");
            if (pos != std::wstring::npos) {
                folderPath.replace(pos, wcslen(L"������Ƽ����޹�˾"), L"");
            }

            // ȥ��ǰ��ո�
            while (!folderPath.empty() && (folderPath[0] == L' ' || folderPath[0] == L'\t' ||
                folderPath[0] == L'\n' || folderPath[0] == L'\r')) {
                folderPath.erase(0, 1);
            }

            while (!folderPath.empty() && (folderPath[folderPath.length() - 1] == L' ' ||
                folderPath[folderPath.length() - 1] == L'\t' || folderPath[folderPath.length() - 1] == L'\n' ||
                folderPath[folderPath.length() - 1] == L'\r')) {
                folderPath.erase(folderPath.length() - 1, 1);
            }

            RegCloseKey(hKey);
            if (!folderPath.empty()) {
                return folderPath;
            }
            else {
                LogOutput(L"������·����Ч��");
            }
        }
        RegCloseKey(hKey);
    }

    return L"";
}

// ���ѡ��
void PopulateOptions() {
    // ������еĵ�ѡ��ť
    HWND hChild = GetWindow(g_hVersionGroup, GW_CHILD);
    while (hChild) {
        HWND hNext = GetWindow(hChild, GW_HWNDNEXT);
        DestroyWindow(hChild);
        hChild = hNext;
    }

    // ����µĵ�ѡ��ť
    int y = 25;
    int buttonId = 1000; // ��ʼID
    HWND firstButton = NULL;

    for (const auto& pair : g_detectedVersions) {
        HWND hRadio = CreateWindowEx(
            0,
            L"BUTTON",
            pair.first.c_str(),
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            15, y,
            350, 20,
            g_hVersionGroup,
            (HMENU)(INT_PTR)buttonId,
            GetModuleHandle(NULL),
            NULL
        );

        SendMessage(hRadio, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        if (!firstButton) {
            firstButton = hRadio;
        }

        y += 25;
        buttonId++;
    }

    // ѡ�е�һ����ť
    if (firstButton) {
        SendMessage(firstButton, BM_SETCHECK, BST_CHECKED, 0);
    }
}

// ����Զ���·��
void AddCustomPath() {
    wchar_t path[MAX_PATH] = { 0 };
    BROWSEINFO bi = { 0 };
    bi.lpszTitle = L"ѡ��װĿ¼";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (pidl != NULL) {
        if (SHGetPathFromIDList(pidl, path)) {
            std::wstring pathStr = path;
            std::wstring version = L"�Զ���·��: " + pathStr;
            g_detectedVersions[version] = pathStr;
            PopulateOptions();
            LogOutput(L"����Զ���·��: " + pathStr);
        }
        CoTaskMemFree(pidl);
    }
}

// ��ʼɨ��
void StartScan() {
    if (g_scanning) {
        return;
    }

    g_scanning = true;
    g_scanPaused = false;
    EnableWindow(GetDlgItem(g_hWnd, IDC_START_SCAN), FALSE);
    EnableWindow(GetDlgItem(g_hWnd, IDC_PAUSE_SCAN), TRUE);
    EnableWindow(GetDlgItem(g_hWnd, IDC_STOP_SCAN), TRUE);
    g_scanStartTime = std::chrono::steady_clock::now();

    // ����ɨ���߳�
    g_scanThread = CreateThread(NULL, 0, ScanThreadProc, NULL, 0, NULL);
    if (g_scanThread == NULL) {
        LogOutput(L"ɨ���̴߳���ʧ��");
        g_scanning = false;
        EnableWindow(GetDlgItem(g_hWnd, IDC_START_SCAN), TRUE);
        EnableWindow(GetDlgItem(g_hWnd, IDC_PAUSE_SCAN), FALSE);
        EnableWindow(GetDlgItem(g_hWnd, IDC_STOP_SCAN), FALSE);
    }
    else {
        LogOutput(L"��ʼɨ��");
    }
}

// ɨ���̹߳���
DWORD WINAPI ScanThreadProc(LPVOID lpParameter) {
    ScanForMicroMiniNew();
    return 0;
}

// ɨ��MicroMiniNew
void ScanForMicroMiniNew() {
    // ���ȳ���ʹ��Windows��������
    if (WindowsIndexSearch()) {
        g_scanning = false;
        PostMessage(g_hWnd, WM_COMMAND, MAKEWPARAM(IDC_STOP_SCAN, 0), 0);

        if (g_scanStartTime != std::chrono::steady_clock::time_point()) {
            auto elapsed = std::chrono::steady_clock::now() - g_scanStartTime;
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
            int hours = static_cast<int>(seconds / 3600);
            int minutes = static_cast<int>((seconds % 3600) / 60);
            int secs = static_cast<int>(seconds % 60);

            wchar_t timeStr[50];
            swprintf_s(timeStr, L"%02d:%02d:%02d", hours, minutes, secs);

            std::wstring* pMessage = new std::wstring(L"ɨ����ɣ�����ʱ: " + std::wstring(timeStr));
            PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
        }
        else {
            std::wstring* pMessage = new std::wstring(L"ɨ�����");
            PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
        }

        return;
    }

    // ���Windows��������ʧ�ܻ�û���ҵ����������ȫ��ɨ��
    FullDiskScan();

    if (g_detectedVersions.empty()) {
        std::wstring* pMessage = new std::wstring(L"δ�ҵ�MicroMiniNew.exe");
        PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
    }

    g_scanning = false;
    PostMessage(g_hWnd, WM_COMMAND, MAKEWPARAM(IDC_STOP_SCAN, 0), 0);

    if (g_scanStartTime != std::chrono::steady_clock::time_point()) {
        auto elapsed = std::chrono::steady_clock::now() - g_scanStartTime;
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
        int hours = static_cast<int>(seconds / 3600);
        int minutes = static_cast<int>((seconds % 3600) / 60);
        int secs = static_cast<int>(seconds % 60);

        wchar_t timeStr[50];
        swprintf_s(timeStr, L"%02d:%02d:%02d", hours, minutes, secs);

        std::wstring* pMessage = new std::wstring(L"ɨ����ɣ�����ʱ: " + std::wstring(timeStr));
        PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
    }
    else {
        std::wstring* pMessage = new std::wstring(L"ɨ�����");
        PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
    }
}

// Windows��������
bool WindowsIndexSearch() {
    // �ⲿ����Ҫʹ��COM�ӿڣ��Ƚϸ��ӣ�����򻯴���
    // ��ʵ��Ӧ���У�����ʹ��OLE DB��ADO����ѯWindows����
    std::wstring* pMessage = new std::wstring(L"Windows��������������C++�汾�м򻯴���");
    PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
    return false;
}

// ȫ��ɨ��
void FullDiskScan() {
    std::wstring* pMessage = new std::wstring(L"��ʼȫ��ɨ��...");
    PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);

    std::vector<std::wstring> excludedFolders = {
        L"$Recycle.Bin", L"System Volume Information", L"Windows",
        L"Program Files", L"Program Files (x86)", L"ProgramData",
        L"temp", L"tmp"
    };

    pMessage = new std::wstring(L"�ų����ļ���: $Recycle.Bin, System Volume Information, Windows, Program Files, Program Files (x86), ProgramData, temp, tmp���Լ�·���а��� 'Temp' ���ļ���");
    PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);

    for (wchar_t drive = L'A'; drive <= L'Z'; drive++) {
        std::wstring drivePath = std::wstring(1, drive) + L":\\";
        if (GetDriveType(drivePath.c_str()) == DRIVE_FIXED) {
            pMessage = new std::wstring(L"����ɨ�������� " + std::wstring(1, drive) + L"...");
            PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);

            std::wstring searchPath = drivePath + L"*";
            WIN32_FIND_DATA findData;
            HANDLE hFind = FindFirstFile(searchPath.c_str(), &findData);

            if (hFind != INVALID_HANDLE_VALUE) {
                std::vector<std::wstring> directories;
                directories.push_back(drivePath);

                while (!directories.empty() && g_scanning) {
                    while (g_scanPaused) {
                        Sleep(100);
                    }

                    if (!g_scanning) {
                        break;
                    }

                    std::wstring currentDir = directories.back();
                    directories.pop_back();

                    searchPath = currentDir + L"*";
                    hFind = FindFirstFile(searchPath.c_str(), &findData);

                    if (hFind != INVALID_HANDLE_VALUE) {
                        do {
                            if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0) {
                                std::wstring fullPath = currentDir + findData.cFileName;

                                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                                    // ����Ƿ�Ϊ�ų����ļ���
                                    bool excluded = false;
                                    for (const auto& folder : excludedFolders) {
                                        if (_wcsicmp(findData.cFileName, folder.c_str()) == 0 ||
                                            wcsstr(findData.cFileName, L"$") == findData.cFileName) {
                                            excluded = true;
                                            break;
                                        }
                                    }

                                    if (!excluded) {
                                        directories.push_back(fullPath + L"\\");
                                    }
                                }
                                else if (wcscmp(findData.cFileName, L"������.exe") == 0) {
                                    std::wstring path = currentDir;
                                    std::wstring version = L"δ֪�汾 (·��: " + path + L")";
                                    AddDetectedVersion(version, path);
                                }
                            }
                        } while (FindNextFile(hFind, &findData) && g_scanning);

                        FindClose(hFind);
                    }
                }
            }
        }
    }
}

// ��Ӽ�⵽�İ汾
void AddDetectedVersion(const std::wstring& version, const std::wstring& path) {
    g_detectedVersions[version] = path;
    std::wstring* pMessage = new std::wstring(L"��⵽: " + version);
    PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
}

// ��ͣɨ��
void PauseScan() {
    if (!g_scanning) {
        return;
    }

    g_scanPaused = !g_scanPaused;
    if (g_scanPaused) {
        SetWindowText(GetDlgItem(g_hWnd, IDC_PAUSE_SCAN), L"����ɨ��");
        LogOutput(L"ɨ������ͣ");
    }
    else {
        SetWindowText(GetDlgItem(g_hWnd, IDC_PAUSE_SCAN), L"��ͣɨ��");
        LogOutput(L"ɨ���Ѽ���");
    }
}

// ֹͣɨ��
void StopScan() {
    if (!g_scanning) {
        return;
    }

    g_scanning = false;
    EnableWindow(GetDlgItem(g_hWnd, IDC_START_SCAN), TRUE);
    EnableWindow(GetDlgItem(g_hWnd, IDC_PAUSE_SCAN), FALSE);
    EnableWindow(GetDlgItem(g_hWnd, IDC_STOP_SCAN), FALSE);

    if (g_scanStartTime != std::chrono::steady_clock::time_point()) {
        auto elapsed = std::chrono::steady_clock::now() - g_scanStartTime;
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
        int hours = static_cast<int>(seconds / 3600);
        int minutes = static_cast<int>((seconds % 3600) / 60);
        int secs = static_cast<int>(seconds % 60);

        wchar_t timeStr[50];
        swprintf_s(timeStr, L"%02d:%02d:%02d", hours, minutes, secs);
        LogOutput(L"�û���ֹɨ�裬����ʱ: " + std::wstring(timeStr));
    }
    else {
        LogOutput(L"ɨ��δ��ʼ�ͱ���ֹ");
    }

    PopulateOptions();
}

// ����Ԥ��
void DownloadPresets() {
    // ��ȡѡ�еİ汾
    std::wstring selectedVersion;
    std::wstring installPath;

    HWND hChild = GetWindow(g_hVersionGroup, GW_CHILD);
    while (hChild) {
        if (SendMessage(hChild, BM_GETCHECK, 0, 0) == BST_CHECKED) {
            wchar_t text[256];
            GetWindowText(hChild, text, 256);
            selectedVersion = text;
            break;
        }
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }

    if (selectedVersion.empty()) {
        MessageBox(g_hWnd, L"����ѡ��һ����Ϸ�汾", L"����", MB_ICONWARNING);
        LogOutput(L"��������Ԥ��ʱδѡ����Ϸ�汾");
        return;
    }

    installPath = g_detectedVersions[selectedVersion];

    if (IsDlgButtonChecked(g_hWnd, IDC_YIRIXC_CHECKBOX) == BST_CHECKED) {
        DownloadAndExtractPreset(L"https://json.scmgzs.top/preset/YiRixc/YiRixc.zip", installPath, L"YiRixc");
    }

    if (IsDlgButtonChecked(g_hWnd, IDC_TIANXINGDASHE_CHECKBOX) == BST_CHECKED) {
        DownloadAndExtractPreset(L"https://json.scmgzs.top/preset/StarSerpent/StarSerpent.zip", installPath, L"StarSerpent");
    }
}

// ���ز���ѹԤ��
void DownloadAndExtractPreset(const std::wstring& url, const std::wstring& installPath, const std::wstring& presetName) {
    std::wstring presetPath = installPath + L"\\presets";
    CreateDirectory(presetPath.c_str(), NULL);
    std::wstring zipPath = presetPath + L"\\" + presetName + L".zip";

    LogOutput(L"��ʼ���� " + presetName + L" Ԥ��...");

    // ʹ��URLDownloadToFile�����ļ�
    HRESULT hr = URLDownloadToFile(NULL, url.c_str(), zipPath.c_str(), 0, NULL);
    if (SUCCEEDED(hr)) {
        LogOutput(presetName + L" Ԥ���������");

        if (ExtractZipWith7z(zipPath, presetPath)) {
            DeleteFile(zipPath.c_str());
            LogOutput(presetName + L" Ԥ�����ز���ѹ���");
        }
        else {
            LogOutput(presetName + L" Ԥ���ѹʧ��");
        }
    }
    else {
        LogOutput(L"���� " + presetName + L" Ԥ��ʱ����: " + std::to_wstring(hr));
    }
}

// ʹ��7z��ѹZIP�ļ�
bool ExtractZipWith7z(const std::wstring& zipPath, const std::wstring& extractPath) {
    LogOutput(L"��ʼ��ѹ�� " + zipPath + L" �� " + extractPath + L"...");

    if (!PathFileExists(zipPath.c_str())) {
        LogOutput(L"ZIP�ļ�������: " + zipPath);
        return false;
    }

    if (!PathFileExists(extractPath.c_str())) {
        CreateDirectory(extractPath.c_str(), NULL);
    }

    std::wstring sevenZipPath = g_applicationPath + L"\\7z\\7z.exe";
    if (!PathFileExists(sevenZipPath.c_str())) {
        LogOutput(L"7z.exe not found at " + sevenZipPath);
        return false;
    }

    int maxAttempts = 3;
    for (int attempt = 0; attempt < maxAttempts; attempt++) {
        try {
            // ����������
            std::wstring command = L"\"" + sevenZipPath + L"\" x \"" + zipPath + L"\" -o\"" + extractPath + L"\" -y";

            // ��������
            STARTUPINFO si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            ZeroMemory(&pi, sizeof(pi));

            // ��������
            if (CreateProcess(NULL, (LPWSTR)command.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                // �ȴ��������
                WaitForSingleObject(pi.hProcess, INFINITE);

                // ��ȡ�����˳���
                DWORD exitCode;
                GetExitCodeProcess(pi.hProcess, &exitCode);

                // �رս��̺��߳̾��
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);

                if (exitCode != 0) {
                    LogOutput(L"7z extraction failed with return code " + std::to_wstring(exitCode));
                    if (attempt < maxAttempts - 1) {
                        LogOutput(L"�ȴ�3�������... (���� " + std::to_wstring(attempt + 2) + L"/" + std::to_wstring(maxAttempts) + L")");
                        Sleep(3000);
                    }
                    else {
                        return false;
                    }
                }
                else {
                    LogOutput(L"��ѹ����ɣ�");
                    return true;
                }
            }
            else {
                LogOutput(L"����7z����ʧ��: " + std::to_wstring(GetLastError()));
                if (attempt < maxAttempts - 1) {
                    LogOutput(L"�ȴ�3�������... (���� " + std::to_wstring(attempt + 2) + L"/" + std::to_wstring(maxAttempts) + L")");
                    Sleep(3000);
                }
                else {
                    return false;
                }
            }
        }
        catch (...) {
            LogOutput(L"��ѹ�������г���");
            if (attempt < maxAttempts - 1) {
                LogOutput(L"�ȴ�3�������... (���� " + std::to_wstring(attempt + 2) + L"/" + std::to_wstring(maxAttempts) + L")");
                Sleep(3000);
            }
            else {
                return false;
            }
        }
    }

    return false;
}

// ��װ����
void InstallAction() {
    // ��ȡѡ�еİ汾
    std::wstring selectedVersion;
    std::wstring installPath;

    HWND hChild = GetWindow(g_hVersionGroup, GW_CHILD);
    while (hChild) {
        if (SendMessage(hChild, BM_GETCHECK, 0, 0) == BST_CHECKED) {
            wchar_t text[256];
            GetWindowText(hChild, text, 256);
            selectedVersion = text;
            break;
        }
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }

    if (selectedVersion.empty()) {
        MessageBox(g_hWnd, L"����ѡ��һ����Ϸ�汾", L"����", MB_ICONWARNING);
        LogOutput(L"���԰�װʱδѡ����Ϸ�汾");
        return;
    }

    installPath = g_detectedVersions[selectedVersion];

    // ��������еĽ���
    std::vector<std::wstring> runningProcesses = CheckRunningProcesses(installPath);
    if (!runningProcesses.empty()) {
        std::wstring processList;
        for (const auto& process : runningProcesses) {
            if (!processList.empty()) {
                processList += L", ";
            }
            processList += process;
        }

        int result = MessageBox(g_hWnd, (L"���½����������У�" + processList + L"\n�Ƿ������Щ���̲�������װ��").c_str(),
            L"����", MB_YESNO | MB_ICONQUESTION);
        if (result == IDYES) {
            for (const auto& process : runningProcesses) {
                TerminateProcessByPath(installPath);
            }
            LogOutput(L"�ȴ�3����ȷ��������ȫ��ֹ...");
            Sleep(3000);
        }
        else {
            return;
        }
    }

    StartInstallation(installPath);
}

// ��������еĽ���
std::vector<std::wstring> CheckRunningProcesses(const std::wstring& installPath) {
    std::vector<std::wstring> runningProcesses;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return runningProcesses;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
            if (hProcess) {
                wchar_t processPath[MAX_PATH];
                if (GetModuleFileNameEx(hProcess, NULL, processPath, MAX_PATH)) {
                    std::wstring processPathStr = processPath;
                    if (processPathStr.find(installPath) == 0) {
                        runningProcesses.push_back(pe32.szExeFile);
                    }
                }
                CloseHandle(hProcess);
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return runningProcesses;
}

// ����·����ֹ����
bool TerminateProcessByPath(const std::wstring& processPath) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool terminated = false;
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
            if (hProcess) {
                wchar_t path[MAX_PATH];
                if (GetModuleFileNameEx(hProcess, NULL, path, MAX_PATH)) {
                    std::wstring pathStr = path;
                    if (pathStr.find(processPath) == 0) {
                        if (TerminateProcess(hProcess, 0)) {
                            terminated = true;
                        }
                    }
                }
                CloseHandle(hProcess);
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return terminated;
}

// ��ʼ��װ
void StartInstallation(const std::wstring& installPath) {
    std::wstring zipPath = g_applicationPath + L"\\reshade\\MiNi-reshade.7z";
    LogOutput(L"���� zip �ļ�·��Ϊ: " + zipPath);

    std::wstring* pMessage = new std::wstring(L"��ʼ��װ...");
    PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);

    bool success = ExtractZipWith7z(zipPath, installPath);

    if (success) {
        pMessage = new std::wstring(L"��װ���");
        PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
        MessageBox(g_hWnd, L"��װ���", L"���", MB_ICONINFORMATION);
    }
    else {
        pMessage = new std::wstring(L"��װʧ��");
        PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
        int result = MessageBox(g_hWnd, L"��װʧ�ܡ��Ƿ�Ҫ����ϵͳ���������°�װ��", L"��װʧ��", MB_YESNO | MB_ICONQUESTION);
        if (result == IDYES) {
            ScheduleRebootAndInstall();
        }
    }

    return ;
    }

// ������������װ
void ScheduleRebootAndInstall() {
    HKEY key;
    if (RegCreateKey(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", &key) == ERROR_SUCCESS) {
        wchar_t path[MAX_PATH];
        GetModuleFileName(NULL, path, MAX_PATH);
        RegSetValueEx(key, L"MiniWorldInstaller", 0, REG_SZ, (BYTE*)path, (wcslen(path) + 1) * sizeof(wchar_t));
        RegCloseKey(key);

        system("shutdown /r /t 1");
    }
}

// ���ļ�ĩβ������¸�������
// ���������ɫ
COLORREF BlendColors(COLORREF color1, COLORREF color2, float factor) {
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;

    BYTE r1 = GetRValue(color1);
    BYTE g1 = GetGValue(color1);
    BYTE b1 = GetBValue(color1);

    BYTE r2 = GetRValue(color2);
    BYTE g2 = GetGValue(color2);
    BYTE b2 = GetBValue(color2);

    BYTE r = (BYTE)(r1 + (r2 - r1) * factor);
    BYTE g = (BYTE)(g1 + (g2 - g1) * factor);
    BYTE b = (BYTE)(b1 + (b2 - b1) * factor);

    return RGB(r, g, b);
}

// ���ƽ������
void DrawGradientRect(HDC hdc, RECT rect, COLORREF colorStart, COLORREF colorEnd) {
    // �������仭ˢ
    TRIVERTEX vertex[2];
    vertex[0].x = rect.left;
    vertex[0].y = rect.top;
    vertex[0].Red = GetRValue(colorStart) << 8;
    vertex[0].Green = GetGValue(colorStart) << 8;
    vertex[0].Blue = GetBValue(colorStart) << 8;
    vertex[0].Alpha = 0x0000;

    vertex[1].x = rect.right;
    vertex[1].y = rect.bottom;
    vertex[1].Red = GetRValue(colorEnd) << 8;
    vertex[1].Green = GetGValue(colorEnd) << 8;
    vertex[1].Blue = GetBValue(colorEnd) << 8;
    vertex[1].Alpha = 0x0000;

    GRADIENT_RECT gRect = { 0, 1 };

    // ���ƽ���
    GradientFill(hdc, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_V);
}

// ���ļ�ĩβ��ӻ�������ʵ��
// ƽ���Ķ��λ���������ʹ����������Ȼ
double EaseInOutQuad(double t) {
    return t < 0.5 ? 2 * t * t : 1 - pow(-2 * t + 2, 2) / 2;
}
