#include "wifi_server.h"

static const char *TAG = "WIFI_SERVER";

// Helper functions

// Guess a MIME type from a file extension
static const char *mime_type(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcasecmp(ext, ".mp4") == 0) return "video/mp4";
    if (strcasecmp(ext, ".txt") == 0) return "text/plain";
    return "application/octet-stream";
}

// Write a null-terminated string to an httpd response (no allocation needed)
static esp_err_t send_str(httpd_req_t *req, const char *s)
{
    return httpd_resp_send_chunk(req, s, strlen(s));
}

// URI handlers

// GET --> HTML home page listing all clip directories
static esp_err_t handle_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");

    send_str(req,
        "<!DOCTYPE html><html><head>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>NightCam</title>"
        "<style>"
        "body{font-family:monospace;background:#0d0d0d;color:#e0e0e0;margin:0;padding:1rem}"
        "h1{color:#ff6b35;margin-bottom:0.5rem}"
        "p.sub{color:#888;font-size:0.85rem;margin-top:0}"
        "ul{list-style:none;padding:0}"
        "li{padding:0.4rem 0;border-bottom:1px solid #1e1e1e}"
        "a{color:#5bc8ff;text-decoration:none}"
        "a:hover{text-decoration:underline}"
        "small{color:#555}"
        "</style></head><body>"
        "<h1>&#128249; NightCam</h1>"
        "<p class='sub'>Select a clip to browse its frames</p><ul>");

    DIR *root = opendir(MOUNT_POINT);
    if (!root) {
        send_str(req, "<li><small>SD card not mounted or empty.</small></li>");
    } else {
        struct dirent *entry;
        int count = 0;
        while ((entry = readdir(root)) != NULL) {
            // Only show clip directories (C0000 - C9999)
            if (entry->d_type != DT_DIR) continue;
            if (entry->d_name[0] != 'C')  continue;

            // Send static HTML and variable name separately
            send_str(req, "<li><a href='/clip/");
            send_str(req, entry->d_name);
            send_str(req, "'>&#128193; ");
            send_str(req, entry->d_name);
            send_str(req, "</a></li>");
            count++;
        }
        closedir(root);

        if (count == 0) {
            send_str(req, "<li><small>No clips recorded yet.</small></li>");
        }
    }

    send_str(req, "</ul></body></html>");
    httpd_resp_send_chunk(req, NULL, 0);   // end chunked response
    return ESP_OK;
}

// GET /clip/<name> --> HTML page listing frames inside a clip directory
static esp_err_t handle_clip(httpd_req_t *req)
{
    // Extract clip name from URI:  /clip/C0001  -->  "C0001"
    const char *uri = req->uri;           // "/clip/C0001"
    const char *slash = strrchr(uri, '/');
    if (!slash || strlen(slash) < 2) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad clip name");
        return ESP_FAIL;
    }
    const char *clip_name = slash + 1;

    char dir_path[64];
    snprintf(dir_path, sizeof(dir_path), MOUNT_POINT "/%s", clip_name);

    DIR *d = opendir(dir_path);
    if (!d) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Clip not found");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");

    // Send static HTML and clip_name separately
    send_str(req,
        "<!DOCTYPE html><html><head>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>");
    send_str(req, clip_name);
    send_str(req,
        "</title>"
        "<style>"
        "body{font-family:monospace;background:#0d0d0d;color:#e0e0e0;margin:0;padding:1rem}"
        "h1{color:#ff6b35}"
        "a.back{color:#888;font-size:0.85rem;display:block;margin-bottom:1rem}"
        "a.back:hover{color:#e0e0e0}"
        "ul{list-style:none;padding:0}"
        "li{padding:0.35rem 0;border-bottom:1px solid #1a1a1a}"
        "a{color:#5bc8ff;text-decoration:none}"
        "a:hover{text-decoration:underline}"
        ".dl{margin-left:1rem;color:#aaa;font-size:0.8rem}"
        "</style></head><body>"
        "<a class='back' href='/'>&#8592; All Clips</a>"
        "<h1>&#128193; ");
    send_str(req, clip_name);
    send_str(req, "</h1>");

    // Button to make video from individual images
    send_str(req, 
        "<div style='margin-bottom:1.5rem; padding-bottom:1.5rem; border-bottom:1px solid #333'>"
        "<button id='renderBtn' style='background:#ff6b35; border:none; padding:10px 15px; color:#111; font-weight:bold; cursor:pointer;'>Make Video</button>"
        "<div id='videoContainer'></div>"
        "</div>"
        "<ul>" 
    );  
    // Display list of all images within the clip
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_type == DT_DIR) continue;

        char full[sizeof(MOUNT_POINT) + 1 + NAME_MAX + 1 + NAME_MAX + 1];
        snprintf(full, sizeof(full), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        char size_str[64] = "";
        if (stat(full, &st) == 0) {
            snprintf(size_str, sizeof(size_str), " <small>(%.1f KB)</small>",
                     (float)st.st_size / 1024.0f);
        }

        send_str(req, "<li><a href='/file/");
        send_str(req, clip_name);
        send_str(req, "/");
        send_str(req, entry->d_name);
        send_str(req, "'>");
        send_str(req, entry->d_name);
        send_str(req, "</a>");
        send_str(req, size_str);
        send_str(req, "<a class='dl' href='/file/");
        send_str(req, clip_name);
        send_str(req, "/");
        send_str(req, entry->d_name);
        send_str(req, "' download>&#11015; download</a></li>");
        count++;
    }
    closedir(d);

    if (count == 0) send_str(req, "<li><small>Empty clip.</small></li>");

    // Close the list 
    send_str(req, "</ul>");

    // Javascript to encode individual images into a full 20 second video
    // Generated by Gemini
    send_str(req, 
        "<script>"
        "document.getElementById('renderBtn').addEventListener('click', async () => {"
        "  const btn = document.getElementById('renderBtn');"
        "  const container = document.getElementById('videoContainer');"
        "  btn.innerText = 'Loading images...';"
        "  btn.disabled = true;"
        "  const links = Array.from(document.querySelectorAll('a.dl')).map(a => a.href);"
        "  if (links.length === 0) {"
        "    btn.innerText = 'No images found!';"
        "    return;"
        "  }"
        "  const images = [];"
        "  for (let url of links) {"
        "    const img = new Image();"
        "    img.src = url;"
        "    await new Promise(resolve => {"
        "      img.onload = resolve;"
        "      img.onerror = resolve;"
        "    });"
        "    images.push(img);"
        "  }"
        "  btn.innerText = 'Encoding video...';"
        "  const canvas = document.createElement('canvas');"
        "  canvas.width = images[0].width;"
        "  canvas.height = images[0].height;"
        "  const ctx = canvas.getContext('2d');"
        "  const stream = canvas.captureStream(30);"
        
        "  let mime = 'video/mp4';"
        "  let ext = 'mp4';"
        "  if (!MediaRecorder.isTypeSupported(mime)) {"
        "    mime = 'video/webm';"
        "    ext = 'webm';"
        "    console.warn('MP4 recording not supported, falling back to WebM');"
        "  }"
        "  const recorder = new MediaRecorder(stream, { mimeType: mime });"
        
        "  const chunks = [];"
        "  recorder.ondataavailable = e => chunks.push(e.data);"
        "  recorder.onstop = () => {"
        "    const blob = new Blob(chunks, { type: mime });"
        "    const videoUrl = URL.createObjectURL(blob);"
        "    container.innerHTML = `<video src='${videoUrl}' controls autoplay style='max-width: 100%; border: 1px solid #333; margin-top: 1rem;'></video><br><a href='${videoUrl}' download='clip.${ext}' style='display: inline-block; margin-top: 0.5rem; background: #ff6b35; color: #000; padding: 0.5rem; border-radius: 4px; text-decoration: none;'>Download Video</a>`;"
        "    btn.innerText = 'Done!';"
        "  };"
        "  const fps = 15;"
        "  const frameDuration = 1000 / fps;"
        "  let frameIndex = 0;"
        "  recorder.start();"
        "  const drawFrame = () => {"
        "    if (frameIndex < images.length) {"
        "      ctx.drawImage(images[frameIndex], 0, 0, canvas.width, canvas.height);"
        "      frameIndex++;"
        "      setTimeout(drawFrame, frameDuration);"
        "    } else {"
        "      recorder.stop();"
        "    }"
        "  };"
        "  drawFrame();"
        "});"
        "</script>"
    );

    // Close the page and finish the HTTP response
    send_str(req, "</body></html>");
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}


// GET /file/<clip>/<filename>  →  stream the raw file to the client
static esp_err_t handle_file(httpd_req_t *req)
{
    // Extract path and strip query params/fragments
    const char *rel_ptr = req->uri + strlen("/file/");
    char rel[64];
    
    // Copy to a local buffer to sanitize it
    strlcpy(rel, rel_ptr, sizeof(rel));
    
    // Strip query strings (?) and hash fragments (#)
    char *query = strchr(rel, '?');
    if (query) *query = '\0';
    char *hash = strchr(rel, '#');
    if (hash) *hash = '\0';

    char full_path[96];
    snprintf(full_path, sizeof(full_path), MOUNT_POINT "/%s", rel);

    // Guard: reject path traversal attempts
    if (strstr(full_path, "..")) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad path");
        return ESP_FAIL;
    }

    FILE *f = fopen(full_path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "fopen failed: %s (errno %d)", full_path, errno);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    // Set content type
    httpd_resp_set_type(req, mime_type(full_path));

    // Suggest filename for download
    const char *fname = strrchr(full_path, '/');
    if (fname) {
        char disp[64];
        snprintf(disp, sizeof(disp), "inline; filename=\"%s\"", fname + 1);
        httpd_resp_set_hdr(req, "Content-Disposition", disp);
    }

    // Stream in fixed-size chunks to avoid large heap allocations
    static char chunk[FILE_CHUNK_SIZE];
    size_t bytes_read;
    esp_err_t ret = ESP_OK;

    while ((bytes_read = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, bytes_read) != ESP_OK) {
            ESP_LOGW(TAG, "Client disconnected mid-transfer: %s", full_path);
            ret = ESP_FAIL;
            break;
        }
    }

    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0); // signal end of response
    return ret;
}

// HTTP server lifecycle
static httpd_handle_t start_server(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port      = SERVER_PORT;
    cfg.max_uri_handlers = 8;
    cfg.stack_size       = 8192;
    cfg.uri_match_fn     = httpd_uri_match_wildcard; // needed for wildcard URIs

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return NULL;
    }

    // Root listing
    httpd_uri_t root_uri = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = handle_root,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &root_uri);

    // Clip directory listing. Wildcard matches /clip/C0001 etc.
    httpd_uri_t clip_uri = {
        .uri      = "/clip/*",
        .method   = HTTP_GET,
        .handler  = handle_clip,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &clip_uri);

    // Individual file download. Wildcard matches /file/C0001/00001.jpg etc.
    httpd_uri_t file_uri = {
        .uri      = "/file/*",
        .method   = HTTP_GET,
        .handler  = handle_file,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &file_uri);

    ESP_LOGI(TAG, "HTTP server started on port %d", SERVER_PORT);
    return server;
}

static void stop_server(httpd_handle_t *server)
{
    if (*server) {
        httpd_stop(*server);
        *server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}

// WiFi Access Point lifecycle
static volatile int s_connected_clients = 0;

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *e = data;
        s_connected_clients++;
        ESP_LOGI(TAG, "Station connected. MAC: %02x:%02x:%02x:%02x:%02x:%02x, Total: %d",
                 e->mac[0], e->mac[1], e->mac[2],
                 e->mac[3], e->mac[4], e->mac[5], s_connected_clients);

    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *e = data;
        if (s_connected_clients > 0) s_connected_clients--;
        ESP_LOGI(TAG, "Station disconnected. MAC: %02x:%02x:%02x:%02x:%02x:%02x, Total: %d",
                 e->mac[0], e->mac[1], e->mac[2],
                 e->mac[3], e->mac[4], e->mac[5], s_connected_clients);

        // If the last user disconnects, signal the main task to shut down
        if (s_connected_clients == 0) {
            xEventGroupSetBits(event_group, WIFI_CLIENT_DISCONNECTED);
        }
    }
}

static bool s_wifi_initialized = false;

static esp_err_t wifi_ap_start(void)
{
    // 1. One-time core network initialization
    if (!s_wifi_initialized) {
        // NVS is required by the WiFi driver
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        if (ret != ESP_OK) return ret;

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_ap();

        wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

        s_wifi_initialized = true;
        ESP_LOGI(TAG, "Core WiFi stack initialized.");
    }

    // 2. Start the AP (Safe to call multiple times over the lifecycle)
    wifi_config_t ap_cfg = {
        .ap = {
            .ssid           = AP_SSID,
            .ssid_len       = strlen(AP_SSID),
            .channel        = AP_CHANNEL,
            .password       = AP_PASSWORD,
            .max_connection = AP_MAX_CONN,
            .authmode       = (strlen(AP_PASSWORD) == 0)
                                   ? WIFI_AUTH_OPEN
                                   : WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP started. SSID: \"%s\"  Password: \"%s\"  IP: 192.168.4.1",
             AP_SSID, AP_PASSWORD);
    return ESP_OK;
}

static void wifi_ap_stop(void)
{
    // Only turn off the radio. Do not de-init the LwIP stack or event loop.
    esp_wifi_stop();
    ESP_LOGI(TAG, "AP stopped (Radio off, stack preserved)");
}

// Main task
void wifi_server_task(void *pv)
{
    httpd_handle_t server = NULL;
    bool ap_up = false;

    while (1) {
        // 1. Sleep until commanded to turn on
        ESP_LOGI(TAG, "WiFi Task sleeping. Waiting for WIFI_ACTIVE...");
        xEventGroupWaitBits(event_group, WIFI_ACTIVE, 
                            pdFALSE,  // Clear on shutdown
                            pdTRUE, 
                            portMAX_DELAY);

        // 2. Boot up AP + Web Server
        if (!ap_up) {
            if (wifi_ap_start() == ESP_OK) {
                ap_up = true;
            } else {
                ESP_LOGE(TAG, "wifi_ap_start failed. Retry in 5 s");
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }
        }

        if (!server) {
            server = start_server();
            if (!server) {
                ESP_LOGE(TAG, "start_server failed. Retry in 5 s");
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }
        }

        // 3. Active Monitoring Loop
        ESP_LOGI(TAG, "WiFi active. Waiting for clients or 5-minute idle timeout");
        
        TickType_t start_ticks = xTaskGetTickCount();
        const TickType_t timeout_ticks = pdMS_TO_TICKS(5 * 60 * 1000); // 5 minutes
        
        // Clear any stale disconnect bits before we begin watching
        xEventGroupClearBits(event_group, WIFI_CLIENT_DISCONNECTED);

        while (1) {
            // Wait up to 1 second for a disconnect or camera active signal. 
            // The 1-second timeout allows us to loop and check the 5-min timer.
            EventBits_t bits = xEventGroupWaitBits(event_group, 
                                    WIFI_CLIENT_DISCONNECTED | CAMERA_ACTIVE, 
                                    pdTRUE,   // Clear bits on exit
                                    pdFALSE,  // Wait for any of the bits
                                    pdMS_TO_TICKS(1000));

            // Condition A: Camera started recording (Safety Override)
            if (bits & CAMERA_ACTIVE) {
                ESP_LOGI(TAG, "Camera active. Suspending WiFi immediately.");
                break;
            }

            // Condition B: The user disconnected
            if (bits & WIFI_CLIENT_DISCONNECTED) {
                ESP_LOGI(TAG, "User disconnected. Shutting down WiFi.");
                break;
            }

            // Condition C: 5-minute timeout with NO users connected
            if (s_connected_clients == 0) {
                if ((xTaskGetTickCount() - start_ticks) >= timeout_ticks) {
                    ESP_LOGI(TAG, "5-minute inactivity timeout. Shutting down WiFi.");
                    break;
                }
            } else {
                // As long as someone is connected, keep resetting the idle timer
                start_ticks = xTaskGetTickCount();
            }
        }

        // 4. Shut Down and Cleanup
        if (server) stop_server(&server);
        if (ap_up) { 
            wifi_ap_stop(); 
            ap_up = false; 
        }

        // Reset state
        s_connected_clients = 0;
        xEventGroupClearBits(event_group, WIFI_CLIENT_DISCONNECTED);

        // Turn off the active flag so the system knows we are fully down
        ESP_LOGI(TAG, "WiFi shutdown complete. Clearing WIFI_ACTIVE bit.");
        xEventGroupClearBits(event_group, WIFI_ACTIVE);
        
        // Loop wraps around and goes back to sleep at Step 1
    }
}