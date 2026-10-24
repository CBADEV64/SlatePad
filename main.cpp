// clang-format off
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <string>
// clang-format on

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")

namespace {

constexpr wchar_t APP_NAME[] = L"SlatePad";
constexpr wchar_t WINDOW_CLASS[] = L"SlatePad_WindowClass";
constexpr wchar_t NOTES_FILE[] = L"notes.txt";

constexpr UINT_PTR TIMER_AUTOSAVE_ID = 1001;
constexpr UINT AUTOSAVE_DELAY_MS = 5000;

constexpr int IDC_MAIN_EDIT = 2001;
constexpr int IDM_TOGGLE_AOT = 3001;
constexpr int IDM_FORCE_SAVE = 3002;

HWND g_hMainWnd = nullptr;
HWND g_hEdit = nullptr;
HFONT g_hFont = nullptr;
HBRUSH g_hDarkBrush = nullptr;

bool g_alwaysOnTop = true;
bool g_isDirty = false;

// Dark theme color palette (Zinc Dark)
constexpr COLORREF COLOR_BG = RGB(24, 24, 27);
constexpr COLORREF COLOR_TEXT = RGB(228, 228, 231);

[[nodiscard]] std::wstring GetNotesFilePath() {
  wchar_t exePath[MAX_PATH];
  DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
  if (len > 0 && len < MAX_PATH) {
    wchar_t *lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) {
      *(lastSlash + 1) = L'\0';
      return std::wstring(exePath) + NOTES_FILE;
    }
  }
  return NOTES_FILE;
}

void UpdateTitleBar(HWND hwnd, bool isTypingNotice = false) {
  wchar_t title[256];
  const wchar_t *const aotState = g_alwaysOnTop ? L"[Pin: ON]" : L"[Pin: OFF]";

  if (g_isDirty) {
    if (isTypingNotice) {
      swprintf_s(title, L"%s * %s (Saving in 5s...)", APP_NAME, aotState);
    } else {
      swprintf_s(title, L"%s * %s (Unsaved changes)", APP_NAME, aotState);
    }
  } else {
    SYSTEMTIME st;
    GetLocalTime(&st);
    swprintf_s(title, L"%s %s - Saved (%02d:%02d:%02d)", APP_NAME, aotState,
               st.wHour, st.wMinute, st.wSecond);
  }

  SetWindowTextW(hwnd, title);
}

void SetAlwaysOnTop(HWND hwnd, bool enable) {
  g_alwaysOnTop = enable;
  SetWindowPos(hwnd, g_alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE);

  HMENU hSysMenu = GetSystemMenu(hwnd, FALSE);
  if (hSysMenu) {
    CheckMenuItem(hSysMenu, IDM_TOGGLE_AOT,
                  MF_BYCOMMAND | (g_alwaysOnTop ? MF_CHECKED : MF_UNCHECKED));
  }

  UpdateTitleBar(hwnd);
}

void ToggleAlwaysOnTop(HWND hwnd) { SetAlwaysOnTop(hwnd, !g_alwaysOnTop); }

bool SaveNotesToFile(HWND hEdit) {
  if (!hEdit)
    return false;

  const int textLength = GetWindowTextLengthW(hEdit);
  std::wstring textBuffer(static_cast<size_t>(textLength) + 1, L'\0');
  if (textLength > 0) {
    GetWindowTextW(hEdit, &textBuffer[0], textLength + 1);
  }
  textBuffer.resize(static_cast<size_t>(textLength));

  // Convert UTF-16 to UTF-8
  int utf8BytesNeeded = 0;
  if (!textBuffer.empty()) {
    utf8BytesNeeded = WideCharToMultiByte(CP_UTF8, 0, textBuffer.data(),
                                          static_cast<int>(textBuffer.size()),
                                          nullptr, 0, nullptr, nullptr);
  }

  std::string utf8Data(static_cast<size_t>(utf8BytesNeeded), '\0');
  if (utf8BytesNeeded > 0) {
    WideCharToMultiByte(CP_UTF8, 0, textBuffer.data(),
                        static_cast<int>(textBuffer.size()), &utf8Data[0],
                        utf8BytesNeeded, nullptr, nullptr);
  }

  const std::wstring filePath = GetNotesFilePath();
  HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (hFile == INVALID_HANDLE_VALUE) {
    return false;
  }

  DWORD bytesWritten = 0;
  if (!utf8Data.empty()) {
    WriteFile(hFile, utf8Data.data(), static_cast<DWORD>(utf8Data.size()),
              &bytesWritten, nullptr);
  }

  CloseHandle(hFile);
  return true;
}

bool LoadNotesFromFile(HWND hEdit) {
  if (!hEdit)
    return false;

  const std::wstring filePath = GetNotesFilePath();
  HANDLE hFile =
      CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (hFile == INVALID_HANDLE_VALUE) {
    return false;
  }

  const DWORD fileSize = GetFileSize(hFile, nullptr);
  if (fileSize == 0 || fileSize == INVALID_FILE_SIZE) {
    CloseHandle(hFile);
    return true;
  }

  std::string utf8Buffer(static_cast<size_t>(fileSize), '\0');
  DWORD bytesRead = 0;
  ReadFile(hFile, &utf8Buffer[0], fileSize, &bytesRead, nullptr);
  CloseHandle(hFile);

  if (bytesRead == 0)
    return true;

  // Convert UTF-8 to UTF-16
  int wideCharsNeeded = MultiByteToWideChar(
      CP_UTF8, 0, utf8Buffer.data(), static_cast<int>(bytesRead), nullptr, 0);

  std::wstring wideBuffer(static_cast<size_t>(wideCharsNeeded), L'\0');
  if (wideCharsNeeded > 0) {
    MultiByteToWideChar(CP_UTF8, 0, utf8Buffer.data(),
                        static_cast<int>(bytesRead), &wideBuffer[0],
                        wideCharsNeeded);
  }

  SetWindowTextW(hEdit, wideBuffer.c_str());

  // Place cursor at end of text
  SendMessageW(hEdit, EM_SETSEL, wideCharsNeeded, wideCharsNeeded);
  SendMessageW(hEdit, EM_SCROLLCARET, 0, 0);

  return true;
}

// Subclass procedure for Edit control to handle hotkeys cleanly (Ctrl+A,
// Ctrl+S, Ctrl+T)
LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                  LPARAM lParam, UINT_PTR uIdSubclass,
                                  DWORD_PTR dwRefData) {
  switch (uMsg) {
  case WM_KEYDOWN: {
    const bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    if (ctrlDown) {
      if (wParam == 'A' || wParam == 'a') {
        SendMessageW(hWnd, EM_SETSEL, 0, -1);
        return 0;
      }
      if (wParam == 'S' || wParam == 's') {
        SendMessageW(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(IDM_FORCE_SAVE, 0),
                     0);
        return 0;
      }
      if (wParam == 'T' || wParam == 't') {
        SendMessageW(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(IDM_TOGGLE_AOT, 0),
                     0);
        return 0;
      }
    }
    break;
  }

  case WM_CHAR: {
    // Prevent beep or control character insertion for handled shortcuts
    if (wParam == 1 || wParam == 19 || wParam == 20) { // Ctrl+A, Ctrl+S, Ctrl+T
      return 0;
    }
    break;
  }

  case WM_NCDESTROY: {
    RemoveWindowSubclass(hWnd, EditSubclassProc, uIdSubclass);
    break;
  }

  default:
    break;
  }

  return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                LPARAM lParam) {
  switch (uMsg) {
  case WM_CREATE: {
    g_hMainWnd = hwnd;

    // Apply Immersive Dark Mode to Window Title Bar
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode,
                          sizeof(useDarkMode));

    // Create Dark Brush for edit background
    g_hDarkBrush = CreateSolidBrush(COLOR_BG);

    // Add Toggle Always on Top to System Menu
    HMENU hSysMenu = GetSystemMenu(hwnd, FALSE);
    if (hSysMenu) {
      AppendMenuW(hSysMenu, MF_SEPARATOR, 0, nullptr);
      AppendMenuW(hSysMenu, MF_STRING | MF_CHECKED, IDM_TOGGLE_AOT,
                  L"Always on Top\tCtrl+T");
    }

    // Create clean multiline Edit Control
    g_hEdit = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL |
            ES_WANTRETURN | ES_NOHIDESEL,
        0, 0, 0, 0, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_MAIN_EDIT)),
        GetModuleHandleW(nullptr), nullptr);

    // Comfortable reading margins (12px left and right)
    SendMessageW(g_hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                 MAKELONG(12, 12));

    // Modern, crisp typography (Consolas / Cascadia)
    g_hFont =
        CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    SendMessageW(g_hEdit, WM_SETFONT, reinterpret_cast<WPARAM>(g_hFont), TRUE);

    // Attach subclass for seamless Ctrl shortcuts
    SetWindowSubclass(g_hEdit, EditSubclassProc, 1, 0);

    // Load existing notes
    LoadNotesFromFile(g_hEdit);
    g_isDirty = false;

    // Enforce initial Always-On-Top state
    SetAlwaysOnTop(hwnd, true);

    // Focus edit control right away
    SetFocus(g_hEdit);
    return 0;
  }

  case WM_SIZE: {
    if (g_hEdit) {
      RECT rc;
      GetClientRect(hwnd, &rc);
      MoveWindow(g_hEdit, 0, 0, rc.right, rc.bottom, TRUE);
    }
    return 0;
  }

  case WM_SETFOCUS: {
    if (g_hEdit) {
      SetFocus(g_hEdit);
    }
    return 0;
  }

  case WM_CTLCOLOREDIT:
  case WM_CTLCOLORSTATIC: {
    HDC hdc = reinterpret_cast<HDC>(wParam);
    SetTextColor(hdc, COLOR_TEXT);
    SetBkColor(hdc, COLOR_BG);
    return reinterpret_cast<LRESULT>(g_hDarkBrush);
  }

  case WM_COMMAND: {
    const WORD cmdId = LOWORD(wParam);
    const WORD notifyCode = HIWORD(wParam);

    if (cmdId == IDC_MAIN_EDIT && notifyCode == EN_CHANGE) {
      g_isDirty = true;
      // Debounced autosave: reset the 5000ms timer on every keystroke
      KillTimer(hwnd, TIMER_AUTOSAVE_ID);
      SetTimer(hwnd, TIMER_AUTOSAVE_ID, AUTOSAVE_DELAY_MS, nullptr);
      UpdateTitleBar(hwnd, true);
      return 0;
    }

    if (cmdId == IDM_TOGGLE_AOT) {
      ToggleAlwaysOnTop(hwnd);
      return 0;
    }

    if (cmdId == IDM_FORCE_SAVE) {
      KillTimer(hwnd, TIMER_AUTOSAVE_ID);
      if (SaveNotesToFile(g_hEdit)) {
        g_isDirty = false;
        UpdateTitleBar(hwnd, false);
      }
      return 0;
    }

    break;
  }

  case WM_SYSCOMMAND: {
    if ((wParam & 0xFFF0) == IDM_TOGGLE_AOT) {
      ToggleAlwaysOnTop(hwnd);
      return 0;
    }
    break;
  }

  case WM_TIMER: {
    if (wParam == TIMER_AUTOSAVE_ID) {
      KillTimer(hwnd, TIMER_AUTOSAVE_ID);
      if (g_isDirty) {
        SaveNotesToFile(g_hEdit);
        g_isDirty = false;
        UpdateTitleBar(hwnd, false);
      }
      return 0;
    }
    break;
  }

  case WM_CLOSE: {
    // Save immediately on close if there are pending changes
    if (g_isDirty) {
      KillTimer(hwnd, TIMER_AUTOSAVE_ID);
      SaveNotesToFile(g_hEdit);
      g_isDirty = false;
    }
    DestroyWindow(hwnd);
    return 0;
  }

  case WM_DESTROY: {
    if (g_hFont) {
      DeleteObject(g_hFont);
      g_hFont = nullptr;
    }
    if (g_hDarkBrush) {
      DeleteObject(g_hDarkBrush);
      g_hDarkBrush = nullptr;
    }
    PostQuitMessage(0);
    return 0;
  }

  default:
    break;
  }

  return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
  // Enable Per-Monitor V2 DPI awareness for crystal-sharp rendering
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  INITCOMMONCONTROLSEX icex{};
  icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icex.dwICC = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&icex);

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = MainWindowProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
  wc.hbrBackground = nullptr;
  wc.lpszClassName = WINDOW_CLASS;

  if (!RegisterClassExW(&wc)) {
    return 1;
  }

  // Default centered position and sleek compact footprint
  constexpr int defaultWidth = 480;
  constexpr int defaultHeight = 620;
  const int screenX = (GetSystemMetrics(SM_CXSCREEN) - defaultWidth) / 2;
  const int screenY = (GetSystemMetrics(SM_CYSCREEN) - defaultHeight) / 2;

  HWND hwnd = CreateWindowExW(WS_EX_TOPMOST, // ALWAYS on top by default
                              WINDOW_CLASS, APP_NAME, WS_OVERLAPPEDWINDOW,
                              screenX, screenY, defaultWidth, defaultHeight,
                              nullptr, nullptr, hInstance, nullptr);

  if (!hwnd) {
    return 1;
  }

  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  MSG msg{};
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  return static_cast<int>(msg.wParam);
}
