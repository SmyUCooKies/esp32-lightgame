#include "web_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"

#include "game.h"

static const char *TAG = "web";

static const char INDEX_HTML[] =
    "<!DOCTYPE html><html lang=\"en\"><head>"
    "<meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>ESP32 Light Game</title>"
    "<style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:system-ui,sans-serif;background:#0f1117;color:#e8eaed;min-height:100vh;padding:24px}"
    "h1{font-size:1.6rem;margin-bottom:4px}"
    ".sub{color:#9aa0a6;margin-bottom:24px;font-size:.9rem}"
    ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:16px}"
    ".card{background:#1a1d27;border:1px solid #2a2f3d;border-radius:12px;padding:20px}"
    ".card h2{font-size:1rem;color:#9aa0a6;margin-bottom:14px;text-transform:uppercase;letter-spacing:.06em}"
    ".stat{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid #252936}"
    ".stat:last-child{border:none}"
    ".stat span:last-child{font-weight:600;color:#fff}"
    ".phase{font-size:1.2rem;font-weight:700;color:#7cb3ff}"
    ".btns{display:flex;gap:10px;flex-wrap:wrap;margin-top:12px}"
    "button{border:none;border-radius:10px;padding:14px 18px;font-size:1rem;font-weight:600;cursor:pointer;color:#fff;flex:1;min-width:90px}"
    "button:active{transform:scale(.97)}"
    ".red{background:#c62828}.green{background:#2e7d32}.blue{background:#1565c0}"
    ".ctrl{background:#333a4d;margin-top:8px;width:100%;flex:none}"
    ".ctrl.primary{background:#3949ab}"
    "label{display:block;margin:12px 0 6px;font-size:.85rem;color:#9aa0a6}"
    "input[type=range]{width:100%}"
    ".strip-preview{display:flex;gap:2px;height:28px;margin-top:12px;border-radius:6px;overflow:hidden;background:#12151c;padding:4px}"
    ".strip-preview div{flex:1;border-radius:2px;background:#222}"
    "</style></head><body>"
    "<h1>Light Strip Shooter</h1>"
    "<p class=\"sub\">Arcade mode works offline. This page adds stats and controls.</p>"
    "<div class=\"grid\">"
    "<div class=\"card\"><h2>Game</h2>"
    "<div class=\"stat\"><span>Phase</span><span class=\"phase\" id=\"phase\">-</span></div>"
    "<div class=\"stat\"><span>Score</span><span id=\"score\">0</span></div>"
    "<div class=\"stat\"><span>High score</span><span id=\"high\">0</span></div>"
    "<div class=\"stat\"><span>Enemy speed</span><span id=\"espeed\">-</span></div>"
    "<div class=\"stat\"><span>Spawn interval</span><span id=\"spawn\">-</span></div>"
    "<div class=\"stat\"><span>Active enemies</span><span id=\"enemies\">0</span></div>"
    "<div class=\"stat\"><span>Active bullets</span><span id=\"bullets\">0</span></div>"
    "<div class=\"strip-preview\" id=\"preview\"></div>"
    "<div class=\"btns\">"
    "<button class=\"red\" onclick=\"shoot(0)\">Shoot Red</button>"
    "<button class=\"green\" onclick=\"shoot(1)\">Shoot Green</button>"
    "<button class=\"blue\" onclick=\"shoot(2)\">Shoot Blue</button>"
    "</div>"
    "<div class=\"btns\">"
    "<button class=\"ctrl primary\" onclick=\"action('start')\">Start</button>"
    "<button class=\"ctrl\" onclick=\"action('pause')\">Pause</button>"
    "<button class=\"ctrl\" onclick=\"action('resume')\">Resume</button>"
    "<button class=\"ctrl\" onclick=\"action('reset')\">Reset</button>"
    "</div></div>"
    "<div class=\"card\"><h2>Settings</h2>"
    "<label>LED count (<span id=\"lcv\">30</span>)</label>"
    "<input type=\"range\" id=\"led_count\" min=\"10\" max=\"60\" value=\"30\" oninput=\"lcv.textContent=this.value\">"
    "<label>Brightness (<span id=\"bvv\">64</span>)</label>"
    "<input type=\"range\" id=\"brightness\" min=\"10\" max=\"255\" value=\"64\" oninput=\"bvv.textContent=this.value\">"
    "<label>Base enemy speed (<span id=\"esv\">2.0</span>)</label>"
    "<input type=\"range\" id=\"enemy_speed\" min=\"5\" max=\"80\" value=\"20\" oninput=\"esv.textContent=(this.value/10).toFixed(1)\">"
    "<label>Spawn interval ms (<span id=\"spv\">2500</span>)</label>"
    "<input type=\"range\" id=\"spawn_interval\" min=\"600\" max=\"5000\" step=\"100\" value=\"2500\" oninput=\"spv.textContent=this.value\">"
    "<button class=\"ctrl primary\" style=\"margin-top:16px\" onclick=\"saveSettings()\">Save settings</button>"
    "</div>"
    "<div class=\"card\"><h2>Hardware</h2>"
    "<div class=\"stat\"><span>LED data pin</span><span>GPIO 18</span></div>"
    "<div class=\"stat\"><span>Red button</span><span>GPIO 13</span></div>"
    "<div class=\"stat\"><span>Green button</span><span>GPIO 12</span></div>"
    "<div class=\"stat\"><span>Blue button</span><span>GPIO 14</span></div>"
    "<div class=\"stat\"><span>Strip power</span><span>5V + GND</span></div>"
    "<p class=\"sub\" style=\"margin-top:12px\">Press any color button to start or restart without the web UI.</p>"
    "</div></div>"
    "<script>"
    "const phaseNames=['Idle','Playing','Paused','Game Over'];"
    "const colors=['rgb(198,40,40)','rgb(46,125,50)','rgb(21,101,192)'];"
    "async function api(path,method,body){"
    "const o={method};if(body){o.headers={'Content-Type':'application/json'};o.body=JSON.stringify(body);}"
    "return fetch(path,o);}"
    "async function shoot(c){await api('/api/action','POST',{action:'shoot',color:c});refresh();}"
    "async function action(a){await api('/api/action','POST',{action:a});refresh();}"
    "async function saveSettings(){"
    "await api('/api/settings','POST',{"
    "led_count:+led_count.value,brightness:+brightness.value,"
    "enemy_speed:+enemy_speed.value/10,spawn_interval_ms:+spawn_interval.value});refresh();}"
    "function renderPreview(d){"
    "const p=document.getElementById('preview');p.innerHTML='';"
    "const n=d.settings.led_count;for(let i=0;i<n;i++){"
    "const el=document.createElement('div');"
    "const e=d.enemies.find(x=>x.active&&Math.round(x.pos)===i);"
    "const b=d.bullets.find(x=>x.active&&Math.round(x.pos)===i);"
    "if(e)el.style.background=colors[e.color];else if(b)el.style.background='#fff';"
    "else if(i===0)el.style.background='#444';p.appendChild(el);}}"
    "async function refresh(){"
    "const r=await fetch('/api/state');const d=await r.json();"
    "phase.textContent=phaseNames[d.phase];score.textContent=d.score;high.textContent=d.high_score;"
    "espeed.textContent=d.current_enemy_speed.toFixed(1)+' LEDs/s';"
    "spawn.textContent=d.current_spawn_interval_ms+' ms';"
    "enemies.textContent=d.enemy_count;bullets.textContent=d.bullet_count;"
    "led_count.value=d.settings.led_count;lcv.textContent=d.settings.led_count;"
    "brightness.value=d.settings.brightness;bvv.textContent=d.settings.brightness;"
    "enemy_speed.value=Math.round(d.settings.enemy_speed*10);esv.textContent=d.settings.enemy_speed.toFixed(1);"
    "spawn_interval.value=d.settings.spawn_interval_ms;spv.textContent=d.settings.spawn_interval_ms;"
    "renderPreview(d);}"
    "refresh();setInterval(refresh,200);"
    "</script></body></html>";

static const char *phase_name(game_phase_t p)
{
    switch (p) {
    case GAME_PHASE_IDLE:
        return "idle";
    case GAME_PHASE_PLAYING:
        return "playing";
    case GAME_PHASE_PAUSED:
        return "paused";
    case GAME_PHASE_GAME_OVER:
        return "game_over";
    default:
        return "unknown";
    }
}

static int json_find_int(const char *body, const char *key, int fallback)
{
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char *pos = strstr(body, pattern);
    if (!pos) {
        return fallback;
    }
    pos += strlen(pattern);
    while (*pos == ' ') {
        pos++;
    }
    return atoi(pos);
}

static float json_find_float(const char *body, const char *key, float fallback)
{
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char *pos = strstr(body, pattern);
    if (!pos) {
        return fallback;
    }
    pos += strlen(pattern);
    while (*pos == ' ') {
        pos++;
    }
    return (float)atof(pos);
}

static char *json_find_string(const char *body, const char *key)
{
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char *start = strstr(body, pattern);
    if (!start) {
        return NULL;
    }
    start += strlen(pattern);
    const char *end = strchr(start, '"');
    if (!end) {
        return NULL;
    }
    size_t len = (size_t)(end - start);
    char *out = malloc(len + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

static esp_err_t handle_index(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_state(httpd_req_t *req)
{
    char *json = malloc(3072);
    if (!json) {
        return ESP_FAIL;
    }

    game_t *g = game_lock();

    int enemy_count = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (g->enemies[i].active) {
            enemy_count++;
        }
    }
    int bullet_count = 0;
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (g->bullets[i].active) {
            bullet_count++;
        }
    }

    int written = snprintf(
        json, 3072,
        "{\"phase\":%d,\"phase_name\":\"%s\",\"score\":%d,\"high_score\":%d,"
        "\"current_enemy_speed\":%.2f,\"current_spawn_interval_ms\":%lu,"
        "\"settings\":{\"led_count\":%u,\"brightness\":%u,\"enemy_speed\":%.2f,"
        "\"bullet_speed\":%.2f,\"spawn_interval_ms\":%lu},"
        "\"enemy_count\":%d,\"bullet_count\":%d,\"enemies\":[",
        g->phase, phase_name(g->phase), g->score, g->high_score, g->current_enemy_speed,
        (unsigned long)g->current_spawn_interval_ms, g->settings.led_count, g->settings.brightness,
        g->settings.enemy_speed, g->settings.bullet_speed,
        (unsigned long)g->settings.spawn_interval_ms, enemy_count, bullet_count);

    int enemy_written = 0;
    for (int i = 0; i < MAX_ENEMIES && written < 2900; i++) {
        if (!g->enemies[i].active) {
            continue;
        }
        enemy_written++;
        written += snprintf(json + written, 3072 - written, "%s{\"pos\":%.2f,\"color\":%d,\"active\":true}",
                            enemy_written == 1 ? "" : ",", g->enemies[i].pos, g->enemies[i].color);
    }

    written += snprintf(json + written, 3072 - written, "],\"bullets\":[");

    int bullet_written = 0;
    for (int i = 0; i < MAX_BULLETS && written < 3000; i++) {
        if (!g->bullets[i].active) {
            continue;
        }
        bullet_written++;
        written += snprintf(json + written, 3072 - written, "%s{\"pos\":%.2f,\"color\":%d,\"active\":true}",
                            bullet_written == 1 ? "" : ",", g->bullets[i].pos, g->bullets[i].color);
    }

    written += snprintf(json + written, 3072 - written, "]}");

    game_unlock();

    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, json);
    free(json);
    return err;
}

static esp_err_t handle_action(httpd_req_t *req)
{
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        return ESP_FAIL;
    }
    buf[len] = '\0';

    char *action = json_find_string(buf, "action");
    if (!action) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing action");
        return ESP_FAIL;
    }

    if (strcmp(action, "shoot") == 0) {
        int color = json_find_int(buf, "color", -1);
        if (color >= 0 && color < GAME_COLOR_COUNT) {
            game_shoot((game_color_t)color);
        }
    } else if (strcmp(action, "start") == 0) {
        game_start();
    } else if (strcmp(action, "pause") == 0) {
        game_pause();
    } else if (strcmp(action, "resume") == 0) {
        game_resume();
    } else if (strcmp(action, "reset") == 0) {
        game_reset();
    }

    free(action);
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t handle_settings(httpd_req_t *req)
{
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        return ESP_FAIL;
    }
    buf[len] = '\0';

    game_settings_t s = *game_get_settings();
    s.led_count = (uint16_t)json_find_int(buf, "led_count", s.led_count);
    s.brightness = (uint8_t)json_find_int(buf, "brightness", s.brightness);
    s.enemy_speed = json_find_float(buf, "enemy_speed", s.enemy_speed);
    s.spawn_interval_ms = (uint32_t)json_find_int(buf, "spawn_interval_ms", s.spawn_interval_ms);

    game_update_settings(&s);
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

bool web_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.lru_purge_enable = true;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "httpd start failed");
        return false;
    }

    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_index,
    };
    httpd_uri_t state_uri = {
        .uri = "/api/state",
        .method = HTTP_GET,
        .handler = handle_state,
    };
    httpd_uri_t action_uri = {
        .uri = "/api/action",
        .method = HTTP_POST,
        .handler = handle_action,
    };
    httpd_uri_t settings_uri = {
        .uri = "/api/settings",
        .method = HTTP_POST,
        .handler = handle_settings,
    };

    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &state_uri);
    httpd_register_uri_handler(server, &action_uri);
    httpd_register_uri_handler(server, &settings_uri);

    ESP_LOGI(TAG, "HTTP server started");
    return true;
}
