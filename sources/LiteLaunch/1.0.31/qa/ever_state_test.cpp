// Code-level production EVER persistence/transaction regression.
// Filesystem mutations are confined to a freshly-created owned temp tree.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <filesystem>

static int qaMoveCount = 0;
static std::vector<int> qaFailedMoves;
static bool qaInject = false;
static int qaWarnings = 0;
static int WINAPI qaMessageBoxW(HWND, LPCWSTR, LPCWSTR, UINT) { ++qaWarnings; return IDNO; }
static BOOL WINAPI qaMoveFileExW(LPCWSTR source, LPCWSTR destination, DWORD flags) {
    if (qaInject && std::find(qaFailedMoves.begin(), qaFailedMoves.end(), ++qaMoveCount) != qaFailedMoves.end()) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    return ::MoveFileExW(source, destination, flags);
}
#define MoveFileExW qaMoveFileExW
#define MessageBoxW qaMessageBoxW
#define LITELAUNCH_EVER_TESTING
#include "../rename.cpp"
#undef MoveFileExW
#undef MessageBoxW

using namespace llrename;
namespace {
int passed = 0, failed = 0;
std::wstring qaRoot;
void check(bool ok, const char *name) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    ok ? ++passed : ++failed;
}
void require(bool ok, const char *what) { if (!ok) throw std::runtime_error(what); }
std::wstring joined(const wchar_t *leaf) { return qaRoot + L"\\" + leaf; }
void makeDirectory(const std::wstring &path) {
    require(CreateDirectoryW(nativePath(path).c_str(), nullptr) != FALSE, "Private fixture directory creation failed");
}
void writeBytes(const std::wstring &path, const std::vector<BYTE> &bytes) {
    HANDLE file = CreateFileW(nativePath(path).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE, "Private fixture write open failed");
    DWORD written = 0;
    const bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size();
    CloseHandle(file);
    require(ok, "Private fixture write failed");
}
void writeText(const std::wstring &path, const std::string &value) { writeBytes(path, std::vector<BYTE>(value.begin(), value.end())); }
std::string readText(const std::wstring &path) { const auto b = eRead(nativePath(path)); return std::string(b.begin(), b.end()); }
Move move(const std::wstring &before, const std::wstring &after) {
    Move result{before, after, {}};
    require(identify(before, result.identity), "Private fixture identity failed");
    return result;
}
std::vector<Move> reversed(const std::vector<Move> &moves) {
    std::vector<Move> result;
    for (auto m : moves) { std::swap(m.before, m.after); result.push_back(m); }
    return result;
}
bool identitiesAtAfter(const std::vector<Move> &moves) {
    for (const auto &m : moves) { Identity current; if (!identify(m.after, current) || !sameIdentity(current, m.identity)) return false; }
    return true;
}
bool equalHistory(const std::vector<Move> &a, const std::vector<Move> &b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) if (a[i].before != b[i].before || a[i].after != b[i].after || !sameIdentity(a[i].identity, b[i].identity)) return false;
    return true;
}
template<class F> bool throws(F action) { try { action(); return false; } catch (const std::exception &) { return true; } }
Result injected(const std::vector<Move> &moves, std::vector<int> failures) {
    qaMoveCount = 0; qaFailedMoves = std::move(failures); qaInject = true;
    try { auto result = transact(moves); qaInject = false; return result; } catch (...) { qaInject = false; throw; }
}
bool tempExists(const std::wstring &directory) {
    WIN32_FIND_DATAW entry{};
    HANDLE find = FindFirstFileW(nativePath(directory + L"\\.~LiteLaunch-*").c_str(), &entry);
    if (find == INVALID_HANDLE_VALUE) return false;
    FindClose(find); return true;
}
void setHome(const std::wstring &root) {
    qaRoot = fullPath(root);
    const auto leaf = fileName(qaRoot);
    require(leaf.compare(0, 17, L"LL_EVER_STATE_QA_") == 0, "Not an owned QA root");
    DWORD attr = attrs(qaRoot);
    require(attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) && !(attr & FILE_ATTRIBUTE_REPARSE_POINT), "QA root unavailable or redirected");
    eTestHome = qaRoot + L"\\state\\";
    require(readText(joined(L"qa-owned.txt")) == "LiteLaunch EVER isolated QA v1", "QA ownership marker missing");
}
int childReverse(const std::wstring &root) {
    setHome(root);
    auto history = eLoadHistory();
    require(!history.empty() && identitiesAtAfter(history), "Child history identity mismatch");
    const auto result = transact(reversed(history));
    require(result.ok && identitiesAtAfter(result.undo), "Child reverse failed");
    require(eStoreHistory(result.undo), "Child reverse persistence failed");
    return 0;
}
bool runChildReverse() {
    wchar_t executable[32768]{}; require(GetModuleFileNameW(nullptr, executable, 32768) != 0, "Fixture executable unavailable");
    std::wstring command = L"\"" + std::wstring(executable) + L"\" --reverse \"" + qaRoot + L"\"";
    STARTUPINFOW startup{sizeof(startup)}; PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, &command[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, qaRoot.c_str(), &startup, &process)) return false;
    CloseHandle(process.hThread);
    const DWORD waited = WaitForSingleObject(process.hProcess, 30000);
    DWORD code = 2;
    if (waited == WAIT_OBJECT_0) GetExitCodeProcess(process.hProcess, &code);
    else { TerminateProcess(process.hProcess, 3); WaitForSingleObject(process.hProcess, 5000); }
    CloseHandle(process.hProcess);
    return waited == WAIT_OBJECT_0 && code == 0;
}
void historyValidation() {
    check(eLoadHistory().empty(), "missing-history-is-empty");
    const auto source = joined(L"中文😀.txt"), destination = joined(L"改后🚀.txt");
    writeText(source, "content-original");
    const std::vector<Move> history{move(source, destination)};
    check(eStoreHistory(history), "history-atomic-store");
    check(equalHistory(history, eLoadHistory()), "history-UTF8-path-and-identity-roundtrip");
    const auto path = eTestHome + L"undo.bin";
    const auto good = eRead(path);
    auto broken = good; broken.back() ^= 0x80;
    writeBytes(path, broken);
    check(throws([] { eLoadHistory(); }), "history-checksum-rejected");
    broken = good; broken.pop_back(); writeBytes(path, broken);
    check(throws([] { eLoadHistory(); }), "history-truncation-rejected");
    broken = good; broken.push_back(0); writeBytes(path, broken);
    check(throws([] { eLoadHistory(); }), "history-trailing-bytes-rejected");
    writeBytes(path, {});
    check(throws([] { eLoadHistory(); }), "existing-empty-history-rejected");
    HANDLE big = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(big != INVALID_HANDLE_VALUE, "Large private fixture unavailable");
    LARGE_INTEGER size{}; size.QuadPart = 16LL * 1024 * 1024 + 1;
    require(SetFilePointerEx(big, size, nullptr, FILE_BEGIN) && SetEndOfFile(big), "Large private fixture write failed");
    CloseHandle(big);
    check(throws([] { eLoadHistory(); }), "oversized-history-rejected");
    writeBytes(path, good);
    HANDLE unreadable = CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(unreadable != INVALID_HANDLE_VALUE, "Private history exclusive lock failed");
    check(throws([] { eLoadHistory(); }), "existing-unreadable-history-rejected");
    CloseHandle(unreadable);
    HANDLE locked = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(locked != INVALID_HANDLE_VALUE, "Private history lock failed");
    check(!eStoreHistory({}), "history-store-failure-reported");
    CloseHandle(locked);
    check(eRead(path) == good, "failed-atomic-store-preserves-old-history");
    check(GetFileAttributesW((path + L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp").c_str()) == INVALID_FILE_ATTRIBUTES, "failed-atomic-store-cleans-private-temp");
    auto invalid = history; invalid.front().after = joined(L"other\\bad.txt");
    check(eStoreHistory(invalid) && throws([] { eLoadHistory(); }), "cross-directory-history-rejected-on-read");
    broken.clear(); ePut32(broken, 0x31485245); ePut32(broken, 100001); ePut32(broken, eHash(broken, broken.size()));
    writeBytes(path, broken);
    check(throws([] { eLoadHistory(); }), "oversized-history-count-rejected");
    broken.clear(); ePut32(broken, 0x31485245); ePut32(broken, 1); ePut32(broken, 1); broken.push_back(0xFF);
    ePutText(broken, destination); ePut32(broken, 0); ePut32(broken, 0); ePut32(broken, 0); ePut32(broken, eHash(broken, broken.size()));
    writeBytes(path, broken);
    check(throws([] { eLoadHistory(); }), "history-invalid-UTF8-rejected-even-with-valid-checksum");
    require(eStoreHistory(history), "Restore private history failed");
}
void swapAndCrossProcess() {
    const auto folder = joined(L"swap"); makeDirectory(folder);
    const auto a = folder + L"\\A.txt", b = folder + L"\\B.txt", c = folder + L"\\C.txt";
    writeText(a, "A-object"); writeText(b, "B-object"); writeText(c, "C-object");
    const std::vector<Move> plan{move(a, b), move(b, c), move(c, a)};
    auto result = transact(plan);
    check(result.ok, "three-way-cycle-completes");
    check(readText(b) == "A-object" && readText(c) == "B-object" && readText(a) == "C-object", "three-way-cycle-content-preserved");
    check(identitiesAtAfter(plan) && !tempExists(folder), "three-way-cycle-identities-and-no-temp");
    require(eStoreHistory(result.undo), "Cycle history persistence failed");
    result.undo.clear();
    check(runChildReverse(), "first-separate-process-regret");
    check(readText(a) == "A-object" && readText(b) == "B-object" && readText(c) == "C-object", "first-regret-original-names-restored");
    check(runChildReverse(), "second-separate-process-regret");
    check(readText(b) == "A-object" && readText(c) == "B-object" && readText(a) == "C-object", "second-regret-renamed-state-restored");
    check(identitiesAtAfter(eLoadHistory()) && !tempExists(folder), "cross-process-history-current-identities");
}
void transactionProtection() {
    const auto folder = joined(L"protection"); makeDirectory(folder);
    const auto a = folder + L"\\A.txt", b = folder + L"\\B.txt", occupied = folder + L"\\occupied.txt";
    writeText(a, "source-A"); writeText(b, "source-B"); writeText(occupied, "external-object");
    auto r = transact({move(a, occupied), move(b, folder + L"\\new-B.txt")});
    check(!r.ok && readText(a) == "source-A" && readText(b) == "source-B" && readText(occupied) == "external-object", "external-collision-zero-mutation");
    r = transact({move(a, folder + L"\\duplicate.txt"), move(b, folder + L"\\duplicate.txt")});
    check(!r.ok && readText(a) == "source-A" && readText(b) == "source-B", "duplicate-target-zero-mutation");
    auto stale = move(a, folder + L"\\stale.txt"); stale.identity.low ^= 1;
    require(eStoreHistory({stale}), "Private stale identity persistence failed");
    r = transact(eLoadHistory());
    check(!r.ok && readText(a) == "source-A" && attrs(stale.after) == INVALID_FILE_ATTRIBUTES, "loaded-stale-identity-blocks-mutation");
    const auto dir = folder + L"\\parent"; makeDirectory(dir); writeText(dir + L"\\child.txt", "inside");
    r = transact({move(dir, folder + L"\\renamed-parent"), move(dir + L"\\child.txt", dir + L"\\new-child.txt")});
    check(!r.ok && readText(dir + L"\\child.txt") == "inside" && attrs(dir) != INVALID_FILE_ATTRIBUTES, "parent-child-batch-zero-mutation");
    const std::vector<Move> plan{move(a, folder + L"\\new-A.txt"), move(b, folder + L"\\new-B.txt")};
    r = injected(plan, {2});
    check(!r.ok && r.undo.empty() && readText(a) == "source-A" && readText(b) == "source-B" && !tempExists(folder), "first-stage-failure-rolls-back-whole-batch");
    r = injected(plan, {4});
    check(!r.ok && r.undo.empty() && readText(a) == "source-A" && readText(b) == "source-B" && !tempExists(folder), "second-stage-failure-rolls-back-whole-batch");
    r = transact({move(a, folder + L"\\a.txt")});
    check(r.ok && readText(folder + L"\\a.txt") == "source-A" && identitiesAtAfter(r.undo), "case-only-rename-preserves-object");
}
void folderSwap() {
    const auto folder = joined(L"folders"); makeDirectory(folder);
    const auto a = folder + L"\\A", b = folder + L"\\B"; makeDirectory(a); makeDirectory(b);
    writeText(a + L"\\payload.txt", "folder-A"); writeText(b + L"\\payload.txt", "folder-B");
    auto r = transact({move(a, b), move(b, a)});
    check(r.ok && readText(b + L"\\payload.txt") == "folder-A" && readText(a + L"\\payload.txt") == "folder-B", "folder-swap-keeps-internal-content");
    check(identitiesAtAfter(r.undo) && !tempExists(folder), "folder-swap-identities-and-no-temp");
    const auto undo = transact(reversed(r.undo));
    check(undo.ok && readText(a + L"\\payload.txt") == "folder-A" && readText(b + L"\\payload.txt") == "folder-B", "folder-swap-regret-restores-content");
}
void partialPersistence() {
    const auto folder = joined(L"partial"); makeDirectory(folder);
    const auto a = folder + L"\\A.txt", b = folder + L"\\B.txt", afterA = folder + L"\\new-A.txt", afterB = folder + L"\\new-B.txt";
    writeText(a, "partial-A"); writeText(b, "partial-B");
    auto initial = transact({move(a, afterA), move(b, afterB)}); require(initial.ok, "Partial initial rename failed");
    auto history = initial.undo;
    auto r = injected(reversed(history), {3, 4});
    check(!r.ok && r.undo.size() == 1 && r.undo.front().after.find(L".~LiteLaunch-") != std::wstring::npos, "failed-regret-returns-real-temp-path");
    reconcileUndo(history, r.undo);
    check(history.size() == 2 && history.front().before == a && identitiesAtAfter(history), "partial-reconcile-keeps-original-endpoint-and-current-identity");
    check(eStoreHistory(history), "partial-current-path-persisted");
    auto expected = history; history.clear(); history = eLoadHistory();
    check(equalHistory(history, expected) && identitiesAtAfter(history), "partial-current-path-survives-reload");
    r = transact(reversed(history));
    check(r.ok && readText(a) == "partial-A" && readText(b) == "partial-B" && !tempExists(folder), "partial-regret-retry-restores-originals");
    require(eStoreHistory(eRegretSuccess(history, r.undo)), "Partial recovered history persistence failed");
    const auto nextToggle = eLoadHistory();
    bool noTemporaryEndpoint = true;
    for (const auto &m : nextToggle) noTemporaryEndpoint = noTemporaryEndpoint && m.before.find(L".~LiteLaunch-") == m.before.npos && m.after.find(L".~LiteLaunch-") == m.after.npos;
    check(noTemporaryEndpoint, "recovered-regret-toggle-does-not-resurrect-temp-name");
}
void templateValidation() {
    const auto path = eTestHome + L"templates.bin";
    check(eTemplates().empty(), "missing-templates-is-empty");
    ever::Rule first; first.text = L"中文😀\n\\path"; first.separator = L"\t"; first.sequenceStart = L"001"; first.pinyinCount = 3; first.frontAnchor = L"前😀"; first.backAnchor = L"后🚀";
    ever::Rule second; second.mode = ever::Mode::FindReplace; second.regex = true; second.replacements = L"(?<x>\\d+)|${x}\n旧|新";
    const std::vector<ETemplate> templates{{L"模板🚀", first}, {L"正则模板", second}};
    check(eStoreTemplates(templates), "templates-atomic-store");
    auto loaded = eTemplates();
    check(loaded.size() == 2 && loaded[0].name == templates[0].name && loaded[1].name == templates[1].name && ever::serialize(loaded[0].rule) == ever::serialize(first) && ever::serialize(loaded[1].rule) == ever::serialize(second), "templates-Chinese-nonBMP-and-all-rule-fields-roundtrip");
    const auto good = eRead(path); auto broken = good; broken.back() ^= 1; writeBytes(path, broken);
    check(throws([] { eTemplates(); }), "templates-checksum-rejected");
    broken = good; broken.resize(7); writeBytes(path, broken);
    check(throws([] { eTemplates(); }), "templates-truncation-rejected");
    broken.clear(); ePut32(broken, 0x31545245); ePut32(broken, 10001); ePut32(broken, eHash(broken, broken.size()));
    writeBytes(path, broken);
    check(throws([] { eTemplates(); }), "oversized-template-count-rejected");
    broken.clear(); ePut32(broken, 0x31545245); ePut32(broken, 1); ePutText(broken, L"damaged"); ePutText(broken, L"EVER_RULE_V1\nmode=999\n"); ePut32(broken, eHash(broken, broken.size()));
    writeBytes(path, broken);
    check(throws([] { eTemplates(); }), "invalid-template-rule-rejected-even-with-valid-checksum");
    writeBytes(path, {});
    check(throws([] { eTemplates(); }), "existing-empty-templates-rejected");
    writeBytes(path, good);
    check(eTemplates().size() == 2, "templates-good-file-restored");
    eSaveSetting(L"Sort", L"Origin"); eSaveSetting(L"LinkApp", L"D:\\中文😀\\编辑器.exe");
    check(eSetting(L"Sort") == L"Origin", "settings-sort-roundtrip");
    check(eSetting(L"LinkApp") == L"D:\\中文😀\\编辑器.exe", "settings-Chinese-nonBMP-roundtrip");
    check(eSetting(L"Missing", L"fallback") == L"fallback", "settings-missing-key-fallback");
    const auto originalHome = eTestHome;
    for (const bool utf8 : {false, true}) {
        const auto migrationHome = joined(utf8 ? L"settings-migration-utf8" : L"settings-migration-acp"); makeDirectory(migrationHome);
        eTestHome = migrationHome + L"\\";
        const std::wstring legacyLink = utf8 ? L"D:\\旧设置😀\\工具.exe" : L"D:\\旧设置\\工具.exe";
        const std::wstring ini = L"[EVER]\r\nSort=FileNameNature\r\nLinkApp=" + legacyLink + L"\r\n";
        const UINT codepage = utf8 ? CP_UTF8 : CP_ACP;
        const int needed = WideCharToMultiByte(codepage, 0, ini.data(), static_cast<int>(ini.size()), nullptr, 0, nullptr, nullptr);
        std::string encoded(needed, 0); WideCharToMultiByte(codepage, 0, ini.data(), static_cast<int>(ini.size()), &encoded[0], needed, nullptr, nullptr);
        std::vector<BYTE> bytes;
        if (utf8) bytes = {0xEF, 0xBB, 0xBF}; bytes.insert(bytes.end(), encoded.begin(), encoded.end());
        writeBytes(eTestHome + L"settings.ini", bytes);
        eSaveSetting(L"NewUnicode", L"新值🚀");
        const auto converted = eRead(eTestHome + L"settings.ini");
        check(converted.size() >= 2 && converted[0] == 0xFF && converted[1] == 0xFE && eSetting(L"Sort") == L"FileNameNature" && eSetting(L"LinkApp") == legacyLink && eSetting(L"NewUnicode") == L"新值🚀", utf8 ? "UTF8-BOM-settings-migration-preserves-existing-and-new-values" : "ACP-settings-migration-preserves-existing-and-new-values");
        eTestHome = originalHome;
    }
}
void recoveredEntryReconcile() {
    const auto folder = joined(L"reconcile-recovered"); makeDirectory(folder);
    const auto a = folder + L"\\A.txt", b = folder + L"\\B.txt", afterA = folder + L"\\new-A.txt", afterB = folder + L"\\new-B.txt";
    writeText(a, "recovered-A"); writeText(b, "pending-B");
    auto initial = transact({move(a, afterA), move(b, afterB)}); require(initial.ok, "Recovered-entry initial rename failed");
    auto history = initial.undo;
    const auto partial = injected(reversed(history), {4, 5, 6});
    check(!partial.ok && readText(a) == "recovered-A" && readText(afterB) == "pending-B", "failed-regret-may-already-restore-one-object");
    reconcileUndo(history, partial.undo);
    check(history.size() == 1 && history[0].before == b && history[0].after == afterB && identitiesAtAfter(history), "reconcile-removes-recovered-object-keeps-only-pending-identity");
    require(eStoreHistory(history), "Recovered-entry persistence failed"); history.clear(); history = eLoadHistory();
    check(history.size() == 1 && history[0].before == b && history[0].after == afterB, "only-pending-object-history-survives-reload");
    const auto retried = transact(reversed(history));
    check(retried.ok && readText(a) == "recovered-A" && readText(b) == "pending-B" && !tempExists(folder), "pending-retry-preserves-already-restored-object");
}
void regretItemsCurrentPath() {
    const auto folder = joined(L"regret-current-items"); makeDirectory(folder);
    const auto a = folder + L"\\A.txt", b = folder + L"\\B.txt", afterA = folder + L"\\new-A.txt", afterB = folder + L"\\new-B.txt";
    writeText(a, "regret-items-A"); writeText(b, "regret-items-B");
    auto initial = transact({move(a, afterA), move(b, afterB)}); require(initial.ok, "Private regret-items initial rename failed");
    EverWindow editor; editor.history = initial.undo;
    require(addPath(editor.items, afterA) && addPath(editor.items, afterB), "Private regret-items unavailable");
    // Exercise production regret() itself; no UI HWND is present in this model case.
    qaMoveCount = 0; qaFailedMoves = {3, 4}; qaInject = true;
    try { editor.regret(); qaInject = false; } catch (...) { qaInject = false; throw; }
    bool current = editor.history.size() == 2 && equalHistory(editor.history, eLoadHistory()) && identitiesAtAfter(editor.history);
    for (const auto &item : editor.items) {
        bool matched = false;
        for (const auto &record : editor.history) if (same(item.path, record.after) && item.name == fileName(record.after)) matched = true;
        current = current && matched;
    }
    auto restored = transact(reversed(editor.history));
    check(current && restored.ok && readText(a) == "regret-items-A" && readText(b) == "regret-items-B" && !tempExists(folder),
          "production-regret-partial-failure-updates-items-to-persisted-real-current-path");
}
void extensionMetadata() {
    std::puts("STEP private-editor-begin");
    const auto folder = joined(L"editor-extension"); makeDirectory(folder);
    const auto a = folder + L"\\甲😀.txt", b = folder + L"\\乙.csv", c = folder + L"\\folder.with.dot";
    writeText(a, "editor-A"); writeText(b, "editor-B"); makeDirectory(c);
    const std::vector<Move> originalObjects{move(a, a), move(b, b), move(c, c)};
    EverWindow editor;
    addPath(editor.items, a); addPath(editor.items, b); addPath(editor.items, c);
    require(Scintilla_RegisterClasses(GetModuleHandleW(nullptr)) != 0, "Private editor registration failed");
    std::puts("STEP private-editor-registered");
    const wchar_t *className = L"LiteLaunch.Ever.Extension.State.QA";
    WNDCLASSEXW cls{sizeof(cls)}; cls.lpfnWndProc = EverWindow::proc; cls.hInstance = GetModuleHandleW(nullptr); cls.lpszClassName = className;
    require(RegisterClassExW(&cls) != 0, "Private extension test window registration failed");
    std::puts("STEP private-editor-host-create");
    HWND host = CreateWindowExW(0, className, L"Hidden private editor regression", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 0, 0, 900, 600, nullptr, nullptr, cls.hInstance, &editor);
    require(host != nullptr && editor.editor != nullptr, "Private production editor creation failed");
    std::puts("STEP private-editor-host-created");
    const auto warningsBefore = qaWarnings;
    const std::vector<std::wstring> initialExtensions{L".txt", L".csv", L""};
    check(!IsWindowVisible(host) && editor.text() == L"甲😀.txt\n乙.csv\nfolder.with.dot" && editor.extensions == initialExtensions && eCurrentWindow == host && renameActiveWindow() == host, "private-production-editor-initial-text-and-metadata");
    editor.toggle(true);
    check(editor.hidden && editor.text() == L"甲😀\n乙\nfolder.with.dot" && editor.extensions == initialExtensions, "hide-extension-text-and-metadata-match");
    editor.sci(SCI_UNDO);
    check(!editor.hidden && editor.text() == L"甲😀.txt\n乙.csv\nfolder.with.dot" && editor.extensions == initialExtensions, "undo-hide-restores-visible-mode-text-and-metadata");
    editor.sci(SCI_REDO);
    check(editor.hidden && editor.text() == L"甲😀\n乙\nfolder.with.dot" && editor.extensions == initialExtensions, "redo-hide-restores-hidden-mode-text-and-metadata");
    editor.setText(L"改甲🚀\n改乙\nfolder.新");
    editor.toggle(false);
    check(!editor.hidden && editor.text() == L"改甲🚀.txt\n改乙.csv\nfolder.新" && editor.extensions == initialExtensions, "show-edited-stems-restores-original-extensions");
    editor.sci(SCI_UNDO);
    check(editor.hidden && editor.text() == L"改甲🚀\n改乙\nfolder.新" && editor.extensions == initialExtensions, "undo-show-restores-edited-hidden-stems");
    editor.sci(SCI_UNDO);
    check(editor.hidden && editor.text() == L"甲😀\n乙\nfolder.with.dot", "undo-hidden-text-edit-keeps-hidden-metadata");
    editor.sci(SCI_UNDO);
    check(!editor.hidden && editor.text() == L"甲😀.txt\n乙.csv\nfolder.with.dot", "multi-step-undo-restores-original-visible-mode");
    editor.sci(SCI_REDO); editor.sci(SCI_REDO); editor.sci(SCI_REDO);
    check(!editor.hidden && editor.text() == L"改甲🚀.txt\n改乙.csv\nfolder.新" && editor.extensions == initialExtensions, "multi-step-redo-restores-shown-edited-state");
    editor.hidden = false; editor.rebuild();
    editor.setText(L"甲😀.dat\n乙.bin\nfolder.with.dot"); editor.toggle(true);
    const std::vector<std::wstring> editedExtensions{L".dat", L".bin", L""};
    check(editor.hidden && editor.extensions == editedExtensions && editor.text() == L"甲😀\n乙\nfolder.with.dot", "hide-recomputes-edited-extension-cache");
    editor.sci(SCI_UNDO);
    check(!editor.hidden && editor.extensions == initialExtensions && editor.text() == L"甲😀.dat\n乙.bin\nfolder.with.dot", "undo-hide-restores-pre-action-extension-cache");
    editor.sci(SCI_REDO);
    check(editor.hidden && editor.extensions == editedExtensions, "redo-hide-restores-edited-extension-cache");
    editor.restoreNames();
    check(editor.hidden && editor.extensions == initialExtensions, "restore-initial-names-restores-original-extension-cache");
    editor.sci(SCI_UNDO);
    check(editor.hidden && editor.extensions == editedExtensions && editor.text() == L"甲😀\n乙\nfolder.with.dot", "undo-restore-initial-names-restores-edited-extension-cache");
    editor.sci(SCI_REDO);
    check(editor.hidden && editor.extensions == initialExtensions, "redo-restore-initial-names-restores-original-cache");
    check(qaWarnings == warningsBefore, "private-editor-metadata-regression-shows-no-modal-warning");
    const bool samplesUnchanged = identitiesAtAfter(originalObjects) && readText(a) == "editor-A" && readText(b) == "editor-B";
    DestroyWindow(host);
    check(samplesUnchanged && eCurrentWindow == nullptr && renameActiveWindow() == nullptr, "editor-only-actions-never-mutate-sample-files");
    UnregisterClassW(className, cls.hInstance); Scintilla_ReleaseResources();
}
std::vector<Entry> entries(const std::vector<std::wstring> &paths) {
    std::vector<Entry> result; for (const auto &path : paths) require(addPath(result, path), "Private resolver entry unavailable"); return result;
}
void nameResolution() {
    const auto folder = joined(L"name-resolution"); makeDirectory(folder);
    const auto a = folder + L"\\A.txt", b = folder + L"\\B.txt"; writeText(a, "resolver-A"); writeText(b, "resolver-B");
    auto items = entries({a, b}); std::wstring error; bool changed = false; std::vector<std::wstring> names{L"B.txt", L"A.txt"};
    check(eResolveNames(items, names, error, changed) && !changed && names[0] == L"B.txt" && names[1] == L"A.txt", "selected-name-swap-needs-no-conflict-suffix");
    auto r = transact({move(a, parent(a) + names[0]), move(b, parent(b) + names[1])});
    check(r.ok && readText(b) == "resolver-A" && readText(a) == "resolver-B", "resolved-selected-swap-transacts-with-correct-content");
    names = {L"A.txt", L"A.txt"};
    check(eResolveNames(items, names, error, changed) && changed && names[0] == L"A.txt" && names[1] == L"A_1.txt", "unchanged-selected-name-is-reserved-from-other-rows");
    names = {L"相同.TXT", L"相同.txt"};
    check(eResolveNames(items, names, error, changed) && changed && names[0] == L"相同.TXT" && names[1] == L"相同_1.txt", "batch-duplicates-resolve-case-insensitively-before-extension");
    writeText(folder + L"\\occupied.txt", "external-0"); writeText(folder + L"\\occupied_1.txt", "external-1"); writeText(folder + L"\\occupied_2.txt", "external-2");
    names = {L"occupied.txt", L"occupied.txt"};
    check(eResolveNames(items, names, error, changed) && changed && names[0] == L"occupied_3.txt" && names[1] == L"occupied_4.txt", "external-occupants-and-batch-reservations-get-unique-suffixes");
    const auto resolved = names; check(eResolveNames(items, names, error, changed) && !changed && names == resolved, "resolved-preview-is-stable-on-second-confirmation");
    r = transact({move(a, parent(a) + names[0]), move(b, parent(b) + names[1])});
    check(r.ok && readText(folder + L"\\occupied_3.txt") == "resolver-B" && readText(folder + L"\\occupied_4.txt") == "resolver-A", "resolved-suffixed-plan-transacts-exact-preview");
    check(readText(folder + L"\\occupied.txt") == "external-0" && readText(folder + L"\\occupied_1.txt") == "external-1" && readText(folder + L"\\occupied_2.txt") == "external-2", "suffix-plan-never-overwrites-existing-object");
    const auto directory = folder + L"\\source.dir"; makeDirectory(directory); makeDirectory(folder + L"\\occupied.with.dot");
    items = entries({directory}); names = {L"occupied.with.dot"};
    check(eResolveNames(items, names, error, changed) && changed && names[0] == L"occupied.with.dot_1", "folder-dot-name-kept-whole-before-suffix");
    const auto other = folder + L"\\other"; makeDirectory(other); writeText(other + L"\\source.txt", "other-source");
    items = entries({folder + L"\\occupied_3.txt", other + L"\\source.txt"}); names = {L"same.txt", L"same.txt"};
    check(eResolveNames(items, names, error, changed) && !changed, "same-target-name-in-distinct-directories-is-valid");
    names = {L"bad/name", L"valid.txt"};
    check(!eResolveNames(items, names, error, changed) && !error.empty(), "invalid-target-rejected-before-file-mutation");
    names = {L"valid.txt"}; check(!eResolveNames(items, names, error, changed), "resolver-row-count-mismatch-rejected");
    const std::wstring longest = std::wstring(251, L'x') + L".txt"; writeText(folder + L"\\" + longest, "long-occupied");
    items = entries({folder + L"\\occupied_3.txt"}); names = {longest};
    check(!eResolveNames(items, names, error, changed) && !error.empty() && readText(folder + L"\\" + longest) == "long-occupied", "suffix-that-exceeds-name-limit-is-rejected");
    check(!tempExists(folder), "name-resolver-leaves-no-transaction-temp");
    check(eNameLines(L"A.txt\r\n\r\nB.txt\n") == std::vector<std::wstring>{L"A.txt", L"B.txt"}, "original-empty-line-filter-keeps-nonempty-row-order");
}
}

int wmain(int argc, wchar_t **argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    try {
        if (argc != 3) throw std::runtime_error("Usage: ever_state_test --suite|--reverse <owned QA root>");
        if (std::wstring(argv[1]) == L"--reverse") return childReverse(argv[2]);
        require(std::wstring(argv[1]) == L"--suite", "Unknown private fixture mode");
        qaRoot = fullPath(argv[2]);
        require(fileName(qaRoot).compare(0, 17, L"LL_EVER_STATE_QA_") == 0, "QA root name required");
        makeDirectory(qaRoot);
        writeText(joined(L"qa-owned.txt"), "LiteLaunch EVER isolated QA v1");
        makeDirectory(joined(L"state")); setHome(qaRoot);
        historyValidation(); swapAndCrossProcess(); transactionProtection(); folderSwap(); partialPersistence(); recoveredEntryReconcile(); templateValidation(); extensionMetadata(); nameResolution(); regretItemsCurrentPath();
        ever::regexShutdown();
        std::printf("RESULT passed=%d failed=%d\n", passed, failed);
        return failed ? 1 : 0;
    } catch (const std::exception &error) {
        qaInject = false;
        std::printf("FATAL %s (Win32=%lu)\n", error.what(), GetLastError());
        return 2;
    }
}
