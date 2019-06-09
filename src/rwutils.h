#ifndef UNIX_SERVER_RWUTIL
#define UNIX_SERVER_RWUTIL

#include <string>

std::string read_message(int fd) noexcept(false);

void write_message(int fd, std::string const &message) noexcept(false);

#endif
