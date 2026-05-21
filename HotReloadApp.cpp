#include <filesystem>
#include <string>
#include <vector>

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
#include <mutex>
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

    GetExitCodeProcess(
        pi.hProcess,
        &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exitCode == 0;
}

enum class BuildState
{
    Idle,
    Pending,
    Building,
    ReadyToLoad,
    Failed
};

std::atomic<bool> sourceChanged = false;
std::atomic<bool> reloadInProgress = false;

std::atomic<BuildState> g_buildState =
BuildState::Idle;

std::chrono::steady_clock::time_point
lastSourceChangeTime;

uint64_t g_reloadCounter = 0;

std::filesystem::path currentLoadedDllPath;

std::mutex g_readyDllMutex;

std::filesystem::path g_readyDllPath;
std::filesystem::path g_readyPdbPath;

std::filesystem::path GenerateTempDllPath()
{
    auto tempDir =
        std::filesystem::temp_directory_path();

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
    auto start =
        std::chrono::steady_clock::now();

    while (true)
    {
#define FILE_SHARE_NONE 0

        HANDLE file =
            CreateFileW(
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
    auto start =
        std::chrono::steady_clock::now();

    uintmax_t lastSize = 0;

    int stableCount = 0;

    while (true)
    {
        std::error_code ec;

        if (std::filesystem::exists(path, ec))
        {
            auto size =
                std::filesystem::file_size(
                    path,
                    ec);

            if (!ec)
            {
                if (size == lastSize)
                {
                    stableCount++;

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

using classInfo_func =
Mirror::TypeInfo* (*)();

enum class WatchType
{
    Source,
    Output
};

struct WatchContext
{
    OVERLAPPED overlapped = {};

    char buffer[1024];

    HANDLE dirHandle =
        INVALID_HANDLE_VALUE;

    WatchType type;
};

bool HasExtension(
    const std::wstring& filename,
    const std::wstring& ext)
{
    if (filename.length() < ext.length())
    {
        return false;
    }

    return filename.ends_with(ext);
}

void CALLBACK DirectoryChangeCallback(
    DWORD errorCode,
    DWORD bytesTransferred,
    LPOVERLAPPED lpOverlapped)
{
    auto* context =
        CONTAINING_RECORD(
            lpOverlapped,
            WatchContext,
            overlapped);

    BYTE* base =
        reinterpret_cast<BYTE*>(
            context->buffer);

    if (errorCode != ERROR_SUCCESS)
    {
        std::cerr
            << "[ERROR] Directory watch failed: "
            << errorCode
            << std::endl;
    }
    else if (bytesTransferred > 0)
    {
        while (true)
        {
            FILE_NOTIFY_INFORMATION* fni =
                reinterpret_cast<FILE_NOTIFY_INFORMATION*>(base);

            std::wstring changedFile(
                fni->FileName,
                fni->FileNameLength /
                sizeof(WCHAR));

            std::wcout
                << L"[INFO] Action "
                << fni->Action
                << L": "
                << changedFile
                << std::endl;

            if (context->type ==
                WatchType::Source)
            {
                if (HasExtension(changedFile, L".cpp") ||
                    HasExtension(changedFile, L".h"))
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

    ZeroMemory(
        &context->overlapped,
        sizeof(OVERLAPPED));

    BOOL success =
        ReadDirectoryChangesW(
            context->dirHandle,
            context->buffer,
            sizeof(context->buffer),
            TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME |
            FILE_NOTIFY_CHANGE_LAST_WRITE,
            nullptr,
            &context->overlapped,
            DirectoryChangeCallback);

    if (!success)
    {
        std::cerr
            << "[ERROR] Failed to re-issue watch: "
            << GetLastError()
            << std::endl;

        CloseHandle(context->dirHandle);
    }
}

bool setupFileWatcher(
    WatchContext& context,
    const std::wstring& directory,
    WatchType type,
    bool watchSubDirs = true)
{
    context.type = type;

    context.dirHandle =
        CreateFileW(
            directory.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ |
            FILE_SHARE_WRITE |
            FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS |
            FILE_FLAG_OVERLAPPED,
            nullptr);

    if (context.dirHandle ==
        INVALID_HANDLE_VALUE)
    {
        std::cerr
            << "[ERROR] Failed to create watch handle: "
            << GetLastError()
            << std::endl;

        return false;
    }

    ZeroMemory(
        &context.overlapped,
        sizeof(OVERLAPPED));

    BOOL success =
        ReadDirectoryChangesW(
            context.dirHandle,
            context.buffer,
            sizeof(context.buffer),
            (int)watchSubDirs,
            FILE_NOTIFY_CHANGE_FILE_NAME |
            FILE_NOTIFY_CHANGE_LAST_WRITE,
            nullptr,
            &context.overlapped,
            DirectoryChangeCallback);

    if (!success)
    {
        std::cerr
            << "[ERROR] Failed to initiate watch: "
            << GetLastError()
            << std::endl;

        CloseHandle(context.dirHandle);

        return false;
    }

    return true;
}

void BuildWorkerThread(
    const std::filesystem::path& libFilePath,
    const std::filesystem::path& pdbPath)
{
    while (true)
    {
        if (g_buildState ==
            BuildState::Pending)
        {
            g_buildState =
                BuildState::Building;

            std::cout
                << "[INFO] Rebuilding plugin..."
                << std::endl;

            bool buildSuccess =
                BuildPlugin();

            if (!buildSuccess)
            {
                std::cerr
                    << "[ERROR] Plugin build failed."
                    << std::endl;

                g_buildState =
                    BuildState::Failed;

                continue;
            }

            if (!WaitForFileReady(libFilePath))
            {
                std::cerr
                    << "[ERROR] DLL never became ready."
                    << std::endl;

                g_buildState =
                    BuildState::Failed;

                continue;
            }

            if (!WaitForFileStable(pdbPath))
            {
                std::cerr
                    << "[ERROR] PDB never became stable."
                    << std::endl;

                g_buildState =
                    BuildState::Failed;

                continue;
            }

            //
            // Copy DLL + PDB HERE
            //

            auto tempDll =
                GenerateTempDllPath();

            auto tempPdb =
                GetPdbPath(tempDll);

            try
            {
                std::filesystem::copy_file(
                    libFilePath,
                    tempDll,
                    std::filesystem::copy_options::overwrite_existing);
            }
            catch (const std::exception& e)
            {
                std::cerr
                    << "[ERROR] Failed to copy DLL: "
                    << e.what()
                    << std::endl;

                g_buildState =
                    BuildState::Failed;

                continue;
            }

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

                g_buildState =
                    BuildState::Failed;

                continue;
            }

            {
                std::scoped_lock lock(
                    g_readyDllMutex);

                g_readyDllPath = tempDll;
                g_readyPdbPath = tempPdb;
            }

            std::cout
                << "[INFO] Plugin build succeeded."
                << std::endl;

            g_buildState =
                BuildState::ReadyToLoad;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(10));
    }
}

int main()
{
    for (auto& entry :
        std::filesystem::directory_iterator(
            std::filesystem::temp_directory_path()))
    {
        auto name =
            entry.path()
            .filename()
            .wstring();

        if (name.starts_with(
            L"plugin_hotreload_"))
        {
            std::error_code ec;

            std::filesystem::remove(
                entry.path(),
                ec);

            auto tempPdb =
                GetPdbPath(entry.path());

            std::filesystem::remove(
                tempPdb,
                ec);
        }
    }

    HMODULE hLib = nullptr;

    run_func run = nullptr;

    classInfo_func getMyStructTypeInfo = nullptr;
    unsigned int latestSizeOfMyStruct = -1;

    std::filesystem::path libFilePath =
        L"Plugin\\x64\\Debug\\Plugin.dll";

    std::filesystem::path pdbPath =
        L"Plugin\\x64\\Debug\\Plugin.pdb";

    // Initial startup DLL copy/load
    g_readyDllPath = GenerateTempDllPath();

    std::filesystem::copy_file(
        libFilePath,
        g_readyDllPath,
        std::filesystem::copy_options::overwrite_existing);

    g_readyPdbPath =
        GetPdbPath(g_readyDllPath);

    std::filesystem::copy_file(
        pdbPath,
        g_readyPdbPath,
        std::filesystem::copy_options::overwrite_existing);

    g_buildState =
        BuildState::ReadyToLoad;

    // Worker and file watcher threads
    std::thread buildThread(
        BuildWorkerThread,
        libFilePath,
        pdbPath);

    buildThread.detach();

    WatchContext pluginSourceWatchContext;
    WatchContext mirrorSourceWatchContext;

    setupFileWatcher(
        pluginSourceWatchContext,
        L"Plugin",
        WatchType::Source,
        false);

    setupFileWatcher(
        mirrorSourceWatchContext,
        L"Mirror",
        WatchType::Source,
        false);

    std::cout
        << "[INFO] Monitoring source changes..."
        << std::endl;

    while (true)
    {
        SleepEx(10, TRUE);

        //
        // BUILD REQUEST PHASE
        //

        if (sourceChanged)
        {
            auto now =
                std::chrono::steady_clock::now();

            if (now - lastSourceChangeTime >
                std::chrono::milliseconds(100))
            {
                sourceChanged = false;

                if (g_buildState ==
                    BuildState::Idle ||
                    g_buildState ==
                    BuildState::Failed)
                {
                    g_buildState =
                        BuildState::Pending;
                }
            }
        }

        //
        // RELOAD PHASE
        //
        if (g_buildState == BuildState::ReadyToLoad &&
            !reloadInProgress)
        {
            reloadInProgress = true;

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

                    getMyStructTypeInfo =
                        nullptr;

                    FreeLibrary(hLib);

                    hLib = nullptr;

                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(10));

                    if (!currentLoadedDllPath.empty())
                    {
                        std::error_code ec;

                        std::filesystem::remove(
                            currentLoadedDllPath,
                            ec);

                        auto tempPdb =
                            GetPdbPath(
                                currentLoadedDllPath);

                        std::filesystem::remove(
                            tempPdb,
                            ec);
                    }
                }

                //
                // grab worker-produced dll
                //

                std::filesystem::path readyDll;

                {
                    std::scoped_lock lock(
                        g_readyDllMutex);

                    readyDll =
                        g_readyDllPath;
                }

                //
                // load dll
                //

                if (readyDll.empty())
                {
                    std::cerr
                        << "[ERROR] No DLL ready to load."
                        << std::endl;

                    break;
                }

                hLib =
                    LoadLibraryW(
                        readyDll.c_str());

                if (!hLib)
                {
                    std::cerr
                        << "[ERROR] Failed to load DLL: "
                        << GetLastError()
                        << std::endl;

                    break;
                }

                currentLoadedDllPath =
                    readyDll;

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
                g_buildState =
                    BuildState::Idle;

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
                if (latestSizeOfMyStruct != getMyStructTypeInfo()->size)
                {
                    latestSizeOfMyStruct = getMyStructTypeInfo()->size;
                    std::cout
                        << "[INFO] Last size of MyStruct was "
                        << latestSizeOfMyStruct
                        << " bytes"
                        << std::endl;
                }
            }
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(10));
    }

    CloseHandle(
        pluginSourceWatchContext.dirHandle);

    CloseHandle(
        mirrorSourceWatchContext.dirHandle);

    return 0;
}