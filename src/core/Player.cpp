#include "Player.h"

bool Player::HasItem(const std::string& type) const {
    for (const auto& it : items)
        if (it == type) return true;
    return false;
}

bool Player::RemoveItem(const std::string& type) {
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i] == type) {
            items.erase(items.begin() + (long)i);
            return true;
        }
    }
    return false;
}
