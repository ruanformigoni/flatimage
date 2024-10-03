///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : macro
///

#pragma once

// Throw
#define qthrow_if(cond, msg) \
  if (cond) { throw std::runtime_error(msg); }

#define dthrow_if(cond, msg) \
  if (cond) { ns_log::debug()(msg); throw std::runtime_error(msg); }

#define ithrow_if(cond, msg) \
  if (cond) { ns_log::info()(msg); throw std::runtime_error(msg); }

#define ethrow_if(cond, msg) \
  if (cond) { ns_log::error()(msg); throw std::runtime_error(msg); }

// Exit
#define qexit_if(cond, ret) \
  if (cond) { exit(ret); }

#define dexit_if(cond, msg, ret) \
  if (cond) { ns_log::debug()(msg); exit(ret); }

#define iexit_if(cond, msg, ret) \
  if (cond) { ns_log::info()(msg); exit(ret); }

#define eexit_if(cond, msg, ret) \
  if (cond) { ns_log::error()(msg); exit(ret); }

// Return
#define qreturn_if(cond, ...) \
  if (cond) { return __VA_ARGS__; }

#define dreturn_if(cond, msg, ...) \
  if (cond) { ns_log::debug()(msg); return __VA_ARGS__; }

#define ireturn_if(cond, msg, ...) \
  if (cond) { ns_log::info()(msg); return __VA_ARGS__; }

#define ereturn_if(cond, msg, ...) \
  if (cond) { ns_log::error()(msg); return __VA_ARGS__; }

#define return_if_else(cond, val1, val2) \
  if (cond) { return val1; } else { return val2; }

// Break
#define qbreak_if(cond) \
  if ( (cond) ) { break; }

#define ebreak_if(cond, msg) \
  if ( (cond) ) { ns_log::error()(msg); break; }

#define ibreak_if(cond, msg) \
  if ( (cond) ) { ns_log::info()(msg); break; }

#define dbreak_if(cond, msg) \
  if ( (cond) ) { ns_log::debug()(msg); break; }

// Continue
#define qcontinue_if(cond) \
  if ( (cond) ) { continue; }

#define econtinue_if(cond, msg) \
  if ( (cond) ) { ns_log::error()(msg); continue; }

#define icontinue_if(cond, msg) \
  if ( (cond) ) { ns_log::info()(msg); continue; }

#define dcontinue_if(cond, msg) \
  if ( (cond) ) { ns_log::debug()(msg); continue; }

// Abort
#define eabort_if(cond, msg) \
  if ( (cond) ) { ns_log::error()(msg); std::abort(); }

// Conditional log
#define elog_if(cond, msg) \
  if ( (cond) ) { ns_log::error()(msg); }

#define ilog_if(cond, msg) \
  if ( (cond) ) { ns_log::info()(msg); }

#define dlog_if(cond, msg) \
  if ( (cond) ) { ns_log::debug()(msg); }
