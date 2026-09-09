#include "lagu.h"
#include <unordered_map>
#include <algorithm>

// Database internal lagu dan URL pemutaran
static const std::unordered_map<std::string, std::string> kSongDatabase = {
    {"lagu a", "https://example.com/audio/lagu_a.mp3"},
    {"lagu b", "https://example.com/audio/lagu_b.mp3"},
    {"bintang kecil", "https://example.com/audio/bintang_kecil.mp3"}
};

std::string GetSongUrl(const std::string& song_name) {
    std::string lower_name = song_name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

    auto it = kSongDatabase.find(lower_name);
    if (it != kSongDatabase.end()) {
        return it->second;
    }
    return "";
}
