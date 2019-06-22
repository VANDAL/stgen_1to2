// Dead-simple conversion from SynchroTrace format v1 to v2.
// Dead-simple as in quite fragile.

#include <filesystem>
#include <thread>
#include "zfstream.h"
#include "fmt/format.h"
#include "fmt/printf.h"

namespace fs = std::filesystem;

// ----------------------------------------------------------------------------
bool is_comm_event(const char* line) {
  while (*line)
    if (*line++ == '#')
      return true;
  return false;
}


bool is_thread_event(const char* line) {
  while (*line)
    if (*line++ == '^')
      return true;
  return false;
}


bool is_marker_event(const char* line) {
  return line[0] == '!';
}


inline void advance_past_eid_and_tid(const char* str, char** next) {
  (void)std::strtoull(str, next, 0);
  (void)std::strtoull(*next+1, next, 0);
}


void convert_comm_event(const char* from, std::string& to) {
  char* next;
  advance_past_eid_and_tid(from, &next);

  // will either be a ' ' or '\0'
  while (*next == ' ') {

    // seek past ' # '
    next += 3;

    const uint64_t source_eid = std::strtoull(next, &next, 0);
    const int64_t source_tid = std::strtoll(next+1, &next, 0);
    const uintptr_t start_addr = std::strtoull(next+1, &next, 0);
    const uintptr_t end_addr = std::strtoull(next+1, &next, 0);
    fmt::format_to(std::back_inserter(to),
                   "# {:d} {:d} {:#x} {:#x} ",
                   source_eid,
                   source_tid,
                   start_addr,
                   end_addr);
  }
}


void convert_thread_event(const char* from, std::string& to) {
  char* next;
  advance_past_eid_and_tid(from, &next);

  // advance past ',pth_ty:'
  next += 8;

  const uint64_t syncType = strtoull(next, &next, 0);
  const uint64_t syncArgs = strtoull(next+1, &next, 0);
  fmt::format_to(std::back_inserter(to), "^ {:d}^{:#x}", syncType, syncArgs);

  // if there's another mutex variable
  if (*next == '&') {
    const uint64_t syncArgs = strtoull(next+1, &next, 0);
    fmt::format_to(std::back_inserter(to), "&{:#x}", syncArgs);
  }
}


void convert_compute_event(const char* from, std::string& to) {
  char* next;
  advance_past_eid_and_tid(from, &next);

  // example:
  // 100,100,12,14
  const uint64_t iops = std::strtoull(next+1, &next, 0);
  const uint64_t flops = std::strtoull(next+1, &next, 0);
  const uint64_t reads = std::strtoull(next+1, &next, 0);
  const uint64_t writes = std::strtoull(next+1, &next, 0);
  fmt::format_to(std::back_inserter(to), "@ {},{},{},{} ", iops, flops, reads, writes);

  // will be '\0' if end, no reads/writes
  if (*next != ' ')
    return;

  // seek past ' '
  ++next;

  while (*next == '$') {
    const uint64_t start_addr =  std::strtoull(next+2, &next, 0);
    const uint64_t end_addr =  std::strtoull(next+1, &next, 0);
    fmt::format_to(std::back_inserter(to), "$ {:#x} {:#x} ", start_addr, end_addr);

    // seek past space, otherwise we're at the end null character
    if (*next)
      ++next;
  }

  if (*next == '\0')
    return;

  // if not null, expected to be '*'
  while (*next == '*') {
    const uint64_t start_addr =  std::strtoull(next+2, &next, 0);
    const uint64_t end_addr =  std::strtoull(next+1, &next, 0);
    fmt::format_to(std::back_inserter(to), "* {:#x} {:#x} ", start_addr, end_addr);

    // seek past space, otherwise we're at the end null character
    if (*next)
      ++next;
  }
}


std::string& parse_stgen_1to2(const char* from, std::string& to) {
  to.clear();
  if (is_marker_event(from)) {
    to = from;  // marker events are unchanged
  } else if (is_comm_event(from)) {
    convert_comm_event(from, to);
  } else if (is_thread_event(from)) {
    convert_thread_event(from, to);
  } else {  // assume it's a compute event
    convert_compute_event(from, to);
  }
  to += "\n";
  return to;
}


// ----------------------------------------------------------------------------
void convert_file(fs::path from, fs::path to) {
  if (from.extension() != ".gz" || to.extension() != ".gz")
    return;

  gzifstream from_gz{from.c_str()};
  gzofstream to_gz{to.c_str()};

  // avoid allocating a new string each time
  std::string linev1;
  std::string linev2;

  while (std::getline(from_gz, linev1))
      to_gz << parse_stgen_1to2(linev1.c_str(), linev2);
}


void convert_dir(fs::path from_dir, fs::path to_dir) {
  if (!fs::is_directory(from_dir) || !fs::is_directory(to_dir))
    return;

  std::vector<std::thread> threads;
  for (auto di = fs::directory_iterator{from_dir}; di != fs::directory_iterator{}; di++)
    if ((di->is_regular_file() || di->is_symlink()) && (di->path().extension() == ".gz"))
      threads.emplace_back(convert_file, di->path(), to_dir / di->path().filename());

  for (auto& t : threads)
    t.join();
}


// ----------------------------------------------------------------------------
struct Args {
  fs::path inputDir_v1;
  // the directory to the v1 traces to be converted

  fs::path outputDir_v2;
  // the directory to the output directory where the v2 traces will be placed
};


Args parse_args(int argc, char* argv[]) {
  if (argc < 3) {
    fmt::printf("USAGE: %s INPUT_DIR OUTPUT_DIR\n", argv[0]);
    std::exit(EXIT_FAILURE);
  }
  return {argv[1], argv[2]};
}


int main(int argc, char* argv[]) {
  auto [from, to] = parse_args(argc, argv);
  convert_dir(from, to);
}
