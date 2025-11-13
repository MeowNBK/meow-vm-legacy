#include "meow_vm.h"
#include "pch.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#if !defined(_WIN32)
#include <unistd.h>
#include <limits.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

// --- helper: lấy thư mục chứa executable (cross-platform) ---
static std::filesystem::path getExecutableDir() {
#if defined(_WIN32)
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (len == 0) throw std::runtime_error("GetModuleFileNameA failed");
    return std::filesystem::path(std::string(buf, static_cast<size_t>(len))).parent_path();
#elif defined(__linux__)
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len == -1) throw std::runtime_error("readlink(/proc/self/exe) failed");
    buf[len] = '\0';
    return std::filesystem::path(std::string(buf)).parent_path();
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size); // will set size
    std::vector<char> buf(size);
    if (_NSGetExecutablePath(buf.data(), &size) != 0) {
        throw std::runtime_error("_NSGetExecutablePath failed");
    }
    std::filesystem::path p(buf.data());
    return std::filesystem::absolute(p).parent_path();
#else
    return std::filesystem::current_path();
#endif
}

// --- helper: expand token $ORIGIN in a path string to exeDir ---
static std::string expandOriginToken(const std::string& raw, const std::filesystem::path& exeDir) {
    std::string out;
    const std::string token = "$ORIGIN";
    size_t pos = 0;
    while (true) {
        size_t p = raw.find(token, pos);
        if (p == std::string::npos) {
            out.append(raw.substr(pos));
            break;
        }
        out.append(raw.substr(pos, p - pos));
        out.append(exeDir.string());
        pos = p + token.size();
    }
    return out;
}

// --- detect stdlib root: đọc file meow-root cạnh binary (1 lần, cached) ---
static std::filesystem::path detectStdlibRoot_cached() {
    static std::optional<std::filesystem::path> cached;
    if (cached.has_value()) return *cached;

    std::filesystem::path result;
    try {
        std::filesystem::path exeDir = getExecutableDir(); // thư mục chứa binary
        std::filesystem::path configFile = exeDir / "meow-root";

        // 1) nếu có file meow-root, đọc và expand $ORIGIN
        if (std::filesystem::exists(configFile)) {
            std::ifstream in(configFile);
            if (in) {
                std::string line;
                std::getline(in, line);
                // trim (simple)
                while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
                size_t i = 0;
                while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
                if (i > 0) line = line.substr(i);

                if (!line.empty()) {
                    std::string expanded = expandOriginToken(line, exeDir);
                    result = std::filesystem::absolute(std::filesystem::path(expanded));
                    cached = result;
                    return result;
                }
            }
        }

        // 2) fallback nhanh: nếu exeDir là "bin", dùng parent; ngược lại dùng exeDir
        if (exeDir.filename() == "bin") {
            result = exeDir.parent_path();
        } else {
            result = exeDir;
        }

        // 3) nếu không tìm thấy stdlib trực tiếp ở root, có thể thử root/lib
        cached = std::filesystem::absolute(result);
        return *cached;
    } catch (...) {
        // fallback to current path nếu có gì sai
        cached = std::filesystem::current_path();
        return *cached;
    }
}


static std::string platformLastError() {
#if defined(_WIN32)
    DWORD err = GetLastError();
    if (!err) return "";
    LPSTR msgBuf = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPSTR)&msgBuf, 0, nullptr);
    std::string msg = msgBuf ? msgBuf : "";
    if (msgBuf) LocalFree(msgBuf);
    return msg;
#else
    const char* e = dlerror();
    return e ? std::string(e) : std::string();
#endif
}

Module MeowVM::_getOrLoadModule(const Str& modulePath, const Str& importerPath, Bool isBinary) {
    if (auto it = moduleCache.find(modulePath); it != moduleCache.end()) {
        return it->second;
    }

#if defined(_WIN32)
    Str libExtension = ".dll";
#elif defined(__APPLE__)
    Str libExtension = ".dylib";
#else
    Str libExtension = ".so";
#endif

    auto resolveLibraryPath = [&](const Str& modPath, const Str& importer) -> Str {
        try {
            std::filesystem::path candidate(modPath);
            std::string ext = candidate.extension().string();

            if (!ext.empty()) {
                std::string extLower = ext;
                for (char &c : extLower) c = (char)std::tolower((unsigned char)c);
                if (extLower == ".meow" || extLower == ".meowb") {
                    return "";
                }
            }

            if (candidate.extension().empty()) {
                candidate.replace_extension(libExtension);
            }

            if (candidate.is_absolute() && std::filesystem::exists(candidate)) {
                return std::filesystem::absolute(candidate).lexically_normal().string();
            }

            std::filesystem::path stdlibRoot = detectStdlibRoot_cached();
            std::filesystem::path stdlibPath = stdlibRoot / candidate;
            if (std::filesystem::exists(stdlibPath)) {
                return std::filesystem::absolute(stdlibPath).lexically_normal().string();
            }

            // thử dưới stdlibRoot/lib/candidate
            stdlibPath = stdlibRoot / "lib" / candidate;
            if (std::filesystem::exists(stdlibPath)) {
                return std::filesystem::absolute(stdlibPath).lexically_normal().string();
            }

            // thử dưới stdlibRoot/stdlib/candidate
            stdlibPath = stdlibRoot / "stdlib" / candidate;
            if (std::filesystem::exists(stdlibPath)) {
                return std::filesystem::absolute(stdlibPath).lexically_normal().string();
            }

            // --- bổ sung: nhiều dự án đặt stdlib trong bin/stdlib hoặc stdlib trong bin ---
            stdlibPath = stdlibRoot / "bin" / "stdlib" / candidate;
            if (std::filesystem::exists(stdlibPath)) {
                return std::filesystem::absolute(stdlibPath).lexically_normal().string();
            }

            stdlibPath = stdlibRoot / "bin" / candidate;
            if (std::filesystem::exists(stdlibPath)) {
                return std::filesystem::absolute(stdlibPath).lexically_normal().string();
            }

            // optional: nếu meow-root thực sự trỏ lên một level, vẫn thử parent/bin/stdlib
            stdlibPath = std::filesystem::absolute(stdlibRoot / ".." / "bin" / "stdlib" / candidate);
            if (std::filesystem::exists(stdlibPath)) {
                return std::filesystem::absolute(stdlibPath).lexically_normal().string();
            }



            std::filesystem::path baseDir = (importer == entryPointDir)
                ? std::filesystem::path(entryPointDir)
                : std::filesystem::path(importer).parent_path();

            std::filesystem::path relativePath = std::filesystem::absolute(baseDir / candidate);
            if (std::filesystem::exists(relativePath)) {
                return relativePath.lexically_normal().string();
            }

            return "";
        } catch (const std::exception& ex) {
            return "";
        }
    };

    Str libPath = resolveLibraryPath(modulePath, importerPath);

    if (!libPath.empty()) {
        void* handle = nullptr;
#if defined(_WIN32)
        handle = (void*)LoadLibraryA(libPath.c_str());
#else
        dlerror();
        handle = dlopen(libPath.c_str(), RTLD_LAZY);
#endif
        if (!handle) {
            std::string detail = platformLastError();
            throw VMError("Không thể tải thư viện native: " + libPath + (detail.empty() ? "" : (" - " + detail)));
        }

        using NativeFunction = Module(*)(MeowEngine*);
        NativeFunction factory = nullptr;
#if defined(_WIN32)

        auto procAddress = GetProcAddress((HMODULE)handle, "CreateMeowModule");

        if (procAddress == nullptr) {
            std::string detail = platformLastError();
            throw VMError("Không tìm thấy cổng giao tiếp 'CreateMeowModule' trong " + libPath + (detail.empty() ? "" : (" - " + detail)));
        }

        factory = reinterpret_cast<NativeFunction>(procAddress);

#else
        dlerror();
        factory = (NativeFunction)dlsym(handle, "CreateMeowModule");
#endif

        Module nativeModule = factory(this);

        moduleCache[modulePath] = nativeModule;
        moduleCache[libPath] = nativeModule;

        return nativeModule;
    }

    std::filesystem::path baseDir = (importerPath == entryPointDir)
        ? std::filesystem::path(entryPointDir)
        : std::filesystem::path(importerPath).parent_path();
    std::filesystem::path resolvedPath = std::filesystem::absolute(baseDir / modulePath);
    Str absolutePath = resolvedPath.lexically_normal().string();

    if (auto it = moduleCache.find(absolutePath); it != moduleCache.end()) {
        return it->second;
    }

    std::unordered_map<Str, Proto> protos;
    if (isBinary) {
        if (!binaryParser.parseFile(absolutePath, *memoryManager))
            throw VMError("Binary parsing failed for file: " + absolutePath);
        protos = binaryParser.protos;
    } else {
        if (!textParser.parseFile(absolutePath, *memoryManager))
            throw VMError("Text parsing failed for file: " + absolutePath);
        protos = textParser.protos;
    }

    const Str mainName = "@main";
    auto pit = protos.find(mainName);
    if (pit == protos.end())
        throw VMError("Module '" + absolutePath + "' phải có một hàm chính tên là '" + mainName + "'.");

    auto newModule = memoryManager->newObject<ObjModule>(modulePath, absolutePath, isBinary);
    newModule->mainProto = pit->second;
    newModule->hasMain = true;

    if (newModule->name != "native") {
        auto itNative = moduleCache.find("native");
        if (itNative != moduleCache.end()) {
            for (const auto& [name, func] : itNative->second->globals) {
                newModule->globals[name] = func;
            }
        }
    }

    moduleCache[absolutePath] = newModule;
    return newModule;
}
