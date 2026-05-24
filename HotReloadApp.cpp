/*
* TODO Move reusable code into new files to better organize and reuse plus platform abstraction
*
* FileIO.h/.cpp (watch, exists, path helpers, copy)
* System.h/.cpp (command line, load/unload DLL helpers or at least abstract platform)
*
* WatchFile(file or dir path, subDirs = false, callback)
* CommandLine("path to MSBuild", "arguments", "working dir")
* CopyFile("existing file path", "new file path")
*/

#include <filesystem>
#include <string>
#include <vector>

#define NOMINMAX
#include <windows.h>

#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>

#include "Mirror/MIR_Mirror.h"

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

bool BuildPlugin(bool showOutput = false)
{
    std::wstring command =
        L"\"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe\" "
        L"Plugin\\Plugin.vcxproj "
        L"/p:Configuration=Debug "
        L"/p:Platform=x64 "
        L"/verbosity:minimal";

    STARTUPINFOW si = {};
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi = {};

    std::vector<wchar_t> cmd(command.begin(), command.end());

    cmd.push_back(L'\0');

    BOOL success = CreateProcessW(
        nullptr,
        cmd.data(),
        nullptr,
        nullptr,
        FALSE,
        showOutput ? 0 : CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi
    );

    if (!success)
    {
        std::cerr
            << "[ERROR] Failed to launch MSBuild: "
            << GetLastError()
            << std::endl;

        return false;
    }

    WaitForSingleObject(
        pi.hProcess,
        INFINITE);

    DWORD exitCode = 1;

    GetExitCodeProcess(
        pi.hProcess,
        &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    std::cout
        << "[INFO] MSBuild exited with code "
        << exitCode
        << std::endl;

    return exitCode == 0;
}

enum class BuildState
{
    Idle,
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

bool IsSourceFile(const std::filesystem::path& path)
{
    std::wstring ext = path.extension().wstring();

    return
        !ext.ends_with(L"~") &&
        !ext.ends_with(L".TMP") &&
        (
            ext == L".cpp" ||
            ext == L".h" ||
            ext == L".hpp" ||
            ext == L".c"
        );
}

std::filesystem::file_time_type
GetNewestSourceTimestamp(
    const std::filesystem::path& directory)
{
    std::filesystem::file_time_type newest =
        std::filesystem::file_time_type::min();

    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        if (!IsSourceFile(entry.path()))
        {
            continue;
        }

        auto writeTime = std::filesystem::last_write_time(entry.path());

        if (writeTime > newest)
        {
            newest = writeTime;
        }
    }

    return newest;
}

bool IsPluginBuildOutdated(
    const std::filesystem::path& dllPath)
{
    if (!std::filesystem::exists(dllPath))
    {
        return true;
    }

    auto dllWriteTime =
        std::filesystem::last_write_time(
            dllPath);

    auto newestPluginSource =
        GetNewestSourceTimestamp(
            L"Plugin");

    auto newestMirrorSource =
        GetNewestSourceTimestamp(
            L"Mirror");

    auto newestSource =
        std::max(
            newestPluginSource,
            newestMirrorSource);

    return newestSource > dllWriteTime;
}

enum class WatchType
{
    Source
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
    WatchContext* context = CONTAINING_RECORD(lpOverlapped, WatchContext, overlapped);

    BYTE* base = reinterpret_cast<BYTE*>( context->buffer);

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

            constexpr bool printOutAllChangedFilesOrDirs = false;
            if (printOutAllChangedFilesOrDirs)
            {
                std::wcout
                    << L"[INFO] Changed: "
                    << changedFile
                    << std::endl;
            }

            if (HasExtension(changedFile, L".cpp") ||
                HasExtension(changedFile, L".h"))
            {
                // #TODO Check what changed and ignore based on detected differences.
                // May need to cache file and diff, ignoring changes to:
                // - Whitespace, formatting, comments
                // - Non-serialized, non-member field(s) or function(s) (like scoped variables) name changes
                // - Default value changes
                if (constexpr bool criticalChangeDetected = true)
                {
                    sourceChanged = true;
                    lastSourceChangeTime = std::chrono::steady_clock::now();

                    if (!printOutAllChangedFilesOrDirs)
                    {
                        std::wcout
                            << L"[INFO] Changed: "
                            << changedFile
                            << std::endl;
                    }
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
        std::cerr << "[ERROR] Failed to re-issue watch: " << GetLastError() << std::endl;
        CloseHandle(context->dirHandle);
    }
}

bool SetupFileWatcher(WatchContext& context, const std::wstring& directory, WatchType type, bool watchSubDirs = true)
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

std::atomic<bool> g_buildWorkerThreadRunning = false;
void BuildWorkerThread(const std::filesystem::path& libFilePath, const std::filesystem::path& pdbPath)
{

    if (IsPluginBuildOutdated(libFilePath))
    {
        std::cout << "[INFO] Plugin build outdated. Queueing rebuild..." << std::endl;
        sourceChanged = true; // #TODO Don't rely on source change flag for startup build of stale library
    }
    else
    {
        std::cout << "[INFO] Existing plugin build found." << std::endl;

        g_readyDllPath = GenerateTempDllPath();

        g_readyPdbPath = GetPdbPath(g_readyDllPath);

        std::error_code ec;
        std::filesystem::copy_file(
            libFilePath,
            g_readyDllPath,
            std::filesystem::copy_options::overwrite_existing,
            ec
        );

        if (ec)
        {
            std::cerr << "[ERROR] Failed startup DLL copy: " << ec.message() << std::endl;
            return;
        }

        ec.clear();

        std::filesystem::copy_file(
            pdbPath,
            g_readyPdbPath,
            std::filesystem::copy_options::overwrite_existing,
            ec);

        if (ec)
        {
            std::cerr << "[ERROR] Failed startup PDB copy: " << ec.message() << std::endl;
            return;
        }

        g_buildState = BuildState::ReadyToLoad;
    }

    // #TODO Review condition for stopping thread
    while (g_buildWorkerThreadRunning)
    {
        constexpr long long pollingFrequencyMilliseconds = 10;
        constexpr long long minMillisecondsSinceChange = 100;

        if (!sourceChanged)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(pollingFrequencyMilliseconds));
            continue;
        }

        const std::chrono::time_point now = std::chrono::steady_clock::now();

        if (now - lastSourceChangeTime > std::chrono::milliseconds(minMillisecondsSinceChange))
        {
            sourceChanged = false;

            // #TODO review idle and failed states
            if (g_buildState == BuildState::Idle || g_buildState == BuildState::Failed)
            {
                g_buildState = BuildState::Building;

                // #TODO Timestamps measuring recompile, copy, and reload times
                std::cout << "[INFO] Rebuilding plugin..." << std::endl;

                if (!BuildPlugin())
                {
                    std::cerr << "[ERROR] Plugin build failed." << std::endl;
                    g_buildState = BuildState::Failed;
                    continue;
                }

                if (!WaitForFileReady(libFilePath))
                {
                    std::cerr << "[ERROR] DLL never became ready." << std::endl;
                    g_buildState = BuildState::Failed;
                    continue;
                }

                if (!WaitForFileStable(pdbPath))
                {
                    std::cerr
                        << "[ERROR] PDB never became stable."
                        << std::endl;

                    g_buildState = BuildState::Failed;
                    continue;
                }

                std::filesystem::path tempDll = GenerateTempDllPath();
                std::filesystem::path tempPdb = GetPdbPath(tempDll);

                std::error_code ec;

                if (!std::filesystem::copy_file(libFilePath, tempDll, std::filesystem::copy_options::overwrite_existing, ec))
                {
                    std::cerr << "[ERROR] Failed to copy DLL: " << ec.message() << std::endl;
                    g_buildState = BuildState::Failed;
                    continue;
                }
                ec.clear();

                if (!std::filesystem::copy_file(pdbPath, tempPdb, std::filesystem::copy_options::overwrite_existing, ec))
                {
                    std::cerr << "[ERROR] Failed to copy PDB: " << ec.message() << std::endl;
                    g_buildState = BuildState::Failed;
                    continue;
                }

                {
                    std::scoped_lock lock(g_readyDllMutex);
                    g_readyDllPath = tempDll;
                    g_readyPdbPath = tempPdb;
                }

                std::cout << "[INFO] Plugin build succeeded." << std::endl;
                g_buildState = BuildState::ReadyToLoad;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(pollingFrequencyMilliseconds));
    }
}

// #define HOTRELOADAPP_EXPORTS
#include "HotReloadApp.h"

std::vector<Entity> entities;
void AddEntity()
{
    entities.emplace_back();
}

int EntityCount()
{
    return (int)entities.size();
}

Entity* GetEntity(int index)
{
    if (index < 0 || index >= entities.size())
    {
        return nullptr;
    }

    return &entities[index];
}

void PrintReflectionInfo(const std::vector<const Mir::TypeInfo*>& structTypeInfos)
{
    for (size_t i = 0; i < structTypeInfos.size(); i++)
    {
        const Mir::TypeInfo* typeInfo = structTypeInfos[i];
        std::cout
            << "[INFO] "
            << typeInfo->stringName.c_str()
            << ": "
            << typeInfo->size
            << " bytes\n"
            << std::endl;

        for (size_t i = 0; i < typeInfo->fields.size(); i++)
        {
            std::cout
                << typeInfo->fields[i].name
                << " "
                << typeInfo->fields[i].typeInfo->size
                << " bytes"
                << std::endl;
        }

        std::cout << std::endl;
    }
}

void PrintEntityState()
{
    std::cout
        << "[INFO] Entity Count: "
        << EntityCount()
        << std::endl;

    for (int i = 0; i < EntityCount(); i++)
    {
        Entity* entity = GetEntity(i);

        std::cout
            << "Entity "
            << i
            << " | Health: "
            << entity->health
            << " | Speed: "
            << entity->speed
            << " | "
            << (entity->alive ? "\033[32mAlive\033[0m" : "\033[31mDead\033[0m")
            << std::endl;
    }
    std::cout << std::endl;
}

HMODULE hLib = nullptr;

using void_func = void(*)();

void_func OnFirstLoaded = nullptr;
void_func OnPreUnload = nullptr;
void_func OnReloaded = nullptr;

void_func run = nullptr;

using classInfo_func = std::vector<const Mirror::TypeInfo*>(*)();
classInfo_func getStructTypeInfos = nullptr;

std::atomic<bool> hasLoaded = false;

void TryReload()
{
    if (g_buildState != BuildState::ReadyToLoad || reloadInProgress)
        return;

    reloadInProgress = true;

    bool reloadSucceeded = false;

    do
    {
        if (hLib)
        {
            std::cout << "[INFO] Reloading plugin..." << std::endl;

            OnFirstLoaded = nullptr;
            OnPreUnload = nullptr;
            OnReloaded = nullptr;

            getStructTypeInfos = nullptr;

            if (OnPreUnload && hasLoaded)
            {
                OnPreUnload();
            }
            FreeLibrary(hLib);

            hLib = nullptr;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            if (!currentLoadedDllPath.empty())
            {
                std::error_code ec;

                std::filesystem::remove(currentLoadedDllPath, ec);

                auto tempPdb = GetPdbPath(currentLoadedDllPath);

                std::filesystem::remove(tempPdb, ec);
            }
        }

        std::filesystem::path readyDll;
        {
            std::scoped_lock lock(g_readyDllMutex);
            readyDll = g_readyDllPath;
        }

        if (readyDll.empty())
        {
            std::cerr
                << "[ERROR] No DLL ready to load."
                << std::endl;

            break;
        }

        hLib = LoadLibraryW(readyDll.c_str());
        if (!hLib)
        {
            std::cerr
                << "[ERROR] Failed to load DLL: "
                << GetLastError()
                << std::endl;

            break;
        }

        currentLoadedDllPath = readyDll;

        OnFirstLoaded = (void_func)GetProcAddress(hLib, "OnFirstLoaded");
        OnPreUnload = (void_func)GetProcAddress(hLib, "OnPreUnload");
        OnReloaded = (void_func)GetProcAddress(hLib, "OnReloaded");

        if (!OnFirstLoaded || !OnPreUnload || !OnReloaded)
        {
            std::cerr << "[ERROR] Missing export function(s): OnFirstLoaded || OnPreUnload || OnReloaded" << std::endl;

            FreeLibrary(hLib);

            hLib = nullptr;

            break;
        }

        getStructTypeInfos = (classInfo_func)GetProcAddress(hLib, "StructTypeInfos");
        if (!getStructTypeInfos)
        {
            std::cerr << "[ERROR] Missing export: StructTypeInfos" << std::endl;

            FreeLibrary(hLib);

            hLib = nullptr;

            break;
        }

        reloadSucceeded = true;

        if (OnFirstLoaded && hasLoaded)
        {
            OnFirstLoaded();
        }
        else if (OnReloaded)
        {
            OnReloaded();
        }
        hasLoaded = true;

    } while (false); // Supports early-out via break, so could just early return

    reloadInProgress = false;

    if (reloadSucceeded)
    {
        g_buildState = BuildState::Idle;

        std::cout << "[INFO] Plugin reload succeeded." << std::endl;

        if (getStructTypeInfos)
        {
            PrintReflectionInfo(getStructTypeInfos());
        }

        PrintEntityState();
    }
}

void Loop()
{

}

int main()
{
    // Clean up old files in temp storage directory
    std::filesystem::directory_iterator tempStorageDir = std::filesystem::directory_iterator(std::filesystem::temp_directory_path());
    for (const std::filesystem::directory_entry& entry : tempStorageDir)
    {
        const std::wstring name = entry.path().filename().wstring();

        if (name.starts_with(L"plugin_hotreload_"))
        {
            std::error_code ec;
            std::filesystem::remove(entry.path(), ec);
            std::filesystem::path tempPdb = GetPdbPath(entry.path());
            std::filesystem::remove(tempPdb, ec);
        }
    }

    const std::filesystem::path libFilePath = L"Plugin\\x64\\Debug\\Plugin.dll";
    const std::filesystem::path pdbPath = L"Plugin\\x64\\Debug\\Plugin.pdb";

    g_buildWorkerThreadRunning = true;
    std::thread buildThread(BuildWorkerThread, libFilePath, pdbPath);

    WatchContext pluginWatchContext; // #TODO Look at how these can be stopped early if needed
    WatchContext mirrorWatchContext;

    SetupFileWatcher(
        pluginWatchContext,
        L"Plugin",
        WatchType::Source,
        false);

    SetupFileWatcher(
        mirrorWatchContext,
        L"Mirror",
        WatchType::Source,
        false);

    std::cout << "[INFO] Monitoring source changes..." << std::endl;

    while (true)
    {
        SleepEx(10, TRUE);

        if (g_buildState == BuildState::ReadyToLoad && !reloadInProgress)
        {
            TryReload();
        }

        Loop();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    g_buildWorkerThreadRunning = false;
    buildThread.join();

    entities.clear();

    return 0;
}