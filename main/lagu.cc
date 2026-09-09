#include "lagu.h"
#include "application.h"
#include <unordered_map>
#include <algorithm>

static const std::unordered_map<std::string, std::string> kSongDatabase = {
    {"aku dan dirimu", "https://archive.org/download/TerpurukKuDisini/Aku%20Dan%20Dirimu.mp3"},
    {"andaikan kau datang kembali", "https://archive.org/download/TerpurukKuDisini/Andaikan%20Kau%20Datang%20Kembali.mp3"},
    {"cinta terbaik", "https://archive.org/download/TerpurukKuDisini/Cinta%20Terbaik.mp3"}
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
