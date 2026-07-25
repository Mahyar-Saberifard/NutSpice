#include "Platform.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <climits>
#elif defined(__linux__)
#include <unistd.h>
#include <climits>
#endif

namespace fs = std::filesystem;

namespace nutspice
{

    namespace
    {

        std::string toLower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        bool hasSuffixIgnoreCase(const std::string &s, const std::string &suffix)
        {
            if (s.size() < suffix.size())
                return false;
            return toLower(s.substr(s.size() - suffix.size())) == toLower(suffix);
        }

        std::string normalizeExtension(std::string ext)
        {
            if (!ext.empty() && ext[0] != '.')
                ext = "." + ext;
            return toLower(ext);
        }

    } // namespace

    std::vector<std::string> listFiles(const std::string &directory,
                                       const std::vector<std::string> &extensions)
    {
        std::vector<std::string> result;
        std::error_code ec;

        if (!fs::exists(directory, ec) || !fs::is_directory(directory, ec))
        {
            return result;
        }

        std::vector<std::string> normalizedExt;
        normalizedExt.reserve(extensions.size());
        for (const auto &e : extensions)
        {
            normalizedExt.push_back(normalizeExtension(e));
        }

        for (const auto &entry : fs::directory_iterator(directory, ec))
        {
            if (ec)
                break;
            if (!entry.is_regular_file())
                continue;

            const std::string name = entry.path().filename().string();
            if (normalizedExt.empty())
            {
                result.push_back(name);
                continue;
            }
            for (const auto &ext : normalizedExt)
            {
                if (hasSuffixIgnoreCase(name, ext))
                {
                    result.push_back(name);
                    break;
                }
            }
        }

        std::sort(result.begin(), result.end());
        return result;
    }

    std::string absolutePath(const std::string &path)
    {
        std::error_code ec;
        fs::path resolved = fs::weakly_canonical(path, ec);
        if (ec)
            return path;
        return resolved.string();
    }

    std::string currentDirectory()
    {
        std::error_code ec;
        fs::path cwd = fs::current_path(ec);
        if (ec)
            return std::string();
        return cwd.string();
    }

    std::string locateFont(const std::vector<std::string> &candidates)
    {
        // 1. Caller-supplied candidates first.
        for (const auto &c : candidates)
        {
            if (!c.empty())
            {
                std::error_code ec;
                if (fs::exists(c, ec) && fs::is_regular_file(c, ec))
                    return c;
            }
        }

        // 2. Platform-specific well-known locations.
        std::vector<std::string> guesses;
#if defined(_WIN32)
        const char *sysRoot = std::getenv("SystemRoot");
        const std::string winDir = sysRoot ? sysRoot : "C:\\Windows";
        guesses.push_back(winDir + "\\Fonts\\consola.ttf");
        guesses.push_back(winDir + "\\Fonts\\arial.ttf");
        guesses.push_back(winDir + "\\Fonts\\segoeui.ttf");
#elif defined(__APPLE__)
        guesses.push_back("/Library/Fonts/Arial.ttf");
        guesses.push_back("/System/Library/Fonts/Helvetica.ttc");
        guesses.push_back("/System/Library/Fonts/Menlo.ttc");
        guesses.push_back("/System/Library/Fonts/SFNSMono.ttf");
#else
        guesses.push_back("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf");
        guesses.push_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
        guesses.push_back("/usr/share/fonts/TTF/DejaVuSansMono.ttf");
        guesses.push_back("/usr/share/fonts/liberation/LiberationMono-Regular.ttf");
        guesses.push_back("/usr/share/fonts/liberation/LiberationSans-Regular.ttf");
#endif

        for (const auto &g : guesses)
        {
            std::error_code ec;
            if (fs::exists(g, ec) && fs::is_regular_file(g, ec))
                return g;
        }
        return {};
    }

    std::string locateAsset(const std::string &filename)
    {
        if (filename.empty())
            return {};

        std::vector<fs::path> searchDirs;
        searchDirs.emplace_back(currentDirectory());
        searchDirs.emplace_back(executableDirectory());

        for (const auto &dir : searchDirs)
        {
            fs::path candidate = dir / filename;
            std::error_code ec;
            if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec))
            {
                return candidate.string();
            }
        }
        return {};
    }

    std::string executableDirectory()
    {
#if defined(_WIN32)
        wchar_t buffer[MAX_PATH] = {0};
        if (GetModuleFileNameW(nullptr, buffer, MAX_PATH) == 0)
            return {};
        fs::path exePath(buffer);
#elif defined(__APPLE__)
        char buffer[PATH_MAX] = {0};
        uint32_t size = sizeof(buffer);
        if (_NSGetExecutablePath(buffer, &size) != 0)
            return {};
        fs::path exePath(buffer);
#elif defined(__linux__)
        char buffer[PATH_MAX] = {0};
        ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
        if (len <= 0)
            return {};
        buffer[len] = '\0';
        fs::path exePath(buffer);
#else
        return {};
#endif

        std::error_code ec;
        return fs::weakly_canonical(exePath.parent_path(), ec).string();
    }

} // namespace nutspice
