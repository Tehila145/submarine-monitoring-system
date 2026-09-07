#include "fleet/Message.h"

namespace fleet {
Message::Message(std::string content, const Submarine* sender)
    : content_(std::move(content)), sender_(sender) {}
const std::string& Message::content() const { return content_; }
const Submarine* Message::sender() const { return sender_; }
}
