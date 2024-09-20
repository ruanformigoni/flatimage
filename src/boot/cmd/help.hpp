///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : help
///

#pragma once

#include <string>
#include <vector>
#include <algorithm>

namespace ns_cmd::ns_help
{

class HelpEntry
{
  private:
    std::string m_msg;
    std::string m_name;
  public:
    HelpEntry(std::string const& name)
      : m_msg("Flatimage - Portable Linux Applications\n")
      , m_name(name)
    {};
  HelpEntry& with_usage(std::string_view usage)
  {
    m_msg.append("Usage: ").append(usage).append("\n");
    return *this;
  }
  HelpEntry& with_example(std::string_view example)
  {
    m_msg.append("Example: ").append(example).append("\n");
    return *this;
  }
  HelpEntry& with_note(std::string_view note)
  {
    m_msg.append("Note: ").append(note).append("\n");
    return *this;
  }
  HelpEntry& with_description(std::string_view description)
  {
    m_msg.append(m_name).append(" : ").append(description).append("\n");
    return *this;
  }
  HelpEntry& with_commands(std::vector<std::pair<std::string,std::string>> commands)
  {
    m_msg.append("Commands:\n   ");
    std::ranges::for_each(commands, [&](auto&& e){ m_msg += e.first + ","; } );
    m_msg.append("\n");
    std::ranges::for_each(commands, [&](auto&& e)
    {
      m_msg += std::string{"   "} + e.first + " : " + e.second + '\n';
    });
    return *this;
  }
  HelpEntry& with_args(std::vector<std::pair<std::string,std::string>> args)
  {
    std::ranges::for_each(args, [&](auto&& e)
    {
      m_msg += std::string{"   <"} + e.first + "> : " + e.second + '\n';
    });
    return *this;
  }
  std::string get()
  {
    return m_msg;
  }
};

inline std::string help_usage()
{
  return HelpEntry{"fim-help"}
    .with_description("See usage details for specified command")
    .with_usage("fim-help <cmd>")
    .with_args({
      { "cmd", "Name of the command to display help details" },
    })
    .with_note("Available commands: fim-{exec,root,perms,env,desktop,layer,bind,commit,boot}")
    .with_example(R"(fim-help bind")")
    .get();
}

inline std::string root_usage()
{
  return HelpEntry{"fim-root"}
    .with_description("Executes a command as the root user")
    .with_usage("fim-root program-name [program-args...]")
    .with_args({
      { "program-name", "Name of the program to execute, it can be the name of a binary or the full path" },
      { "program-args...", "Arguments for the executed program" },
    })
    .with_example(R"(fim-root id -u")")
    .get();
}

inline std::string exec_usage()
{
  return HelpEntry{"fim-exec"}
    .with_description("Executes a command as a regular user")
    .with_usage("fim-exec program-name [program-args...]")
    .with_args({
      { "program-name", "Name of the program to execute, it can be the name of a binary or the full path" },
      { "program-args...", "Arguments for the executed program" },
    })
    .with_example(R"(fim-exec echo -e "hello\nworld")")
    .get();
}

inline std::string perms_usage()
{
  return HelpEntry{"fim-perms"}
    .with_description("Edit current permissions for the flatimage")
    .with_commands({
      { "add", "Allow one or more permissions" },
      { "del", "Delete one or more permissions" },
      { "list", "List current permissions" },
    })
    .with_note("Permissions: home,media,audio,wayland,xorg,dbus_user,dbus_system,udev,usb,input,gpu,network")
    .with_usage("fim-perms add <perms...>")
    .with_args({
      { "perms", "One or more permissions" },
    })
    .with_usage("fim-perms del <perms...>")
    .with_args({
      { "perms", "One or more permissions" },
    })
    .with_usage("fim-perms list")
    .with_example("fim-perms add home,network,gpu")
    .get();
}

inline std::string env_usage()
{
  return HelpEntry{"fim-env"}
    .with_description("Edit current permissions for the flatimage")
    .with_commands({
      { "set", "Redefines the environment variables as the input arguments" },
      { "add", "Include a novel environment variable" },
    })
    .with_usage("fim-env add <'key=value'...>")
    .with_example("fim-env add 'APP_NAME=hello-world' 'HOME=/home/my-app'")
    .with_usage("fim-env set <'key=value'...>")
    .with_example("fim-env set 'APP_NAME=hello-world' 'HOME=/home/my-app'")
    .get();
}

inline std::string desktop_usage()
{
  return HelpEntry{"fim-desktop"}
    .with_description("Configure the desktop integration")
    .with_commands({
      { "setup", "Sets up the desktop integration with an input json file" },
      { "enable", "Enables the desktop integration selectively" },
    })
    .with_usage("fim-desktop setup <json-file>")
    .with_args({
      { "json-file", "Path to the json file with the desktop configuration"},
    })
    .with_usage("fim-desktop enable [entry,mimetype,icon]")
    .with_args({
      { "entry", "Enable the start menu desktop entry"},
      { "mimetype", "Enable the mimetype"},
      { "icon", "Enable the icon for the file manager and desktop entry"},
    })
    .get();
}

inline std::string layer_usage()
{
  return HelpEntry{"fim-layer"}
    .with_description("Manage the layers of the current FlatImage")
    .with_commands({
      { "create", "Creates a novel layer from <in-dir> and save in <out-file>" },
      { "add", "Includes the novel layer <in-file> in the image in the top of the layer stack" },
    })
    .with_usage("fim-layer create <in-dir> <out-file>")
    .with_args({
      { "in-dir", "Input directory to create a novel layer from"},
      { "out-file" , "Output file name of the layer file"},
    })
    .with_usage("fim-layer add <in-file>")
    .with_args({
      { "in-file", "Path to the layer file to include in the FlatImage"},
    })
    .get();
}

inline std::string bind_usage()
{
  return HelpEntry{"fim-bind"}
    .with_description("Bind paths from the host to inside the container")
    .with_commands({
      { "add", "Create a novel binding of type <type> from <src> to <dst>" },
      { "del", "Deletes a binding with the specified index" },
      { "list", "List current bindings"}
    })
    .with_usage("fim-bind add <type> <src> <dst>")
    .with_args({
      { "type", "ro, rw, dev" },
      { "src" , "A file, directory, or device" },
      { "dst" , "A file, directory, or device" },
    })
    .with_usage("fim-bind del <index>")
    .with_args({
      { "index" , "Index of the binding to erase" },
    })
    .with_usage("fim-bind list")
    .get();
}

inline std::string commit_usage()
{
  return HelpEntry{"fim-commit"}
    .with_description("Compresses current changes and inserts them into the FlatImage")
    .with_commands({
      { "commit", "Compress and include changes in the image" },
    })
    .with_usage("fim-commit")
    .get();
}

inline std::string boot_usage()
{
  return HelpEntry{"fim-boot"}
    .with_description("Configure the default startup command")
    .with_commands({
      { "boot", "Execute <command> with optional [args] when FlatImage is launched" },
    })
    .with_usage("fim-boot <command> [args...]")
    .with_args({
      { "command" , "Startup command" },
      { "args" , "Arguments for the startup command" },
    })
    .with_example("fim-boot echo test")
    .with_note("To restore the default behavior use `fim-boot bash`")
    .get();
}


} // namespace ns_cmd::ns_help

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
