///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : portal_guest
///

#include "../cpp/macro.hpp"
#include "../cpp/lib/log.hpp"
#include "../cpp/lib/ipc.hpp"

// main() {{{
int main(int argc, char** argv)
{
  // Check args
  ereturn_if( argc < 2, "Incorrect arguments", EXIT_FAILURE);

  // Get file path for IPC
  const char* env = assign_and_ereturn_if(getenv("FIM_FILE_BINARY")
    , env
    , "Could not read FIM_FILE_BINARY"
    , EXIT_FAILURE
  );

  // Create ipc instance
  auto ipc = ns_ipc::Ipc::guest(env);

  // Send message
  ipc.send("IPC_ARGV_START");
  for(int i = 1; i < argc; ++i)
  {
    ipc.send(argv[i]);
  } // for
  ipc.send("IPC_ARGV_END");

  return EXIT_SUCCESS;
} // main() }}}

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
