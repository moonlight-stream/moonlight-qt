#pragma once

#define AUSS_HPP

#include <sstream>

#ifdef AUSS_USE_OWN_NAMESPACE
	#ifndef AUSS_OWN_NAMESPACE_NAME
	#define AUSS_OWN_NAMESPACE_NAME auss
	#endif
namespace AUSS_OWN_NAMESPACE_NAME {
#endif

class AutoStringStream {
public:
	AutoStringStream() : m_stream() {}
	virtual ~AutoStringStream() {}

	template<typename T>
	AutoStringStream&  operator<<(const T&  arg) {
		m_stream << arg;
		return *this;
	}

	AutoStringStream&  operator<<(const bool arg) {
		m_stream << (arg ? "true" : "false");
		return *this;
	}

	operator std::string() const {
		return m_stream.str();
	}

	const std::string  to_string() const {
		return m_stream.str();
	}
private:
	std::stringstream  m_stream;
};

#ifndef AUSS_CUSTOM_TYPEDEF
typedef AutoStringStream auss_t;
#endif

#ifdef AUSS_USE_OWN_NAMESPACE
}
#endif
