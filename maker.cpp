// -O3 -march=native -Wall -Wextra -Wpedantic -Wshadow -Wfloat-equal
// -Wconversion -Wsign-conversion -Wcast-qual -Wcast-align -Wold-style-cast
// -Wuseless-cast -Wzero-as-null-pointer-constant -Wdouble-promotion
// -Wnull-dereference -Wformat=2 -Wformat-overflow=2 -Wformat-truncation=2
// -Wshift-overflow=2 -Wshift-negative-value -Warray-bounds=2
// -Wstringop-overflow=4 -Wstrict-overflow=4 -Wimplicit-fallthrough=5
// -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wrestrict
// -Winit-self
// -Wundef -Wuninitialized -Wmaybe-uninitialized -Wunused -Wunused-parameter
// -Wunused-but-set-variable -Wredundant-decls -Wmissing-declarations
// -Wtrampolines -Wvector-operation-performance -Wdisabled-optimization
// -Woverloaded-virtual -Wctor-dtor-privacy -Wnon-virtual-dtor
// -Wno-unused-result -Wno-sign-compare -D_GLIBCXX_DEBUG
// -D_GLIBCXX_DEBUG_PEDANTIC -D_FORTIFY_SOURCE=2
// -fsanitize=address,undefined,float-divide-by-zero,float-cast-overflow,bounds-strict
// -fno-sanitize-recover=all -fno-omit-frame-pointer -fstack-protector-all
// -fstrict-aliasing -Wstrict-aliasing=3 -g3 -ggdb

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <sstream>

namespace fs = std::filesystem;

// ---------------- FORWARD DECLARATIONS ----------------
uint64_t hashFile(const fs::path &path);
uint64_t hashString(const std::string &text);
uint64_t buildFingerprint(const fs::path &src, const std::string &signature);
int parse(int argc, char *argv[]);
bool isSafeFilename(const std::string &name);
void setupCcache();
int safeExec(const std::vector<std::string> &args);
int compile();
bool build(const fs::path &src, const fs::path &save,
           const std::string &signature);
void saveHash(const fs::path &src, const fs::path &save,
              const std::string &signature);
std::string findExecutable(const std::string &name);
std::vector<std::string> getFlags();
std::string joinFlags(const std::vector<std::string> &flags);

// ------------------ VARIABLES ---------------------------
std::string filename;
std::string cppVer;
std::string outputName;
bool oFlag = false;
bool iFlag = false;
bool noRun = false;
bool fast = false;
bool vFlag = false;
bool hasCcache = false;
bool ufFlags = false;

// kolory
constexpr const char *CLR_RESET = "\033[0m";
constexpr const char *CLR_RED = "\033[31m";
constexpr const char *CLR_GREEN = "\033[32m";
constexpr const char *CLR_YELLOW = "\033[33m";
constexpr const char *CLR_BLUE = "\033[34m";

// ---------------- HASH ----------------
// simple fast 64-bit FNV-1a
uint64_t hashFile(const fs::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return 0;

  uint64_t hash = 1469598103934665603ULL;
  char buffer[8192];

  while (file.read(buffer, sizeof(buffer)) || file.gcount()) {
    std::streamsize n = file.gcount();
    for (std::streamsize i = 0; i < n; ++i) {
      hash ^= static_cast<unsigned char>(buffer[i]);
      hash *= 1099511628211ULL;
    }
  }

  return hash;
}

uint64_t hashString(const std::string &text) {
  uint64_t hash = 1469598103934665603ULL;
  for (unsigned char c : text) {
    hash ^= c;
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::string joinFlags(const std::vector<std::string> &flags) {
  std::string out;
  for (const auto &f : flags) {
    out += f;
    out += '\n';
  }
  return out;
}

uint64_t buildFingerprint(const fs::path &src, const std::string &signature) {
  return hashString(std::to_string(hashFile(src)) + "\n" + signature);
}

std::vector<std::string> getFlags() {
  if (fast) {
    return {"-O3",
            "-march=native",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Wno-sign-compare",
            "-Wno-unused-result",
            "-funroll-loops",
            "-flto"};
  }

  if (ufFlags) {
    return {"-Ofast",           "-march=native", "-mtune=native",
            "-ftree-vectorize", "-ffast-math",   "-fomit-frame-pointer",
            "-funroll-loops",   "-flto",         "-fstrict-aliasing",
            "-fno-exceptions",  "-fno-rtti"};
  }

  return {"-O0",
          "-g3",
          "-ggdb",
          "-Wall",
          "-Wextra",
          "-Wpedantic",
          "-Wshadow",
          "-Wfloat-equal",
          "-Wconversion",
          "-Wsign-conversion",
          "-Wcast-qual",
          "-Wcast-align",
          "-Wold-style-cast",
          "-Wuseless-cast",
          "-Wzero-as-null-pointer-constant",
          "-Wdouble-promotion",
          "-Wnull-dereference",
          "-Wformat=2",
          "-Wformat-overflow=2",
          "-Wformat-truncation=2",
          "-Wshift-overflow=2",
          "-Wshift-negative-value",
          "-Warray-bounds=2",
          "-Wstringop-overflow=4",
          "-Wstrict-overflow=4",
          "-Wimplicit-fallthrough=5",
          "-Wduplicated-cond",
          "-Wduplicated-branches",
          "-Wlogical-op",
          "-Wrestrict",
          "-Winit-self",
          "-Wundef",
          "-Wuninitialized",
          "-Wmaybe-uninitialized",
          "-Wunused",
          "-Wunused-parameter",
          "-Wunused-but-set-variable",
          "-Wredundant-decls",
          "-Wmissing-declarations",
          "-Wtrampolines",
          "-Wvector-operation-performance",
          "-Wdisabled-optimization",
          "-Woverloaded-virtual",
          "-Wctor-dtor-privacy",
          "-Wnon-virtual-dtor",
          "-Wno-unused-result",
          "-Wno-sign-compare",
          "-D_GLIBCXX_DEBUG",
          "-D_GLIBCXX_DEBUG_PEDANTIC",
          "-D_FORTIFY_SOURCE=2",
          "-fsanitize=address,undefined,float-divide-by-zero,float-cast-"
          "overflow,bounds-strict",
          "-fno-sanitize-recover=all",
          "-fno-omit-frame-pointer",
          "-fstack-protector-all",
          "-fstrict-aliasing",
          "-Wstrict-aliasing=3"};
}

// ---------------- PARSE ----------------
int parse(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << CLR_YELLOW
              << "Usage: maker <file.cpp> [-o](format) [-i](c++17) "
                 "[-r](don't run) [-f](fast mode, debug is deafult) "
                 "[-v](version) [-uf](ultra fast flags)\n"
              << CLR_RESET;
    return 1;
  }

  filename = argv[1];

  if (filename == "-v") {
    vFlag = true;
    return 0;
  }

  for (int i = 2; i < argc; i++) {
    std::string_view arg = argv[i];
    if (arg == "-o")
      oFlag = true;
    else if (arg == "-i")
      iFlag = true;
    else if (arg == "-r")
      noRun = true;
    else if (arg == "-f")
      fast = true;
    else if (arg == "-uf")
      ufFlags = true;
  }

  return 0;
}

// ---------------- FILENAME VALIDATION ----------------
bool isSafeFilename(const std::string &name) {
  std::regex valid("^[a-zA-Z0-9_\\-\\.]+$");
  return std::regex_match(name, valid);
}

// ---------------- CCACHE ----------------
// std::string findExecutable(const std::string &name) {
//   const char *pathEnv = std::getenv("PATH");
//   if (!pathEnv)
//     return "";

//   std::string pathStr = pathEnv;
//   std::string delimiter = ":"; // Na Windowsie byłoby ";"
//   size_t pos = 0;

//   while ((pos = pathStr.find(delimiter)) != std::string::npos ||
//          !pathStr.empty()) {
//     std::string dir = pathStr.substr(0, pos);
//     fs::path fullPath = fs::path(dir) / name;

//     if (fs::exists(fullPath)) {
//       return fullPath.string();
//     }

//     if (pos == std::string::npos)
//       break;
//     pathStr.erase(0, pos + delimiter.length());
//   }
//   return "";
// }

std::string findExecutable(const std::string &name) {
  const char *pathEnv = std::getenv("PATH");
  if (!pathEnv)
    return "";

  std::istringstream stream(pathEnv);
  std::string dir;

  while (std::getline(stream, dir, ':')) {
    fs::path fullPath = fs::path(dir) / name;
    if (fs::exists(fullPath))
      return fullPath.string();
  }

  return "";
}

void setupCcache() {
  std::string ccachePath = findExecutable("ccache");

  if (!ccachePath.empty()) {
    hasCcache = true;

    const char *home = std::getenv("HOME");
    std::string ccacheDir;
    if (home) {
      ccacheDir = std::string(home) + "/.cache/ccache";
    } else {
      ccacheDir = "/tmp/ccache_fallback";
    }

    setenv("CCACHE_MAXSIZE", "5G", 0);
    setenv("CCACHE_DIR", ccacheDir.c_str(), 0);
    setenv("CCACHE_SLOPPINESS", "time_macros", 0);
  }
}

// ---------------- SAFE EXEC ----------------
int safeExec(const std::vector<std::string> &args) {
  std::vector<const char *> cargs;
  for (const auto &arg : args)
    cargs.push_back(arg.c_str());
  cargs.push_back(nullptr);

  pid_t pid = fork();
  if (pid == 0) {
    execvp(cargs[0], const_cast<char *const *>(cargs.data()));
    std::perror("execvp failed");
    std::exit(1);
  } else if (pid < 0) {
    std::perror("fork failed");
    return 1;
  } else {
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
  }
}

// ---------------- COMPILE ----------------
int compile() {
  if (!isSafeFilename(filename) || !isSafeFilename(outputName)) {
    std::cerr << CLR_RED << "Unsafe filename detected!" << CLR_RESET << '\n';
    return 1;
  }

  std::string compilerPath = findExecutable("g++");
  if (compilerPath.empty()) {
    std::cerr << CLR_RED << "No g++ detected!" << CLR_RESET << '\n';
    return 1;
  }

  std::vector<std::string> cmd;
  if (hasCcache)
    cmd.push_back("ccache");
  cmd.push_back(compilerPath);
  cmd.push_back(filename);
  cmd.push_back("-o");
  cmd.push_back(outputName);
  cmd.push_back("-std=" + cppVer);

  auto flags = getFlags();
  cmd.insert(cmd.end(), flags.begin(), flags.end());

  return safeExec(cmd);
}

// ---------------- BUILD CHECK ----------------
bool build(const fs::path &src, const fs::path &save,
           const std::string &signature) {
  uint64_t currentHash = buildFingerprint(src, signature);

  if (!fs::exists(save))
    return true;

  std::ifstream in(save);
  uint64_t oldHash = 0;
  in >> oldHash;

  return currentHash != oldHash;
}

void saveHash(const fs::path &src, const fs::path &save,
              const std::string &signature) {
  uint64_t h = buildFingerprint(src, signature);
  std::ofstream out(save);
  out << h;
}

// ---------------- MAIN ----------------
int main(int argc, char *argv[]) {
  if (parse(argc, argv)) {
    return 1;
  }

  if (vFlag) {
    std::cout << CLR_RED << "|--------- MAKER ---------\n"
              << CLR_GREEN << "|version 1.1.0 \n"
              << CLR_YELLOW << "|autor -> ABK \n"
              << CLR_BLUE << "|slightly vibecoded -_-\n"
              << CLR_RED << "|-------------------------" << CLR_RESET;
    return 0;
  }

  if (!isSafeFilename(filename)) {
    std::cerr << CLR_RED << "Invalid filename!" << CLR_RESET << '\n';
    return 1;
  }

  setupCcache();

  fs::path srcPath(filename);
  outputName = srcPath.stem().string();
  fs::path savePath = outputName + ".save";

  cppVer = iFlag ? "c++17" : "c++23";
  std::string buildSignature = cppVer + "\n" + joinFlags(getFlags());

  if (oFlag) {
    safeExec({"clang-format", "-i", filename});
  }

  if (build(srcPath, savePath, buildSignature)) {
    std::cout << CLR_BLUE << "Compiling..." << CLR_RESET << '\n';

    if (compile())
      return 1;// -O3 -march=native -Wall -Wextra -Wpedantic -Wshadow -Wfloat-equal
// -Wconversion -Wsign-conversion -Wcast-qual -Wcast-align -Wold-style-cast
// -Wuseless-cast -Wzero-as-null-pointer-constant -Wdouble-promotion
// -Wnull-dereference -Wformat=2 -Wformat-overflow=2 -Wformat-truncation=2
// -Wshift-overflow=2 -Wshift-negative-value -Warray-bounds=2
// -Wstringop-overflow=4 -Wstrict-overflow=4 -Wimplicit-fallthrough=5
// -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wrestrict
// -Winit-self
// -Wundef -Wuninitialized -Wmaybe-uninitialized -Wunused -Wunused-parameter
// -Wunused-but-set-variable -Wredundant-decls -Wmissing-declarations
// -Wtrampolines -Wvector-operation-performance -Wdisabled-optimization
// -Woverloaded-virtual -Wctor-dtor-privacy -Wnon-virtual-dtor
// -Wno-unused-result -Wno-sign-compare -D_GLIBCXX_DEBUG
// -D_GLIBCXX_DEBUG_PEDANTIC -D_FORTIFY_SOURCE=2
// -fsanitize=address,undefined,float-divide-by-zero,float-cast-overflow,bounds-strict
// -fno-sanitize-recover=all -fno-omit-frame-pointer -fstack-protector-all
// -fstrict-aliasing -Wstrict-aliasing=3 -g3 -ggdb

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <sstream>

namespace fs = std::filesystem;

// ---------------- FORWARD DECLARATIONS ----------------
uint64_t hashFile(const fs::path &path);
uint64_t hashString(const std::string &text);
uint64_t buildFingerprint(const fs::path &src, const std::string &signature);
int parse(int argc, char *argv[]);
bool isSafeFilename(const std::string &name);
void setupCcache();
int safeExec(const std::vector<std::string> &args);
int compile();
bool build(const fs::path &src, const fs::path &save,
           const std::string &signature);
void saveHash(const fs::path &src, const fs::path &save,
              const std::string &signature);
std::string findExecutable(const std::string &name);
std::vector<std::string> getFlags();
std::string joinFlags(const std::vector<std::string> &flags);

// ------------------ VARIABLES ---------------------------
std::string filename;
std::string cppVer;
std::string outputName;
bool oFlag = false;
bool iFlag = false;
bool noRun = false;
bool fast = false;
bool vFlag = false;
bool hasCcache = false;
bool ufFlags = false;

// kolory
constexpr const char *CLR_RESET = "\033[0m";
constexpr const char *CLR_RED = "\033[31m";
constexpr const char *CLR_GREEN = "\033[32m";
constexpr const char *CLR_YELLOW = "\033[33m";
constexpr const char *CLR_BLUE = "\033[34m";

// ---------------- HASH ----------------
// simple fast 64-bit FNV-1a
uint64_t hashFile(const fs::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return 0;

  uint64_t hash = 1469598103934665603ULL;
  char buffer[8192];

  while (file.read(buffer, sizeof(buffer)) || file.gcount()) {
    std::streamsize n = file.gcount();
    for (std::streamsize i = 0; i < n; ++i) {
      hash ^= static_cast<unsigned char>(buffer[i]);
      hash *= 1099511628211ULL;
    }
  }

  return hash;
}

uint64_t hashString(const std::string &text) {
  uint64_t hash = 1469598103934665603ULL;
  for (unsigned char c : text) {
    hash ^= c;
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::string joinFlags(const std::vector<std::string> &flags) {
  std::string out;
  for (const auto &f : flags) {
    out += f;
    out += '\n';
  }
  return out;
}

uint64_t buildFingerprint(const fs::path &src, const std::string &signature) {
  return hashString(std::to_string(hashFile(src)) + "\n" + signature);
}

std::vector<std::string> getFlags() {
  if (fast) {
    return {"-O3",
            "-march=native",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Wno-sign-compare",
            "-Wno-unused-result",
            "-funroll-loops",
            "-flto"};
  }

  if (ufFlags) {
    return {"-Ofast",           "-march=native", "-mtune=native",
            "-ftree-vectorize", "-ffast-math",   "-fomit-frame-pointer",
            "-funroll-loops",   "-flto",         "-fstrict-aliasing",
            "-fno-exceptions",  "-fno-rtti"};
  }

  return {"-O0",
          "-g3",
          "-ggdb",
          "-Wall",
          "-Wextra",
          "-Wpedantic",
          "-Wshadow",
          "-Wfloat-equal",
          "-Wconversion",
          "-Wsign-conversion",
          "-Wcast-qual",
          "-Wcast-align",
          "-Wold-style-cast",
          "-Wuseless-cast",
          "-Wzero-as-null-pointer-constant",
          "-Wdouble-promotion",
          "-Wnull-dereference",
          "-Wformat=2",
          "-Wformat-overflow=2",
          "-Wformat-truncation=2",
          "-Wshift-overflow=2",
          "-Wshift-negative-value",
          "-Warray-bounds=2",
          "-Wstringop-overflow=4",
          "-Wstrict-overflow=4",
          "-Wimplicit-fallthrough=5",
          "-Wduplicated-cond",
          "-Wduplicated-branches",
          "-Wlogical-op",
          "-Wrestrict",
          "-Winit-self",
          "-Wundef",
          "-Wuninitialized",
          "-Wmaybe-uninitialized",
          "-Wunused",
          "-Wunused-parameter",
          "-Wunused-but-set-variable",
          "-Wredundant-decls",
          "-Wmissing-declarations",
          "-Wtrampolines",
          "-Wvector-operation-performance",
          "-Wdisabled-optimization",
          "-Woverloaded-virtual",
          "-Wctor-dtor-privacy",
          "-Wnon-virtual-dtor",
          "-Wno-unused-result",
          "-Wno-sign-compare",
          "-D_GLIBCXX_DEBUG",
          "-D_GLIBCXX_DEBUG_PEDANTIC",
          "-D_FORTIFY_SOURCE=2",
          "-fsanitize=address,undefined,float-divide-by-zero,float-cast-"
          "overflow,bounds-strict",
          "-fno-sanitize-recover=all",
          "-fno-omit-frame-pointer",
          "-fstack-protector-all",
          "-fstrict-aliasing",
          "-Wstrict-aliasing=3"};
}

// ---------------- PARSE ----------------
int parse(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << CLR_YELLOW
              << "Usage: maker <file.cpp> [-o](format) [-i](c++17) "
                 "[-r](don't run) [-f](fast mode, debug is deafult) "
                 "[-v](version) [-uf](ultra fast flags)\n"
              << CLR_RESET;
    return 1;
  }

  filename = argv[1];

  if (filename == "-v") {
    vFlag = true;
    return 0;
  }

  for (int i = 2; i < argc; i++) {
    std::string_view arg = argv[i];
    if (arg == "-o")
      oFlag = true;
    else if (arg == "-i")
      iFlag = true;
    else if (arg == "-r")
      noRun = true;
    else if (arg == "-f")
      fast = true;
    else if (arg == "-uf")
      ufFlags = true;
  }

  return 0;
}

// ---------------- FILENAME VALIDATION ----------------
bool isSafeFilename(const std::string &name) {
  std::regex valid("^[a-zA-Z0-9_\\-\\.]+$");
  return std::regex_match(name, valid);
}

// ---------------- CCACHE ----------------
// std::string findExecutable(const std::string &name) {
//   const char *pathEnv = std::getenv("PATH");
//   if (!pathEnv)
//     return "";

//   std::string pathStr = pathEnv;
//   std::string delimiter = ":"; // Na Windowsie byłoby ";"
//   size_t pos = 0;

//   while ((pos = pathStr.find(delimiter)) != std::string::npos ||
//          !pathStr.empty()) {
//     std::string dir = pathStr.substr(0, pos);
//     fs::path fullPath = fs::path(dir) / name;

//     if (fs::exists(fullPath)) {
//       return fullPath.string();
//     }

//     if (pos == std::string::npos)
//       break;
//     pathStr.erase(0, pos + delimiter.length());
//   }
//   return "";
// }

std::string findExecutable(const std::string &name) {
  const char *pathEnv = std::getenv("PATH");
  if (!pathEnv)
    return "";

  std::istringstream stream(pathEnv);
  std::string dir;

  while (std::getline(stream, dir, ':')) {
    fs::path fullPath = fs::path(dir) / name;
    if (fs::exists(fullPath))
      return fullPath.string();
  }

  return "";
}

void setupCcache() {
  std::string ccachePath = findExecutable("ccache");

  if (!ccachePath.empty()) {
    hasCcache = true;

    const char *home = std::getenv("HOME");
    std::string ccacheDir;
    if (home) {
      ccacheDir = std::string(home) + "/.cache/ccache";
    } else {
      ccacheDir = "/tmp/ccache_fallback";
    }

    setenv("CCACHE_MAXSIZE", "5G", 0);
    setenv("CCACHE_DIR", ccacheDir.c_str(), 0);
    setenv("CCACHE_SLOPPINESS", "time_macros", 0);
  }
}

// ---------------- SAFE EXEC ----------------
int safeExec(const std::vector<std::string> &args) {
  std::vector<const char *> cargs;
  for (const auto &arg : args)
    cargs.push_back(arg.c_str());
  cargs.push_back(nullptr);

  pid_t pid = fork();
  if (pid == 0) {
    execvp(cargs[0], const_cast<char *const *>(cargs.data()));
    std::perror("execvp failed");
    std::exit(1);
  } else if (pid < 0) {
    std::perror("fork failed");
    return 1;
  } else {
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
  }
}

// ---------------- COMPILE ----------------
int compile() {
  if (!isSafeFilename(filename) || !isSafeFilename(outputName)) {
    std::cerr << CLR_RED << "Unsafe filename detected!" << CLR_RESET << '\n';
    return 1;
  }

  std::string compilerPath = findExecutable("g++");
  if (compilerPath.empty()) {
    std::cerr << CLR_RED << "No g++ detected!" << CLR_RESET << '\n';
    return 1;
  }

  std::vector<std::string> cmd;
  if (hasCcache)
    cmd.push_back("ccache");
  cmd.push_back(compilerPath);
  cmd.push_back(filename);
  cmd.push_back("-o");
  cmd.push_back(outputName);
  cmd.push_back("-std=" + cppVer);

  auto flags = getFlags();
  cmd.insert(cmd.end(), flags.begin(), flags.end());

  return safeExec(cmd);
}

// ---------------- BUILD CHECK ----------------
bool build(const fs::path &src, const fs::path &save,
           const std::string &signature) {
  uint64_t currentHash = buildFingerprint(src, signature);

  if (!fs::exists(save))
    return true;

  std::ifstream in(save);
  uint64_t oldHash = 0;
  in >> oldHash;

  return currentHash != oldHash;
}

void saveHash(const fs::path &src, const fs::path &save,
              const std::string &signature) {
  uint64_t h = buildFingerprint(src, signature);
  std::ofstream out(save);
  out << h;
}

// ---------------- MAIN ----------------
int main(int argc, char *argv[]) {
  if (parse(argc, argv)) {
    return 1;
  }

  if (vFlag) {
    std::cout << CLR_RED << "|--------- MAKER ---------\n"
              << CLR_GREEN << "|version 1.1.0 \n"
              << CLR_YELLOW << "|autor -> ABK \n"
              << CLR_BLUE << "|slightly vibecoded -_-\n"
              << CLR_RED << "|-------------------------" << CLR_RESET;
    return 0;
  }

  if (!isSafeFilename(filename)) {
    std::cerr << CLR_RED << "Invalid filename!" << CLR_RESET << '\n';
    return 1;
  }

  setupCcache();

  fs::path srcPath(filename);
  outputName = srcPath.stem().string();
  fs::path savePath = outputName + ".save";

  cppVer = iFlag ? "c++17" : "c++23";
  std::string buildSignature = cppVer + "\n" + joinFlags(getFlags());

  if (oFlag) {
    safeExec({"clang-format", "-i", filename});
  }

  if (build(srcPath, savePath, buildSignature)) {
    std::cout << CLR_BLUE << "Compiling..." << CLR_RESET << '\n';

    if (compile())
      return 1;

    std::cout << CLR_GREEN << "Ready!" << CLR_RESET << '\n';
    saveHash(srcPath, savePath, buildSignature);
  } else {
    std::cout << CLR_YELLOW << "Skipping build!" << CLR_RESET << '\n';
  }

  int result = 0;
  if (!noRun) {
    result = safeExec({"./" + outputName});
  }

  return result != 0;
}

    std::cout << CLR_GREEN << "Ready!" << CLR_RESET << '\n';
    saveHash(srcPath, savePath, buildSignature);
  } else {
    std::cout << CLR_YELLOW << "Skipping build!" << CLR_RESET << '\n';
  }

  int result = 0;
  if (!noRun) {
    result = safeExec({"./" + outputName});
  }

  return result != 0;
}