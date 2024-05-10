///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : portal_guest
///

#include "../cpp/log.hpp"
#include "../cpp/ipc.hpp"

// main() {{{
int main(int argc, char** argv)
{
  // Check args
  return_if( argc < 2, (ns_log::err("Incorrect arguments"), EXIT_FAILURE));

  // Get file path for IPC
  const char* env = assign_or_return(getenv("FIM_FILE_BINARY")
    , env
    , (ns_log::err("Could not read FIM_FILE_BINARY"), EXIT_FAILURE)
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
