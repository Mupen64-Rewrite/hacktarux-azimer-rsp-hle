#pragma once

extern HINSTANCE g_instance;
extern std::string g_app_path;

INT_PTR CALLBACK ConfigDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
