#pragma once
#include <string>
#include <unordered_map>
#include <chrono>
#include <cstdint>
#include "ffa/config.hpp"

namespace ffa {

// Speaks/prints an enrolled person's details AT MOST ONCE per cooldown window.
// This is what stops the app from repeating "Sarah… Sarah… Sarah…" while she
// stays in view. Unknown/unsure results are never announced.
class Announcer {
public:
    explicit Announcer(const Config& cfg);

    // Announce this person if they haven't been announced within the cooldown.
    // Returns true if it actually announced.
    bool announce(int64_t personId, const std::string& name,
                  const std::string& reminder);

    void mute(bool m) { muted_ = m; }
    bool muted() const { return muted_; }

private:
    Config cfg_;
    bool   muted_ = false;
    std::unordered_map<int64_t, std::chrono::steady_clock::time_point> lastSpoken_;
};

}  // namespace ffa
