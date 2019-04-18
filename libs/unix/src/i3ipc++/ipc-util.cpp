extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <errno.h>
}

#include <cstring>
#include <ios>

#include <auss.hpp>

#include <i3ipc++/ipc-util.hpp>

namespace i3ipc {

static std::string  format_errno(const std::string&  msg = std::string()) {
	auss_t  a;
	if (msg.size() > 0)
		a << msg << ": ";
	a << "errno " << errno << " (" << strerror(errno) << ')';
	return a;
}

errno_error::errno_error() : ipc_error(format_errno()) {}
errno_error::errno_error(const std::string&  msg) : ipc_error(format_errno(msg)) {}

static const std::string  g_i3_ipc_magic = "i3-ipc";

buf_t::buf_t(uint32_t  payload_size) : size(sizeof(header_t) + payload_size) {
	data = new uint8_t[size];
	header = (header_t*)data;
	payload = (char*)(data + sizeof(header_t));
	strncpy(header->magic, g_i3_ipc_magic.c_str(), sizeof(header->magic));
	header->size = payload_size;
	header->type = 0x0;
}
buf_t::~buf_t() {
	delete[] data;
}

void  buf_t::realloc_payload_to_header() {
	uint8_t*  new_data = new uint8_t[sizeof(header_t) + header->size];
	memcpy(new_data, header, sizeof(header_t));
	delete[] data;
	data = new_data;
	header = (header_t*)data;
	payload = (char*)(data + sizeof(header_t));
}


int32_t  i3_connect(const std::string&  socket_path) {
	int32_t  sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (sockfd == -1) {
		throw errno_error("Could not create a socket");
	}

	(void)fcntl(sockfd, F_SETFD, FD_CLOEXEC); // What for?
	
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_LOCAL;
	strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
	if (connect(sockfd, (const struct sockaddr*)&addr, sizeof(struct sockaddr_un)) < 0) {
		throw errno_error("Failed to connect to i3");
	}

	return sockfd;
}


void  i3_disconnect(const int32_t  sockfd) {
	close(sockfd);
}


std::shared_ptr<buf_t>  i3_pack(const ClientMessageType  type, const std::string&  payload) {
	buf_t*  buff = new buf_t(payload.length());
	buff->header->type = static_cast<uint32_t>(type);
	strncpy(buff->payload, payload.c_str(), buff->header->size);
	return std::shared_ptr<buf_t>(buff);
}

ssize_t  writeall(int  fd, const uint8_t*  buf, size_t  count) {
	size_t written = 0;
	ssize_t n = 0;

	while (written < count) {
		n = write(fd, buf + written, count - written);
		if (n == -1) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return n;
		}
		written += (size_t)n;
	}

	return written;
}

ssize_t  swrite(int  fd, const uint8_t*  buf, size_t  count) {
	ssize_t n;

	n = writeall(fd, buf, count);
	if (n == -1)
		throw errno_error(auss_t() << "Failed to write " << std::hex << fd);
	else
		return n;
}

void   i3_send(const int32_t  sockfd, const buf_t&  buff) {
	swrite(sockfd, buff.data, buff.size);
}

std::shared_ptr<buf_t>   i3_recv(const int32_t  sockfd) {
	buf_t*  buff = new buf_t(0);
	const uint32_t  header_size = sizeof(header_t);

	{
		uint8_t*  header = (uint8_t*)buff->header;
		uint32_t  readed = 0;
		while (readed < header_size) {
			int  n = read(sockfd, header + readed, header_size - readed);
			if (n == -1) {
				throw errno_error(auss_t() << "Failed to read header from socket 0x" << std::hex << sockfd);
			}
			if (n == 0) {
				throw eof_error("Unexpected EOF while reading header");
			}

			readed += n;
		}
	}

	if (g_i3_ipc_magic != std::string(buff->header->magic, g_i3_ipc_magic.length())) {
		throw invalid_header_error("Invalid magic in reply");
	}

	buff->realloc_payload_to_header();

	{
		uint32_t  readed = 0;
		int n;
		while (readed < buff->header->size) {
			if ((n = read(sockfd, buff->payload + readed, buff->header->size - readed)) == -1) {
				if (errno == EINTR || errno == EAGAIN)
					continue;
				throw errno_error(auss_t() << "Failed to read payload from socket 0x" << std::hex << sockfd);
			}

			readed += n;
		}
	}

	return std::shared_ptr<buf_t>(buff);
}


std::shared_ptr<buf_t>  i3_msg(const int32_t  sockfd, const ClientMessageType  type, const std::string&  payload) {
	auto  send_buff = i3_pack(type, payload);
	i3_send(sockfd, *send_buff);
	auto  recv_buff = i3_recv(sockfd);
	if (send_buff->header->type != recv_buff->header->type) {
		throw invalid_header_error(auss_t() << "Invalid reply type: Expected 0x" << std::hex << send_buff->header->type << ", got 0x" << recv_buff->header->type);
	}
	return recv_buff;
}

}
