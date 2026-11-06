#include "model.hpp"
#define JSON_NOEXCEPTION 1
#include "json.hpp"
#include <algorithm>
#include <cstdio>
namespace rd {
std::vector<int> shelfCategories(const std::vector<Category> &categories, int tab) {
    std::vector<int> result;
    for (int i = 0; i < (int)categories.size(); ++i) {
        auto name = lower(categories[i].name);
        int group =
            (name == "music" || name == "ebook" || name == "ebooks" || name == "e-book" || name == "e-books")
                ? 1
                : 0;
        if (group == tab)
            result.push_back(i);
    }
    return result;
}

std::string lower(std::string v) {
    for (auto &c : v)
        if (c >= 'A' && c <= 'Z')
            c += 32;
    return v;
}
bool safeName(const std::string &v) {
    if (v.empty() || v == "." || v == ".." || v.size() > 240 || v.back() == '.' || v.back() == ' ')
        return false;
    for (unsigned char c : v)
        if (c < 32 || c == 127 || std::string("/\\:*?\"<>|").find(c) != std::string::npos)
            return false;
    return true;
}
bool archivePath(const std::string &v, std::string &out) {
    out.clear();
    if (v.empty() || v[0] == '/' || v[0] == '\\')
        return false;
    std::string part;
    auto append = [&]() {
        if (part == ".") {
            part.clear();
            return true;
        }
        if (!safeName(part))
            return false;
        if (!out.empty())
            out += '/';
        out += part;
        part.clear();
        return true;
    };
    for (char c : v) {
        if (c == '/' || c == '\\') {
            if (!part.empty() && !append())
                return false;
        } else
            part += c;
    }
    if (!part.empty() && !append())
        return false;
    return !out.empty();
}
bool normalizeSdPath(const std::string &value, std::string &normalized) {
    if (value.rfind("sdmc:/", 0) != 0)
        return false;
    if (value == "sdmc:/") {
        normalized = value;
        return true;
    }
    std::string relative;
    if (!archivePath(value.substr(6), relative))
        return false;
    normalized = "sdmc:/" + relative + "/";
    return true;
}
bool parseCatalog(const std::string &data, std::vector<Category> &out, std::string &error) {
    auto json = nlohmann::json::parse(data, nullptr, false);
    if (json.is_discarded()) {
        error = "Catalog is not valid JSON";
        return false;
    }
    std::vector<Category> result;
    std::set<std::string> seen;
    auto record = [&](const nlohmann::json &j) {
        if (!j.is_object())
            return;
        auto field = [&](const char *k) {
            auto i = j.find(k);
            return i != j.end() && i->is_string() ? i->get<std::string>() : std::string();
        };
        Item item{field("name"), field("console"), field("downloadUrl")};
        if (item.url.empty())
            item.url = field("download_url");
        if (!safeName(item.category))
            return;
        if (item.name != ".empty" && (!safeName(item.name) || item.url.rfind("https://", 0) != 0))
            return;
        auto it = std::find_if(result.begin(), result.end(),
                               [&](const Category &c) { return c.name == item.category; });
        if (it == result.end()) {
            result.push_back({item.category, {}});
            it = result.end() - 1;
        }
        if (item.name != ".empty" && seen.insert(lower(item.key())).second)
            it->items.push_back(item);
    };
    // Accept an array, or a wrapper object containing arrays of records.
    if (json.is_array())
        for (const auto &j : json)
            record(j);
    else if (json.is_object()) {
        record(json);
        for (const auto &pair : json.items())
            if (pair.value().is_array())
                for (const auto &j : pair.value())
                    record(j);
    }
    if (result.empty()) {
        error = "Catalog contains no valid collections";
        return false;
    }
    std::sort(result.begin(), result.end(),
              [](const Category &a, const Category &b) { return lower(a.name) < lower(b.name); });
    for (auto &c : result)
        std::sort(c.items.begin(), c.items.end(),
                  [](const Item &a, const Item &b) { return lower(a.name) < lower(b.name); });
    out = std::move(result);
    error.clear();
    return true;
}
std::vector<int> filterItems(const Category &c, const std::string &query,
                             const std::set<std::string> &favorites, bool only, bool descending) {
    std::vector<int> v;
    auto q = lower(query);
    for (size_t i = 0; i < c.items.size(); ++i)
        if (lower(c.items[i].name).find(q) != std::string::npos &&
            (!only || favorites.count(c.items[i].key())))
            v.push_back(i);
    if (descending)
        std::reverse(v.begin(), v.end());
    return v;
}
std::string bytes(uint64_t value) {
    char b[40];
    if (value < 1024)
        return std::to_string(value) + " B";
    double n = value;
    const char *unit[] = {"KiB", "MiB", "GiB", "TiB"};
    int i = 0;
    n /= 1024;
    while (n >= 1024 && i < 3) {
        n /= 1024;
        ++i;
    }
    snprintf(b, sizeof(b), "%.1f %s", n, unit[i]);
    return b;
}
} // namespace rd
