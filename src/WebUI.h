#pragma once
#include <Arduino.h>
#include <WebServer.h>

// The device's HTTP interface.
//
// WebUI owns the WebServer and the shared page chrome; the individual pages
// live in WebPage*.cpp and register their own routes through WebPages.h.
namespace WebUI {

extern WebServer server;

// Register every route and start listening. Call once, after WiFi is up.
void begin();

// Call from loop().
void loopOnce();

bool up();

// ── Shared page chrome ───────────────────────────────────────────────────────

// Emits doctype, head (charset, viewport, title, optional extra head markup,
// the page's own CSS) and the opening <body>.
void pageHead(String &p, const __FlashStringHelper *title,
              const __FlashStringHelper *css,
              const __FlashStringHelper *extraHead = nullptr);

// 303 See Other — the standard reply to every POST handler here.
void redirect(const __FlashStringHelper *location);

}  // namespace WebUI
