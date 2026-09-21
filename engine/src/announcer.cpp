#include "ffa/announcer.hpp"
#include <cstdlib>
#include <iostream>

namespace ffa {

Announcer::Announcer(const Config& cfg) : cfg_(cfg) {}

bool Announcer::announce(int64_t personId, const std::string& name,
                         const std::string& reminder) {
    auto now = std::chrono::steady_clock::now();
    auto it  = lastSpoken_.find(personId);
    if (it != lastSpoken_.end()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
        if (elapsed < cfg_.cooldownSeconds) return false;  // still in cooldown
    }
    lastSpoken_[personId] = now;

    std::string phrase = "Possible match: " + name;
    if (!reminder.empty()) phrase += ", " + reminder;

    std::cout << ">>> " << phrase << std::endl;

    if (cfg_.speak && !muted_) {
        // macOS text-to-speech, local. Quote the phrase for the shell.
        std::string cmd = "say \"" + phrase + "\" >/dev/null 2>&1 &";
        std::system(cmd.c_str());
    }
    return true;
}

}  // namespace ffa
