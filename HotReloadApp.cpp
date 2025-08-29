#include <filesystem>
#include <string>

// const std::string libFileAbsolutePath = "B:\\HotReloadApp\\x64\\Debug\\Plugin.dll";

#include <windows.h>
std::wstring LibFileRelativeDirectory()
{
    if (IsDebuggerPresent())
    {
        return L"x64\\Debug";
    }
    return L".";
}

const std::wstring libFileName = L"Plugin.dll";
const std::string tempLibFileName = "plugin_temp.dll";

#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>

using run_func = void(*)();

std::atomic<bool> pluginChanged = false;

struct WatchContext {
    OVERLAPPED overlapped = {};
    char buffer[1024];
    HANDLE dirHandle = INVALID_HANDLE_VALUE;
    wchar_t targetFile[260];
};

// Completion routine called by the OS when a directory change occurs
void CALLBACK DirectoryChangeCallback(DWORD errorCode, DWORD bytesTransferred, LPOVERLAPPED lpOverlapped) {
    if (errorCode != ERROR_SUCCESS || bytesTransferred == 0)
        return;

    auto* context = reinterpret_cast<WatchContext*>(lpOverlapped);

    FILE_NOTIFY_INFORMATION* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(context->buffer);
    std::wstring changedFile(fni->FileName, fni->FileNameLength / sizeof(WCHAR));

    if (changedFile == libFileName) {
        pluginChanged = true;
    }

    // Re-issue the directory watch
    ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));
    ReadDirectoryChangesW(
        context->dirHandle,
        context->buffer,
        sizeof(context->buffer),
        FALSE,
        FILE_NOTIFY_CHANGE_LAST_WRITE,
        nullptr,
        &context->overlapped,
        DirectoryChangeCallback
    );
}

bool setupFileWatcher(WatchContext& context) {
    context.dirHandle = CreateFileW(
        LibFileRelativeDirectory().c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr
    );

    if (context.dirHandle == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create directory handle for watching: " << GetLastError() << std::endl;
        return false;
    }

    ZeroMemory(&context.overlapped, sizeof(OVERLAPPED));
    wcscpy_s(context.targetFile, libFileName.c_str());

    BOOL success = ReadDirectoryChangesW(
        context.dirHandle,
        context.buffer,
        sizeof(context.buffer),
        FALSE, // don't watch subdirs
        FILE_NOTIFY_CHANGE_LAST_WRITE,
        nullptr,
        &context.overlapped,
        DirectoryChangeCallback
    );

    if (!success) {
        std::cerr << "Failed to initiate directory changes watch: " << GetLastError() << std::endl;
        CloseHandle(context.dirHandle);
        return false;
    }

    return true;
}

int main() {

    HMODULE hLib = nullptr;
    run_func run = nullptr;

    std::filesystem::path libFilePath = LibFileRelativeDirectory();
    libFilePath /= libFileName;

    WatchContext watchContext;
    if (!setupFileWatcher(watchContext)) {
        return 1;
    }

    std::cout << "[INFO] Monitoring for changes to plugin.dll..." << std::endl;

    while (true) {
        // Allow the OS to execute completion routines (APC) via alertable sleep
        SleepEx(100, TRUE);

        if (pluginChanged || !hLib) {
            pluginChanged = false;

            if (hLib) {
                std::cout << "[INFO] Reloading plugin..." << std::endl;
                FreeLibrary(hLib);
                hLib = nullptr;
            }

            // Wait until file is ready (some editors take time to write)
            for (int retries = 0; retries < 5; ++retries) {
                try {
                    std::filesystem::copy_file(libFilePath, tempLibFileName, std::filesystem::copy_options::overwrite_existing);
                    break;
                }
                catch (...) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
            }

            hLib = LoadLibraryA(tempLibFileName.c_str());
            if (!hLib) {
                std::cerr << "Failed to load plugin DLL: " << GetLastError() << std::endl;
                continue;
            }

            run = (run_func)GetProcAddress(hLib, "run");
            if (!run) {
                std::cerr << "Failed to find symbol 'run'" << std::endl;
                continue;
            }
        }

        if (run) {
            run();
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    CloseHandle(watchContext.dirHandle);
    return 0;
}
