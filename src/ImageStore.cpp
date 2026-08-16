#include "ImageStore.h"

bool ImageStore::begin() {
    _mounted = LittleFS.begin(true);   // format on first boot
    return _mounted;
}

String ImageStore::displayName(const String &name) {
    return name.startsWith("/") ? name.substring(1) : name;
}

String ImageStore::path(const String &name) {
    return name.startsWith("/") ? name : ("/" + name);
}

void ImageStore::forEach(const std::function<void(const String &, size_t)> &cb) const {
    File root = LittleFS.open("/");
    if (!root) return;
    File entry = root.openNextFile();
    while (entry) {
        String nm = String(entry.name());
        size_t sz = entry.size();
        entry.close();
        entry = root.openNextFile();
        if (isImage(nm)) cb(displayName(nm), sz);
    }
    root.close();
}

String ImageStore::nth(int n) const {
    String found;
    int    i = 0;
    forEach([&](const String &nm, size_t) {
        if (found.isEmpty() && i++ == n) found = path(nm);
    });
    return found;
}

int ImageStore::count() const {
    int n = 0;
    forEach([&](const String &, size_t) { n++; });
    return n;
}

bool ImageStore::remove(const String &name) {
    return LittleFS.remove(path(name));
}

File ImageStore::open(const String &name) const {
    return LittleFS.open(path(name), "r");
}

size_t ImageStore::totalBytes() const { return LittleFS.totalBytes(); }
size_t ImageStore::freeBytes()  const { return LittleFS.totalBytes() - LittleFS.usedBytes(); }

// ── Upload sink ──────────────────────────────────────────────────────────────

void ImageStore::uploadBegin(const String &filename) {
    _uploadPath = path(filename);
    if (!isImage(_uploadPath)) _uploadPath += ".raw";
    _uploadFile = LittleFS.open(_uploadPath, "w");
    _uploadOk   = false;
}

void ImageStore::uploadWrite(const uint8_t *buf, size_t len) {
    if (_uploadFile) _uploadFile.write(buf, len);
}

bool ImageStore::uploadEnd(size_t totalSize) {
    if (!_uploadFile) return false;
    _uploadFile.close();
    if (totalSize == IMAGE_BYTES) {
        _uploadOk = true;
        Serial.printf("Image saved: %s\n", _uploadPath.c_str());
    } else {
        LittleFS.remove(_uploadPath);
        Serial.printf("Image rejected (%u B): %s\n",
                      (unsigned)totalSize, _uploadPath.c_str());
    }
    return _uploadOk;
}
