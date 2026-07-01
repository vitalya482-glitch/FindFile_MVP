#pragma once

#include "app/AppConfig.h"

#include <string>
#include <vector>

namespace findfile::app {

class CommandLineParser final {
public:
    AppConfig parse(int argc, wchar_t* argv[]) const;

private:
    AppCommand parseCommand(const std::wstring& value) const;
    IndexMode parseMode(const std::wstring& value) const;
    std::vector<std::wstring> splitSemicolonList(const std::wstring& value) const;
    bool parseBool(const std::wstring& value) const;
};

}
