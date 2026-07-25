#ifndef NUTSPICE_PLATFORM_H
#define NUTSPICE_PLATFORM_H

#include <string>
#include <vector>

namespace nutspice
{

    // Returns a list of regular files in `directory` whose extension matches any
    // of `extensions` (case-insensitive, with or without leading dot).
    // e.g.  listFiles(".", {".txt", ".cir"});
    std::vector<std::string> listFiles(const std::string &directory,
                                       const std::vector<std::string> &extensions);

    // Returns the absolute, canonical form of `path` (best-effort, falls back to
    // the original path on failure).
    std::string absolutePath(const std::string &path);

    // Returns the current working directory (empty string on failure).
    std::string currentDirectory();

    // Locates a readable font file.  Tries the provided candidate list, then a
    // small set of well-known locations on each platform.  Returns empty string
    // if nothing readable was found.
    std::string locateFont(const std::vector<std::string> &candidates = {});

    // Locates an icon file shipped alongside the executable or in the working
    // directory.  Returns empty string if not found.
    std::string locateAsset(const std::string &filename);

    // Returns the directory that contains the running executable.  Used to look
    // up bundled assets (icon, default font, etc.).
    std::string executableDirectory();

} // namespace nutspice

#endif // NUTSPICE_PLATFORM_H
