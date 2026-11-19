#include "log/views/log_browser_view.hpp"

#include "settings/settings_manager.hpp"
#include "app/views/theme.hpp"

#include <borealis/core/i18n.hpp>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace akira::log::views {

using namespace brls::literals;

namespace {

constexpr size_t kMaxLogBytes = 256 * 1024;
constexpr size_t kMaxLogLines = 2000;

struct LogEntry {
    std::string name;
    std::string path;
    bool directory = false;
    off_t size = 0;
    time_t modified = 0;
};

std::string joinPath(const std::string& base, const std::string& name)
{
    return base.empty() || base.back() == '/' ? base + name : base + "/" + name;
}

std::string baseName(const std::string& path)
{
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string formatSize(off_t bytes)
{
    if (bytes < 1024)
        return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024)
        return std::to_string((bytes + 512) / 1024) + " KB";

    char value[32];
    std::snprintf(value, sizeof(value), "%.1f MB",
                  static_cast<double>(bytes) / (1024.0 * 1024.0));
    return value;
}

std::string formatModified(time_t modified)
{
    if (modified <= 0)
        return {};

    struct tm local {};
    if (!localtime_r(&modified, &local))
        return {};

    char value[32];
    if (std::strftime(value, sizeof(value), "%d %b %Y  %H:%M", &local) == 0)
        return {};
    return value;
}

std::string sanitizeLogText(const std::string& input)
{
    std::string output;
    output.reserve(input.size());

    for (size_t i = 0; i < input.size(); i++) {
        const unsigned char c = static_cast<unsigned char>(input[i]);

        // File logs may contain terminal colour sequences. They render as noisy
        // glyphs in Borealis, so remove complete ANSI CSI sequences.
        if (c == 0x1b && i + 1 < input.size() && input[i + 1] == '[') {
            i += 2;
            while (i < input.size()) {
                const unsigned char part = static_cast<unsigned char>(input[i]);
                if (part >= 0x40 && part <= 0x7e)
                    break;
                i++;
            }
            continue;
        }

        if (c == '\r')
            continue;
        if (c == '\n' || c == '\t' || c >= 0x20)
            output.push_back(static_cast<char>(c));
    }

    return output;
}

struct LogContents {
    std::string text;
    off_t totalBytes = 0;
    size_t shownLines = 0;
    bool truncated = false;
    bool ok = false;
};

enum class LogLevel {
    Info,
    Warning,
    Error,
    Debug,
    Verbose,
};

struct LogBlock {
    LogLevel level = LogLevel::Info;
    std::string text;
};

bool findLevel(const std::string& line, LogLevel* level)
{
    if (line.find("[ERROR]") != std::string::npos ||
        line.find("[FATAL]") != std::string::npos) {
        *level = LogLevel::Error;
        return true;
    }
    if (line.find("[WARN]") != std::string::npos ||
        line.find("[WARNING]") != std::string::npos) {
        *level = LogLevel::Warning;
        return true;
    }
    if (line.find("[DEBUG]") != std::string::npos) {
        *level = LogLevel::Debug;
        return true;
    }
    if (line.find("[TRACE]") != std::string::npos ||
        line.find("[VERBOSE]") != std::string::npos) {
        *level = LogLevel::Verbose;
        return true;
    }
    if (line.find("[INFO]") != std::string::npos) {
        *level = LogLevel::Info;
        return true;
    }
    return false;
}

std::vector<LogBlock> colorizeLog(const std::string& text)
{
    std::vector<LogBlock> blocks;
    LogLevel currentLevel = LogLevel::Info;
    size_t offset = 0;

    while (offset < text.size()) {
        const size_t newline = text.find('\n', offset);
        const size_t length = newline == std::string::npos
                                ? text.size() - offset
                                : newline - offset + 1;
        const std::string line = text.substr(offset, length);

        // Stack traces and wrapped messages inherit the preceding line's level.
        findLevel(line, &currentLevel);
        if (blocks.empty() || blocks.back().level != currentLevel)
            blocks.push_back({currentLevel, line});
        else
            blocks.back().text += line;

        offset += length;
    }
    return blocks;
}

NVGcolor colorForLevel(LogLevel level)
{
    const auto& palette = app::views::active();
    switch (level) {
        case LogLevel::Error: return palette.danger;
        case LogLevel::Warning: return palette.warning;
        case LogLevel::Debug: return palette.success;
        case LogLevel::Verbose: return palette.textDim;
        case LogLevel::Info: return palette.accent;
    }
    return palette.text;
}

LogContents readLogTail(const std::string& path)
{
    LogContents result;
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file)
        return result;

    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return result;
    }

    const long end = std::ftell(file);
    if (end < 0) {
        std::fclose(file);
        return result;
    }
    result.totalBytes = static_cast<off_t>(end);

    long start = std::max<long>(0, end - static_cast<long>(kMaxLogBytes));
    result.truncated = start > 0;
    if (std::fseek(file, start, SEEK_SET) != 0) {
        std::fclose(file);
        return result;
    }

    std::string raw(static_cast<size_t>(end - start), '\0');
    const size_t read = raw.empty() ? 0 : std::fread(raw.data(), 1, raw.size(), file);
    std::fclose(file);
    raw.resize(read);

    // If we started in the middle of a line, discard that partial line.
    if (start > 0) {
        const size_t newline = raw.find('\n');
        if (newline != std::string::npos)
            raw.erase(0, newline + 1);
    }

    std::string clean = sanitizeLogText(raw);
    size_t lines = clean.empty() ? 0 : 1;
    for (char c : clean)
        if (c == '\n')
            lines++;

    if (lines > kMaxLogLines) {
        size_t keepFrom = clean.size();
        size_t found = 0;
        while (keepFrom > 0 && found < kMaxLogLines) {
            keepFrom--;
            if (clean[keepFrom] == '\n')
                found++;
        }
        if (keepFrom < clean.size())
            clean.erase(0, keepFrom + 1);
        lines = kMaxLogLines;
        result.truncated = true;
    }

    result.text = std::move(clean);
    result.shownLines = lines;
    result.ok = true;
    return result;
}

class LogFileView final : public brls::Box {
public:
    explicit LogFileView(std::string path)
        : path(std::move(path))
    {
        setAxis(brls::Axis::COLUMN);
        setWidth(brls::View::AUTO);
        setHeight(brls::View::AUTO);
        setGrow(1.0f);
        setPadding(22.0f, 40.0f, 22.0f, 40.0f);
        setBackgroundColor(brls::Application::getTheme()["brls/background"]);

        auto* header = new brls::Box(brls::Axis::ROW);
        header->setWidth(brls::View::AUTO);
        header->setHeight(brls::View::AUTO);
        header->setAlignItems(brls::AlignItems::CENTER);
        header->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);
        header->setMarginBottom(4.0f);

        auto* title = new brls::Label();
        title->setText(baseName(this->path));
        title->setFontSize(25.0f);
        title->setHorizontalAlign(brls::HorizontalAlign::LEFT);
        header->addView(title);

        statusLabel = new brls::Label();
        statusLabel->setFontSize(15.0f);
        statusLabel->setTextColor(app::views::active().textMuted);
        statusLabel->setHorizontalAlign(brls::HorizontalAlign::RIGHT);
        header->addView(statusLabel);
        addView(header);

        pathLabel = new brls::Label();
        pathLabel->setText(this->path);
        pathLabel->setFontSize(14.0f);
        pathLabel->setTextColor(app::views::active().textMuted);
        pathLabel->setHorizontalAlign(brls::HorizontalAlign::LEFT);
        pathLabel->setMarginBottom(12.0f);
        addView(pathLabel);

        scroll = new brls::ScrollingFrame();
        scroll->setWidth(brls::View::AUTO);
        scroll->setHeight(brls::View::AUTO);
        scroll->setGrow(1.0f);

        logContainer = new brls::Box(brls::Axis::COLUMN);
        logContainer->setWidth(brls::View::AUTO);
        logContainer->setHeight(brls::View::AUTO);
        logContainer->setPadding(16.0f, 18.0f, 16.0f, 18.0f);
        logContainer->setCornerRadius(12.0f);
        logContainer->setBackgroundColor(brls::Application::getTheme()["color/card"]);

        scroll->setContentView(logContainer);
        addView(scroll);

        auto* footer = new brls::Box(brls::Axis::ROW);
        footer->setWidth(brls::View::AUTO);
        footer->setHeight(brls::View::AUTO);
        footer->setJustifyContent(brls::JustifyContent::CENTER);
        footer->setMarginTop(14.0f);

        auto* refreshButton = new brls::Button();
        refreshButton->setText("akira/settings/log_refresh"_i18n);
        refreshButton->setStyle(&brls::BUTTONSTYLE_BORDERED);
        refreshButton->setMarginRight(16.0f);
        refreshButton->registerClickAction([this](brls::View*) {
            refresh(true);
            return true;
        });
        footer->addView(refreshButton);

        auto* latestButton = new brls::Button();
        latestButton->setText("akira/settings/log_latest"_i18n);
        latestButton->setStyle(&brls::BUTTONSTYLE_BORDERED);
        latestButton->setMarginRight(16.0f);
        latestButton->registerClickAction([this](brls::View*) {
            needsScrollToBottom = true;
            return true;
        });
        footer->addView(latestButton);

        auto* closeButton = new brls::Button();
        closeButton->setText("akira/common/close"_i18n);
        closeButton->setStyle(&brls::BUTTONSTYLE_BORDERED);
        closeButton->registerClickAction([](brls::View*) {
            brls::Application::popActivity();
            return true;
        });
        footer->addView(closeButton);
        addView(footer);

        registerAction("akira/settings/log_refresh"_i18n,
                       brls::ControllerButton::BUTTON_X,
                       [this](brls::View*) {
                           refresh(true);
                           return true;
                       }, true);
        registerAction("akira/common/back"_i18n,
                       brls::ControllerButton::BUTTON_B,
                       [](brls::View*) {
                           brls::Application::popActivity();
                           return true;
                       }, true);

        refresh(true);
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override
    {
        Box::draw(vg, x, y, width, height, style, ctx);
        if (!needsScrollToBottom || !scroll || !logContainer)
            return;

        const float contentHeight = logContainer->getHeight();
        const float frameHeight = scroll->getHeight();
        if (contentHeight > frameHeight)
            scroll->setContentOffsetY(-(contentHeight - frameHeight), false);
        needsScrollToBottom = false;
    }

private:
    void refresh(bool scrollToLatest)
    {
        const LogContents contents = readLogTail(path);
        logContainer->clearViews();
        if (!contents.ok) {
            auto* error = new brls::Label();
            error->setWidth(brls::View::AUTO);
            error->setHeight(brls::View::AUTO);
            error->setFontSize(15.0f);
            error->setTextColor(app::views::active().danger);
            error->setHorizontalAlign(brls::HorizontalAlign::LEFT);
            error->setText("akira/settings/log_read_error"_i18n);
            logContainer->addView(error);
            statusLabel->setText(std::strerror(errno));
            return;
        }

        if (contents.text.empty()) {
            auto* empty = new brls::Label();
            empty->setWidth(brls::View::AUTO);
            empty->setHeight(brls::View::AUTO);
            empty->setFontSize(15.0f);
            empty->setTextColor(app::views::active().textMuted);
            empty->setHorizontalAlign(brls::HorizontalAlign::LEFT);
            empty->setText("akira/settings/log_file_empty"_i18n);
            logContainer->addView(empty);
        } else {
            for (const LogBlock& block : colorizeLog(contents.text)) {
                auto* label = new brls::Label();
                label->setWidth(brls::View::AUTO);
                label->setHeight(brls::View::AUTO);
                label->setFontSize(15.0f);
                label->setTextColor(colorForLevel(block.level));
                label->setHorizontalAlign(brls::HorizontalAlign::LEFT);
                label->setText(block.text);
                logContainer->addView(label);
            }
        }

        std::string status = formatSize(contents.totalBytes) + "  ·  "
                           + std::to_string(contents.shownLines) + " "
                           + "akira/settings/log_lines"_i18n;
        if (contents.truncated)
            status += "  ·  " + "akira/settings/log_tail_notice"_i18n;
        statusLabel->setText(status);
        needsScrollToBottom = scrollToLatest;
    }

    std::string path;
    brls::Label* statusLabel = nullptr;
    brls::Label* pathLabel = nullptr;
    brls::ScrollingFrame* scroll = nullptr;
    brls::Box* logContainer = nullptr;
    bool needsScrollToBottom = false;
};

} // namespace

LogBrowserView::LogBrowserView()
    : LogBrowserView(settings::SettingsManager::LOG_DIR, settings::SettingsManager::LOG_DIR)
{
}

LogBrowserView::LogBrowserView(std::string rootPath, std::string currentPath)
    : rootPath(std::move(rootPath)), currentPath(std::move(currentPath))
{
    buildLayout();
    refreshEntries();
}

void LogBrowserView::buildLayout()
{
    setAxis(brls::Axis::COLUMN);
    setWidth(brls::View::AUTO);
    setHeight(brls::View::AUTO);
    setGrow(1.0f);
    setPadding(24.0f, 40.0f, 24.0f, 40.0f);
    setBackgroundColor(brls::Application::getTheme()["brls/background"]);

    auto* header = new brls::Box(brls::Axis::ROW);
    header->setWidth(brls::View::AUTO);
    header->setHeight(brls::View::AUTO);
    header->setAlignItems(brls::AlignItems::CENTER);
    header->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);

    auto* title = new brls::Label();
    title->setText(currentPath == rootPath
                       ? "akira/settings/log_browser_title"_i18n
                       : baseName(currentPath));
    title->setFontSize(28.0f);
    title->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    header->addView(title);

    statusLabel = new brls::Label();
    statusLabel->setFontSize(15.0f);
    statusLabel->setTextColor(app::views::active().textMuted);
    statusLabel->setHorizontalAlign(brls::HorizontalAlign::RIGHT);
    header->addView(statusLabel);
    addView(header);

    auto* path = new brls::Label();
    path->setText(currentPath);
    path->setFontSize(14.0f);
    path->setTextColor(app::views::active().textMuted);
    path->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    path->setMarginBottom(14.0f);
    addView(path);

    auto* scroll = new brls::ScrollingFrame();
    scroll->setWidth(brls::View::AUTO);
    scroll->setHeight(brls::View::AUTO);
    scroll->setGrow(1.0f);

    list = new brls::Box(brls::Axis::COLUMN);
    list->setWidth(brls::View::AUTO);
    list->setHeight(brls::View::AUTO);
    list->setCornerRadius(14.0f);
    list->setBackgroundColor(brls::Application::getTheme()["color/card"]);
    scroll->setContentView(list);
    addView(scroll);

    auto* footer = new brls::Box(brls::Axis::ROW);
    footer->setWidth(brls::View::AUTO);
    footer->setHeight(brls::View::AUTO);
    footer->setJustifyContent(brls::JustifyContent::CENTER);
    footer->setMarginTop(14.0f);

    auto* refreshButton = new brls::Button();
    refreshButton->setText("akira/settings/log_refresh"_i18n);
    refreshButton->setStyle(&brls::BUTTONSTYLE_BORDERED);
    refreshButton->setMarginRight(16.0f);
    refreshButton->registerClickAction([this](brls::View*) {
        brls::sync([this]() { refreshEntries(); });
        return true;
    });
    footer->addView(refreshButton);

    auto* closeButton = new brls::Button();
    closeButton->setText("akira/common/close"_i18n);
    closeButton->setStyle(&brls::BUTTONSTYLE_BORDERED);
    closeButton->registerClickAction([](brls::View*) {
        brls::Application::popActivity();
        return true;
    });
    footer->addView(closeButton);
    addView(footer);

    registerAction("akira/settings/log_refresh"_i18n,
                   brls::ControllerButton::BUTTON_X,
                   [this](brls::View*) {
                       brls::sync([this]() { refreshEntries(); });
                       return true;
                   }, true);
    registerAction("akira/common/back"_i18n,
                   brls::ControllerButton::BUTTON_B,
                   [](brls::View*) {
                       brls::Application::popActivity();
                       return true;
                   }, true);
}

void LogBrowserView::refreshEntries()
{
    list->clearViews();

    std::vector<LogEntry> entries;
    DIR* directory = opendir(currentPath.c_str());
    if (directory) {
        while (dirent* item = readdir(directory)) {
            const std::string name = item->d_name;
            if (name == "." || name == ".." || name.find('/') != std::string::npos)
                continue;

            LogEntry entry;
            entry.name = name;
            entry.path = joinPath(currentPath, name);

            struct stat info {};
            if (lstat(entry.path.c_str(), &info) != 0)
                continue;
            if (S_ISLNK(info.st_mode))
                continue;
            if (!S_ISDIR(info.st_mode) && !S_ISREG(info.st_mode))
                continue;

            entry.directory = S_ISDIR(info.st_mode);
            entry.size = info.st_size;
            entry.modified = info.st_mtime;
            entries.push_back(std::move(entry));
        }
        closedir(directory);
    }

    std::sort(entries.begin(), entries.end(), [](const LogEntry& a, const LogEntry& b) {
        if (a.directory != b.directory)
            return a.directory > b.directory;
        if (!a.directory && a.modified != b.modified)
            return a.modified > b.modified;
        return a.name < b.name;
    });

    statusLabel->setText(directory
                             ? std::to_string(entries.size()) + " " +
                                   "akira/settings/log_items"_i18n
                             : "akira/settings/log_folder_unavailable"_i18n);

    if (entries.empty()) {
        auto* empty = new brls::Label();
        empty->setText(directory ? "akira/settings/log_folder_empty"_i18n
                                 : "akira/settings/log_folder_unavailable_hint"_i18n);
        empty->setFontSize(16.0f);
        empty->setTextColor(app::views::active().textMuted);
        empty->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        empty->setMargins(30.0f, 26.0f, 30.0f, 26.0f);
        list->addView(empty);
        return;
    }

    for (const LogEntry& entry : entries) {
        auto* cell = new brls::DetailCell();
        cell->setMarginLeft(15.0f);
        cell->setMarginRight(15.0f);
        cell->setText((entry.directory ? "\xEE\xA2\xB7  " : "\xEE\x85\xA0  ") + entry.name);

        if (entry.directory) {
            cell->setDetailText("akira/settings/log_folder"_i18n);
            const std::string nextPath = entry.path;
            const std::string root = rootPath;
            cell->registerClickAction([root, nextPath](brls::View*) {
                brls::Application::pushActivity(
                    new brls::Activity(new LogBrowserView(root, nextPath)),
                    brls::TransitionAnimation::NONE);
                return true;
            });
        } else {
            std::string detail = formatSize(entry.size);
            const std::string modified = formatModified(entry.modified);
            if (!modified.empty())
                detail += "  ·  " + modified;
            cell->setDetailText(detail);

            const std::string filePath = entry.path;
            cell->registerClickAction([filePath](brls::View*) {
                brls::Application::pushActivity(
                    new brls::Activity(new LogFileView(filePath)),
                    brls::TransitionAnimation::NONE);
                return true;
            });
        }
        list->addView(cell);
    }
}

}
