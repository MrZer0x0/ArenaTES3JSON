#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)

namespace {
constexpr wchar_t kVersion[] = WIDEN(ARENA_APP_VERSION);
constexpr wchar_t kPayloadId[] = WIDEN(ARENA_PAYLOAD_ID);

std::wstring quoteArg(const std::wstring &arg)
{
    if (arg.find_first_of(L" \t\"") == std::wstring::npos) return arg;
    std::wstring out = L"\"";
    unsigned backslashes = 0;
    for (const wchar_t c : arg) {
        if (c == L'\\') {
            ++backslashes;
            continue;
        }
        if (c == L'\"') {
            out.append(backslashes * 2 + 1, L'\\');
            out.push_back(L'\"');
            backslashes = 0;
            continue;
        }
        out.append(backslashes, L'\\');
        backslashes = 0;
        out.push_back(c);
    }
    out.append(backslashes * 2, L'\\');
    out.push_back(L'\"');
    return out;
}

std::wstring psLiteral(std::wstring value)
{
    size_t pos = 0;
    while ((pos = value.find(L'\'', pos)) != std::wstring::npos) {
        value.insert(pos, 1, L'\'');
        pos += 2;
    }
    return L"'" + value + L"'";
}

bool runAndWait(const std::wstring &application, std::wstring commandLine, DWORD flags, DWORD *exitCode = nullptr)
{
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    if (!CreateProcessW(application.empty() ? nullptr : application.c_str(),
                        mutableCommand.data(), nullptr, nullptr, FALSE, flags,
                        nullptr, nullptr, &si, &pi)) {
        return false;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (exitCode) *exitCode = code;
    return true;
}

bool writePayload(const fs::path &path)
{
    HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(101), RT_RCDATA);
    if (!resource) return false;
    HGLOBAL loaded = LoadResource(nullptr, resource);
    if (!loaded) return false;
    const DWORD size = SizeofResource(nullptr, resource);
    const void *data = LockResource(loaded);
    if (!data || size == 0) return false;

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
    return out.good();
}

std::wstring readSmallText(const fs::path &path)
{
    std::wifstream in(path);
    if (!in) return {};
    std::wstring value;
    std::getline(in, value);
    return value;
}

bool writeSmallText(const fs::path &path, const std::wstring &value)
{
    std::wofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << value;
    return out.good();
}

[[noreturn]] void fail(const wchar_t *message)
{
    MessageBoxW(nullptr, message, L"ArenaTES3JSON", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

fs::path runtimeRoot()
{
    wchar_t localAppData[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE,
                                nullptr, SHGFP_TYPE_CURRENT, localAppData))) {
        fail(L"Не удалось получить каталог LocalAppData.");
    }
    return fs::path(localAppData) / L"ArenaTES3JSON" / kVersion;
}

bool prepareRuntime(const fs::path &root)
{
    const fs::path appDir = root / L"app";
    const fs::path marker = root / L".payload-id";
    const fs::path gui = appDir / L"ArenaTES3JSON.exe";
    const fs::path core = appDir / L"ArenaTES3JSON-core.exe";

    if (fs::exists(gui) && fs::exists(core) && readSmallText(marker) == kPayloadId) {
        return true;
    }

    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(appDir, ec);
    if (ec) return false;

    const fs::path zip = root / L"payload.zip";
    if (!writePayload(zip)) return false;

    std::wstring ps = L"powershell.exe -NoLogo -NoProfile -NonInteractive -WindowStyle Hidden "
                      L"-ExecutionPolicy Bypass -Command \"Expand-Archive -LiteralPath "
                      + psLiteral(zip.wstring()) + L" -DestinationPath "
                      + psLiteral(appDir.wstring()) + L" -Force\"";
    DWORD code = 1;
    if (!runAndWait(L"powershell.exe", ps, CREATE_NO_WINDOW, &code) || code != 0) {
        return false;
    }
    fs::remove(zip, ec);
    if (!fs::exists(gui) || !fs::exists(core)) return false;
    return writeSmallText(marker, kPayloadId);
}

bool hasArgument(const wchar_t *wanted)
{
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return false;
    bool found = false;
    for (int i = 1; i < argc; ++i) {
        if (std::wstring(argv[i]) == wanted) {
            found = true;
            break;
        }
    }
    LocalFree(argv);
    return found;
}

std::wstring forwardedArguments()
{
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return {};
    std::wstring args;
    for (int i = 1; i < argc; ++i) {
        if (!args.empty()) args.push_back(L' ');
        args += quoteArg(argv[i]);
    }
    LocalFree(argv);
    return args;
}
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    try {
        const fs::path root = runtimeRoot();
        if (!prepareRuntime(root)) {
            fail(L"Не удалось распаковать встроенный runtime ArenaTES3JSON.");
        }

        const fs::path appDir = root / L"app";
        if (hasArgument(L"--onefile-selftest")) {
            const fs::path core = appDir / L"ArenaTES3JSON-core.exe";
            DWORD code = 1;
            const std::wstring coreCommand = quoteArg(core.wstring()) + L" --help";
            return runAndWait(core.wstring(), coreCommand, CREATE_NO_WINDOW, &code) && code == 0 ? 0 : 2;
        }

        const fs::path gui = appDir / L"ArenaTES3JSON.exe";
        std::wstring commandLine = quoteArg(gui.wstring());
        const std::wstring extra = forwardedArguments();
        if (!extra.empty()) commandLine += L" " + extra;

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
        mutableCommand.push_back(L'\0');

        if (!CreateProcessW(gui.c_str(), mutableCommand.data(), nullptr, nullptr, FALSE, 0,
                            nullptr, appDir.c_str(), &si, &pi)) {
            fail(L"Не удалось запустить ArenaTES3JSON из встроенного runtime.");
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 0;
    } catch (...) {
        fail(L"Непредвиденная ошибка однофайлового запуска ArenaTES3JSON.");
    }
}
