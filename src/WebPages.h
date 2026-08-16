#pragma once
#include <WebServer.h>

// Route registration, one function per page group. Each is implemented in the
// matching WebPage*.cpp and is called only from WebUI::begin().
namespace WebPages {

void registerHome    (WebServer &s);   // /            /save   /reset  /cal
void registerStatus  (WebServer &s);   // /wx
void registerLocation(WebServer &s);   // /location*
void registerCalib   (WebServer &s);   // /calib*
void registerImages  (WebServer &s);   // /images*

}  // namespace WebPages
