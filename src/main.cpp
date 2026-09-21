#include <windows.h>

#include <cstdio>
#include <string>

#include "app/Application.h"
#include "app/Platform.h"
#include "core/SelfTest.h"

namespace {

// Attaches to the parent console (or opens one) so that the CLI modes can print.
void attachConsole() {
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) AllocConsole();
    FILE* dummy = nullptr;
    freopen_s(&dummy, "CONOUT$", "w", stdout);
    freopen_s(&dummy, "CONOUT$", "w", stderr);
}

std::string cleanArgument(const std::string& raw) {
    std::string value;
    for (char c : raw) {
        if (value.empty() && (c == ' ' || c == '\t' || c == '=')) continue;
        value += c;
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        value = value.substr(1, value.size() - 2);
    return value;
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR commandLine, int) {
    const std::string args = se::platform::toUtf8(commandLine ? commandLine : L"");

    if (args.find("--selftest") != std::string::npos) {
        attachConsole();
        const std::string report = se::platform::appConfigDir() + "/selftest.txt";
        const int result = se::runSelfTest(report);
        std::string text;
        se::platform::readFile(report, &text);
        std::printf("%s\nReport: %s\n", text.c_str(), report.c_str());
        std::fflush(stdout);
        return result;
    }

    const size_t demoPos = args.find("--demo");
    if (demoPos != std::string::npos) {
        attachConsole();
        const std::string path = cleanArgument(args.substr(demoPos + 6));
        if (path.empty()) {
            std::printf("Nutzung: StoryEditor.exe --demo <Ordner>\n");
            std::fflush(stdout);
            return 1;
        }
        const int result = se::createDemoProject(path);
        std::printf("%s: %s\n", result == 0 ? "Demo-Projekt erstellt" : "Fehler beim Erstellen",
                    path.c_str());
        std::fflush(stdout);
        return result;
    }

    se::Application app;
    return app.run();
}
