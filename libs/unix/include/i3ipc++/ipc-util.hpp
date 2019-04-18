#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <stdexcept>

// extern "C" {
// #include <i3/ipc.h>
// }

namespace i3ipc {

/** @defgroup i3ipc_util i3 IPC internal utilities
 * Stuff for internal usage in I3Connection 
 * @{
 */

// extern "C" {

/**
 * i3 IPC header
 */
struct header_t {
	/* 6 = strlen(I3_IPC_MAGIC) */
	char magic[6]; ///< Magic string @see I3_IPC_MAGIC
	uint32_t size; ///< Size of payload
	uint32_t type; ///< Message type
}  __attribute__ ((packed));


/**
 * @brief Base class of i3 IPC errors
 */
class ipc_error : public std::runtime_error { using std::runtime_error::runtime_error; };

/**
 * @brief Something wrong in message header (wrong magic number, message type etc.)
 */
class invalid_header_error : public ipc_error { using ipc_error::ipc_error; };

/**
 * @brief Socket return EOF, but expected a data
 */
class eof_error : public ipc_error { using ipc_error::ipc_error; };

/**
 * @brief If something wrong in a payload of i3's reply
 */
class invalid_reply_payload_error : public ipc_error { using ipc_error::ipc_error; };

/**
 * @brief If any error occured, while using C-functions
 */
class errno_error : public ipc_error {
public:
	errno_error();
	errno_error(const std::string&  msg);
};


/**
 * @brief Messages (requests), that can be sended from the client
 */
enum class ClientMessageType : uint32_t {
	COMMAND = 0,
	GET_WORKSPACES = 1,
	SUBSCRIBE = 2,
	GET_OUTPUTS = 3,
	GET_TREE = 4,
	GET_MARKS = 5,
	GET_BAR_CONFIG = 6,
	GET_VERSION = 7,
};


/**
 * @brief Replies, that can be sended from the i3 to the client
 */
enum class ReplyType : uint32_t {
	COMMAND = 0,
	WORKSPACES = 1,
	SUBSCRIBE = 2,
	OUTPUTS = 3,
	TREE = 4,
	MARKS = 5,
	BAR_CONFIG = 6,
	VERSION = 7,
};


/**
 * @brief i3 IPC message buffer
 */
struct buf_t {
	uint32_t  size; ///< @brief Size of whole buffer
	uint8_t*  data; ///< @brief Pointer to the message

	/**
	 * @brief i3 IPC message header
	 *
	 * Pointing on the begining
	 */
	header_t*  header;

	/**
	 * @brief Message payload
	 *
	 * Pointing on the byte after the header
	 */
	char*  payload;

	buf_t(uint32_t  payload_size);
	~buf_t();

	/**
	 * @brief Resize payload to the payload_size in the header
	 */
	void  realloc_payload_to_header();
};

/**
 * Connect to the i3 socket
 * @param  socket_path a socket path
 * @return             socket id
 */
int32_t  i3_connect(const std::string&  socket_path);

/**
 * @brief Close the connection
 * @param  sockfd socket
 */
void  i3_disconnect(const int32_t  sockfd);

/**
 * @brief Send message to the socket
 * @param  sockfd a socket
 * @param  buff   a message
 */
void   i3_send(const int32_t  sockfd, const buf_t&  buff);

/**
 * @brief Recive a message from i3
 * @param  sockfd  a socket
 * @return  a buffer of the message
 */
std::shared_ptr<buf_t>   i3_recv(const int32_t  sockfd);

/**
 * @brief Pack a buffer of message
 */
std::shared_ptr<buf_t>  i3_pack(const ClientMessageType  type, const std::string&  payload);

/**
 * @brief Pack, send a message and receiv a reply
 *
 * Almost same to:
 * @code{.cpp}
 * i3_send(sockfd, i3_pack(type, payload));
 * auto  reply = i3_recv(sockfd);
 * @endcode
 */
std::shared_ptr<buf_t>  i3_msg(const int32_t  sockfd, const ClientMessageType  type, const std::string&  payload = std::string());

/**
 * @}
 */

}
