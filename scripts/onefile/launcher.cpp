#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <setupapi.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)

namespace {
constexpr wchar_t kPayloadId[] = WIDEN(ARENA_PAYLOAD_ID);

struct ExtractContext {
    fs::path root;
    DWORD error = ERROR_SUCCESS;
};

bool isRussianUi()
{
    const LANGID user = GetUserDefaultUILanguage();
    if (PRIMARYLANGID(user) == LANG_RUSSIAN) return true;
    const LANGID system = GetSystemDefaultUILanguage();
    return PRIMARYLANGID(system) == LANG_RUSSIAN;
}

std::wstring tr(const wchar_t *english, const wchar_t *russian)
{
    return isRussianUi() ? russian : english;
}

std::wstring formatWin32Error(DWORD code)
{
    if (code == ERROR_SUCCESS) return {};
    wchar_t *buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                      | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(flags, nullptr, code, 0,
                                        reinterpret_cast<wchar_t *>(&buffer), 0, nullptr);
    std::wstring text;
    if (length && buffer) {
        text.assign(buffer, length);
        while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' ')) {
            text.pop_back();
        }
    }
    if (buffer) LocalFree(buffer);
    return text;
}

[[noreturn]] void fail(const wchar_t *english, const wchar_t *russian, DWORD code = ERROR_SUCCESS)
{
    std::wstring message = tr(english, russian);
    if (code != ERROR_SUCCESS) {
        message += L"\n\n";
        message += tr(L"Windows error code: ", L"Код ошибки Windows: ");
        message += std::to_wstring(code);
        const std::wstring systemText = formatWin32Error(code);
        if (!systemText.empty()) {
            message += L"\n";
            message += systemText;
        }
    }
    MessageBoxW(nullptr, message.c_str(), L"ArenaTES3JSON", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

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

bool writePayload(const fs::path &path, DWORD &error)
{
    HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(101), RT_RCDATA);
    if (!resource) {
        error = GetLastError();
        return false;
    }
    HGLOBAL loaded = LoadResource(nullptr, resource);
    if (!loaded) {
        error = GetLastError();
        return false;
    }
    const DWORD size = SizeofResource(nullptr, resource);
    const void *data = LockResource(loaded);
    if (!data || size == 0) {
        error = ERROR_RESOURCE_DATA_NOT_FOUND;
        return false;
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = GetLastError();
        if (error == ERROR_SUCCESS) error = ERROR_OPEN_FAILED;
        return false;
    }
    out.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
    if (!out.good()) {
        error = ERROR_WRITE_FAULT;
        return false;
    }
    error = ERROR_SUCCESS;
    return true;
}

bool safeCabinetRelativePath(const std::wstring &name)
{
    if (name.empty()) return false;
    fs::path p(name);
    if (p.is_absolute() || p.has_root_directory() || p.has_root_name()) return false;
    for (const auto &part : p) {
        if (part == L"..") return false;
    }
    return true;
}

UINT CALLBACK cabinetCallback(PVOID contextPointer, UINT notification, UINT_PTR param1, UINT_PTR)
{
    auto &context = *static_cast<ExtractContext *>(contextPointer);

    if (notification == SPFILENOTIFY_FILEINCABINET) {
        auto *info = reinterpret_cast<FILE_IN_CABINET_INFO_W *>(param1);
        if (!info || !info->NameInCabinet || !safeCabinetRelativePath(info->NameInCabinet)) {
            context.error = ERROR_INVALID_NAME;
            return FILEOP_SKIP;
        }

        const fs::path target = context.root / fs::path(info->NameInCabinet);
        std::error_code ec;
        fs::create_directories(target.parent_path(), ec);
        if (ec) {
            context.error = static_cast<DWORD>(ec.value());
            return FILEOP_SKIP;
        }

        const std::wstring targetString = target.wstring();
        if (targetString.size() >= std::size(info->FullTargetName)) {
            context.error = ERROR_FILENAME_EXCED_RANGE;
            return FILEOP_SKIP;
        }
        wcscpy_s(info->FullTargetName, std::size(info->FullTargetName), targetString.c_str());
        return FILEOP_DOIT;
    }

    if (notification == SPFILENOTIFY_FILEEXTRACTED) {
        auto *paths = reinterpret_cast<FILEPATHS_W *>(param1);
        if (paths && paths->Win32Error != ERROR_SUCCESS) {
            context.error = paths->Win32Error;
        }
    }

    return NO_ERROR;
}

bool extractCabinet(const fs::path &cabinet, const fs::path &destination, DWORD &error)
{
    ExtractContext context{destination, ERROR_SUCCESS};
    SetLastError(ERROR_SUCCESS);
    const BOOL ok = SetupIterateCabinetW(cabinet.c_str(), 0, cabinetCallback, &context);
    if (!ok) {
        error = context.error != ERROR_SUCCESS ? context.error : GetLastError();
        if (error == ERROR_SUCCESS) error = ERROR_INVALID_DATA;
        return false;
    }
    if (context.error != ERROR_SUCCESS) {
        error = context.error;
        return false;
    }
    error = ERROR_SUCCESS;
    return true;
}

fs::path baseRuntimeRoot()
{
    PWSTR knownFolder = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &knownFolder))
        && knownFolder && *knownFolder) {
        fs::path result(knownFolder);
        CoTaskMemFree(knownFolder);
        return result / L"ArenaTES3JSON" / L"Runtime";
    }
    if (knownFolder) CoTaskMemFree(knownFolder);

    wchar_t temp[MAX_PATH + 1]{};
    const DWORD length = GetTempPathW(static_cast<DWORD>(std::size(temp)), temp);
    if (length > 0 && length < std::size(temp)) {
        return fs::path(temp) / L"ArenaTES3JSON" / L"Runtime";
    }
    fail(L"Could not locate a writable runtime directory.",
         L"Не удалось найти каталог для встроенного runtime.", GetLastError());
}

fs::path runtimeRoot()
{
    std::wstring id(kPayloadId);
    if (id.size() > 24) id.resize(24);
    return baseRuntimeRoot() / id;
}

bool writeReadyMarker(const fs::path &path, DWORD &error)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = ERROR_WRITE_FAULT;
        return false;
    }
    out.write("ready", 5);
    if (!out.good()) {
        error = ERROR_WRITE_FAULT;
        return false;
    }
    error = ERROR_SUCCESS;
    return true;
}

bool prepareRuntime(const fs::path &root, DWORD &error)
{
    const fs::path gui = root / L"ArenaTES3JSON.exe";
    const fs::path core = root / L"ArenaTES3JSON-core.exe";
    const fs::path ready = root / L".ready";
    if (fs::exists(gui) && fs::exists(core) && fs::exists(ready)) {
        error = ERROR_SUCCESS;
        return true;
    }

    std::error_code ec;
    fs::remove_all(root, ec);
    ec.clear();
    fs::create_directories(root, ec);
    if (ec) {
        error = static_cast<DWORD>(ec.value());
        return false;
    }

    const fs::path cabinet = root / L"payload.cab";
    if (!writePayload(cabinet, error)) return false;
    if (!extractCabinet(cabinet, root, error)) {
        fs::remove(cabinet, ec);
        return false;
    }
    fs::remove(cabinet, ec);

    if (!fs::exists(gui) || !fs::exists(core)) {
        error = ERROR_FILE_NOT_FOUND;
        return false;
    }
    return writeReadyMarker(ready, error);
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

        std::wstring mutexId(kPayloadId);
        if (mutexId.size() > 24) mutexId.resize(24);
        const std::wstring mutexName = L"Local\\ArenaTES3JSON-runtime-" + mutexId;
        HANDLE runtimeMutex = CreateMutexW(nullptr, FALSE, mutexName.c_str());
        if (!runtimeMutex) {
            fail(L"Could not create the ArenaTES3JSON runtime lock.",
                 L"Не удалось создать блокировку runtime ArenaTES3JSON.", GetLastError());
        }
        const DWORD wait = WaitForSingleObject(runtimeMutex, INFINITE);
        if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) {
            const DWORD code = GetLastError();
            CloseHandle(runtimeMutex);
            fail(L"Could not lock the ArenaTES3JSON runtime directory.",
                 L"Не удалось заблокировать каталог runtime ArenaTES3JSON.", code);
        }

        DWORD extractionError = ERROR_SUCCESS;
        const bool runtimeReady = prepareRuntime(root, extractionError);
        ReleaseMutex(runtimeMutex);
        CloseHandle(runtimeMutex);
        if (!runtimeReady) {
            fail(L"Could not unpack the embedded ArenaTES3JSON runtime.",
                 L"Не удалось распаковать встроенный runtime ArenaTES3JSON.",
                 extractionError);
        }

        if (hasArgument(L"--onefile-selftest")) {
            const fs::path core = root / L"ArenaTES3JSON-core.exe";
            DWORD code = 1;
            const std::wstring coreCommand = quoteArg(core.wstring()) + L" --help";
            return runAndWait(core.wstring(), coreCommand, CREATE_NO_WINDOW, &code) && code == 0 ? 0 : 2;
        }

        const fs::path gui = root / L"ArenaTES3JSON.exe";
        std::wstring commandLine = quoteArg(gui.wstring());
        const std::wstring extra = forwardedArguments();
        if (!extra.empty()) commandLine += L" " + extra;

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
        mutableCommand.push_back(L'\0');

        if (!CreateProcessW(gui.c_str(), mutableCommand.data(), nullptr, nullptr, FALSE, 0,
                            nullptr, root.c_str(), &si, &pi)) {
            fail(L"Could not start ArenaTES3JSON from the embedded runtime.",
                 L"Не удалось запустить ArenaTES3JSON из встроенного runtime.", GetLastError());
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 0;
    } catch (const fs::filesystem_error &error) {
        fail(L"A filesystem error occurred while preparing ArenaTES3JSON.",
             L"Ошибка файловой системы при подготовке ArenaTES3JSON.",
             static_cast<DWORD>(error.code().value()));
    } catch (...) {
        fail(L"Unexpected one-file launcher error.",
             L"Непредвиденная ошибка однофайлового запуска ArenaTES3JSON.");
    }
}
