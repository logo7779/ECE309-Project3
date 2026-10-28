// src/tools/workspace.cpp — lexical workspace containment check.
#include "tools/workspace.h"

namespace fs = std::filesystem;

bool Workspace::resolve(std::string_view user_path, fs::path* out,
                        std::string* reason) const {
    if (user_path.empty()) {
        *reason = "empty path"; //no path exists
        return false;
    }
    if (user_path.find('\0') != std::string_view::npos) { // check for NULL in path data, prevent ambiguity whe ncalling OS functions
        *reason = "path contains a NUL byte";
        return false;
    }

    try {
        const fs::path p{std::string(user_path)};
        if (p.is_absolute() || p.has_root_path()) {
            *reason = "absolute paths are not allowed";
            return false;
        }

        const fs::path normalized = p.lexically_normal(); // simplify path without accessing file system
        if (normalized.begin() != normalized.end() && *normalized.begin() == "..") { // see if path tries to leave workspace root
            *reason = "path escapes the workspace"; // it also prevents climbing out and then back into root workspace, path changes must stay inside workspace
            return false;
        }

        *out = normalized == "." ? root() : root() / normalized; //reconstruct path relative to the worksace, or on . return the root workspace path
        reason->clear();
        return true;
    } catch (...) { // if any exceptions are thrown anyway, provides a reason for exception
        *reason = "invalid path";
        return false;
    }
}
