#include <windows.h>
#include <iostream>
#include <string>

int main() {
    std::wstring cmd = L"cmd.exe /c \"\"C:\\Users\\Tlqkf\\CLionProjects\\PLC_Emulator\\cmake-build-release-mingw\\bin\\tools\\mingw64\\bin\\make.cmd\" -j4 -C \"C:/Users/Tlqkf/AppData/Local/PLCSimulator/rtl_cache/8daaab8842ca4d37/obj\" -f Vfull_adder.mk CXX=\"C:/Users/Tlqkf/CLionProjects/PLC_Emulator/cmake-build-release-mingw/bin/tools/mingw64/bin/g++.exe\"\"";
    
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    if (CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exit_code;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        std::cout << "Exit code: " << exit_code << "\n";
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        std::cout << "CreateProcess failed: " << GetLastError() << "\n";
    }
    return 0;
}