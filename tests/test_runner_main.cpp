#include <gtest/gtest.h>
#include <iostream>
#include <filesystem>
#include <fstream>

// Portable helpers for Windows paths
static std::filesystem::path exe_dir_from_argv0(const char* argv0) {
  std::error_code ec;
  std::filesystem::path p(argv0);
  // If argv0 is not absolute, try current_path / argv0
  if (!p.has_root_path()) {
    p = std::filesystem::current_path(ec) / p;
  }
  return p.parent_path();
}

static std::string exe_basename_no_ext(const char* argv0) {
  std::filesystem::path p(argv0);
  return p.stem().string();
}

int main(int argc, char** argv) {
  // Initialize GoogleTest
  ::testing::InitGoogleTest(&argc, argv);

  // Determine output directories next to the exe
  auto exe_dir = exe_dir_from_argv0(argv[0]);
  auto exe_name = exe_basename_no_ext(argv[0]);
  std::error_code ec;

  auto logs_dir = exe_dir / "logs";
  auto reports_dir = exe_dir / "reports";
  std::filesystem::create_directories(logs_dir, ec);
  std::filesystem::create_directories(reports_dir, ec);

  // Prepare log file path and open append stream
  auto log_path = logs_dir / (exe_name + std::string(".log"));
  std::ofstream log_stream(log_path, std::ios::out | std::ios::app);

  // Inform the user where logs/reports will be saved
  std::cout << "Running tests in: " << exe_name << "\n";
  std::cout << "Log file: " << log_path.string() << "\n";
  std::cout << "XML report: set via --gtest_output=xml:<path> (optional). Default disabled.\n";

  // Mirror console output to log file (simple tee)
  struct TeeBuf : public std::streambuf {
    std::streambuf* sb1; std::streambuf* sb2;
    TeeBuf(std::streambuf* s1, std::streambuf* s2) : sb1(s1), sb2(s2) {}
    virtual int overflow(int c) override {
      if (c == EOF) return 0;
      int r1 = sb1->sputc(c);
      int r2 = sb2 ? sb2->sputc(c) : c;
      return (r1 == EOF || r2 == EOF) ? EOF : c;
    }
    virtual int sync() override {
      int r1 = sb1->pubsync();
      int r2 = sb2 ? sb2->pubsync() : 0;
      return (r1 == 0 && r2 == 0) ? 0 : -1;
    }
  };

  TeeBuf tee(std::cout.rdbuf(), log_stream.rdbuf());
  std::ostream out(&tee);

  // Redirect GoogleTest's printer to our tee by temporarily swapping cout's rdbuf
  auto* oldbuf = std::cout.rdbuf(tee.sb1); // keep std::cout printing to console
  // We'll explicitly print to 'out' for our messages to be mirrored
  out << "Starting GoogleTest...\n";

  int result = RUN_ALL_TESTS();

  out << "\nTests finished with code: " << result << "\n";
  out << "Press Enter to exit..." << std::endl;

  // Restore cout buffer and flush
  std::cout.rdbuf(oldbuf);
  log_stream.flush();

  // Pause so double-clicked exe keeps the window open
  std::cin.get();
  return result;
}
