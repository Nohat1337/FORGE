#include "file_explorer.hpp"
#include "terminal.hpp"
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <climits>

FileExplorer::FileExplorer() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        rootPath_ = cwd;
    }
}

FileExplorer::~FileExplorer() {}

void FileExplorer::setRoot(const std::string& path) {
    rootPath_ = path;
    refresh();
}

void FileExplorer::refresh() {
    entries_.clear();
    selectedIdx_ = 0;
    scrollOffset_ = 0;

    FileEntry root;
    root.name = rootPath_;
    root.fullPath = rootPath_;
    root.isDir = true;
    root.isExpanded = true;
    root.depth = 0;
    entries_.push_back(root);

    buildTree(rootPath_, 1);
}

void FileExplorer::buildTree(const std::string& path, int depth) {
    DIR* dir = opendir(path.c_str());
    if (!dir) return;

    struct dirent* entry;
    std::vector<std::pair<std::string, bool>> items;

    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        if (name[0] == '.') continue; // skip hidden files

        std::string fullPath = path + "/" + name;
        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) continue;

        bool isDir = S_ISDIR(st.st_mode);
        items.push_back({name, isDir});
    }
    closedir(dir);

    // Sort: directories first, then alphabetical
    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });

    for (auto& [name, isDir] : items) {
        std::string fullPath = path + "/" + name;
        FileEntry fe;
        fe.name = name;
        fe.fullPath = fullPath;
        fe.isDir = isDir;
        fe.isExpanded = false;
        fe.depth = depth;
        entries_.push_back(fe);
    }
}

bool FileExplorer::isForgeFile(const std::string& name) const {
    if (name.size() < 4) return false;
    return name.substr(name.size() - 4) == ".fge";
}

std::string FileExplorer::truncate(const std::string& s, int maxLen) const {
    if ((int)s.size() <= maxLen) return s;
    return s.substr(0, maxLen - 1) + "~";
}

int FileExplorer::render(int startRow, int startCol, int height, int width, int selectedIdx) {
    auto& term = Terminal::instance();
    selectedIdx_ = selectedIdx;

    std::string output;
    output.reserve(height * width * 2);

    // Title bar
    output += ansi::move(startRow, startCol);
    output += ansi::BG_PANEL + ansi::FG_BOLD_WHITE + ansi::BOLD;
    std::string title = " EXPLORER ";
    int pad = width - (int)title.size();
    output += title;
    for (int i = 0; i < pad; i++) output += ' ';
    output += ansi::RESET;

    // File entries
    int visibleHeight = height - 1;
    int contentWidth = width - 4; // leave room for indent and icons

    if (selectedIdx_ < scrollOffset_) scrollOffset_ = selectedIdx_;
    if (selectedIdx_ >= scrollOffset_ + visibleHeight) scrollOffset_ = selectedIdx_ - visibleHeight + 1;

    for (int i = 0; i < visibleHeight; i++) {
        int entryIdx = i + scrollOffset_;
        output += ansi::move(startRow + 1 + i, startCol);
        output += ansi::BG_PANEL;

        if (entryIdx < (int)entries_.size()) {
            const FileEntry& e = entries_[entryIdx];
            bool selected = (entryIdx == selectedIdx_);

            if (selected) {
                output += ansi::BG_BLUE + ansi::FG_WHITE;
            }

            // Indentation
            int indent = e.depth * 2;
            for (int j = 0; j < indent && j + 2 < contentWidth; j++) output += ' ';

            // Tree connector
            if (e.depth > 0) {
                output += "+-";
            }

            // Icon
            if (e.isDir) {
                if (e.isExpanded) output += ansi::FG_YELLOW + std::string("[-] ") + ansi::RESET;
                else output += ansi::FG_YELLOW + std::string("[+] ") + ansi::RESET;
            } else {
                if (isForgeFile(e.name)) {
                    output += ansi::FG_BOLD_CYAN + std::string("* ") + ansi::RESET;
                } else {
                    output += "  ";
                }
            }

            // Name
            int nameMaxWidth = contentWidth - indent - 4;
            if (nameMaxWidth < 4) nameMaxWidth = 4;
            std::string displayName = truncate(e.name, nameMaxWidth);

            if (selected) output += ansi::BG_BLUE + ansi::FG_WHITE;
            else if (e.isDir) output += ansi::FG_BOLD_WHITE;
            else if (isForgeFile(e.name)) output += ansi::FG_BOLD_CYAN;
            else output += ansi::FG_WHITE;

            output += displayName;
            output += ansi::RESET + ansi::BG_PANEL;

            // Fill rest of line
            int used = indent + (e.depth > 0 ? 2 : 0) + 2 + (int)displayName.size();
            for (int j = used; j < contentWidth; j++) output += ' ';
        } else {
            for (int j = 0; j < contentWidth; j++) output += ' ';
        }
    }

    term.write(output);
    return scrollOffset_;
}

int FileExplorer::getEntryCount() const {
    return (int)entries_.size();
}

const FileEntry& FileExplorer::getEntry(int idx) const {
    return entries_[idx];
}

std::string FileExplorer::getSelectedPath() const {
    if (selectedIdx_ >= 0 && selectedIdx_ < (int)entries_.size()) {
        return entries_[selectedIdx_].fullPath;
    }
    return "";
}

void FileExplorer::moveUp() {
    if (selectedIdx_ > 0) selectedIdx_--;
}

void FileExplorer::moveDown() {
    if (selectedIdx_ < (int)entries_.size() - 1) selectedIdx_++;
}

void FileExplorer::toggleExpand() {
    if (selectedIdx_ < 0 || selectedIdx_ >= (int)entries_.size()) return;
    FileEntry& e = entries_[selectedIdx_];
    if (!e.isDir) return;

    if (e.isExpanded) {
        // Collapse: remove all children
        e.isExpanded = false;
        std::string prefix = e.fullPath + "/";
        auto it = entries_.begin() + selectedIdx_ + 1;
        while (it != entries_.end()) {
            if (it->fullPath.substr(0, prefix.size()) == prefix) {
                it = entries_.erase(it);
            } else {
                break;
            }
        }
    } else {
        // Expand: add children
        e.isExpanded = true;
        int insertPos = selectedIdx_ + 1;
        int depth = e.depth + 1;

        DIR* dir = opendir(e.fullPath.c_str());
        if (!dir) return;

        struct dirent* entry;
        std::vector<std::pair<std::string, bool>> items;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name == "." || name == ".." || name[0] == '.') continue;
            std::string fp = e.fullPath + "/" + name;
            struct stat st;
            if (stat(fp.c_str(), &st) != 0) continue;
            items.push_back({name, S_ISDIR(st.st_mode)});
        }
        closedir(dir);

        std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
            if (a.second != b.second) return a.second > b.second;
            return a.first < b.first;
        });

        for (auto& [name, isDir] : items) {
            FileEntry fe;
            fe.name = name;
            fe.fullPath = e.fullPath + "/" + name;
            fe.isDir = isDir;
            fe.isExpanded = false;
            fe.depth = depth;
            entries_.insert(entries_.begin() + insertPos, fe);
            insertPos++;
        }
    }
}
