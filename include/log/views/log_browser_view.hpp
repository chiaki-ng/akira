#ifndef AKIRA_LOG_BROWSER_VIEW_HPP
#define AKIRA_LOG_BROWSER_VIEW_HPP

#include <borealis.hpp>

#include <string>

class LogBrowserView : public brls::Box {
public:
    LogBrowserView();

private:
    LogBrowserView(std::string rootPath, std::string currentPath);

    void buildLayout();
    void refreshEntries();

    std::string rootPath;
    std::string currentPath;
    brls::Box* list = nullptr;
    brls::Label* statusLabel = nullptr;
};

#endif // AKIRA_LOG_BROWSER_VIEW_HPP
