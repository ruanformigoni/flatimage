///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : ipc
///

#include <filesystem>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <cstring>

#include "../common.hpp"
#include "../std/concepts.hpp"
#include "../std/string.hpp"
#include "log.hpp"

namespace ns_ipc
{

namespace fs = std::filesystem;

struct message_buffer
{
  long message_type;
  char message_text[1024];
};

// class Ipc {{{
class Ipc
{
  private:
    key_t m_key;
    int m_message_queue_id;
    message_buffer m_buffer;
    bool m_is_guest;
  public:
    // Constructors
    Ipc(key_t key, int message_queue_id, message_buffer buffer, bool is_guest)
      : m_key(key)
      , m_message_queue_id(message_queue_id)
      , m_buffer(buffer)
      , m_is_guest(is_guest)
    {}
    ~Ipc();
    // Factory methods
    static Ipc guest(fs::path path_file);
    static Ipc host(fs::path path_file);
    // Send/Recv Operations
    template<ns_concept::AsString T>
    void send(T&& t);
    std::optional<std::string> recv();

}; // class Ipc }}}

// Ipc::guest() {{{
inline Ipc Ipc::guest(fs::path path_file)
{
  std::string identifier = ns_string::to_string(fs::canonical(path_file));
  ns_log::info("key identifier: {}", identifier);

  // Use a unique key for the message queue.
  key_t key;
  if(key = ftok(identifier.c_str(), 65); key == -1 )
  {
    perror("Could not generate token for message queue");
    "Could not generate key for message queue with identifier '{}': {}"_throw(identifier, strerror(errno));
  } // if
  ns_log::info("Generated message_queue key: {}", key);

  // Connect to the message queue
  int message_queue_id = -1;
  if (message_queue_id = msgget(key, 0666); message_queue_id == -1 )
  {
    perror("Could not create message queue");
    "msgget failed, could not create message queue for identifier '{}': {}"_throw(identifier, strerror(errno));
  } // if
  ns_log::info("Message queue id: {}", message_queue_id);

  return Ipc(key, message_queue_id, message_buffer{.message_type=1, .message_text=""}, true);
} // Ipc::guest() }}}

// Ipc::host() {{{
inline Ipc Ipc::host(fs::path path_file)
{
  std::string identifier = ns_string::to_string(fs::canonical(path_file));
  ns_log::info("key identifier: {}", identifier);

  // Use a unique key for the message queue.
  key_t key;
  if(key = ftok(identifier.c_str(), 65); key == -1 )
  {
    perror("Could not generate token for message queue");
    "Could not generate key for message queue with identifier '{}': {}"_throw(identifier, strerror(errno));
  } // if
  ns_log::info("Generated message_queue key: {}", key);

  // Close existing queue
  if ( int message_queue_id = msgget(key, 0666); message_queue_id != -1 and msgctl(message_queue_id, IPC_RMID, NULL) == -1)
  {
    ns_log::error("Could not remove existing message queue");
    perror("Could not remove existing message queue");
  } // if
  else
  {
    ns_log::info("Closed existing message queue");
  } // else

  // Connect to the message queue
  int message_queue_id = -1;
  if (message_queue_id = msgget(key, 0666 | IPC_CREAT); message_queue_id == -1 )
  {
    perror("Could not create message queue");
    "msgget failed, could not create message queue for identifier '{}': {}"_throw(identifier, strerror(errno));
  } // if
  ns_log::info("Message queue id: {}", message_queue_id);

  return Ipc(key, message_queue_id, message_buffer{.message_type=1, .message_text=""}, false);
} // Ipc::host() }}}

// Ipc::~Ipc() {{{
inline Ipc::~Ipc()
{
  // No managed by guest
  if ( m_is_guest ) { return; } // if

  // Close
  if ( msgctl(m_message_queue_id, IPC_RMID, NULL) == -1 )
  {
    ns_log::error("Could not remove the message queue");
    perror("Could not remove message queue");
  } // if
} // Ipc::~Ipc() }}}

// Ipc::send() {{{
template<ns_concept::AsString T>
void Ipc::send(T&& t)
{
  std::string data = ns_string::to_string(t);
  ns_log::info("Sending message '{}'", data);
  // Copy the contents of std::string to the message_text buffer
  strncpy(m_buffer.message_text, data.c_str(), sizeof(m_buffer.message_text));
  // Ensure null termination
  m_buffer.message_text[sizeof(m_buffer.message_text) - 1] = '\0';
  // Send message
  if ( msgsnd(m_message_queue_id, &m_buffer, sizeof(m_buffer), 0) == -1 )
  {
    perror("Failure to send message");
  } // if
} // Ipc::send() }}}

// Ipc::recv() {{{
inline std::optional<std::string> Ipc::recv()
{
  int length = msgrcv(m_message_queue_id, &m_buffer, sizeof(m_buffer.message_text), 0, MSG_NOERROR);

  // Read message
  if (length == -1)
  {
    perror("Failure to receive message");
    ns_log::error("Failed to receive message: {}", strerror(errno));
    return std::nullopt;
  } // if

  // Ensure null termination
  m_buffer.message_text[sizeof(m_buffer.message_text) - 1] = '\0';

  // Convert message text to std::string
  return std::make_optional(m_buffer.message_text);
} // Ipc::recv() }}}

} // namespace ns_ipc

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
