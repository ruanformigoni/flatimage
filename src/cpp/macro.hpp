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
  if ( (cond) ) { ns_log::debug()(msg); break; }

#define ibreak_if(cond, msg) \
  if ( (cond) ) { ns_log::info()(msg); break; }

#define dbreak_if(cond, msg) \
  if ( (cond) ) { ns_log::error()(msg); break; }

// Continue
#define qcontinue_if(cond) \
  if ( (cond) ) { continue; }

#define econtinue_if(cond, msg) \
  if ( (cond) ) { ns_log::debug()(msg); continue; }

#define icontinue_if(cond, msg) \
  if ( (cond) ) { ns_log::info()(msg); continue; }

#define dcontinue_if(cond, msg) \
  if ( (cond) ) { ns_log::error()(msg); continue; }
