#include "WebUI.h"
#include "WebPages.h"
#include "AppContext.h"
#include "config.h"

// /images — gallery, in-browser photo converter/uploader, boot-image pick and
// slideshow interval. Conversion to 240x240 RGB565 happens in the browser so
// the device only ever receives a fixed-size raw blob.

namespace {

const char CSS[] PROGMEM =
    "body{font-family:sans-serif;max-width:560px;margin:20px auto;padding:0 12px}"
    "h1{font-size:1.2em}h2{font-size:1em;margin:0 0 6px}"
    ".card{display:flex;align-items:center;gap:10px;background:#f9f9f9;"
          "border:1px solid #ddd;border-radius:6px;padding:10px;margin:8px 0}"
    "canvas{width:80px;height:80px;flex-shrink:0;border-radius:50%;border:2px solid #ccc}"
    ".info{flex:1;min-width:0}.fn{font-weight:bold;word-break:break-all;font-size:.9em}"
    ".fs{font-size:.75em;color:#666;margin:2px 0 4px}"
    ".btn{display:inline-block;padding:5px 10px;background:#1fa3ec;color:#fff;"
         "border:none;border-radius:4px;cursor:pointer;text-decoration:none;"
         "margin:2px 2px 2px 0;font-size:.85em}"
    ".red{background:#c00}.grn{background:#2a2}"
    ".box{background:#f9f9f9;border:1px solid #ddd;border-radius:6px;padding:14px;margin:14px 0}"
    "pre{background:#eee;padding:8px;border-radius:4px;font-size:.72em;"
        "overflow-x:auto;white-space:pre;margin:4px 0}"
    "input[type=file]{display:block;margin:6px 0}"
    "small{color:#666}";

// Renders each stored .raw into its <canvas> by fetching the bytes and
// expanding RGB565 to RGBA in the browser.
const char GALLERY_JS[] PROGMEM =
    "function L(id,nm){"
    "fetch('/images/file?name='+encodeURIComponent(nm))"
    ".then(r=>r.arrayBuffer()).then(b=>{"
    "var d=new Uint8Array(b);"
    "var c=document.getElementById(id),ctx=c.getContext('2d');"
    "var img=ctx.createImageData(240,240);"
    "for(var i=0;i<57600;i++){"
    "var px=(d[i*2]<<8)|d[i*2+1];"
    "img.data[i*4]=(px>>11)<<3;"
    "img.data[i*4+1]=((px>>5)&63)<<2;"
    "img.data[i*4+2]=(px&31)<<3;"
    "img.data[i*4+3]=255;}"
    "ctx.putImageData(img,0,0);});}";

// Centre-crops the picked photo to a square, scales to 240x240, packs RGB565
// and POSTs the result as a multipart file.
const char UPLOAD_JS[] PROGMEM =
    "var _f=null;"
    "document.getElementById('pick').addEventListener('change',function(e){"
    "var f=e.target.files[0];if(!f)return;_f=f;"
    "var url=URL.createObjectURL(f),im=new Image();"
    "im.onload=function(){"
    "var c=document.getElementById('prev'),ctx=c.getContext('2d');"
    "var sw=im.naturalWidth,sh=im.naturalHeight,side=Math.min(sw,sh);"
    "ctx.drawImage(im,(sw-side)/2,(sh-side)/2,side,side,0,0,240,240);"
    "URL.revokeObjectURL(url);"
    "c.style.display='block';"
    "document.getElementById('upbtn').style.display='inline-block';"
    "document.getElementById('pname').textContent=f.name;"
    "};im.src=url;"
    "});"
    "document.getElementById('upbtn').addEventListener('click',function(){"
    "if(!_f)return;"
    "var btn=this,st=document.getElementById('ust');"
    "btn.disabled=true;st.textContent='Converting...';"
    "setTimeout(function(){"
    "var c=document.getElementById('prev');"
    "var pix=c.getContext('2d').getImageData(0,0,240,240).data;"
    "var raw=new Uint8Array(115200);"
    "for(var i=0;i<57600;i++){"
    "var r=pix[i*4],g=pix[i*4+1],b=pix[i*4+2];"
    "var px=((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3);"
    "raw[i*2]=(px>>8)&0xFF;raw[i*2+1]=px&0xFF;}"
    "st.textContent='Uploading...';"
    "var nm=_f.name.replace(/\\.[^.]+$/,'')+'.raw';"
    "var fd=new FormData();"
    "fd.append('img',new Blob([raw],{type:'application/octet-stream'}),nm);"
    "fetch('/images/upload',{method:'POST',body:fd})"
    ".then(function(){window.location='/images';})"
    ".catch(function(){btn.disabled=false;st.textContent='Upload error!';});"
    "},50);"
    "});";

// One gallery card. Returns the JS call that fills its canvas.
String appendCard(String &p, int idx, const String &name, size_t bytes, bool isBoot) {
    char cid[8];
    snprintf(cid, sizeof(cid), "c%d", idx);

    p += F("<div class='card'><canvas id='"); p += cid;
    p += F("' width='240' height='240'></canvas><div class='info'>"
           "<div class='fn'>"); p += name;
    p += F("</div><div class='fs'>");
    if (bytes == ImageStore::IMAGE_BYTES) {
        p += F("240\xc3\x97""240 RGB565");
    } else {
        char buf[28];
        snprintf(buf, sizeof(buf), "%u B (invalid!)", (unsigned)bytes);
        p += buf;
    }
    p += F("</div>");

    if (isBoot) {
        p += F("<span class='btn grn'>&#10003; Boot image</span> ");
    } else {
        p += F("<form method='POST' action='/images/setboot' style='display:inline'>"
               "<input type='hidden' name='file' value='"); p += name;
        p += F("'><button class='btn' type='submit'>Set as boot</button></form> ");
    }
    p += F("<form method='POST' action='/images/delete' style='display:inline'"
           " onsubmit=\"return confirm('Delete ");
    p += name;
    p += F("?')\">"
           "<input type='hidden' name='file' value='"); p += name;
    p += F("'><button class='btn red' type='submit'>Delete</button></form>"
           "</div></div>");

    return String("L('") + cid + "','" + name + "');";
}

String buildPage() {
    const String boot     = app.settings.loadBootImage();
    const String bootDisp = ImageStore::displayName(boot);

    String p;
    p.reserve(6144);
    WebUI::pageHead(p, F("Tjofia WX \xe2\x80\x94 Images"), FPSTR(CSS));
    p += F("<h1>&#128444; Images</h1>");

    // ── Gallery ──────────────────────────────────────────────────────────────
    String jsLoads; jsLoads.reserve(256);
    int    idx = 0;
    app.images.forEach([&](const String &name, size_t bytes) {
        jsLoads += appendCard(p, idx++, name, bytes, name == bootDisp);
    });
    const bool any = (idx > 0);
    if (!any) p += F("<p><em>No images yet &mdash; upload one below.</em></p>");

    if (WebUI::server.hasArg("err"))
        p += F("<p style='color:#c00'><b>Upload failed</b> &mdash; please try again.</p>");

    {
        size_t tot   = app.images.totalBytes();
        size_t fr    = app.images.freeBytes();
        int    slots = (int)(fr / ImageStore::IMAGE_BYTES);
        char   sbuf[96];
        snprintf(sbuf, sizeof(sbuf),
                 "%u kB free of %u kB &mdash; room for %d more image%s",
                 (unsigned)(fr / 1024), (unsigned)(tot / 1024),
                 slots, slots == 1 ? "" : "s");
        p += F("<p><small>"); p += sbuf; p += F("</small></p>");
    }

    // ── Upload ───────────────────────────────────────────────────────────────
    p += F("<div class='box'><h2>Add photo</h2>"
           "<p><small>Pick any photo &mdash; the browser converts it to 240\xc3\x97""240"
           " RGB565 and uploads it to the device. No external tool needed.</small></p>"
           "<input type='file' id='pick' accept='image/*'>"
           "<div id='pname' style='font-size:.8em;color:#666;margin:2px 0'></div>"
           "<canvas id='prev' width='240' height='240'"
           " style='width:80px;height:80px;border-radius:50%;border:2px solid #ccc;"
           "display:none;margin:6px 0'></canvas><br>"
           "<button id='upbtn' class='btn' style='display:none'>Convert &amp; Upload</button>"
           " <span id='ust' style='font-size:.85em;color:#555'></span>"
           "</div>");

    // ── Boot image ───────────────────────────────────────────────────────────
    p += F("<div class='box'><h2>Boot image</h2><p>");
    if (boot.isEmpty()) {
        p += F("None &mdash; weather display shows Teknosofen splash on startup.");
    } else {
        p += F("Currently: <b>"); p += bootDisp;
        p += F("</b></p>"
               "<form method='POST' action='/images/clearboot'>"
               "<button class='btn red' type='submit'>Clear (use splash)</button></form>");
    }
    p += F("</p></div>");

    p += F("<script>");
    if (any) { p += FPSTR(GALLERY_JS); p += jsLoads; }
    p += FPSTR(UPLOAD_JS);
    p += F("</script>");

    // ── Slideshow ────────────────────────────────────────────────────────────
    p += F("<div class='box'><h2>&#9654; Slideshow</h2>"
           "<p><small>The weather display alternates between live weather and stored photos: "
           "weather &rarr; photo 1 &rarr; weather &rarr; photo 2 &rarr; &hellip; "
           "Each slide stays visible for the configured interval. "
           "Requires at least one stored image.</small></p>"
           "<form method='POST' action='/images/slide-save'>"
           "<label>Interval (seconds)&nbsp;"
           "<input type='number' name='sec' min='1' max='300' step='1' value='");
    p += app.slideshow.intervalSec();
    p += F("' style='width:72px'></label>&nbsp;"
           "<button class='btn' type='submit'>Save</button>"
           "</form></div>");

    p += F("<p><a class='btn' href='/'>&#8592; Back</a></p></body></html>");
    return p;
}

void handlePage() {
    WebUI::server.send(200, "text/html", buildPage());
}

void handleSlideSave() {
    if (WebUI::server.hasArg("sec")) {
        int sec = WebUI::server.arg("sec").toInt();
        if (sec >= Slideshow::MIN_SEC && sec <= Slideshow::MAX_SEC) {
            app.slideshow.setIntervalSec(sec);
            app.slideshow.restartTimer(millis());
            app.settings.saveSlideSec(sec);
            Serial.printf("Slideshow interval: %d s\n", sec);
        }
    }
    WebUI::redirect(F("/images"));
}

void handleUpload() {
    HTTPUpload &up = WebUI::server.upload();
    if      (up.status == UPLOAD_FILE_START) app.images.uploadBegin(up.filename);
    else if (up.status == UPLOAD_FILE_WRITE) app.images.uploadWrite(up.buf, up.currentSize);
    else if (up.status == UPLOAD_FILE_END)   app.images.uploadEnd(up.totalSize);
}

void handleUploadDone() {
    WebUI::redirect(app.images.uploadAccepted() ? F("/images") : F("/images?err=1"));
}

void handleDelete() {
    if (WebUI::server.hasArg("file")) {
        const String name = WebUI::server.arg("file");
        app.images.remove(name);
        // Drop the boot-image pin if it pointed at the file just deleted.
        if (ImageStore::displayName(app.settings.loadBootImage()) == name) {
            app.settings.clearBootImage();
            app.bootImg = "";
        }
        Serial.printf("Image deleted: %s\n", ImageStore::path(name).c_str());
    }
    WebUI::redirect(F("/images"));
}

void handleSetBoot() {
    if (WebUI::server.hasArg("file")) {
        app.bootImg = ImageStore::path(WebUI::server.arg("file"));
        app.settings.saveBootImage(app.bootImg);
        Serial.printf("Boot image set: %s\n", app.bootImg.c_str());
    }
    WebUI::redirect(F("/images"));
}

void handleClearBoot() {
    app.bootImg = "";
    app.settings.clearBootImage();
    Serial.println("Boot image cleared");
    WebUI::redirect(F("/images"));
}

void handleFile() {
    if (!WebUI::server.hasArg("name")) { WebUI::server.send(400); return; }
    File f = app.images.open(WebUI::server.arg("name"));
    if (!f) { WebUI::server.send(404); return; }
    WebUI::server.streamFile(f, "application/octet-stream");
    f.close();
}

}  // namespace

void WebPages::registerImages(WebServer &s) {
    s.on("/images",            HTTP_GET,  handlePage);
    s.on("/images/upload",     HTTP_POST, handleUploadDone, handleUpload);
    s.on("/images/delete",     HTTP_POST, handleDelete);
    s.on("/images/setboot",    HTTP_POST, handleSetBoot);
    s.on("/images/clearboot",  HTTP_POST, handleClearBoot);
    s.on("/images/file",       HTTP_GET,  handleFile);
    s.on("/images/slide-save", HTTP_POST, handleSlideSave);
}
