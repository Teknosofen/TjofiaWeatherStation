#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <functional>

// LittleFS-backed store of 240x240 RGB565 images (.raw).
//
// Owns every filesystem detail the rest of the firmware used to open-code:
// the ".raw" extension filter, the leading-slash normalisation, the exact
// byte count a valid image must have, and the HTTP upload sink.
class ImageStore {
public:
    // 240 x 240 pixels x 2 bytes (RGB565). A file of any other size is rejected.
    static constexpr size_t IMAGE_BYTES = 240u * 240u * 2u;   // 115200

    // Mount LittleFS (formatting on first boot). Safe to call once in setup().
    bool begin();
    bool mounted() const { return _mounted; }

    // Path of the nth stored image (0-based) as "/name.raw", or "" if absent.
    String nth(int n) const;
    String first() const { return nth(0); }
    int    count() const;

    // cb(displayName, sizeBytes) for each image, in directory order.
    // displayName has no leading slash — it is what the web UI shows and posts back.
    void forEach(const std::function<void(const String &, size_t)> &cb) const;

    bool   remove(const String &name);
    File   open(const String &name) const;

    size_t totalBytes() const;
    size_t freeBytes() const;

    // Leading-slash normalisation. Names travel through HTML forms without the
    // slash and through LittleFS with it; these two are the only conversions.
    static String displayName(const String &name);   // "/a.raw" -> "a.raw"
    static String path(const String &name);          // "a.raw"  -> "/a.raw"
    static bool   isImage(const String &name) { return name.endsWith(".raw"); }

    // ── HTTP upload sink ─────────────────────────────────────────────────────
    // Drives the three-phase WebServer upload callback. The file is written
    // straight to flash and deleted again if the final size is not IMAGE_BYTES.
    void uploadBegin(const String &filename);
    void uploadWrite(const uint8_t *buf, size_t len);
    bool uploadEnd(size_t totalSize);                // true = accepted and kept
    bool uploadAccepted() const { return _uploadOk; }
    const String &uploadPath() const { return _uploadPath; }

private:
    bool   _mounted = false;
    File   _uploadFile;
    bool   _uploadOk = false;
    String _uploadPath;
};
