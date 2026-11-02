#include "extraction_policy.hpp"
#include "model.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
using namespace rd;
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x);                                        \
            std::exit(1);                                                                                    \
        }                                                                                                    \
    } while (0)
int main() {
    const std::string archive = "archive-cleanup-test.zip";
    CHECK(!std::filesystem::exists(archive));
    {
        std::ofstream file(archive);
        file << "original archive";
    }
    for (auto result : {finishArchive(archive, "NES", false, true, false, 1, 0),
                        finishArchive(archive, "NES", true, false, false, 1, 0),
                        finishArchive(archive, "NES", true, true, true, 1, 0),
                        finishArchive(archive, "NES", true, true, false, 1, 1),
                        finishArchive(archive, "NES", true, true, false, 0, 0),
                        finishArchive(archive, "ArCaDe", true, true, false, 1, 0)}) {
        CHECK(result == ArchiveCleanup::Kept);
        CHECK(std::filesystem::exists(archive));
    }
    {
        std::ifstream file(archive);
        std::string content;
        std::getline(file, content);
        CHECK(content == "original archive");
    }
    CHECK(finishArchive(archive, "NES", true, true, false, 2, 0) == ArchiveCleanup::Deleted);
    CHECK(!std::filesystem::exists(archive));
    CHECK(finishArchive(archive, "NES", true, true, false, 2, 0) == ArchiveCleanup::Failed);
    std::vector<Category> catalog;
    std::string error;
    CHECK(parseCatalog(
        R"([{"name":"Zelda.zip","console":"NES","downloadUrl":"https://example.org/z"},{"name":"alpha.zip","console":"NES","download_url":"https://example.org/a"},{"name":"ALPHA.zip","console":"NES","downloadUrl":"https://example.org/duplicate"},{"name":"../unsafe","console":"NES","downloadUrl":"https://example.org/x"},{"name":"plain","console":"NES","downloadUrl":"http://example.org/x"},{"name":".empty","console":"Music"}])",
        catalog, error));
    CHECK(catalog.size() == 2);
    CHECK(catalog[0].name == "Music");
    CHECK(catalog[1].items.size() == 2);
    CHECK(catalog[1].items[0].name == "alpha.zip");
    std::set<std::string> favorites{"NES/Zelda.zip"};
    CHECK(filterItems(catalog[1], "ALPHA", favorites, false, false) == std::vector<int>{0});
    CHECK(filterItems(catalog[1], "", favorites, true, false) == std::vector<int>{1});
    CHECK(filterItems(catalog[1], "", favorites, false, true) == (std::vector<int>{1, 0}));
    CHECK(!parseCatalog("bad json", catalog, error));
    CHECK(catalog.size() == 2);
    CHECK(parseCatalog(
        R"({"files":[{"name":"say \"hello\".zip","console":"Test","downloadUrl":"https://example.org/a"},{"name":"Unicode café.zip","console":"Test","downloadUrl":"https://example.org/b"}]})",
        catalog, error));
    CHECK(catalog[0].items.size() == 1);
    CHECK(!safeName(".."));
    CHECK(!safeName("test/thing"));
    CHECK(!safeName("thing\\name"));
    CHECK(!safeName("file. "));
    CHECK(safeName("café.zip"));
    std::string path;
    CHECK(archivePath("folder/sub/file.nes", path) && path == "folder/sub/file.nes");
    CHECK(archivePath("folder\\sub\\file.nes", path));
    CHECK(archivePath("./folder/file", path) && path == "folder/file");
    for (auto s : {"../outside", "folder/../../x", "/absolute", "C:/drive", "\\absolute", "folder/a:b",
                   "folder/..", "folder/file."})
        CHECK(!archivePath(s, path));
    CHECK(bytes(1024) == "1.0 KiB");
    CHECK(normalizeSdPath("sdmc:/roms", path) && path == "sdmc:/roms/");
    CHECK(normalizeSdPath("sdmc:/", path) && path == "sdmc:/");
    CHECK(!normalizeSdPath("sdmc:/../outside", path));
    CHECK(!normalizeSdPath("C:/outside", path));
    CHECK(bytes(1024ULL * 1024 * 1024 * 5) == "5.0 GiB");
    std::puts(
        "PASS: catalog validation, duplicate filenames, search, favorites, sort, archive paths, byte sizes");
}
