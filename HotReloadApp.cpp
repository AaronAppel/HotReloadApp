#include <filesystem>
#include <string>

#include <windows.h>

std::wstring LibFileRelativeDirectory()
{
    if (IsDebuggerPresent())
    {
        return L"x64\\Debug";
    }
    return L".";
}

std::wstring PluginOutputDirectory()
{
    return L"Plugin\\x64\\Debug";
}

#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>
#include <winnt.h>

bool BuildPlugin()
{
    std::wstring command =
        L"\"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe\" "
        L"Plugin\\Plugin.vcxproj "
        L"/p:Configuration=Debug "
        L"/p:Platform=x64";

    STARTUPINFOW si = {};
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi = {};

    std::vector<wchar_t> cmd(
        command.begin(),
        command.end());

    cmd.push_back(L'\0');

    BOOL success = CreateProcessW(
        nullptr,
        cmd.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi);

    if (!success)
    {
        std::cerr
            << "[ERROR] Failed to launch MSBuild: "
            << GetLastError()
            << std::endl;

        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exitCode == 0;
}

std::atomic<bool> sourceChanged = false;
std::atomic<bool> pluginChanged = false;
std::atomic<bool> buildInProgress = false;
std::atomic<bool> reloadInProgress = false;
std::chrono::steady_clock::time_point lastSourceChangeTime;

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

std::filesystem::path GetPdbPath(const std::filesystem::path& dllPath)
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
        // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew
#define    FILE_SHARE_NONE 0
        // 0                    = 0 (Exclusive lock)
        // FILE_SHARE_READ      = 1 (Allow read)
        // FILE_SHARE_WRITE     = 2 (Allow write)
        // FILE_SHARE_DELETE    = 4 (Allow deletion)
        HANDLE file = CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_NONE,
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

enum class WatchType
{
    Source,
    Output
};

struct WatchContext {
    OVERLAPPED overlapped = {};
    char buffer[1024];
    HANDLE dirHandle = INVALID_HANDLE_VALUE;
    WatchType type;
};

bool HasExtension(
    const std::wstring& filename,
    const std::wstring& ext)
{
    if (filename.length() < ext.length())
        return false;

    return filename.ends_with(ext);
}

// Completion routine called by the OS when a directory change occurs
void CALLBACK DirectoryChangeCallback(DWORD errorCode, DWORD bytesTransferred, LPOVERLAPPED lpOverlapped) {

    auto* context =
        CONTAINING_RECORD(
            lpOverlapped,
            WatchContext,
            overlapped);
    BYTE* base = reinterpret_cast<BYTE*>(context->buffer);

    if (errorCode != ERROR_SUCCESS)
    {
        std::cerr
            << "[ERROR] Directory watch failed: "
            << errorCode
            << std::endl;
    }
    else if (bytesTransferred > 0)
    {
        // Process notifications

        while (true)
        {
            FILE_NOTIFY_INFORMATION* fni =
                reinterpret_cast<FILE_NOTIFY_INFORMATION*>(base);

            std::wstring changedFile(
                fni->FileName,
                fni->FileNameLength / sizeof(WCHAR));

            std::wcout
                << L"[INFO] Action "
                << fni->Action
                << L": "
                << changedFile
                << std::endl;

            if (context->type == WatchType::Output)
            {
                if (HasExtension(changedFile, L".dll"))
                {
                    pluginChanged = true;
                }
            }
            else if (context->type == WatchType::Source)
            {
                if (true ||
                    HasExtension(changedFile, L".h") ||
                    HasExtension(changedFile, L".cpp"))
                {
                    sourceChanged = true;

                    lastSourceChangeTime =
                        std::chrono::steady_clock::now();
                }
            }

            if (fni->NextEntryOffset == 0)
            {
                break;
            }

            base += fni->NextEntryOffset;
        }
    }

    // Re-issue the directory watch
    ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));
    BOOL success = ReadDirectoryChangesW(
        context->dirHandle,
        context->buffer,
        sizeof(context->buffer),
        TRUE, // Watch subdirs
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

bool setupFileWatcher(WatchContext& context, const std::wstring& directory, WatchType type, bool watchSubDirs = true)
{
    context.type = type;
    context.dirHandle = CreateFileW(
        directory.c_str(),
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

    BOOL success = ReadDirectoryChangesW(
        context.dirHandle,
        context.buffer,
        sizeof(context.buffer),
        (int)watchSubDirs,
        FILE_NOTIFY_CHANGE_FILE_NAME |
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

int main()
{
    // #TODO Multi-thread to do work on separate thread and the main thread can just reload the already build DLL

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

            auto tempPdb = GetPdbPath(entry.path());

            std::filesystem::remove(tempPdb, ec);
        }
    }

    HMODULE hLib = nullptr;

    run_func run = nullptr;

    classInfo_func getMyStructTypeInfo = nullptr;

    std::filesystem::path libFilePath =
        L"Plugin\\x64\\Debug\\Plugin.dll";

    std::filesystem::path pdbPath =
        L"Plugin\\x64\\Debug\\Plugin.pdb";

    WatchContext pluginSourceWatchContext;
    WatchContext mirrorSourceWatchContext;
    WatchContext outputWatchContext;

    setupFileWatcher(pluginSourceWatchContext, L"Plugin", WatchType::Source, false);
    setupFileWatcher(mirrorSourceWatchContext, L"Mirror", WatchType::Source, false);

    if (!setupFileWatcher(
        outputWatchContext,
        L"Plugin\\x64\\Debug",
        WatchType::Output))
    {
        return 1;
    }

    std::atomic<bool> reloadInProgress = false;

    std::cout
        << "[INFO] Monitoring source + plugin changes..."
        << std::endl;

    while (true)
    {
        // Let APC callbacks execute
        SleepEx(100, TRUE);

        //
        // BUILD PHASE
        //

        if (sourceChanged && !buildInProgress)
        {
            auto now = std::chrono::steady_clock::now();

            // debounce filesystem spam
            if (now - lastSourceChangeTime >
                std::chrono::milliseconds(100))
            {
                sourceChanged = false;

                buildInProgress = true;

                std::cout
                    << "[INFO] Rebuilding plugin..."
                    << std::endl;

                bool buildSuccess = BuildPlugin();

                buildInProgress = false;

                if (!buildSuccess)
                {
                    std::cerr
                        << "[ERROR] Plugin build failed."
                        << std::endl;
                }
                else
                {
                    std::cout
                        << "[INFO] Plugin build succeeded."
                        << std::endl;

                    // force reload even if watcher misses event
                    pluginChanged = true;
                }
            }
        }

        //
        // RELOAD PHASE
        //

        if ((pluginChanged || !hLib) &&
            !reloadInProgress)
        {
            reloadInProgress = true;

            pluginChanged = false;

            bool reloadSucceeded = false;

            do
            {
                //
                // unload old dll
                //

                if (hLib)
                {
                    std::cout
                        << "[INFO] Reloading plugin..."
                        << std::endl;

                    run = nullptr;
                    getMyStructTypeInfo = nullptr;

                    FreeLibrary(hLib);

                    hLib = nullptr;

                    std::this_thread::sleep_for(std::chrono::milliseconds(10));

                    if (!currentLoadedDllPath.empty())
                    {
                        std::error_code ec;

                        std::filesystem::remove(
                            currentLoadedDllPath,
                            ec);

                        auto tempPdb =
                            GetPdbPath(currentLoadedDllPath);

                        std::filesystem::remove(
                            tempPdb,
                            ec);
                    }
                }

                //
                // wait for linker output
                //

                if (!WaitForFileReady(libFilePath))
                {
                    std::cerr
                        << "[ERROR] Timed out waiting for DLL."
                        << std::endl;

                    break;
                }

                if (!WaitForFileStable(pdbPath))
                {
                    std::cerr
                        << "[ERROR] Timed out waiting for PDB."
                        << std::endl;

                    break;
                }

                //
                // copy dll
                //

                currentLoadedDllPath =
                    GenerateTempDllPath();

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

                    break;
                }

                //
                // copy pdb
                //

                auto tempPdb =
                    GetPdbPath(currentLoadedDllPath);

                std::error_code ec;

                if (!std::filesystem::copy_file(
                    pdbPath,
                    tempPdb,
                    std::filesystem::copy_options::overwrite_existing,
                    ec))
                {
                    std::cerr
                        << "[ERROR] Failed to copy PDB: "
                        << ec.message()
                        << std::endl;

                    break;
                }

                //
                // load dll
                //

                hLib =
                    LoadLibraryW(
                        currentLoadedDllPath.c_str());

                if (!hLib)
                {
                    std::cerr
                        << "[ERROR] Failed to load DLL: "
                        << GetLastError()
                        << std::endl;

                    break;
                }

                //
                // load exports
                //

                run =
                    (run_func)GetProcAddress(
                        hLib,
                        "run");

                if (!run)
                {
                    std::cerr
                        << "[ERROR] Missing export: run"
                        << std::endl;

                    FreeLibrary(hLib);

                    hLib = nullptr;

                    break;
                }

                getMyStructTypeInfo =
                    (classInfo_func)GetProcAddress(
                        hLib,
                        "myStructTypeInfo");

                if (!getMyStructTypeInfo)
                {
                    std::cerr
                        << "[ERROR] Missing export: "
                        << "myStructTypeInfo"
                        << std::endl;

                    FreeLibrary(hLib);

                    hLib = nullptr;

                    break;
                }

                reloadSucceeded = true;

            } while (false);

            reloadInProgress = false;

            if (reloadSucceeded)
            {
                std::cout
                    << "[INFO] Plugin reload succeeded."
                    << std::endl;
            }
        }

        //
        // RUN PHASE
        //

        if (run)
        {
            run();

            if (getMyStructTypeInfo)
            {
                std::cout
                    << "[INFO] Size of MyStruct is "
                    << getMyStructTypeInfo()->size
                    << " bytes"
                    << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CloseHandle(pluginSourceWatchContext.dirHandle);
    CloseHandle(mirrorSourceWatchContext.dirHandle);
    CloseHandle(outputWatchContext.dirHandle);

    return 0;
}
