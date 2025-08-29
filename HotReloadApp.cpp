#include <windows.h>
#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>

using run_func = void(*)();

int main() {

    // const std::string libFileAbsolutePath = "B:\\HotReloadApp\\x64\\Debug\\Plugin.dll";
    // std::filesystem::path currentPath= std::filesystem::current_path();

    const std::string libFileRelativeDirectory = "x64\\Debug";
    const std::string libFileName = "Plugin.dll";
    const std::string tempLibFileName = "plugin_temp.dll";

    std::filesystem::path libFilePath = libFileRelativeDirectory;
    libFilePath /= libFileName;

    std::filesystem::path tempLibFilePath = libFileRelativeDirectory;
    tempLibFilePath /= tempLibFileName;

    HMODULE hLib = nullptr;
    run_func run = nullptr;
    std::filesystem::file_time_type lastWriteTime;

    while (std::filesystem::exists(libFilePath)) {
        auto currentWriteTime = std::filesystem::last_write_time(libFilePath);

        if (!hLib || currentWriteTime != lastWriteTime) {
            if (hLib) {
                std::cout << "Reloading plugin..." << std::endl;
                FreeLibrary(hLib);
                hLib = nullptr;
            }

            // Copy plugin.dll to plugin_temp.dll to avoid locking the original file
            std::filesystem::copy_file(libFilePath, tempLibFilePath, std::filesystem::copy_options::overwrite_existing);

            hLib = LoadLibraryA(tempLibFilePath.string().c_str());
            if (!hLib) {
                std::cerr << "Failed to load plugin DLL: " << GetLastError() << std::endl;
                return 1;
            }

            run = (run_func)GetProcAddress(hLib, "run");
            if (!run) {
                std::cerr << "Failed to locate symbol 'run'" << std::endl;
                return 1;
            }

            lastWriteTime = currentWriteTime;
        }

        if (run) {
            run();
        }

        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    return 0;
}
