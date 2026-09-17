#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <curl/curl.h>
#include <minizip/unzip.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <cctype>

#include "themes.h" // Loads UI Palette variables and Switcher Logic

// --- App State Engine ---
enum AppState {
    STATE_MAIN,          // Console subfolder selection
    STATE_ROMS,          // ROM list in selected console
    STATE_PATH_PICKER,   // Select output SD folder
    STATE_PATH_HISTORY,  // Select from saved path history
    STATE_DOWNLOADING    // Download and extract progress
};

struct RepoItem {
    std::string name;
    std::string path;
    std::string downloadUrl;
    bool isDir;
    bool isSelected = false;
};

// --- Global Variables ---
AppState currentState = STATE_MAIN;
bool sfxEnabled = true;
std::string currentConsoleName = "";
std::string currentConsolePath = "";
std::string currentSdPath = "sdmc:/";

std::vector<RepoItem> consoleList;
std::vector<RepoItem> otherList; // For eBook and Music
std::map<std::string, std::vector<RepoItem>> consoleRomMap;
std::vector<RepoItem> romList;
std::vector<std::string> dirList;
std::vector<std::string> pathHistory;

int selectedConsoleIdx = 0;
int selectedOtherIdx = 0;
int selectedSection = 0; // 0 = Consoles, 1 = Other

int selectedRomIdx = 0;
int selectedDirIdx = 0;
int selectedHistoryIdx = 0;

int consoleScrollOffset = 0;
int otherScrollOffset = 0;
int romScrollOffset = 0;
int dirScrollOffset = 0;

// Download/Extract Progress Globals
std::string currentStatusText = "";

TTF_Font* font24 = nullptr;
TTF_Font* font18 = nullptr;
TTF_Font* fontLabel = nullptr;
SDL_Renderer* globalRenderer = nullptr;
SDL_Texture* scanlineTexture = nullptr;

Mix_Chunk* sfxNav = nullptr;
Mix_Chunk* sfxClick = nullptr;
Mix_Chunk* sfxComp = nullptr;

// Cache map for console icons
std::map<std::string, SDL_Texture*> coverCache;

// --- Forward Declarations ---
void renderProgressScreen(const std::string& statusText, float percent);
int downloadProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

// --- Scanline Overlay Pre-renderer ---
void initScanlineTexture() {
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_RGBA8888);
    if (surf) {
        SDL_FillRect(surf, NULL, SDL_MapRGBA(surf->format, 0, 0, 0, 0));
        Uint32 lineCol = SDL_MapRGBA(surf->format, 0, 0, 0, 15);
        for (int y = 0; y < 720; y += 3) {
            SDL_Rect line = { 0, y, 1280, 1 };
            SDL_FillRect(surf, &line, lineCol);
        }
        scanlineTexture = SDL_CreateTextureFromSurface(globalRenderer, surf);
        SDL_FreeSurface(surf);
        if (scanlineTexture) {
            SDL_SetTextureBlendMode(scanlineTexture, SDL_BLENDMODE_BLEND);
        }
    }
}

void renderScanlines() {
    if (scanlineTexture) {
        SDL_RenderCopy(globalRenderer, scanlineTexture, NULL, NULL);
    }
}

void loadSettings() {
    std::ifstream in("sdmc:/switch/ROM_Downloader/settings.txt");
    if (in.is_open()) {
        int sfxVal;
        if (in >> sfxVal) sfxEnabled = (sfxVal != 0);
        
        int themeVal;
        // Read the theme index and apply it, defaulting to 0 if it goes out of bounds
        if (in >> themeVal) {
            if (themeVal >= 0 && themeVal <= 6) {
                applyTheme(themeVal);
            } else {
                applyTheme(0);
            }
        }
    }
}

void saveSettings() {
    mkdir("sdmc:/switch/ROM_Downloader", 0777);
    std::ofstream out("sdmc:/switch/ROM_Downloader/settings.txt");
    // Save both SFX state and Theme Index separated by a space
    out << (sfxEnabled ? 1 : 0) << " " << currentThemeIdx << "\n";
}

int playSfx(Mix_Chunk* chunk) {
    if (sfxEnabled && chunk) {
        return Mix_PlayChannel(-1, chunk, 0); 
    }
    return -1; 
}

// --- Network Callbacks ---
size_t StringWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

size_t FileWriteCallback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    return fwrite(ptr, size, nmemb, stream);
}

struct MemoryBuffer {
    unsigned char* data;
    size_t size;
};

size_t MemoryWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    MemoryBuffer* mem = (MemoryBuffer*)userp;
    unsigned char* ptr = (unsigned char*)realloc(mem->data, mem->size + total);
    if (!ptr) return 0;
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, total);
    mem->size += total;
    return total;
}

// --- Manifest Caching & Encryption ---
void cryptManifest(std::string& data) {
    const std::string key = "S1ckDuck69!SwitchApp"; 
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] ^= key[i % key.length()];
    }
}

void saveManifestCache(std::string data) {
    mkdir("sdmc:/switch/ROM_Downloader", 0777);
    cryptManifest(data);
    std::ofstream out("sdmc:/switch/ROM_Downloader/manifest.enc", std::ios::binary);
    if (out.is_open()) {
        out.write(data.c_str(), data.size());
        out.close();
    }
}

bool loadManifestCache(std::string& outData) {
    std::ifstream in("sdmc:/switch/ROM_Downloader/manifest.enc", std::ios::binary);
    if (!in.is_open()) return false;
    
    outData.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    
    cryptManifest(outData);
    return true;
}

// Custom Manifest JSON Parser
void parseManifestJSON(const std::string& json) {
    consoleList.clear();
    otherList.clear();
    consoleRomMap.clear();

    size_t pos = 0;
    while ((pos = json.find("{", pos)) != std::string::npos) {
        size_t end = json.find("}", pos);
        if (end == std::string::npos) break;

        auto getValue = [](const std::string& b, const std::string& key) {
            size_t kPos = b.find("\"" + key + "\":");
            if (kPos == std::string::npos) return std::string("");
            size_t start = b.find("\"", kPos + key.length() + 2);
            if (start == std::string::npos) return std::string("");
            size_t finish = b.find("\"", start + 1);
            if (finish == std::string::npos) return std::string("");
            return b.substr(start + 1, finish - start - 1);
        };

        std::string block = json.substr(pos, end - pos + 1);
        std::string name = getValue(block, "name");
        std::string console = getValue(block, "console");
        std::string downloadUrl = getValue(block, "downloadUrl");
        if (downloadUrl.empty()) downloadUrl = getValue(block, "download_url");

        if (!console.empty()) {
            if (consoleRomMap.find(console) == consoleRomMap.end()) {
                RepoItem cItem;
                cItem.name = console;
                cItem.path = console;
                cItem.isDir = true;
                
                if (console == "eBook" || console == "Music") {
                    otherList.push_back(cItem);
                } else {
                    consoleList.push_back(cItem);
                }
                
                consoleRomMap[console] = std::vector<RepoItem>();
            }

            if (name == ".empty") {
                pos = end + 1;
                continue;
            }

            if (!name.empty() && !downloadUrl.empty()) {
                RepoItem rItem;
                rItem.name = name;
                rItem.path = console + "/" + name;
                rItem.downloadUrl = downloadUrl;
                rItem.isDir = false;
                consoleRomMap[console].push_back(rItem);
            }
        }
        pos = end + 1;
    }
}

// --- Helper to fix spaces in URLs ---
std::string urlEncode(const std::string& value) {
    std::string result;
    for (char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            result += c;
        } else if (c == ' ') {
            result += "%20"; 
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
            result += buf;
        }
    }
    return result;
}

// --- Fixed Cover Downloader ---
void fetchCoverImage(const std::string& folderName) {
    mkdir("sdmc:/switch/ROM_Downloader", 0777);
    mkdir("sdmc:/switch/ROM_Downloader/covers", 0777);
    std::string localPath = "sdmc:/switch/ROM_Downloader/covers/" + folderName + ".png";

    struct stat st;
    if (stat(localPath.c_str(), &st) == 0 && st.st_size > 100) return;

    std::string coverUrl = "https://raw.githubusercontent.com/SickDuck696969/ROM-Collection/master/" + urlEncode(folderName) + "/cover.png";
    
    CURL* curl = curl_easy_init();
    if (!curl) return;

    MemoryBuffer mem = { nullptr, 0 };
    curl_easy_setopt(curl, CURLOPT_URL, coverUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, MemoryWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mem);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ROM-Downloader-Switch");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L); 
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);

    CURLcode res = curl_easy_perform(curl);
    
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_easy_cleanup(curl);

    if (res == CURLE_OK && http_code == 200 && mem.size > 100) {
        FILE* fp = fopen(localPath.c_str(), "wb");
        if (fp) {
            fwrite(mem.data, 1, mem.size, fp);
            fclose(fp);
        }
    }
    
    if (mem.data) free(mem.data);
}

// Fetch Manifest via Raw GitHub CDN
void fetchManifest(bool forceDownload = false) {
    std::string buffer;

    if (!forceDownload && loadManifestCache(buffer) && !buffer.empty()) {
        parseManifestJSON(buffer);
        
        Uint64 startTicks = SDL_GetTicks64();
        Uint64 currentTicks = startTicks;
        
        while (currentTicks - startTicks < 3000) { 
            float percent = ((float)(currentTicks - startTicks) / 3000.0f) * 100.0f;
            renderProgressScreen("Loading Database...", percent);
            SDL_Delay(16); 
            currentTicks = SDL_GetTicks64();
        }
        
        renderProgressScreen("System Ready!", 100.0f);
        SDL_Delay(200); 
        return;
    }

    std::string url = "https://raw.githubusercontent.com/SickDuck696969/ROM-Collection/master/manifest.json";
    CURL* curl = curl_easy_init();
    
    currentStatusText = forceDownload ? "Updating ROM Database..." : "Fetching ROM Database...";

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StringWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "ROM-Downloader-Switch");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 64 * 1024L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
        
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, downloadProgressCallback);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK && !buffer.empty()) {
            saveManifestCache(buffer);
            parseManifestJSON(buffer);

            int totalCategories = consoleList.size() + otherList.size();
            int currentCat = 0;
            
            for (size_t i = 0; i < consoleList.size(); ++i) {
                currentCat++;
                currentStatusText = "Caching Covers (" + std::to_string(currentCat) + "/" + std::to_string(totalCategories) + ")";
                renderProgressScreen(currentStatusText, ((float)currentCat / (float)totalCategories) * 100.0f);
                fetchCoverImage(consoleList[i].name);
            }
            
            for (size_t i = 0; i < otherList.size(); ++i) {
                currentCat++;
                currentStatusText = "Caching Covers (" + std::to_string(currentCat) + "/" + std::to_string(totalCategories) + ")";
                renderProgressScreen(currentStatusText, ((float)currentCat / (float)totalCategories) * 100.0f);
                fetchCoverImage(otherList[i].name);
            }
            
            renderProgressScreen("Database Updated!", 100.0f);
            
            int sfxChannel = playSfx(sfxComp);
            if (sfxChannel != -1) {
                while (Mix_Playing(sfxChannel)) {
                    SDL_Delay(16); 
                }
            } else {
                SDL_Delay(1000); 
            }
        }
    }
}

// Path History Management
void loadHistory() {
    pathHistory.clear();
    mkdir("sdmc:/switch/ROM_Downloader", 0777);
    std::ifstream in("sdmc:/switch/ROM_Downloader/history.txt");
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) pathHistory.push_back(line);
    }
}

void addPathToHistory(const std::string& path) {
    if (std::find(pathHistory.begin(), pathHistory.end(), path) == pathHistory.end()) {
        pathHistory.push_back(path);
        std::ofstream out("sdmc:/switch/ROM_Downloader/history.txt");
        for (const auto& p : pathHistory) out << p << "\n";
    }
}

// FAST COVER TEXTURE LOAD
SDL_Texture* getCoverTexture(const std::string& folderName) {
    if (coverCache.count(folderName)) return coverCache[folderName];

    std::string localPath = "sdmc:/switch/ROM_Downloader/covers/" + folderName + ".png";

    SDL_Texture* localTex = IMG_LoadTexture(globalRenderer, localPath.c_str());
    if (localTex) {
        coverCache[folderName] = localTex;
        return localTex;
    }

    return nullptr;
}

// Directory loader 
void loadDirectoryList(const std::string& path) {
    dirList.clear();
    if (path != "sdmc:/") dirList.push_back("..");

    DIR* dir = opendir(path.c_str());
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type == DT_DIR) {
                std::string name = ent->d_name;
                if (name != "." && name != "..") dirList.push_back(name);
            }
        }
        closedir(dir);
    }
    
    std::sort(dirList.begin(), dirList.end(), [](const std::string& a, const std::string& b) {
        if (a == "..") return true;
        if (b == "..") return false;
        
        std::string lower_a = a;
        std::string lower_b = b;
        std::transform(lower_a.begin(), lower_a.end(), lower_a.begin(), ::tolower);
        std::transform(lower_b.begin(), lower_b.end(), lower_b.begin(), ::tolower);
        
        return lower_a < lower_b;
    });
    
    selectedDirIdx = 0;
    dirScrollOffset = 0;
}

// --- UI Rendering Helpers ---
void renderText(const std::string& text, int x, int y, SDL_Color color, TTF_Font* font = font24) {
    if (text.empty() || !font) return;
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(globalRenderer, surface);
        SDL_Rect dest = { x, y, surface->w, surface->h };
        SDL_RenderCopy(globalRenderer, texture, NULL, &dest);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    }
}

void renderTextCentered(const std::string& text, int centerX, int y, SDL_Color color, TTF_Font* font = font18) {
    if (text.empty() || !font) return;
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(globalRenderer, surface);
        SDL_Rect dest = { centerX - surface->w / 2, y, surface->w, surface->h };
        SDL_RenderCopy(globalRenderer, texture, NULL, &dest);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    }
}

// Modern Progress Screen Render
void renderProgressScreen(const std::string& statusText, float percent) {
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;

    SDL_SetRenderDrawColor(globalRenderer, uiBg.r, uiBg.g, uiBg.b, 255);
    SDL_RenderClear(globalRenderer);

    SDL_Rect box = { 340, 240, 600, 240 };
    SDL_SetRenderDrawColor(globalRenderer, uiPanel.r, uiPanel.g, uiPanel.b, 255);
    SDL_RenderFillRect(globalRenderer, &box);
    
    SDL_SetRenderDrawColor(globalRenderer, uiBorder.r, uiBorder.g, uiBorder.b, 255);
    SDL_RenderDrawRect(globalRenderer, &box);

    renderTextCentered(statusText, 640, 290, uiText, font24);

    SDL_Rect barBg = { 390, 360, 500, 30 };
    SDL_SetRenderDrawColor(globalRenderer, uiBg.r, uiBg.g, uiBg.b, 255);
    SDL_RenderFillRect(globalRenderer, &barBg);

    int fillWidth = (int)((496.0f * percent) / 100.0f);
    SDL_Rect barFill = { 392, 362, fillWidth, 26 };
    SDL_SetRenderDrawColor(globalRenderer, uiSuccess.r, uiSuccess.g, uiSuccess.b, 255);
    SDL_RenderFillRect(globalRenderer, &barFill);

    char percentBuf[16];
    snprintf(percentBuf, sizeof(percentBuf), "%.1f%%", percent);
    renderTextCentered(percentBuf, 640, 420, uiTextDim, font18);

    renderScanlines();
    SDL_RenderPresent(globalRenderer);
}

// Download Network Progress Callback 
int downloadProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    if (dltotal > 0) {
        float percent = ((float)dlnow / (float)dltotal) * 100.0f;
        
        static Uint64 lastTicks = 0;
        Uint64 currentTicks = SDL_GetTicks64();

        if (currentTicks - lastTicks >= 16 || percent >= 100.0f) {
            renderProgressScreen(currentStatusText, percent);
            lastTicks = currentTicks;
        }
    }
    return 0;
}

// Zip Extraction Engine
bool extractZipFile(const std::string& zipPath, const std::string& destDir) {
    unzFile zip = unzOpen(zipPath.c_str());
    if (!zip) return false;

    unz_global_info globalInfo;
    if (unzGetGlobalInfo(zip, &globalInfo) != UNZ_OK) {
        unzClose(zip);
        return false;
    }

    uLong totalFiles = globalInfo.number_entry;
    char filename[512];
    unz_file_info fileInfo;
    static Uint64 lastExtTicks = 0;

    for (uLong i = 0; i < totalFiles; i++) {
        if (unzGetCurrentFileInfo(zip, &fileInfo, filename, sizeof(filename), NULL, 0, NULL, 0) == UNZ_OK) {
            std::string fullPath = destDir;
            if (fullPath.back() != '/') fullPath += "/";
            fullPath += filename;

            if (filename[strlen(filename) - 1] == '/') {
                mkdir(fullPath.c_str(), 0777);
            } else {
                if (unzOpenCurrentFile(zip) == UNZ_OK) {
                    FILE* outFile = fopen(fullPath.c_str(), "wb");
                    if (outFile) {
                        setvbuf(outFile, NULL, _IOFBF, 64 * 1024);
                        char buffer[32768]; 
                        int readBytes = 0;
                        uLong totalRead = 0;
                        uLong expectedSize = fileInfo.uncompressed_size;

                        while ((readBytes = unzReadCurrentFile(zip, buffer, sizeof(buffer))) > 0) {
                            fwrite(buffer, 1, readBytes, outFile);
                            totalRead += readBytes;

                            Uint64 curExtTicks = SDL_GetTicks64();
                            if (curExtTicks - lastExtTicks >= 33) { 
                                float fileProgress = expectedSize > 0 ? ((float)totalRead / (float)expectedSize) : 0.0f;
                                float overallPercent = (((float)i + fileProgress) / (float)totalFiles) * 100.0f;
                                
                                renderProgressScreen(currentStatusText, overallPercent);
                                lastExtTicks = curExtTicks;
                            }
                        }
                        fclose(outFile);
                    }
                    unzCloseCurrentFile(zip);
                }
            }
        }

        Uint64 curTicks = SDL_GetTicks64();
        if (curTicks - lastExtTicks >= 33 || i == totalFiles - 1) {
            float endPercent = (((float)(i + 1)) / (float)totalFiles) * 100.0f;
            renderProgressScreen(currentStatusText, endPercent);
            lastExtTicks = curTicks;
        }

        if (i < totalFiles - 1) unzGoToNextFile(zip);
    }

    unzClose(zip);
    renderProgressScreen(currentStatusText, 100.0f);
    SDL_Delay(100);

    return true;
}

// Download Execution Queue
void executeDownloads() {
    std::vector<RepoItem> targets;
    for (const auto& item : romList) {
        if (item.isSelected) targets.push_back(item);
    }
    if (targets.empty() && !romList.empty()) {
        targets.push_back(romList[selectedRomIdx]);
    }

    for (size_t i = 0; i < targets.size(); ++i) {
        const auto& item = targets[i];
        std::string targetZip = currentSdPath;
        if (targetZip.back() != '/') targetZip += "/";
        targetZip += item.name;

        currentStatusText = "Downloading [" + std::to_string(i + 1) + "/" + std::to_string(targets.size()) + "]: " + item.name;

        CURL* curl = curl_easy_init();
        if (curl) {
            FILE* fp = fopen(targetZip.c_str(), "wb");
            if (fp) {
                setvbuf(fp, NULL, _IOFBF, 128 * 1024);

                curl_easy_setopt(curl, CURLOPT_URL, item.downloadUrl.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, FileWriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
                curl_easy_setopt(curl, CURLOPT_USERAGENT, "ROM-Downloader-Switch");
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
                curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, downloadProgressCallback);
                curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 64 * 1024L);
                curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

                CURLcode res = curl_easy_perform(curl);
                fclose(fp);

                if (res == CURLE_OK) {
                    renderProgressScreen(currentStatusText, 100.0f);
                    SDL_Delay(100);
                }

                std::string lowerName = item.name;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                bool isZip = (lowerName.length() >= 4 && lowerName.substr(lowerName.length() - 4) == ".zip");
                bool isArcade = (currentConsoleName == "Arcade");

                if (res == CURLE_OK && isZip && !isArcade) {
                    currentStatusText = "Extracting: " + item.name;
                    extractZipFile(targetZip, currentSdPath);
                    remove(targetZip.c_str());
                }
            }
            curl_easy_cleanup(curl);
        }
    }

    playSfx(sfxComp);
    currentState = STATE_ROMS;
}

// MAIN ENTRY
int main(int argc, char* argv[]) {
    romfsInit();
    socketInitializeDefault();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    TTF_Init();

    applyTheme(0); // Initialize Theme Palette First

    Mix_Init(MIX_INIT_OGG);
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);

    sfxNav = Mix_LoadWAV("romfs:/nav.wav");
    sfxClick = Mix_LoadWAV("romfs:/click.wav");
    sfxComp = Mix_LoadWAV("romfs:/comp.wav");

    SDL_Window* window = SDL_CreateWindow("ROM Downloader", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, 0);
    globalRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    initScanlineTexture();

    // Fonts increased significantly for legibility 
    font24 = TTF_OpenFont("romfs:/font.ttf", 42); 
    font18 = TTF_OpenFont("romfs:/font.ttf", 30); 
    fontLabel = TTF_OpenFont("romfs:/font.ttf", 36); 

    loadHistory();
    loadSettings();
    
    fetchManifest(false);

    bool initialLaunchSfxPending = true;
    int repeatTimer = 0;
    u64 lastHeldButton = 0;
    u64 repeatableButtons = HidNpadButton_Up | HidNpadButton_Down |
                            HidNpadButton_Left | HidNpadButton_Right |
                            HidNpadButton_StickLUp | HidNpadButton_StickLDown |
                            HidNpadButton_StickLLeft | HidNpadButton_StickLRight;

    bool running = true;
    while (appletMainLoop() && running) {
        padUpdate(&pad);
        u64 kDownRaw = padGetButtonsDown(&pad);
        u64 kHeldRaw = padGetButtons(&pad);

        u64 kDown = kDownRaw;
        if (kDownRaw & repeatableButtons) {
            repeatTimer = 0;
            lastHeldButton = (kDownRaw & repeatableButtons);
        } else if (kHeldRaw & lastHeldButton) {
            repeatTimer++;
            if (repeatTimer >= 25) {
                if (repeatTimer % 5 == 0) kDown |= lastHeldButton;
            }
        } else {
            lastHeldButton = 0;
            repeatTimer = 0;
        }

        // --- THEME SWITCHER DETECT (Simultaneous ZL + ZR) ---
        if (((kHeldRaw & HidNpadButton_ZL) && (kDown & HidNpadButton_ZR)) || 
            ((kHeldRaw & HidNpadButton_ZR) && (kDown & HidNpadButton_ZL))) {
            cycleTheme();
            saveSettings(); // Saves the new theme instantly
            playSfx(sfxComp);
        }

        if (kDown & (repeatableButtons | HidNpadButton_L | HidNpadButton_R)) playSfx(sfxNav);
        if (kDown & HidNpadButton_Minus) {
            sfxEnabled = !sfxEnabled;
            saveSettings();
            if (sfxEnabled) playSfx(sfxClick);
        } else if (kDown & (HidNpadButton_A | HidNpadButton_B | HidNpadButton_X | HidNpadButton_Y | HidNpadButton_Plus)) {
            playSfx(sfxClick);
        }

        // Controller Engine State Management
        if (currentState == STATE_MAIN) {
            if (kDown & HidNpadButton_Plus) running = false;
            
            if (kDown & HidNpadButton_Y) {
                fetchManifest(true);
            }

            // Tab Toggle
            if (kDown & HidNpadButton_X) {
                selectedSection = (selectedSection == 0) ? 1 : 0;
            }

            if (selectedSection == 0) { // Consoles Active
                if (!consoleList.empty()) {
                    if (kDown & (HidNpadButton_Up | HidNpadButton_StickLUp)) {
                        if (selectedConsoleIdx >= 4) selectedConsoleIdx -= 4;
                    }
                    if (kDown & (HidNpadButton_Down | HidNpadButton_StickLDown)) {
                        if (selectedConsoleIdx + 4 < (int)consoleList.size()) {
                            selectedConsoleIdx += 4;
                        }
                    }
                    if (kDown & (HidNpadButton_Left | HidNpadButton_StickLLeft)) {
                        selectedConsoleIdx = std::max(0, selectedConsoleIdx - 1);
                    }
                    if (kDown & (HidNpadButton_Right | HidNpadButton_StickLRight)) {
                        selectedConsoleIdx = std::min((int)consoleList.size() - 1, selectedConsoleIdx + 1);
                    }

                    if (kDown & HidNpadButton_L) selectedConsoleIdx = std::max(0, selectedConsoleIdx - 12);
                    if (kDown & HidNpadButton_R) selectedConsoleIdx = std::min((int)consoleList.size() - 1, selectedConsoleIdx + 12);

                    if (kDown & HidNpadButton_A) {
                        currentConsoleName = consoleList[selectedConsoleIdx].name;
                        currentConsolePath = consoleList[selectedConsoleIdx].path;

                        romList = consoleRomMap[currentConsoleName];
                        selectedRomIdx = 0;
                        romScrollOffset = 0;
                        currentState = STATE_ROMS;
                    }
                } 
            } else if (selectedSection == 1) { // Other Section Active
                if (!otherList.empty()) {
                    if (kDown & (HidNpadButton_Up | HidNpadButton_StickLUp)) {
                        if (selectedOtherIdx >= 4) selectedOtherIdx -= 4;
                    }
                    if (kDown & (HidNpadButton_Down | HidNpadButton_StickLDown)) {
                        if (selectedOtherIdx + 4 < (int)otherList.size()) {
                            selectedOtherIdx += 4;
                        }
                    }
                    if (kDown & (HidNpadButton_Left | HidNpadButton_StickLLeft)) {
                        selectedOtherIdx = std::max(0, selectedOtherIdx - 1);
                    }
                    if (kDown & (HidNpadButton_Right | HidNpadButton_StickLRight)) {
                        selectedOtherIdx = std::min((int)otherList.size() - 1, selectedOtherIdx + 1);
                    }

                    if (kDown & HidNpadButton_L) selectedOtherIdx = std::max(0, selectedOtherIdx - 12);
                    if (kDown & HidNpadButton_R) selectedOtherIdx = std::min((int)otherList.size() - 1, selectedOtherIdx + 12);

                    if (kDown & HidNpadButton_A) {
                        currentConsoleName = otherList[selectedOtherIdx].name;
                        currentConsolePath = otherList[selectedOtherIdx].path;

                        romList = consoleRomMap[currentConsoleName];
                        selectedRomIdx = 0;
                        romScrollOffset = 0;
                        currentState = STATE_ROMS;
                    }
                }
            }
        } else if (currentState == STATE_ROMS) {
            if (kDown & HidNpadButton_B) {
                for (auto& item : consoleRomMap[currentConsoleName]) {
                    item.isSelected = false;
                }
                for (auto& item : romList) {
                    item.isSelected = false;
                }
                currentState = STATE_MAIN;
            }

            if (kDown & HidNpadButton_Y) {
                romList = consoleRomMap[currentConsoleName];
                selectedRomIdx = 0;
                romScrollOffset = 0;
            }

            if (!romList.empty()) {
                if (kDown & (HidNpadButton_Up | HidNpadButton_StickLUp)) selectedRomIdx = std::max(0, selectedRomIdx - 1);
                if (kDown & (HidNpadButton_Down | HidNpadButton_StickLDown)) selectedRomIdx = std::min((int)romList.size() - 1, selectedRomIdx + 1);

                if (kDown & (HidNpadButton_Left | HidNpadButton_StickLLeft)) selectedRomIdx = std::max(0, selectedRomIdx - 9);
                if (kDown & (HidNpadButton_Right | HidNpadButton_StickLRight)) selectedRomIdx = std::min((int)romList.size() - 1, selectedRomIdx + 9);

                if (kDown & HidNpadButton_X) {
                    romList[selectedRomIdx].isSelected = !romList[selectedRomIdx].isSelected;
                    for (auto& masterItem : consoleRomMap[currentConsoleName]) {
                        if (masterItem.name == romList[selectedRomIdx].name) {
                            masterItem.isSelected = romList[selectedRomIdx].isSelected;
                            break;
                        }
                    }
                }

                if (kDown & HidNpadButton_A) {
                    loadDirectoryList(currentSdPath);
                    currentState = STATE_PATH_PICKER;
                }
            }

            if (kDown & HidNpadButton_Plus) {
                SwkbdConfig swkbd;
                if (R_SUCCEEDED(swkbdCreate(&swkbd, 0))) {
                    swkbdConfigMakePresetDefault(&swkbd);
                    swkbdConfigSetGuideText(&swkbd, "Search folder (empty to reset)");
                    
                    char inputBuf[256] = {0};
                    if (R_SUCCEEDED(swkbdShow(&swkbd, inputBuf, sizeof(inputBuf)))) {
                        std::string query = inputBuf;
                        std::transform(query.begin(), query.end(), query.begin(), ::tolower);

                        romList.clear();
                        for (const auto& item : consoleRomMap[currentConsoleName]) {
                            std::string lowerName = item.name;
                            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                            
                            if (query.empty() || lowerName.find(query) != std::string::npos) {
                                romList.push_back(item);
                            }
                        }
                        selectedRomIdx = 0;
                        romScrollOffset = 0;
                    }
                    swkbdClose(&swkbd);
                }
            }

        } else if (currentState == STATE_PATH_PICKER) {
            if (kDown & HidNpadButton_B) currentState = STATE_ROMS;

            if (!dirList.empty()) {
                if (kDown & (HidNpadButton_Up | HidNpadButton_StickLUp)) selectedDirIdx = std::max(0, selectedDirIdx - 1);
                if (kDown & (HidNpadButton_Down | HidNpadButton_StickLDown)) selectedDirIdx = std::min((int)dirList.size() - 1, selectedDirIdx + 1);

                if (kDown & (HidNpadButton_Left | HidNpadButton_StickLLeft)) selectedDirIdx = std::max(0, selectedDirIdx - 9);
                if (kDown & (HidNpadButton_Right | HidNpadButton_StickLRight)) selectedDirIdx = std::min((int)dirList.size() - 1, selectedDirIdx + 9);

                if (kDown & HidNpadButton_A) {
                    std::string chosen = dirList[selectedDirIdx];
                    if (chosen == "..") {
                        size_t lastSlash = currentSdPath.find_last_of('/');
                        if (lastSlash != std::string::npos && lastSlash >= 5) {
                            currentSdPath = currentSdPath.substr(0, lastSlash);
                            if (currentSdPath.length() <= 5) currentSdPath = "sdmc:/";
                        }
                    } else {
                        if (currentSdPath.back() != '/') currentSdPath += "/";
                        currentSdPath += chosen;
                    }
                    loadDirectoryList(currentSdPath);
                }
            }

            if (kDown & HidNpadButton_Y) {
                if (!pathHistory.empty()) {
                    selectedHistoryIdx = 0;
                    currentState = STATE_PATH_HISTORY;
                }
            }

            if (kDown & HidNpadButton_X) {
                addPathToHistory(currentSdPath);
                currentState = STATE_DOWNLOADING;
                executeDownloads();
            }
        } else if (currentState == STATE_PATH_HISTORY) {
            if (kDown & HidNpadButton_B) currentState = STATE_PATH_PICKER;

            if (!pathHistory.empty()) {
                if (kDown & (HidNpadButton_Up | HidNpadButton_StickLUp)) selectedHistoryIdx = std::max(0, selectedHistoryIdx - 1);
                if (kDown & (HidNpadButton_Down | HidNpadButton_StickLDown)) selectedHistoryIdx = std::min((int)pathHistory.size() - 1, selectedHistoryIdx + 1);

                if (kDown & (HidNpadButton_Left | HidNpadButton_StickLLeft)) selectedHistoryIdx = std::max(0, selectedHistoryIdx - 9);
                if (kDown & (HidNpadButton_Right | HidNpadButton_StickLRight)) selectedHistoryIdx = std::min((int)pathHistory.size() - 1, selectedHistoryIdx + 9);

                if (kDown & HidNpadButton_A) {
                    currentSdPath = pathHistory[selectedHistoryIdx];
                    loadDirectoryList(currentSdPath);
                    currentState = STATE_PATH_PICKER;
                }
            }
        }

        // ==========================
        // UI Render Pass 
        // ==========================
        SDL_SetRenderDrawColor(globalRenderer, uiBg.r, uiBg.g, uiBg.b, 255);
        SDL_RenderClear(globalRenderer);

        // Top Header Base
        SDL_Rect headerRect = {0, 0, 1280, 75};
        SDL_SetRenderDrawColor(globalRenderer, uiPanel.r, uiPanel.g, uiPanel.b, 255);
        SDL_RenderFillRect(globalRenderer, &headerRect);
        
        // Header Bottom Shadow / Border
        SDL_SetRenderDrawColor(globalRenderer, uiBorder.r, uiBorder.g, uiBorder.b, 255);
        SDL_RenderDrawLine(globalRenderer, 0, 75, 1280, 75);

        // Header Content (Adjusted padding to compensate for larger font24)
        renderText("ROM Downloader", 40, 15, uiText, font24);
        renderText("SFX: " + std::string(sfxEnabled ? "ON" : "OFF"), 1140, 20, sfxEnabled ? uiSuccess : uiTextDim, font18);

        // Bottom Footer Base (anchors hints)
        SDL_Rect footerRect = {0, 660, 1280, 60};
        SDL_SetRenderDrawColor(globalRenderer, uiPanel.r, uiPanel.g, uiPanel.b, 255);
        SDL_RenderFillRect(globalRenderer, &footerRect);
        SDL_SetRenderDrawColor(globalRenderer, uiBorder.r, uiBorder.g, uiBorder.b, 255);
        SDL_RenderDrawLine(globalRenderer, 0, 660, 1280, 660);

        if (currentState == STATE_MAIN) {
            std::string tabSwitchText = selectedSection == 0 ? "Other Tab" : "Consoles Tab";
            renderText("[A] Open   [Y] Update   [-] SFX   [L/R] Page   [X] " + tabSwitchText + "   [ZL+ZR] Theme   [+] Exit", 40, 675, uiTextDim, font18);

            int cols = 4;
            int maxVisible = 12; // 3 rows of 4 (4x3 layout)
            int startY = 90;
            int startX = 40;
            int cellW = 1200 / cols;
            int cellH = 185; 

            if (selectedSection == 0) { // Consoles View
                int currentPage = selectedConsoleIdx / maxVisible;
                consoleScrollOffset = currentPage * maxVisible;

                for (size_t i = consoleScrollOffset; i < consoleList.size() && i < (size_t)(consoleScrollOffset + maxVisible); ++i) {
                    int r = (i - consoleScrollOffset) / cols;
                    int c = (i - consoleScrollOffset) % cols;

                    int xPos = startX + c * cellW;
                    int yPos = startY + r * cellH;

                    SDL_Rect cell = {xPos + 10, yPos + 10, cellW - 20, cellH - 20};

                    if ((int)i == selectedConsoleIdx) {
                        SDL_SetRenderDrawColor(globalRenderer, uiAccent.r, uiAccent.g, uiAccent.b, 255);
                        SDL_RenderFillRect(globalRenderer, &cell);
                    } else {
                        SDL_SetRenderDrawColor(globalRenderer, uiPanel.r, uiPanel.g, uiPanel.b, 255);
                        SDL_RenderFillRect(globalRenderer, &cell);
                        SDL_SetRenderDrawColor(globalRenderer, uiBorder.r, uiBorder.g, uiBorder.b, 255);
                        SDL_RenderDrawRect(globalRenderer, &cell);
                    }

                    int iconSize = 110;
                    int imgX = xPos + (cellW / 2) - (iconSize / 2);
                    int imgY = yPos + 18;
                    SDL_Rect imgRect = {imgX, imgY, iconSize, iconSize};

                    SDL_Texture* cover = getCoverTexture(consoleList[i].name);
                    if (cover) {
                        SDL_RenderCopy(globalRenderer, cover, NULL, &imgRect);
                    } else {
                        SDL_SetRenderDrawColor(globalRenderer, uiBg.r, uiBg.g, uiBg.b, 255);
                        SDL_RenderFillRect(globalRenderer, &imgRect);
                    }

                    renderTextCentered(consoleList[i].name, xPos + (cellW / 2), imgY + iconSize + 5, uiText, font18);
                }
            } else if (selectedSection == 1) { // Other Section View
                int currentPage = selectedOtherIdx / maxVisible;
                otherScrollOffset = currentPage * maxVisible;

                for (size_t i = otherScrollOffset; i < otherList.size() && i < (size_t)(otherScrollOffset + maxVisible); ++i) {
                    int r = (i - otherScrollOffset) / cols;
                    int c = (i - otherScrollOffset) % cols;

                    int xPos = startX + c * cellW;
                    int yPos = startY + r * cellH;

                    SDL_Rect cell = {xPos + 10, yPos + 10, cellW - 20, cellH - 20};

                    if ((int)i == selectedOtherIdx) {
                        SDL_SetRenderDrawColor(globalRenderer, uiAccent.r, uiAccent.g, uiAccent.b, 255);
                        SDL_RenderFillRect(globalRenderer, &cell);
                    } else {
                        SDL_SetRenderDrawColor(globalRenderer, uiPanel.r, uiPanel.g, uiPanel.b, 255);
                        SDL_RenderFillRect(globalRenderer, &cell);
                        SDL_SetRenderDrawColor(globalRenderer, uiBorder.r, uiBorder.g, uiBorder.b, 255);
                        SDL_RenderDrawRect(globalRenderer, &cell);
                    }

                    int iconSize = 110;
                    int imgX = xPos + (cellW / 2) - (iconSize / 2);
                    int imgY = yPos + 18;
                    SDL_Rect imgRect = {imgX, imgY, iconSize, iconSize};

                    SDL_Texture* cover = getCoverTexture(otherList[i].name);
                    if (cover) {
                        SDL_RenderCopy(globalRenderer, cover, NULL, &imgRect);
                    } else {
                        SDL_SetRenderDrawColor(globalRenderer, uiBg.r, uiBg.g, uiBg.b, 255);
                        SDL_RenderFillRect(globalRenderer, &imgRect);
                    }

                    renderTextCentered(otherList[i].name, xPos + (cellW / 2), imgY + iconSize + 5, uiText, font18);
                }
            }
        } else if (currentState == STATE_ROMS) {
            std::string labelType = "games";
            if (currentConsoleName == "eBook") labelType = "books";
            else if (currentConsoleName == "Music") labelType = "songs";
            
            std::string titleText = currentConsoleName + " (" + std::to_string(romList.size()) + " " + labelType + ")";
            renderTextCentered(titleText, 640, 24, uiTextDim, font24);
            
            renderText("[A] Confirm/DL   [X] Select   [Y] Reset Search   [+] Search   [L/R] Page   [B] Back", 40, 675, uiTextDim, font18);

            int maxVisible = 9; // Reduced to 9 rows to give larger fonts more breathing room
            if (selectedRomIdx < romScrollOffset) romScrollOffset = selectedRomIdx;
            if (selectedRomIdx >= romScrollOffset + maxVisible) romScrollOffset = selectedRomIdx - maxVisible + 1;

            int yPos = 90;
            for (size_t i = romScrollOffset; i < romList.size() && i < (size_t)(romScrollOffset + maxVisible); ++i) {
                SDL_Rect row = {40, yPos, 1200, 56}; // Taller row
                
                // Active/Hover row highlight
                if ((int)i == selectedRomIdx) {
                    SDL_SetRenderDrawColor(globalRenderer, uiAccent.r, uiAccent.g, uiAccent.b, 255);
                    SDL_RenderFillRect(globalRenderer, &row);
                } else if (i % 2 == 0) {
                    SDL_SetRenderDrawColor(globalRenderer, uiPanel.r, uiPanel.g, uiPanel.b, 100);
                    SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_BLEND);
                    SDL_RenderFillRect(globalRenderer, &row);
                    SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_NONE);
                }

                // Graphical Checkbox - Scaled up for legibility
                SDL_Rect checkOuter = { row.x + 15, row.y + 16, 24, 24 };
                SDL_SetRenderDrawColor(globalRenderer, uiBorder.r, uiBorder.g, uiBorder.b, 255);
                SDL_RenderDrawRect(globalRenderer, &checkOuter);

                if (romList[i].isSelected) {
                    SDL_Rect checkInner = { row.x + 19, row.y + 20, 16, 16 };
                    SDL_SetRenderDrawColor(globalRenderer, uiSuccess.r, uiSuccess.g, uiSuccess.b, 255);
                    SDL_RenderFillRect(globalRenderer, &checkInner);
                }

                SDL_Color textColor = ((int)i == selectedRomIdx) ? uiText : (romList[i].isSelected ? uiSuccess : uiText);
                renderText(romList[i].name, row.x + 55, row.y + 10, textColor, fontLabel);
                
                yPos += 60; // Taller increment
            }
        } else if (currentState == STATE_PATH_PICKER) {
            renderText("SD Path: " + currentSdPath, 40, 85, uiTextDim, font18);
            renderText("[A] Enter   [X] Confirm   [Y] History   [Left/Right] Page   [B] Back", 40, 675, uiTextDim, font18);

            int maxVisible = 9;
            if (selectedDirIdx < dirScrollOffset) dirScrollOffset = selectedDirIdx;
            if (selectedDirIdx >= dirScrollOffset + maxVisible) dirScrollOffset = selectedDirIdx - maxVisible + 1;

            int yPos = 130;
            for (size_t i = dirScrollOffset; i < dirList.size() && i < (size_t)(dirScrollOffset + maxVisible); ++i) {
                SDL_Rect row = {40, yPos, 1200, 52};
                if ((int)i == selectedDirIdx) {
                    SDL_SetRenderDrawColor(globalRenderer, uiAccent.r, uiAccent.g, uiAccent.b, 255);
                    SDL_RenderFillRect(globalRenderer, &row);
                } else if (i % 2 == 0) {
                    SDL_SetRenderDrawColor(globalRenderer, uiPanel.r, uiPanel.g, uiPanel.b, 100);
                    SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_BLEND);
                    SDL_RenderFillRect(globalRenderer, &row);
                    SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_NONE);
                }
                
                std::string labelText = (dirList[i] == "..") ? "  ↑  [Up Directory]" : "  📁  " + dirList[i];
                renderText(labelText, 60, yPos + 10, uiText, font18);
                yPos += 55;
            }
        } else if (currentState == STATE_PATH_HISTORY) {
            renderText("Select Path from History", 40, 85, uiTextDim, font24);
            renderText("[A] Select   [Left/Right] Page   [B] Cancel", 40, 675, uiTextDim, font18);

            int yPos = 130;
            for (size_t i = 0; i < pathHistory.size() && i < 9; ++i) {
                SDL_Rect row = {40, yPos, 1200, 52};
                if ((int)i == selectedHistoryIdx) {
                    SDL_SetRenderDrawColor(globalRenderer, uiAccent.r, uiAccent.g, uiAccent.b, 255);
                    SDL_RenderFillRect(globalRenderer, &row);
                } else if (i % 2 == 0) {
                    SDL_SetRenderDrawColor(globalRenderer, uiPanel.r, uiPanel.g, uiPanel.b, 100);
                    SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_BLEND);
                    SDL_RenderFillRect(globalRenderer, &row);
                    SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_NONE);
                }
                renderText("  📌  " + pathHistory[i], 60, yPos + 10, uiText, font18);
                yPos += 55;
            }
        }

        renderScanlines();
        SDL_RenderPresent(globalRenderer);

        if (initialLaunchSfxPending) {
            playSfx(sfxComp);
            initialLaunchSfxPending = false;
        }
    }

    for (auto& pair : coverCache) {
        if (pair.second) SDL_DestroyTexture(pair.second);
    }
    if (scanlineTexture) SDL_DestroyTexture(scanlineTexture);

    if (sfxNav) Mix_FreeChunk(sfxNav);
    if (sfxClick) Mix_FreeChunk(sfxClick);
    if (sfxComp) Mix_FreeChunk(sfxComp);

    Mix_CloseAudio();
    Mix_Quit();

    TTF_CloseFont(font24);
    TTF_CloseFont(font18);
    TTF_CloseFont(fontLabel);
    TTF_Quit();
    IMG_Quit();
    SDL_DestroyRenderer(globalRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    curl_global_cleanup();
    socketExit();
    romfsExit();

    return 0;
}