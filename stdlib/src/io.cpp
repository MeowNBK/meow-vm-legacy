
#include "meow_script.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <system_error>
#include <iostream>

namespace fs = std::filesystem;



Value native_io_input(Arguments args) {
    if (args.size() > 0 && !args[0].is<Str>()) return Value(Null{});
    if (args.size() > 0) std::cout << args[0].get<Str>();
    std::string line;
    if (!std::getline(std::cin, line)) return Value(Null{});
    return Value(line);
}


Value native_io_read(Arguments args) {
    if (args.size() < 1 || !args[0].is<Str>()) return Value(Null{});
    const Str& path = args[0].get<Str>();
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return Value(Null{});
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return Value(ss.str());
}


Value native_io_write(Arguments args) {
    if (args.size() < 2 || !args[0].is<Str>()) return Value(false);
    const Str& path = args[0].get<Str>();

    std::string data;
    if (args[1].is<Str>()) data = args[1].get<Str>();
    else {

        if (args[1].is<Int>()) data = std::to_string(args[1].get<Int>());
        else if (args[1].is<Real>()) {
            std::ostringstream ss; ss << args[1].get<Real>(); data = ss.str();
        } else if (args[1].is<Bool>()) data = args[1].get<Bool>() ? "true" : "false";
        else data = "";
    }
    bool append = false;
    if (args.size() > 2 && args[2].is<Bool>()) append = args[2].get<Bool>();

    std::ofstream ofs;
    if (append) ofs.open(path, std::ios::binary | std::ios::app);
    else ofs.open(path, std::ios::binary | std::ios::trunc);

    if (!ofs) return Value(false);
    ofs << data;
    return Value(true);
}


Value native_io_fileExists(Arguments args) {
    if (args.size() < 1 || !args[0].is<Str>()) return Value(false);
    const Str& path = args[0].get<Str>();
    std::error_code ec;
    return Value(fs::exists(path, ec));
}


Value native_io_isDirectory(Arguments args) {
    if (args.size() < 1 || !args[0].is<Str>()) return Value(false);
    const Str& path = args[0].get<Str>();
    std::error_code ec;
    return Value(fs::is_directory(path, ec));
}


Value native_io_listDir(Arguments args) {
    if (args.size() < 1 || !args[0].is<Str>()) return Value(Null{});
    const Str& path = args[0].get<Str>();
    std::error_code ec;
    if (!fs::exists(path, ec) || !fs::is_directory(path, ec)) return Value(Null{});
    Array out = new ObjArray();
    for (auto& entry : fs::directory_iterator(path, ec)) {
        if (ec) break;
        out->elements.emplace_back(Value(entry.path().filename().string()));
    }
    return Value(out);
}


Value native_io_createDir(Arguments args) {
    if (args.size() < 1 || !args[0].is<Str>()) return Value(false);
    const Str& path = args[0].get<Str>();
    std::error_code ec;
    bool ok = fs::create_directories(path, ec);
    if (ec) ok = false;
    return Value(ok);
}


Value native_io_deleteFile(Arguments args) {
    if (args.size() < 1 || !args[0].is<Str>()) return Value(false);
    const Str& path = args[0].get<Str>();
    std::error_code ec;
    bool ok = fs::remove(path, ec);
    if (ec) ok = false;
    return Value(ok);
}


Value native_io_getFileTimestamp(Arguments args) {
    if (args.size() < 1 || !args[0].is<Str>()) return Value(static_cast<Int>(-1));
    const Str& path = args[0].get<Str>();
    std::error_code ec;
    auto ftime = fs::last_write_time(path, ec);
    if (ec) return Value(static_cast<Int>(-1));

    using sys_clock = std::chrono::system_clock;
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + sys_clock::now()
    );
    std::time_t cftime = sys_clock::to_time_t(sctp);
    return Value(static_cast<Int>(cftime));
}


Value native_io_getFileSize(Arguments args) {
    if (args.size() < 1 || !args[0].is<Str>()) return Value(static_cast<Int>(-1));
    const Str& path = args[0].get<Str>();
    std::error_code ec;
    auto sz = fs::file_size(path, ec);
    if (ec) return Value(static_cast<Int>(-1));
    return Value(static_cast<Int>(sz));
}


Value native_io_renameFile(Arguments args) {
    if (args.size() < 2 || !args[0].is<Str>() || !args[1].is<Str>()) return Value(false);
    const Str& src = args[0].get<Str>();
    const Str& dst = args[1].get<Str>();
    std::error_code ec;
    fs::rename(src, dst, ec);
    return Value(!ec);
}


Value native_io_copyFile(Arguments args) {
    if (args.size() < 2 || !args[0].is<Str>() || !args[1].is<Str>()) return Value(false);
    const Str& src = args[0].get<Str>();
    const Str& dst = args[1].get<Str>();
    std::error_code ec;
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    return Value(!ec);
}


Value native_io_getFileName(Arguments args) {
    if (args.size() < 1 || !args[0].is<Str>()) return Value(Null{});
    const Str& path = args[0].get<Str>();
    return Value(fs::path(path).filename().string());
}


Value native_io_getFileStem(Arguments args) {
    if (args.size() < 1 || !args[0].is<Str>()) return Value(Null{});
    const Str& path = args[0].get<Str>();
    return Value(fs::path(path).stem().string());
}


Value native_io_getFileExtension(Arguments args) {
    if (args.size() < 1 || !args[0].is<Str>()) return Value(Null{});
    const Str& path = args[0].get<Str>();
    std::string ext = fs::path(path).extension().string();
    if (!ext.empty() && ext[0] == '.') ext.erase(0,1);
    return Value(ext);
}


Value native_io_getAbsolutePath(Arguments args) {
    if (args.size() < 1 || !args[0].is<Str>()) return Value(Null{});
    const Str& path = args[0].get<Str>();
    std::error_code ec;
    fs::path p = fs::absolute(path, ec);
    if (ec) return Value(Null{});
    return Value(p.string());
}


Module CreateMeowModule(MeowEngine* engine) {
    MemoryManager* mm = engine->getMemoryManager();
    auto mod = mm->newObject<ObjModule>("io", "native:io");

    mod->exports["input"] = Value(native_io_input);
    mod->exports["read"]  = Value(native_io_read);
    mod->exports["write"] = Value(native_io_write);
    mod->exports["fileExists"] = Value(native_io_fileExists);
    mod->exports["isDirectory"] = Value(native_io_isDirectory);
    mod->exports["listDir"] = Value(native_io_listDir);
    mod->exports["createDir"] = Value(native_io_createDir);
    mod->exports["deleteFile"] = Value(native_io_deleteFile);

    mod->exports["getFileTimestamp"] = Value(native_io_getFileTimestamp);
    mod->exports["getFileSize"] = Value(native_io_getFileSize);
    mod->exports["renameFile"] = Value(native_io_renameFile);
    mod->exports["copyFile"] = Value(native_io_copyFile);

    mod->exports["getFileName"] = Value(native_io_getFileName);
    mod->exports["getFileStem"] = Value(native_io_getFileStem);
    mod->exports["getFileExtension"] = Value(native_io_getFileExtension);
    mod->exports["getAbsolutePath"] = Value(native_io_getAbsolutePath);

    return mod;
}
