#pragma once
#include <string>

namespace fleet {
class Submarine;  // non-owning reference only

class Message {
public:
    Message(std::string content, const Submarine* sender);
    const std::string& content() const;
    const Submarine* sender() const;
private:
    std::string content_;
    const Submarine* sender_;
};
}
