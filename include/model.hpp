#pragma once
#include <cstdint>
#include <set>
#include <string>
#include <vector>
namespace rd {
struct Item {
    std::string name, category, url;
    std::string key() const {
        return category + "/" + name;
    }
};
struct Category {
    std::string name;
    std::vector<Item> items;
};
std::string lower(std::string value);
bool safeName(const std::string &value);
bool archivePath(const std::string &value, std::string &normalized);
bool normalizeSdPath(const std::string &value, std::string &normalized);
bool parseCatalog(const std::string &data, std::vector<Category> &out, std::string &error);
std::vector<int> filterItems(const Category &category, const std::string &query,
                             const std::set<std::string> &favorites, bool favoritesOnly, bool descending);
std::string bytes(uint64_t value);
} // namespace rd
