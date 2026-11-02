#include "app.hpp"
#include "extraction_policy.hpp"
#include <algorithm>
#include <cstring>
#include <curl/curl.h>
#include <fstream>
#include <minizip/unzip.h>
#include <sys/stat.h>
#include <unistd.h>
namespace rd {
std::atomic<bool> cancelWork{false}, pauseWork{false};
namespace {
Thread thread;
Mutex mutex;
bool active = false, catalogTask = false, refreshTask = false, extractTask = false;
bool deleteOriginalTask = false;
std::atomic<bool> done{false};
Progress shared;
std::vector<Category> catalog;
std::vector<Job> jobs;
std::string output, error;
bool catalogOkay = false;
size_t catalogLimit = 4 * 1024 * 1024;
void publish(const Progress &p) {
    mutexLock(&mutex);
    shared = p;
    mutexUnlock(&mutex);
}
bool interrupted() {
    while (pauseWork && !cancelWork)
        svcSleepThread(20000000);
    return cancelWork;
}
void configure(CURL *c, const std::string &url) {
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_USERAGENT, "ROM-Downloader/2.0");
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 12L);
    curl_easy_setopt(c, CURLOPT_PROTOCOLS, (long)CURLPROTO_HTTPS);
    curl_easy_setopt(c, CURLOPT_REDIR_PROTOCOLS, (long)CURLPROTO_HTTPS);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
}
size_t stringWrite(char *b, size_t s, size_t n, void *ptr) {
    size_t len = s * n;
    auto &v = *(std::string *)ptr;
    if (v.size() + len > catalogLimit)
        return 0;
    v.append(b, len);
    return len;
}
size_t fileWrite(char *b, size_t s, size_t n, void *ptr) {
    return fwrite(b, 1, s * n, (FILE *)ptr);
}
struct Context {
    Progress p;
    uint64_t tick = 0, last = 0, activity = 0, lastNow = 0;
};
int progress(void *ptr, curl_off_t total, curl_off_t now, curl_off_t, curl_off_t) {
    auto &c = *(Context *)ptr;
    auto before = SDL_GetTicks64();
    if (interrupted())
        return 1;
    auto tick = SDL_GetTicks64();
    // A user pause must never count toward a network stall timeout or speed sample.
    c.tick += tick - before;
    c.activity += tick - before;
    c.p.now = std::max<curl_off_t>(0, now);
    c.p.total = std::max<curl_off_t>(0, total);
    if (!c.activity || c.p.now != c.lastNow)
        c.activity = tick;
    c.lastNow = c.p.now;
    if (tick - c.activity > 40000)
        return 1;
    if (now > 0)
        c.p.detail = "Receiving data";
    if (tick - c.tick >= 400) {
        c.p.speed = 1000.0 * (c.p.now - c.last) / (tick - c.tick);
        c.tick = tick;
        c.last = c.p.now;
    }
    publish(c.p);
    return 0;
}
void crypt(std::string &s) {
    const std::string key = "S1ckDuck69!SwitchApp";
    for (size_t i = 0; i < s.size(); ++i)
        s[i] ^= key[i % key.size()];
}
bool directory(const std::string &path) {
    struct stat st{};
    if (lstat(path.c_str(), &st) == 0)
        return S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode);
    return mkdir(path.c_str(), 0777) == 0;
}
bool parents(const std::string &relative) {
    std::string p = output;
    for (size_t i = 0; i < relative.size(); ++i)
        if (relative[i] == '/') {
            if (!directory(p + relative.substr(0, i)))
                return false;
        }
    return true;
}
bool extractZip(const std::string &path, const std::string &category, Context &ctx, std::string &message) {
    unzFile z = unzOpen64(path.c_str());
    if (!z) {
        message = "Saved; ZIP could not be opened";
        return false;
    }
    unz_global_info64 global{};
    bool okay = unzGetGlobalInfo64(z, &global) == UNZ_OK && global.number_entry <= 20000;
    std::set<std::string> seen;
    // Validate every entry before creating output. Existing files are always preserved.
    for (ZPOS64_T i = 0; okay && i < global.number_entry; ++i) {
        if (interrupted()) {
            okay = false;
            break;
        }
        unz_file_info64 info{};
        char name[1024]{};
        std::string normalized;
        okay = unzGetCurrentFileInfo64(z, &info, name, sizeof(name), nullptr, 0, nullptr, 0) == UNZ_OK &&
               info.size_filename < sizeof(name) && archivePath(name, normalized) && (info.flag & 1) == 0;
        unsigned mode = (info.external_fa >> 16) & 0170000;
        if (mode && mode != 0100000 && mode != 0040000)
            okay = false;
        if (okay && !seen.insert(lower(normalized)).second)
            okay = false;
        if (i + 1 < global.number_entry && unzGoToNextFile(z) != UNZ_OK)
            okay = false;
    }
    if (!okay) {
        unzClose(z);
        message = cancelWork ? "Saved; extraction cancelled" : "Saved; ZIP has unsupported or unsafe entries";
        return false;
    }
    unzGoToFirstFile(z);
    ctx.p.extracting = true;
    ctx.p.total = global.number_entry;
    ctx.p.now = 0;
    ctx.p.speed = 0;
    unsigned written = 0, skipped = 0;
    for (ZPOS64_T i = 0; okay && i < global.number_entry; ++i) {
        unz_file_info64 info{};
        char name[1024]{};
        unzGetCurrentFileInfo64(z, &info, name, sizeof(name), nullptr, 0, nullptr, 0);
        std::string relative;
        archivePath(name, relative);
        ctx.p.detail = relative;
        ctx.p.now = i;
        publish(ctx.p);
        if (interrupted()) {
            okay = false;
            break;
        }
        std::string target = output + relative;
        bool dir = name[strlen(name) - 1] == '/' || name[strlen(name) - 1] == '\\';
        if (!parents(relative)) {
            okay = false;
            break;
        }
        if (dir) {
            okay = directory(target);
        } else {
            struct stat st{};
            if (lstat(target.c_str(), &st) == 0) {
                ++skipped;
                if (!S_ISREG(st.st_mode))
                    okay = false;
            } else if (unzOpenCurrentFile(z) != UNZ_OK)
                okay = false;
            else {
                std::string pattern = output + ".rom-extract-XXXXXX";
                std::vector<char> temp(pattern.begin(), pattern.end());
                temp.push_back(0);
                int fd = mkstemp(temp.data());
                FILE *f = fd >= 0 ? fdopen(fd, "wb") : nullptr;
                if (!f && fd >= 0)
                    close(fd);
                okay = f != nullptr;
                char buffer[65536];
                int read = 0;
                uint64_t count = 0;
                while (okay && (read = unzReadCurrentFile(z, buffer, sizeof(buffer))) > 0) {
                    if (interrupted() || fwrite(buffer, 1, read, f) != (size_t)read) {
                        okay = false;
                        break;
                    }
                    count += read;
                    ctx.p.detail = relative + "  " + bytes(count) + " / " + bytes(info.uncompressed_size);
                    publish(ctx.p);
                }
                if (read < 0 || count != info.uncompressed_size)
                    okay = false;
                if (f && fclose(f) != 0)
                    okay = false;
                if (unzCloseCurrentFile(z) != UNZ_OK)
                    okay = false;
                if (okay && rename(temp.data(), target.c_str()) != 0)
                    okay = false;
                if (!okay)
                    remove(temp.data());
                else
                    ++written;
            }
        }
        if (i + 1 < global.number_entry && unzGoToNextFile(z) != UNZ_OK)
            okay = false;
    }
    if (unzClose(z) != UNZ_OK)
        okay = false;
    message = okay         ? "Saved + extracted (archive kept)"
              : cancelWork ? "Saved; extraction cancelled"
                           : "Saved; extraction failed (archive kept)";
    auto cleanup = finishArchive(path, category, deleteOriginalTask, okay, cancelWork, written, skipped);
    if (cleanup == ArchiveCleanup::Deleted)
        message = "Extracted; original ZIP deleted";
    else if (cleanup == ArchiveCleanup::Failed)
        message = "Extracted; could not delete original ZIP";
    else if (okay && deleteOriginalTask && skipped)
        message = "Extracted; ZIP kept (existing files skipped)";
    return okay;
}
void runCatalog() {
    std::string data;
    std::ifstream cached(std::string(config) + "manifest.enc", std::ios::binary | std::ios::ate);
    if (cached && cached.tellg() >= 0 && (size_t)cached.tellg() <= catalogLimit) {
        cached.seekg(0);
        data.assign(std::istreambuf_iterator<char>(cached), {});
        crypt(data);
    }
    if (!refreshTask && !data.empty() && parseCatalog(data, catalog, error)) {
        catalogOkay = true;
        return;
    }
    std::string fresh;
    CURL *c = curl_easy_init();
    Context ctx;
    ctx.p.title = "Updating your collections";
    ctx.p.detail = "Connecting to catalog";
    ctx.tick = SDL_GetTicks64();
    publish(ctx.p);
    if (c) {
        configure(c, "https://raw.githubusercontent.com/SickDuck696969/ROM-Collection/master/manifest.json");
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, stringWrite);
        curl_easy_setopt(c, CURLOPT_WRITEDATA, &fresh);
        curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, progress);
        curl_easy_setopt(c, CURLOPT_XFERINFODATA, &ctx);
        curl_easy_setopt(c, CURLOPT_TIMEOUT, 60L);
        auto r = curl_easy_perform(c);
        curl_easy_cleanup(c);
        if (r == CURLE_OK && parseCatalog(fresh, catalog, error)) {
            catalogOkay = true;
            crypt(fresh);
            std::string p = std::string(config) + "manifest.enc";
            std::ofstream f(p + ".tmp", std::ios::binary);
            f.write(fresh.data(), fresh.size());
            f.close();
            if (f) {
                remove((p + ".bak").c_str());
                rename(p.c_str(), (p + ".bak").c_str());
                if (rename((p + ".tmp").c_str(), p.c_str()) != 0)
                    rename((p + ".bak").c_str(), p.c_str());
            }
            return;
        }
        if (r != CURLE_OK)
            error = cancelWork ? "Catalog update cancelled" : curl_easy_strerror(r);
    } else
        error = "Network initialization failed";
    std::string unused;
    if (!data.empty() && parseCatalog(data, catalog, unused)) {
        catalogOkay = true;
        error = "Offline catalog loaded; update unavailable";
    }
}
void runDownloads() {
    for (size_t i = 0; i < jobs.size(); ++i) {
        auto &j = jobs[i];
        if (j.status != Status::Waiting)
            continue;
        if (cancelWork) {
            j.status = Status::Cancelled;
            j.detail = "Cancelled";
            continue;
        }
        j.status = Status::Active;
        Context ctx;
        ctx.p.title = j.item.name;
        ctx.p.index = i + 1;
        ctx.p.count = jobs.size();
        ctx.p.detail = "Connecting";
        ctx.tick = SDL_GetTicks64();
        publish(ctx.p);
        std::string target = output + j.item.name;
        struct stat st{};
        if (lstat(target.c_str(), &st) == 0) {
            j.status = S_ISREG(st.st_mode) ? Status::Skipped : Status::Failed;
            j.detail =
                S_ISREG(st.st_mode) ? "Already exists; preserved" : "Destination is not a regular file";
            continue;
        }
        std::string pattern = output + ".rom-download-XXXXXX";
        std::vector<char> temp(pattern.begin(), pattern.end());
        temp.push_back(0);
        int fd = mkstemp(temp.data());
        FILE *f = fd >= 0 ? fdopen(fd, "wb") : nullptr;
        if (!f && fd >= 0)
            close(fd);
        CURL *c = curl_easy_init();
        CURLcode r = CURLE_WRITE_ERROR;
        if (f && c) {
            setvbuf(f, nullptr, _IOFBF, 65536);
            configure(c, j.item.url);
            curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, fileWrite);
            curl_easy_setopt(c, CURLOPT_WRITEDATA, f);
            curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, progress);
            curl_easy_setopt(c, CURLOPT_XFERINFODATA, &ctx);
            r = curl_easy_perform(c);
        }
        if (f && fclose(f) != 0)
            r = CURLE_WRITE_ERROR;
        if (c)
            curl_easy_cleanup(c);
        if (r == CURLE_OK && !cancelWork && rename(temp.data(), target.c_str()) == 0) {
            j.status = Status::Saved;
            j.detail = "Saved";
            std::string name = lower(j.item.name);
            if (extractTask && name.size() > 4 && name.substr(name.size() - 4) == ".zip" &&
                lower(j.item.category) != "arcade") {
                extractZip(target, j.item.category, ctx, j.detail);
            }
        } else {
            remove(temp.data());
            j.status = cancelWork ? Status::Cancelled : Status::Failed;
            j.detail = cancelWork      ? "Cancelled"
                       : r == CURLE_OK ? "Could not save file"
                                       : curl_easy_strerror(r);
        }
    }
}
void entry(void *) {
    if (catalogTask)
        runCatalog();
    else
        runDownloads();
    done = true;
}
bool launch() {
    cancelWork = false;
    pauseWork = false;
    done = false;
    shared = {};
    mutexInit(&mutex);
    if (R_FAILED(threadCreate(&thread, entry, nullptr, nullptr, 256 * 1024, 0x3B, -2))) {
        notify("Could not start background task");
        return false;
    }
    if (R_FAILED(threadStart(&thread))) {
        threadClose(&thread);
        notify("Could not start worker");
        return false;
    }
    active = true;
    return true;
}
} // namespace
void startCatalog(bool refresh) {
    if (active)
        return;
    catalogTask = true;
    catalogLimit = app.compact ? 4 * 1024 * 1024 : 24 * 1024 * 1024;
    refreshTask = refresh;
    catalogOkay = false;
    catalog.clear();
    error.clear();
    if (launch())
        app.loading = true;
}
void startDownloads(bool retry) {
    if (active)
        return;
    catalogTask = false;
    output = app.path;
    extractTask = app.settings.extract;
    deleteOriginalTask = app.settings.deleteOriginal;
    jobs = app.jobs;
    if (retry)
        for (auto &j : jobs)
            if (j.status == Status::Failed || j.status == Status::Cancelled) {
                j.status = Status::Waiting;
                j.detail.clear();
            }
    if (launch()) {
        app.progress = {};
        go(Screen::Transfer);
    }
}
void pollWorker() {
    if (!active)
        return;
    mutexLock(&mutex);
    app.progress = shared;
    mutexUnlock(&mutex);
    if (!done)
        return;
    threadWaitForExit(&thread);
    threadClose(&thread);
    active = false;
    if (catalogTask) {
        app.loading = false;
        if (catalogOkay) {
            app.categories = std::move(catalog);
            app.category = app.shelfCursor = 0;
            std::set<std::string> retained;
            for (const auto &c : app.categories)
                for (const auto &item : c.items)
                    if (app.selected.count(item.key()))
                        retained.insert(item.key());
            app.selected = std::move(retained);
            rebuild();
        }
        notify(error.empty() ? "Collections ready" : error);
    } else {
        app.jobs = std::move(jobs);
        go(Screen::Results);
        if (app.settings.sound && app.complete)
            Mix_PlayChannel(-1, app.complete, 0);
    }
}
void stopWorker() {
    if (active) {
        cancelWork = true;
        pauseWork = false;
        threadWaitForExit(&thread);
        threadClose(&thread);
        active = false;
    }
}
} // namespace rd
