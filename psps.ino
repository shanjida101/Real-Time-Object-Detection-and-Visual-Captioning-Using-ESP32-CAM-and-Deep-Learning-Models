/**** ESP32-CAM (AI Thinker) minimal web streamer
 *  Endpoints:
 *    /          -> simple HTML with links
 *    /stream    -> MJPEG stream
 *    /capture   -> single JPEG
 ****/

#include <WiFi.h>
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "esp_http_server.h"

// ---------- Wi-Fi (edit if needed) ----------
const char* ssid     = "Mitsuha";
const char* password = "Shanjida11";

// ---------- AI Thinker pin map ----------
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ---------- Simple index page ----------
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>ESP32-CAM</title>
<style>body{font-family:Arial;margin:16px}a{display:inline-block;margin:8px 0}</style>
</head><body>
<h2>ESP32-CAM</h2>
<p><a href="/stream">Start Stream</a></p>
<p><a href="/capture" target="_blank">Capture (JPEG)</a></p>
</body></html>
)HTML";

// ---------- Handlers ----------
static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) return ESP_FAIL;

  if (fb->format != PIXFORMAT_JPEG) {
    // should already be JPEG; convert if not
    uint8_t *jpg_buf = nullptr;
    size_t jpg_len = 0;
    bool ok = frame2jpg(fb, 90, &jpg_buf, &jpg_len);
    esp_camera_fb_return(fb);
    if (!ok) return ESP_FAIL;
    httpd_resp_set_type(req, "image/jpeg");
    esp_err_t res = httpd_resp_send(req, (const char *)jpg_buf, jpg_len);
    free(jpg_buf);
    return res;
  } else {
    httpd_resp_set_type(req, "image/jpeg");
    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    return res;
  }
}

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
static const char* _STREAM_BOUNDARY     = "--frame";
static const char* _STREAM_PART         = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t stream_handler(httpd_req_t *req) {
  esp_err_t res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  char part_buf[64];

  while (true) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) { res = ESP_FAIL; break; }

    // If not JPEG, convert
    uint8_t *jpg_buf = fb->buf;
    size_t   jpg_len = fb->len;
    if (fb->format != PIXFORMAT_JPEG) {
      uint8_t * _jpg_buf = nullptr; size_t _jpg_len = 0;
      if (!frame2jpg(fb, 85, &_jpg_buf, &_jpg_len)) {
        esp_camera_fb_return(fb); res = ESP_FAIL; break;
      }
      esp_camera_fb_return(fb);
      jpg_buf = _jpg_buf; jpg_len = _jpg_len;
    } else {
      // reuse buffer then return
    }

    // boundary
    if (httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY)) != ESP_OK ||
        httpd_resp_send_chunk(req, "\r\n", 2) != ESP_OK) {
      if (fb->format == PIXFORMAT_JPEG) esp_camera_fb_return(fb);
      else free(jpg_buf);
      res = ESP_FAIL; break;
    }

    // headers for this frame
    size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, (unsigned)jpg_len);
    if (httpd_resp_send_chunk(req, part_buf, hlen) != ESP_OK) {
      if (fb->format == PIXFORMAT_JPEG) esp_camera_fb_return(fb);
      else free(jpg_buf);
      res = ESP_FAIL; break;
    }

    // payload
    if (httpd_resp_send_chunk(req, (const char*)jpg_buf, jpg_len) != ESP_OK ||
        httpd_resp_send_chunk(req, "\r\n", 2) != ESP_OK) {
      if (fb->format == PIXFORMAT_JPEG) esp_camera_fb_return(fb);
      else free(jpg_buf);
      res = ESP_FAIL; break;
    }

    // return/release buffer
    if (fb->format == PIXFORMAT_JPEG) esp_camera_fb_return(fb);
    else free(jpg_buf);
    // allow yielding
    vTaskDelay(1);
  }
  return res;
}

// ---------- Server setup ----------
httpd_handle_t start_webserver() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.max_uri_handlers = 8;
  httpd_handle_t server = NULL;

  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_uri_t index_uri   = { .uri="/",       .method=HTTP_GET, .handler=index_handler,   .user_ctx=NULL };
    httpd_uri_t capture_uri = { .uri="/capture",.method=HTTP_GET, .handler=capture_handler, .user_ctx=NULL };
    httpd_uri_t stream_uri  = { .uri="/stream", .method=HTTP_GET, .handler=stream_handler,  .user_ctx=NULL };
    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &capture_uri);
    httpd_register_uri_handler(server, &stream_uri);
  }
  return server;
}

// ---------- Camera init ----------
bool init_camera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA;  // good start: 640x480
    config.jpeg_quality = 12;           // lower = better quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA; // 320x240 if no PSRAM
    config.jpeg_quality = 14;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with 0x%x\n", err);
    return false;
  }
  sensor_t * s = esp_camera_sensor_get();
  // Flip if your image is upside down:
  // s->set_vflip(s, 1);
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  Serial.println("\nBooting...");

  if (!init_camera()) {
    Serial.println("Camera init failed. Rebooting in 5s...");
    delay(5000);
    ESP.restart();
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Connecting to %s", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());

  start_webserver();
  Serial.println("HTTP server started.");
  Serial.println("Open:");
  Serial.println("  /        -> index");
  Serial.println("  /stream  -> MJPEG stream");
  Serial.println("  /capture -> single JPEG");
}

void loop() {
  // nothing; HTTP server runs in background
}
