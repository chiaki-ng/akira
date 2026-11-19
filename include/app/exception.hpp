#ifndef AKIRA_EXCEPTION_HPP
#define AKIRA_EXCEPTION_HPP

#include <exception>
#include <string>

class Exception : public std::exception
{
private:
    std::string msg;

public:
    explicit Exception(const char *msg) : msg(msg) {}
    explicit Exception(std::string msg) : msg(std::move(msg)) {}
    const char *what() const noexcept override { return msg.c_str(); }
};

#endif // AKIRA_EXCEPTION_HPP
