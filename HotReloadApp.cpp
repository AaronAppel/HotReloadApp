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

#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>

const std::wstring libFileName = L"Plugin.dll";
const std::wstring cppFileName = L"Plugin.cpp";

uint64_t g_reloadCounter = 0;
std::filesystem::path currentLoadedDllPath;

std::filesystem::path GenerateTempDllPath()
{
    auto tempDir = std::filesystem::temp_directory_path();

    std::wstring filename =
        L"plugin_hotreload_" +
        std::to_wstring(GetTickCount64()) +
        L"_" +
        std::to_wstring(g_reloadCounter++) +
        L".dll";

    return tempDir / filename;
}

std::filesystem::path GetPdbPath(
    const std::filesystem::path& dllPath)
{
    auto pdb = dllPath;
    pdb.replace_extension(L".pdb");
    return pdb;
}

bool WaitForFileReady(
    const std::filesystem::path& path,
    std::chrono::milliseconds timeout =
    std::chrono::seconds(5))
{
    auto start = std::chrono::steady_clock::now();

    while (true)
    {
        HANDLE file = CreateFileW(
            path.c_str(),
            GENERIC_READ,
            0,                  // NO sharing
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (file != INVALID_HANDLE_VALUE)
        {
            CloseHandle(file);
            return true;
        }

        DWORD error = GetLastError();

        // Optional diagnostics
        if (error != ERROR_SHARING_VIOLATION &&
            error != ERROR_LOCK_VIOLATION)
        {
            std::cerr
                << "[WARN] Unexpected file wait error: "
                << error
                << std::endl;
        }

        if (std::chrono::steady_clock::now() - start >= timeout)
        {
            return false;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(50));
    }
}

bool WaitForFileStable(
    const std::filesystem::path& path,
    std::chrono::milliseconds timeout =
    std::chrono::seconds(5))
{
    auto start = std::chrono::steady_clock::now();

    uintmax_t lastSize = 0;
    int stableCount = 0;

    while (true)
    {
        std::error_code ec;

        if (std::filesystem::exists(path, ec))
        {
            auto size = std::filesystem::file_size(path, ec);

            if (!ec)
            {
                if (size == lastSize)
                {
                    stableCount++;

                    // Require stability across several checks
                    if (stableCount >= 3)
                    {
                        return true;
                    }
                }
                else
                {
                    stableCount = 0;
                    lastSize = size;
                }
            }
        }

        if (std::chrono::steady_clock::now() - start >= timeout)
        {
            return false;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(50));
    }
}

using run_func = void(*)();

#include "Mirror/MIR_Mirror.h"
#include "Plugin/Plugin.h"
using classInfo_func = Mirror::TypeInfo*(*)();

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
    else if (changedFile == cppFileName)
    {
        // #TODO Add/change a watch to the plugin's .cpp file and reload DLL via code change instead of manual recompile
    }

    // Re-issue the directory watch
    ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));
    BOOL success = ReadDirectoryChangesW(
        context->dirHandle,
        context->buffer,
        sizeof(context->buffer),
        FALSE,
        FILE_NOTIFY_CHANGE_FILE_NAME |
        FILE_NOTIFY_CHANGE_LAST_WRITE,
        nullptr,
        &context->overlapped,
        DirectoryChangeCallback
    );

    if (!success) {
        std::cerr << "Failed to re-issue directory changes watch: " << GetLastError() << std::endl;
        CloseHandle(context->dirHandle);
    }
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

    // Clean up previous temp files
    for (auto& entry :
        std::filesystem::directory_iterator(
            std::filesystem::temp_directory_path()))
    {
        auto name = entry.path().filename().wstring();

        if (name.starts_with(L"plugin_hotreload_"))
        {
            std::error_code ec;
            std::filesystem::remove(entry.path(), ec);

            auto tempPdb = GetPdbPath(currentLoadedDllPath);
            std::filesystem::remove(tempPdb, ec);
        }
    }

    HMODULE hLib = nullptr;
    run_func run = nullptr;
    classInfo_func getMyStructTypeInfo = nullptr;

    std::filesystem::path libFilePath = LibFileRelativeDirectory();
    libFilePath /= libFileName;
    std::filesystem::path pdbPath = libFilePath.parent_path() / L"Plugin.pdb";

    WatchContext watchContext;
    if (!setupFileWatcher(watchContext)) {
        return 1;
    }

    std::cout << "[INFO] Monitoring for changes to plugin.dll..." << std::endl;

    while (true) {
        // Allow the OS to execute completion routines (APC) via alertable sleep
        SleepEx(1000, TRUE); // #TODO Avoid SleepEx by overlapping ReadDirectoryChangesW calls so OS threads work instead

        if (pluginChanged || !hLib) {
            pluginChanged = false;

            if (hLib)
            {
                std::cout << "[INFO] Reloading plugin..." << std::endl;

                run = nullptr;
                getMyStructTypeInfo = nullptr;

                FreeLibrary(hLib);
                hLib = nullptr;

                // Give Windows a moment to fully release file handles
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                // Delete previous temp DLL
                if (!currentLoadedDllPath.empty())
                {
                    std::error_code ec;
                    std::filesystem::remove(currentLoadedDllPath, ec);
                }
            }

            // Wait until file is ready (some editors take time to write) by checking how many locks the .dll and .pdb files have
            if (!WaitForFileReady(libFilePath))
            {
                std::cerr
                    << "[ERROR] Timed out waiting for DLL rebuild."
                    << std::endl;

                continue;
            }

            if (!WaitForFileStable(pdbPath))
            {
                std::cerr
                    << "[ERROR] Timed out waiting for PDB."
                    << std::endl;

                continue;
            }

            // Copy new .dll and .pdb files
            currentLoadedDllPath = GenerateTempDllPath();

            try
            {
                std::filesystem::copy_file(
                    libFilePath,
                    currentLoadedDllPath,
                    std::filesystem::copy_options::overwrite_existing);
            }
            catch (const std::exception& e)
            {
                std::cerr
                    << "[ERROR] Failed to copy DLL: "
                    << e.what()
                    << std::endl;

                continue;
            }

            std::filesystem::path originalPdb =
                libFilePath.parent_path() / L"Plugin.pdb";

            std::filesystem::path tempPdb =
                GetPdbPath(currentLoadedDllPath);

            std::error_code ec;
            if (!std::filesystem::copy_file(
                originalPdb,
                tempPdb,
                std::filesystem::copy_options::overwrite_existing,
                ec))
            {
                std::cerr
                    << "[ERROR] Failed to copy PDB: "
                    << ec.message()
                    << std::endl;

                continue;
            }

            hLib = LoadLibraryW(currentLoadedDllPath.c_str());
            if (!hLib) {
                std::cerr << "Failed to load plugin DLL: " << GetLastError() << std::endl;
                continue;
            }

            run = (run_func)GetProcAddress(hLib, "run");
            if (!run) {
                std::cerr << "Failed to find symbol 'run'" << std::endl;
                continue;
            }
            getMyStructTypeInfo = (classInfo_func)GetProcAddress(hLib, "myStructTypeInfo");
            if (!getMyStructTypeInfo) {
                std::cerr << "Failed to find symbol 'myStructTypeInfo'" << std::endl;
                continue;
            }
        }

        if (run) {
            run();
            if (getMyStructTypeInfo)
            {
                std::cout << "[INFO] Size of MyStruct is " << getMyStructTypeInfo()->size << " bytes" << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    CloseHandle(watchContext.dirHandle);
    return 0;
}
