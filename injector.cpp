#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>

DWORD GetProcessId(const char* processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32);
    
    if (Process32First(snapshot, &processEntry)) {
        do {
            if (std::string(processEntry.szExeFile) == processName) {
                CloseHandle(snapshot);
                return processEntry.th32ProcessID;
            }
        } while (Process32Next(snapshot, &processEntry));
    }
    CloseHandle(snapshot);
    return 0;
}

bool InjectDLL(DWORD pid, const std::string& dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (hProcess == nullptr) {
        std::cout << "Cannot open process. Run as Administrator!" << std::endl;
        return false;
    }
    
    LPVOID pDllPath = VirtualAllocEx(hProcess, nullptr, dllPath.size() + 1, 
                                    MEM_COMMIT, PAGE_READWRITE);
    if (pDllPath == nullptr) {
        CloseHandle(hProcess);
        return false;
    }
    
    WriteProcessMemory(hProcess, pDllPath, dllPath.c_str(), dllPath.size() + 1, nullptr);
    
    HMODULE hKernel32 = GetModuleHandle("Kernel32");
    LPTHREAD_START_ROUTINE loadLibrary = (LPTHREAD_START_ROUTINE)
        GetProcAddress(hKernel32, "LoadLibraryA");
    
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, 
                                      loadLibrary, pDllPath, 0, nullptr);
    
    if (hThread == nullptr) {
        VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    
    WaitForSingleObject(hThread, INFINITE);
    VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);
    
    return true;
}

int main() {
    std::cout << "=== VimeWorld AutoMapper Injector ===" << std::endl;
    
    // Ждем запуска Minecraft
    DWORD pid = 0;
    while (pid == 0) {
        std::cout << "Waiting for javaw.exe..." << std::endl;
        pid = GetProcessId("javaw.exe");
        Sleep(2000);
    }
    
    std::cout << "Found javaw.exe (PID: " << pid << ")" << std::endl;
    
    // ✅ ПРАВИЛЬНЫЙ путь к DLL
    char dllPath[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, dllPath);
    strcat_s(dllPath, "\\mapper.dll"); // Просто добавляем имя файла
    
    std::cout << "Injecting: " << dllPath << std::endl;
    
    if (InjectDLL(pid, dllPath)) {
        std::cout << "SUCCESS: DLL injected! Check automapper_log.txt" << std::endl;
    } else {
        std::cout << "FAILED: Injection failed!" << std::endl;
    }
    
    system("pause");
    return 0;
}