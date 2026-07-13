#pragma once

#include <string>
#include <vector>
#include <functional>

struct FileEntry {
    std::string name;
    std::string fullPath;
    bool isDir;
    bool isExpanded;
    int depth;
};

class FileExplorer {
public:
    FileExplorer();
    ~FileExplorer();

    void setRoot(const std::string& path);
    void refresh();
    int render(int startRow, int startCol, int height, int width, int selectedIdx);
    int getEntryCount() const;
    const FileEntry& getEntry(int idx) const;
    std::string getSelectedPath() const;

    void moveUp();
    void moveDown();
    void toggleExpand();

    int getSelectedIndex() const { return selectedIdx_; }

private:
    std::string rootPath_;
    std::vector<FileEntry> entries_;
    int selectedIdx_ = 0;
    int scrollOffset_ = 0;

    void buildTree(const std::string& path, int depth);
    std::string truncate(const std::string& s, int maxLen) const;
    bool isForgeFile(const std::string& name) const;
};
