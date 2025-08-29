#include <windows.h>
#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>

using run_func = void(*)();

std::atomic<bool> pluginChanged = false;

// Background thread to watch for file changes
void watchPluginFile(const std::wstring& directory, const std::wstring& targetFileName) {
    HANDLE dirHandle = CreateFileW(
        directory.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr
    );

    if (dirHandle == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open directory handle for watching." << std::endl;
        return;
    }

    char buffer[1024];
    DWORD bytesReturned;

    while (true) {
        if (ReadDirectoryChangesW(
            dirHandle,
            &buffer,
            sizeof(buffer),
            FALSE,
            FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytesReturned,
            nullptr,
            nullptr)) {

            FILE_NOTIFY_INFORMATION* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(&buffer);
            std::wstring changedFile(fni->FileName, fni->FileNameLength / sizeof(WCHAR));

            if (changedFile == targetFileName) {
                pluginChanged = true;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    CloseHandle(dirHandle);
}


int main() {

    // const std::string libFileAbsolutePath = "B:\\HotReloadApp\\x64\\Debug\\Plugin.dll";
    std::filesystem::path currentPath= std::filesystem::current_path();

    const std::string libFileRelativeDirectory = ".";// "x64\\Debug";
    const std::string libFileName = "Plugin.dll";
    const std::string tempLibFileName = "plugin_temp.dll";

    std::filesystem::path libFilePath = libFileRelativeDirectory;
    libFilePath /= libFileName;

    std::filesystem::path tempLibFilePath = libFileRelativeDirectory;
    tempLibFilePath /= tempLibFileName;

    HMODULE hLib = nullptr;
    run_func run = nullptr;

    const std::wstring directory = L"."; // L"x64\\Debug";
    const std::wstring targetFileName = L"Plugin.dll";

    std::thread watcherThread(watchPluginFile, directory, targetFileName);
    watcherThread.detach();

    while (std::filesystem::exists(libFilePath)) {
        if (pluginChanged || !hLib) {
            pluginChanged = false;

            if (hLib) {
                std::cout << "[INFO] Reloading plugin..." << std::endl;
                FreeLibrary(hLib);
                hLib = nullptr;
            }

            try {
                std::filesystem::copy_file(libFilePath, tempLibFilePath, std::filesystem::copy_options::overwrite_existing);
            }
            catch (std::exception& e) {
                std::cerr << "Failed to copy plugin: " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }

            hLib = LoadLibraryA(tempLibFilePath.string().c_str());
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

    return 0;
}
