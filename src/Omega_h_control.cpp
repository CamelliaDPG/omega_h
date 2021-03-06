#include "Omega_h_control.hpp"

#include <cxxabi.h>
#include <execinfo.h>
#include <csignal>
#include <cstdarg>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "Omega_h_cmdline.hpp"
#include "Omega_h_library.hpp"

namespace Omega_h {

bool should_log_memory = false;
char* max_memory_stacktrace = nullptr;

// stacktrace.h (c) 2008, Timo Bingmann from http://idlebox.net/
// published under the WTFPL v2.0

/* Dan Ibanez: found this code for stacktrace printing,
   cleaned it up for compilation as strict C++11,
   fixed the parser for OS X */

/** Print a demangled stack backtrace of the caller function to FILE* out. */

void print_stacktrace(std::ostream& out, int max_frames) {
  out << "stack trace:\n";
  // storage array for stack trace address data
  void** addrlist = new void*[max_frames];
  // retrieve current stack addresses
  int addrlen = backtrace(addrlist, max_frames);
  if (addrlen == 0) {
    out << "  <empty, possibly corrupt>\n";
    return;
  }
  // resolve addresses into strings containing "filename(function+address)",
  // this array must be free()-ed
  char** symbollist = backtrace_symbols(addrlist, addrlen);
  delete[] addrlist;
  // iterate over the returned symbol lines. skip the first, it is the
  // address of this function.
  for (int i = 1; i < addrlen; i++) {
#if defined(__APPLE__)
    std::string line(symbollist[i]);
    std::stringstream instream(line);
    std::string num;
    instream >> num;
    std::string obj;
    instream >> obj;
    obj.resize(20, ' ');
    std::string addr;
    instream >> addr;
    std::string symbol;
    instream >> symbol;
    std::string offset;
    instream >> offset >> offset;
    int status;
    char* demangled = abi::__cxa_demangle(symbol.c_str(), NULL, NULL, &status);
    if (status == 0) {
      symbol = demangled;
    }
    free(demangled);
    out << "  " << num << ' ' << obj << ' ' << addr << ' ' << symbol << " + "
        << offset << '\n';
#elif defined(__linux__)
    std::string line(symbollist[i]);
    auto open_paren = line.find_first_of('(');
    auto start = open_paren + 1;
    auto plus = line.find_first_of('+');
    if (!(start < line.size() && start < plus && plus < line.size())) {
      out << symbollist[i] << '\n';
      continue;
    }
    auto symbol = line.substr(start, (plus - start));
    int status;
    char* demangled =
        abi::__cxa_demangle(symbol.c_str(), nullptr, nullptr, &status);
    if (status == 0) {
      symbol = demangled;
    }
    free(demangled);
    out << line.substr(0, start) << symbol << line.substr(plus, line.size())
        << '\n';
#else
    out << symbollist[i] << '\n';
    fprintf(out, "%s\n", symbollist[i]);
#endif
  }
  free(symbollist);
}

char const* Library::static_version() { return OMEGA_H_SEMVER; }

char const* Library::version() { return static_version(); }

#ifdef OMEGA_H_CHECK_FPE
#if defined(__GNUC__) && (!defined(__clang__))
#define _GNU_SOURCE 1
#include <fenv.h>
static void enable_floating_point_exceptions() {
  feclearexcept(FE_ALL_EXCEPT);
  // FE_INEXACT inexact result: rounding was necessary to store the result of an earlier floating-point operation
  // sounds like the above would happen in almost any floating point operation involving non-whole numbers ???
  // As for underflow, there are plenty of cases where we will have things like ((a + eps) - (a)) -> eps,
  // where eps can be arbitrarily close to zero (usually it would have been zero with infinite precision).
  feenableexcept(FE_ALL_EXCEPT - FE_INEXACT - FE_UNDERFLOW);
}
#elif defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
// Intel system
static void enable_floating_point_exceptions() {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif
  _MM_SET_EXCEPTION_MASK(_MM_GET_EXCEPTION_MASK() & ~_MM_MASK_INVALID);
#ifdef __clang__
#pragma clang diagnostic pop
#endif
}
#else
#error "FPE enabled but not supported"
#endif
#else  // don't check FPE
static void enable_floating_point_exceptions() {}
#endif

void Library::initialize(char const* head_desc, int* argc, char*** argv
#ifdef OMEGA_H_USE_MPI
    ,
    MPI_Comm comm_mpi
#endif
) {
  std::string lib_desc = OMEGA_H_SEMVER;
  if (lib_desc != head_desc) {
    std::stringstream msg;
    msg << "omega_h description string mismatch.\n";
    msg << "header says: " << head_desc << '\n';
    msg << "library says: " << lib_desc << '\n';
    std::string msg_str = msg.str();
    Omega_h_fail("%s\n", msg_str.c_str());
  }
#ifdef OMEGA_H_USE_MPI
  int mpi_is_init;
  OMEGA_H_CHECK(MPI_SUCCESS == MPI_Initialized(&mpi_is_init));
  if (!mpi_is_init) {
    OMEGA_H_CHECK(MPI_SUCCESS == MPI_Init(argc, argv));
    we_called_mpi_init = true;
  } else {
    we_called_mpi_init = false;
  }
  enable_floating_point_exceptions();
  MPI_Comm world_dup;
  MPI_Comm_dup(comm_mpi, &world_dup);
  world_ = CommPtr(new Comm(this, world_dup));
#else
  world_ = CommPtr(new Comm(this, false, false));
  self_ = CommPtr(new Comm(this, false, false));
#endif
  Omega_h::CmdLine cmdline;
  cmdline.add_flag(
      "--osh-memory", "print amount and stacktrace of max memory use");
  cmdline.add_flag(
      "--osh-time", "print amount of time spend in certain functions");
  cmdline.add_flag("--osh-signal", "catch signals and print a stacktrace");
  cmdline.add_flag("--osh-silent", "suppress all output");
  auto& self_send_flag =
      cmdline.add_flag("--osh-self-send", "control self send threshold");
  self_send_flag.add_arg<int>("value");
  if (argc && argv) {
    OMEGA_H_CHECK(cmdline.parse(world_, argc, *argv));
  }
  Omega_h::should_log_memory = cmdline.parsed("--osh-memory");
  should_time_ = cmdline.parsed("--osh-time");
  bool should_protect = cmdline.parsed("--osh-signal");
  self_send_threshold_ = 1000 * 1000;
  if (cmdline.parsed("--osh-self-send")) {
    self_send_threshold_ = cmdline.get<int>("--osh-self-send", "value");
  }
  silent_ = cmdline.parsed("--osh-silent");
#ifdef OMEGA_H_USE_KOKKOSCORE
  if (!Kokkos::is_initialized()) {
    OMEGA_H_CHECK(argc != nullptr);
    OMEGA_H_CHECK(argv != nullptr);
    Kokkos::initialize(*argc, *argv);
    we_called_kokkos_init = true;
  } else {
    we_called_kokkos_init = false;
  }
#endif
  if (should_protect) Omega_h_protect();
}

Library::Library(Library const& other)
    : world_(other.world_),
      self_(other.self_)
#ifdef OMEGA_H_USE_MPI
      ,
      we_called_mpi_init(other.we_called_mpi_init)
#endif
#ifdef OMEGA_H_USE_KOKKOSCORE
      ,
      we_called_kokkos_init(other.we_called_kokkos_init)
#endif
{
}

Library::~Library() {
#ifdef OMEGA_H_USE_KOKKOSCORE
  if (we_called_kokkos_init) {
    Kokkos::finalize();
    we_called_kokkos_init = false;
  }
#endif
  if (Omega_h::should_log_memory) {
    auto mem_used = get_max_bytes();
    auto max_mem_used =
        std::size_t(world_->allreduce(I64(mem_used), OMEGA_H_MAX));
    auto max_mem_rank =
        (mem_used == max_mem_used) ? world_->rank() : world_->size();
    max_mem_rank = world_->allreduce(max_mem_rank, OMEGA_H_MIN);
    if (world_->rank() == max_mem_rank) {
      std::cout << "maximum Omega_h memory usage: " << mem_used << '\n';
      if (Omega_h::max_memory_stacktrace) {
        std::cout << Omega_h::max_memory_stacktrace;
      }
    }
  }
  // need to destroy all Comm objects prior to MPI_Finalize()
  world_ = CommPtr();
  self_ = CommPtr();
#ifdef OMEGA_H_USE_MPI
  if (we_called_mpi_init) {
    OMEGA_H_CHECK(MPI_SUCCESS == MPI_Finalize());
    we_called_mpi_init = false;
  }
#endif
  delete[] Omega_h::max_memory_stacktrace;
}

CommPtr Library::world() { return world_; }

CommPtr Library::self() {
#ifdef OMEGA_H_USE_MPI
  if (!self_) {
    MPI_Comm self_dup;
    MPI_Comm_dup(MPI_COMM_SELF, &self_dup);
    self_ = CommPtr(new Comm(this, self_dup));
  }
#endif
  return self_;
}

LO Library::self_send_threshold() const { return self_send_threshold_; }

}  // end namespace Omega_h
