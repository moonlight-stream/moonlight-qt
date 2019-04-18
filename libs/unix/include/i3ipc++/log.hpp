#pragma once

#include <ostream>
#include <vector>
#include <auss.hpp>

/**
 * @addtogroup logging Logging
 * @{
 */

namespace i3ipc {

/**
 * @brief Common logging outputs
 */
extern std::vector<std::ostream*>  g_logging_outs;

/**
 * @brief Logging outputs for error messages
 */
extern std::vector<std::ostream*>  g_logging_err_outs;

/**
 * @brief Put to a logging outputs some dtat
 * @param  data  data, that you want to put to the logging outputs
 * @param  err  is your information is error report or something that must be putted to the error logging outputs
 */
template<typename T>
inline void  log(const T&  data, const bool  err=false) {
	for (auto  out : (err ? g_logging_err_outs : g_logging_outs)) {
		*out << data << std::endl;
	}
}

template<>
inline void  log(const auss_t&  data, const bool  err) {
	log(data.to_string());
}

}

/**
 * Internal macro used in I3IPC_*-logging macros
 */
#define I3IPC_LOG(T, ERR) \
	::i3ipc::log((T), (ERR));

/**
 * Put information message to log
 * @param T message
 */
#define I3IPC_INFO(T) I3IPC_LOG(auss_t() << "i: " << T, false)

/**
 * Put error message to log
 * @param T message
 */
#define I3IPC_ERR(T) I3IPC_LOG(auss_t() << "E: " << T, true)

/**
 * Put warning message to log
 * @param T message
 */
#define I3IPC_WARN(T) I3IPC_LOG(auss_t() << "W: " << T, true)

#ifdef DEBUG

/**
 * Put debug message to log
 * @param T message
 */
#define I3IPC_DEBUG(T) I3IPC_LOG(auss_t() << "D: " << T, true)

#else

/**
 * Put debug message to log
 * @param T message
 */
#define I3IPC_DEBUG(T)
#endif

/**
 * @}
 */
