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
std::map<std::string, std::vector<RepoItem>> consoleRomMap;
std::vector<RepoItem> romList;
std::vector<std::string> dirList;
std::vector<std::string> pathHistory;

int selectedConsoleIdx = 0;
int selectedRomIdx = 0;
int selectedDirIdx = 0;
int selectedHistoryIdx = 0;

int consoleScrollOffset = 0;
int romScrollOffset = 0;
int dirScrollOffset = 0;

// Download/Extract Smooth Progress Globals
std::string currentStatusText = "";
static float targetProgressPercent = 0.0f;
static float displayedProgressPercent = 0.0f;

TTF_Font* font24 = nullptr;
TTF_Font* font18 = nullptr;
TTF_Font* fontLabel = nullptr;
SDL_Renderer* globalRenderer = nullptr;
SDL_Texture* bgTexture = nullptr;

Mix_Chunk* sfxNav = nullptr;
Mix_Chunk* sfxClick = nullptr;
Mix_Chunk* sfxComp = nullptr;

// Cache map for console icons
std::map<std::string, SDL_Texture*> coverCache;

// --- Forward Declarations ---
void renderProgressScreen(const std::string& statusText, float percent);
int downloadProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

// --- Audio & Settings Helpers ---
void loadSettings() {
    std::ifstream in("sdmc:/switch/ROM_Downloader/settings.txt");
    if (in.is_open()) {
        int val;
        if (in >> val) sfxEnabled = (val != 0);
    }
}

void saveSettings() {
    mkdir("sdmc:/switch/ROM_Downloader", 0777);
    std::ofstream out("sdmc:/switch/ROM_Downloader/settings.txt");
    out << (sfxEnabled ? 1 : 0);
}

void playSfx(Mix_Chunk* chunk) {
    if (sfxEnabled && chunk) {
        Mix_PlayChannel(-1, chunk, 0);
    }
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
    // Simple XOR Cipher to encrypt/decrypt the manifest locally
    const std::string key = "S1ckDuck69!SwitchApp"; 
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] ^= key[i % key.length()];
    }
}

void saveManifestCache(std::string data) {
    mkdir("sdmc:/switch/ROM_Downloader", 0777);
    cryptManifest(data); // Encrypt before writing
    std::ofstream out("sdmc:/switch/ROM_Downloader/manifest.enc", std::ios::binary);
    if (out.is_open()) {
        out.write(data.c_str(), data.size());
        out.close();
    }
}

bool loadManifestCache(std::string& outData) {
    std::ifstream in("sdmc:/switch/ROM_Downloader/manifest.enc", std::ios::binary);
    if (!in.is_open()) return false;
    
    // Read raw encrypted data
    outData.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    
    // Decrypt back to valid JSON
    cryptManifest(outData);
    return true;
}

// Custom Manifest JSON Parser (Bypasses GitHub REST API)
void parseManifestJSON(const std::string& json) {
    consoleList.clear();
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
                consoleList.push_back(cItem);
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

// Fetch Manifest via Raw GitHub CDN (Unlimited Requests)
void fetchManifest(bool forceDownload = false) {
    std::string buffer;

    // Check for encrypted local cache first for instant boot
    if (!forceDownload && loadManifestCache(buffer) && !buffer.empty()) {
        currentStatusText = "Loading Encrypted Cache...";
        renderProgressScreen(currentStatusText, 100.0f);
        parseManifestJSON(buffer);
        SDL_Delay(100); 
        return;
    }

    std::string url = "https://raw.githubusercontent.com/SickDuck696969/ROM-Collection/master/manifest.json";
    CURL* curl = curl_easy_init();
    
    currentStatusText = forceDownload ? "Updating ROM Database..." : "Fetching ROM Database...";
    targetProgressPercent = 0.0f;
    displayedProgressPercent = 0.0f;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StringWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "ROM-Downloader-Switch");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 64 * 1024L);
        
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, downloadProgressCallback);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK && !buffer.empty()) {
            saveManifestCache(buffer); // Encrypt and save for next time
            parseManifestJSON(buffer);
            
            renderProgressScreen("Database Updated!", 100.0f);
            SDL_Delay(300);
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

// Download cover image
SDL_Texture* loadCoverTexture(const std::string& folderName) {
    if (coverCache.count(folderName)) return coverCache[folderName];

    std::string coverUrl = "https://raw.githubusercontent.com/SickDuck696969/ROM-Collection/master/" + folderName + "/cover.png";
    CURL* curl = curl_easy_init();
    if (!curl) return nullptr;

    MemoryBuffer mem = { nullptr, 0 };
    curl_easy_setopt(curl, CURLOPT_URL, coverUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, MemoryWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mem);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ROM-Downloader-Switch");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK && mem.size > 0) {
        SDL_RWops* rw = SDL_RWFromMem(mem.data, mem.size);
        SDL_Surface* surf = IMG_Load_RW(rw, 1);
        free(mem.data);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(globalRenderer, surf);
            SDL_FreeSurface(surf);
            coverCache[folderName] = tex;
            return tex;
        }
    }
    if (mem.data) free(mem.data);
    return nullptr;
}

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
    std::sort(dirList.begin(), dirList.end());
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

void renderProgressScreen(const std::string& statusText, float percent) {
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;

    SDL_Color retroBg = {10, 15, 20, 255};
    SDL_Color retroCyan = {50, 200, 255, 255};
    SDL_Color retroGreen = {50, 255, 100, 255};
    SDL_Color retroDark = {20, 30, 40, 255};

    SDL_SetRenderDrawColor(globalRenderer, retroBg.r, retroBg.g, retroBg.b, 255);
    SDL_RenderClear(globalRenderer);

    if (bgTexture) {
        SDL_RenderCopy(globalRenderer, bgTexture, NULL, NULL);
    }

    SDL_Rect box = { 340, 240, 600, 240 };
    SDL_SetRenderDrawColor(globalRenderer, retroDark.r, retroDark.g, retroDark.b, 255);
    SDL_RenderFillRect(globalRenderer, &box);
    SDL_SetRenderDrawColor(globalRenderer, retroCyan.r, retroCyan.g, retroCyan.b, 255);
    SDL_RenderDrawRect(globalRenderer, &box);

    SDL_Rect innerBox = { 342, 242, 596, 236 };
    SDL_RenderDrawRect(globalRenderer, &innerBox);

    renderTextCentered(statusText, 640, 280, retroGreen, font24);

    SDL_Rect barBg = { 380, 340, 520, 40 };
    SDL_SetRenderDrawColor(globalRenderer, 30, 45, 55, 255);
    SDL_RenderFillRect(globalRenderer, &barBg);
    SDL_SetRenderDrawColor(globalRenderer, retroCyan.r, retroCyan.g, retroCyan.b, 255);
    SDL_RenderDrawRect(globalRenderer, &barBg);

    int fillWidth = (int)((520.0f * percent) / 100.0f);
    SDL_Rect barFill = { 380, 340, fillWidth, 40 };
    SDL_SetRenderDrawColor(globalRenderer, retroGreen.r, retroGreen.g, retroGreen.b, 255);
    SDL_RenderFillRect(globalRenderer, &barFill);

    char percentBuf[16];
    snprintf(percentBuf, sizeof(percentBuf), "%.1f%%", percent);
    renderTextCentered(percentBuf, 640, 346, {0, 0, 0, 255}, font18);

    SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(globalRenderer, 0, 0, 0, 60);
    for (int i = 0; i < 720; i += 3) {
        SDL_RenderDrawLine(globalRenderer, 0, i, 1280, i);
    }
    SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_NONE);

    SDL_RenderPresent(globalRenderer);
}

// Smooth Progress LERP Callback
int downloadProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    if (dltotal > 0) {
        targetProgressPercent = ((float)dlnow / (float)dltotal) * 100.0f;
    }

    static Uint64 lastTicks = 0;
    Uint64 currentTicks = SDL_GetTicks64();

    if (currentTicks - lastTicks >= 16) {
        displayedProgressPercent += (targetProgressPercent - displayedProgressPercent) * 0.20f;

        if (std::abs(targetProgressPercent - displayedProgressPercent) < 0.5f) {
            displayedProgressPercent = targetProgressPercent;
        }

        renderProgressScreen(currentStatusText, displayedProgressPercent);
        lastTicks = currentTicks;
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
    uLong extractedFiles = 0;
    char filename[512];
    unz_file_info fileInfo;

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
                        char buffer[8192];
                        int readBytes = 0;
                        while ((readBytes = unzReadCurrentFile(zip, buffer, sizeof(buffer))) > 0) {
                            fwrite(buffer, 1, readBytes, outFile);
                        }
                        fclose(outFile);
                    }
                    unzCloseCurrentFile(zip);
                }
            }
        }
        extractedFiles++;

        static Uint64 lastExtTicks = 0;
        Uint64 curExtTicks = SDL_GetTicks64();
        if (curExtTicks - lastExtTicks >= 16) {
            targetProgressPercent = ((float)extractedFiles / (float)totalFiles) * 100.0f;
            displayedProgressPercent += (targetProgressPercent - displayedProgressPercent) * 0.20f;
            renderProgressScreen("Extracting archive...", displayedProgressPercent);
            lastExtTicks = curExtTicks;
        }

        if (i < totalFiles - 1) unzGoToNextFile(zip);
    }

    unzClose(zip);
    renderProgressScreen("Extracting archive...", 100.0f);
    SDL_Delay(150);

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
        targetProgressPercent = 0.0f;
        displayedProgressPercent = 0.0f;

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

                CURLcode res = curl_easy_perform(curl);
                fclose(fp);

                if (res == CURLE_OK) {
                    renderProgressScreen(currentStatusText, 100.0f);
                    SDL_Delay(150);
                }

                std::string lowerName = item.name;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                bool isZip = (lowerName.length() >= 4 && lowerName.substr(lowerName.length() - 4) == ".zip");
                bool isArcade = (currentConsoleName == "Arcade");

                if (res == CURLE_OK && isZip && !isArcade) {
                    currentStatusText = "Extracting: " + item.name;
                    targetProgressPercent = 0.0f;
                    displayedProgressPercent = 0.0f;
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

    Mix_Init(MIX_INIT_OGG);
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);

    sfxNav = Mix_LoadWAV("romfs:/nav.wav");
    sfxClick = Mix_LoadWAV("romfs:/click.wav");
    sfxComp = Mix_LoadWAV("romfs:/comp.wav");

    SDL_Window* window = SDL_CreateWindow("ROM Downloader", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, 0);
    globalRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    font24 = TTF_OpenFont("romfs:/font.ttf", 36);
    font18 = TTF_OpenFont("romfs:/font.ttf", 26);
    fontLabel = TTF_OpenFont("romfs:/font.ttf", 30);

    bgTexture = IMG_LoadTexture(globalRenderer, "sdmc:/switch/ROM_Downloader/bg.png");
    if (!bgTexture) {
        bgTexture = IMG_LoadTexture(globalRenderer, "sdmc:/switch/ROM_Downloader/bg.jpg");
    }
    if (bgTexture) {
        SDL_SetTextureAlphaMod(bgTexture, 20);
    }

    SDL_Color retroWhite = {220, 255, 220, 255};
    SDL_Color retroText  = {220, 255, 220, 255};
    SDL_Color retroGreen = {50, 255, 100, 255};
    SDL_Color retroPink = {255, 50, 150, 255};
    SDL_Color retroCyan = {50, 200, 255, 255};
    SDL_Color retroYellow = {240, 220, 50, 255};
    SDL_Color retroBg = {10, 15, 20, 255};
    SDL_Color retroHeader = {20, 30, 40, 255};

    loadHistory();
    loadSettings();
    
    // Attempt to load from encrypted local cache (false = do not force download)
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
            
            // Force fetch latest manifest if Y is pressed
            if (kDown & HidNpadButton_Y) {
                fetchManifest(true);
            }

            if (!consoleList.empty()) {
                if (kDown & (HidNpadButton_Up | HidNpadButton_StickLUp)) selectedConsoleIdx = std::max(0, selectedConsoleIdx - 4);
                if (kDown & (HidNpadButton_Down | HidNpadButton_StickLDown)) selectedConsoleIdx = std::min((int)consoleList.size() - 1, selectedConsoleIdx + 4);
                if (kDown & (HidNpadButton_Left | HidNpadButton_StickLLeft)) selectedConsoleIdx = std::max(0, selectedConsoleIdx - 1);
                if (kDown & (HidNpadButton_Right | HidNpadButton_StickLRight)) selectedConsoleIdx = std::min((int)consoleList.size() - 1, selectedConsoleIdx + 1);

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
        } else if (currentState == STATE_ROMS) {
            if (kDown & HidNpadButton_B) currentState = STATE_MAIN;

            if (!romList.empty()) {
                if (kDown & (HidNpadButton_Up | HidNpadButton_StickLUp)) selectedRomIdx = std::max(0, selectedRomIdx - 1);
                if (kDown & (HidNpadButton_Down | HidNpadButton_StickLDown)) selectedRomIdx = std::min((int)romList.size() - 1, selectedRomIdx + 1);

                if (kDown & (HidNpadButton_Left | HidNpadButton_StickLLeft)) selectedRomIdx = std::max(0, selectedRomIdx - 10);
                if (kDown & (HidNpadButton_Right | HidNpadButton_StickLRight)) selectedRomIdx = std::min((int)romList.size() - 1, selectedRomIdx + 10);

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
                    swkbdConfigSetGuideText(&swkbd, "Search console ROMs (empty to reset)");
                    
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

                if (kDown & (HidNpadButton_Left | HidNpadButton_StickLLeft)) selectedDirIdx = std::max(0, selectedDirIdx - 10);
                if (kDown & (HidNpadButton_Right | HidNpadButton_StickLRight)) selectedDirIdx = std::min((int)dirList.size() - 1, selectedDirIdx + 10);

                if (kDown & HidNpadButton_A) {
                    std::string chosen = dirList[selectedDirIdx];
                    if (chosen == "..") {
                        size_t lastSlash = currentSdPath.find_last_of('/');
                        if (lastSlash != std::string::npos && lastSlash > 5) {
                            currentSdPath = currentSdPath.substr(0, lastSlash);
                            if (currentSdPath == "sdmc:") currentSdPath = "sdmc:/";
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

                if (kDown & (HidNpadButton_Left | HidNpadButton_StickLLeft)) selectedHistoryIdx = std::max(0, selectedHistoryIdx - 10);
                if (kDown & (HidNpadButton_Right | HidNpadButton_StickLRight)) selectedHistoryIdx = std::min((int)pathHistory.size() - 1, selectedHistoryIdx + 10);

                if (kDown & HidNpadButton_A) {
                    currentSdPath = pathHistory[selectedHistoryIdx];
                    loadDirectoryList(currentSdPath);
                    currentState = STATE_PATH_PICKER;
                }
            }
        }

        // Render Frame
        SDL_SetRenderDrawColor(globalRenderer, retroBg.r, retroBg.g, retroBg.b, 255);
        SDL_RenderClear(globalRenderer);

        if (bgTexture) {
            SDL_RenderCopy(globalRenderer, bgTexture, NULL, NULL);
        }

        SDL_Rect headerRect = {0, 0, 1280, 65};
        SDL_SetRenderDrawColor(globalRenderer, retroHeader.r, retroHeader.g, retroHeader.b, 255);
        SDL_RenderFillRect(globalRenderer, &headerRect);

        SDL_SetRenderDrawColor(globalRenderer, retroCyan.r, retroCyan.g, retroCyan.b, 255);
        SDL_RenderDrawLine(globalRenderer, 0, 65, 1280, 65);
        SDL_RenderDrawLine(globalRenderer, 0, 66, 1280, 66);

        renderText("ROM Downloader", 40, 14, retroWhite, font24);
        renderText("SFX: " + std::string(sfxEnabled ? "[ON]" : "[OFF]"), 1130, 20, sfxEnabled ? retroGreen : retroYellow, font18);

        if (currentState == STATE_MAIN) {
            // Updated UI instructions to reflect the Y button refresh command
            renderText("[A] Open   [Y] Update List   [-] Toggle SFX   [L/R] Page   [+] Exit", 40, 675, retroCyan, font18);

            int cols = 4;
            int rows = 3;
            int maxVisible = cols * rows;

            int currentPage = selectedConsoleIdx / maxVisible;
            consoleScrollOffset = currentPage * maxVisible;

            int startY = 85;
            int startX = 40;
            int cellW = 1200 / cols;
            int cellH = 190;

            for (size_t i = consoleScrollOffset; i < consoleList.size() && i < (size_t)(consoleScrollOffset + maxVisible); ++i) {
                int r = (i - consoleScrollOffset) / cols;
                int c = (i - consoleScrollOffset) % cols;

                int xPos = startX + c * cellW;
                int yPos = startY + r * cellH;

                SDL_Rect cell = {xPos + 5, yPos + 5, cellW - 10, cellH - 10};

                if ((int)i == selectedConsoleIdx) {
                    SDL_SetRenderDrawColor(globalRenderer, retroPink.r, retroPink.g, retroPink.b, 255);
                    SDL_RenderDrawRect(globalRenderer, &cell);

                    SDL_Rect inner = {cell.x + 1, cell.y + 1, cell.w - 2, cell.h - 2};
                    SDL_RenderDrawRect(globalRenderer, &inner);

                    SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(globalRenderer, retroPink.r, retroPink.g, retroPink.b, 40);
                    SDL_RenderFillRect(globalRenderer, &cell);
                    SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_NONE);
                }

                int iconSize = 140;
                int imgX = xPos + (cellW / 2) - (iconSize / 2);
                int imgY = yPos + 15;
                SDL_Rect imgRect = {imgX, imgY, iconSize, iconSize};

                SDL_Texture* cover = loadCoverTexture(consoleList[i].name);
                if (cover) {
                    SDL_RenderCopy(globalRenderer, cover, NULL, &imgRect);
                } else {
                    SDL_SetRenderDrawColor(globalRenderer, retroHeader.r, retroHeader.g, retroHeader.b, 255);
                    SDL_RenderFillRect(globalRenderer, &imgRect);
                }

                renderTextCentered(consoleList[i].name, xPos + (cellW / 2), imgY + iconSize - 1, retroWhite, fontLabel);
            }
        } else if (currentState == STATE_ROMS) {
            renderText("Console: " + currentConsoleName, 400, 16, retroYellow, font24);
            renderText("[A] Confirm / DL   [X] Select   [+] Search   [Left/Right] Page   [B] Back", 40, 675, retroCyan, font18);

            int maxVisible = 10;
            if (selectedRomIdx < romScrollOffset) romScrollOffset = selectedRomIdx;
            if (selectedRomIdx >= romScrollOffset + maxVisible) romScrollOffset = selectedRomIdx - maxVisible + 1;

            int yPos = 85;
            for (size_t i = romScrollOffset; i < romList.size() && i < (size_t)(romScrollOffset + maxVisible); ++i) {
                SDL_Rect row = {40, yPos, 1200, 50};
                if (romList[i].isSelected) {
                    SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(globalRenderer, retroGreen.r, retroGreen.g, retroGreen.b, 40);
                    SDL_RenderFillRect(globalRenderer, &row);
                    SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_NONE);
                }
                if ((int)i == selectedRomIdx) {
                    SDL_SetRenderDrawColor(globalRenderer, retroPink.r, retroPink.g, retroPink.b, 255);
                    SDL_RenderDrawRect(globalRenderer, &row);
                    SDL_Rect inner = {row.x + 1, row.y + 1, row.w - 2, row.h - 2};
                    SDL_RenderDrawRect(globalRenderer, &inner);

                    SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(globalRenderer, retroPink.r, retroPink.g, retroPink.b, 30);
                    SDL_RenderFillRect(globalRenderer, &row);
                    SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_NONE);
                }

                std::string prefix = romList[i].isSelected ? "[*] " : "[-] ";
                renderText(prefix + romList[i].name, 60, yPos + 8, romList[i].isSelected ? retroYellow : retroText, fontLabel);
                yPos += 55;
            }
        } else if (currentState == STATE_PATH_PICKER) {
            renderText("SD Path: " + currentSdPath, 40, 75, retroYellow, font18);
            renderText("[A] Enter   [X] Confirm   [Y] History   [Left/Right] Page   [B] Back", 40, 675, retroCyan, font18);

            int maxVisible = 10;
            if (selectedDirIdx < dirScrollOffset) dirScrollOffset = selectedDirIdx;
            if (selectedDirIdx >= dirScrollOffset + maxVisible) dirScrollOffset = selectedDirIdx - maxVisible + 1;

            int yPos = 120;
            for (size_t i = dirScrollOffset; i < dirList.size() && i < (size_t)(dirScrollOffset + maxVisible); ++i) {
                SDL_Rect row = {40, yPos, 1200, 45};
                if ((int)i == selectedDirIdx) {
                    SDL_SetRenderDrawColor(globalRenderer, retroPink.r, retroPink.g, retroPink.b, 255);
                    SDL_RenderDrawRect(globalRenderer, &row);
                    SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(globalRenderer, retroPink.r, retroPink.g, retroPink.b, 30);
                    SDL_RenderFillRect(globalRenderer, &row);
                    SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_NONE);
                }
                renderText(dirList[i] == ".." ? "<- [Up Directory]" : "[DIR] " + dirList[i], 60, yPos + 10, retroWhite, font18);
                yPos += 50;
            }
        } else if (currentState == STATE_PATH_HISTORY) {
            renderText("Select Path from History", 40, 75, retroYellow, font24);
            renderText("[A] Select   [Left/Right] Page   [B] Cancel", 40, 675, retroCyan, font18);

            int yPos = 120;
            for (size_t i = 0; i < pathHistory.size() && i < 10; ++i) {
                SDL_Rect row = {40, yPos, 1200, 45};
                if ((int)i == selectedHistoryIdx) {
                    SDL_SetRenderDrawColor(globalRenderer, retroPink.r, retroPink.g, retroPink.b, 255);
                    SDL_RenderDrawRect(globalRenderer, &row);
                    SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(globalRenderer, retroPink.r, retroPink.g, retroPink.b, 30);
                    SDL_RenderFillRect(globalRenderer, &row);
                    SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_NONE);
                }
                renderText(pathHistory[i], 60, yPos + 10, retroWhite, font18);
                yPos += 50;
            }
        }

        SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(globalRenderer, 0, 0, 0, 70);
        for (int i = 0; i < 720; i += 3) {
            SDL_RenderDrawLine(globalRenderer, 0, i, 1280, i);
        }
        SDL_SetRenderDrawBlendMode(globalRenderer, SDL_BLENDMODE_NONE);

        SDL_RenderPresent(globalRenderer);

        if (initialLaunchSfxPending) {
            playSfx(sfxComp);
            initialLaunchSfxPending = false;
        }
    }

    for (auto& pair : coverCache) {
        if (pair.second) SDL_DestroyTexture(pair.second);
    }
    if (bgTexture) SDL_DestroyTexture(bgTexture);

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