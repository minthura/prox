#include "openweather.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_crt_bundle.h"

static const char *TAG = "openweather";

/* Maximum size of the HTTP response body we'll buffer */
#define RESPONSE_BUFFER_SIZE  2048

/* Static copy of the user configuration */
static openweather_config_t s_config = {0};
static bool                 s_initialised = false;

/* Response accumulation buffer used during the HTTP event callback */
typedef struct {
    char  *buf;
    int    len;
    int    cap;
} http_buf_t;

/* -------------------------------------------------------------------------
 * HTTP event callback – accumulates the body into http_buf_t
 * ---------------------------------------------------------------------- */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_buf_t *rb = (http_buf_t *)evt->user_data;

    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (!esp_http_client_is_chunked_response(evt->client)) {
            int remaining = rb->cap - rb->len - 1;
            int to_copy   = evt->data_len < remaining ? evt->data_len : remaining;
            memcpy(rb->buf + rb->len, evt->data, to_copy);
            rb->len += to_copy;
            rb->buf[rb->len] = '\0';
        }
        break;

    case HTTP_EVENT_ON_FINISH:
    case HTTP_EVENT_DISCONNECTED:
    case HTTP_EVENT_ERROR:
    default:
        break;
    }
    return ESP_OK;
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */
esp_err_t openweather_init(const openweather_config_t *config)
{
    if (config == NULL || config->api_key == NULL) {
        ESP_LOGE(TAG, "Invalid configuration.");
        return ESP_ERR_INVALID_ARG;
    }

    memset(&s_config, 0, sizeof(s_config));
    s_config.api_key = config->api_key;
    s_config.lat     = config->lat;
    s_config.lon     = config->lon;
    s_initialised    = true;

    ESP_LOGI(TAG, "Initialised. lat=%.4f lon=%.4f", s_config.lat, s_config.lon);
    return ESP_OK;
}

esp_err_t openweather_fetch(openweather_data_t *out)
{
    if (!s_initialised) {
        ESP_LOGE(TAG, "Call openweather_init() first.");
        return ESP_ERR_INVALID_STATE;
    }
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Build the request URL */
    char url[256];
    snprintf(url, sizeof(url),
             "https://api.openweathermap.org/data/2.5/weather"
             "?lat=%.4f&lon=%.4f&appid=%s",
             s_config.lat, s_config.lon, s_config.api_key);

    /* Allocate response buffer */
    char *resp_buf = (char *)malloc(RESPONSE_BUFFER_SIZE);
    if (resp_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate response buffer.");
        return ESP_ERR_NO_MEM;
    }
    memset(resp_buf, 0, RESPONSE_BUFFER_SIZE);

    http_buf_t rb = {
        .buf = resp_buf,
        .len = 0,
        .cap = RESPONSE_BUFFER_SIZE,
    };

    /* Set up the HTTP client */
    esp_http_client_config_t http_cfg = {
        .url                = url,
        .event_handler      = http_event_handler,
        .user_data          = &rb,
        .transport_type     = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach  = esp_crt_bundle_attach,  /* use bundled Mozilla CA store */
        .timeout_ms         = 10000,
    };

    esp_err_t ret        = ESP_FAIL;
    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to init HTTP client.");
        free(resp_buf);
        return ESP_FAIL;
    }

    ret = esp_http_client_perform(client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP GET failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGE(TAG, "HTTP status %d – body: %s", status, resp_buf);
        ret = ESP_FAIL;
        goto cleanup;
    }

    ESP_LOGD(TAG, "Response: %s", resp_buf);

    /* Parse the JSON response */
    cJSON *root = cJSON_Parse(resp_buf);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON parse error.");
        ret = ESP_FAIL;
        goto cleanup;
    }

    /* "weather" is an array; grab the first element */
    cJSON *weather_arr = cJSON_GetObjectItem(root, "weather");
    if (cJSON_IsArray(weather_arr) && cJSON_GetArraySize(weather_arr) > 0) {
        cJSON *w = cJSON_GetArrayItem(weather_arr, 0);

        cJSON *desc = cJSON_GetObjectItem(w, "description");
        cJSON *main = cJSON_GetObjectItem(w, "main");

        if (cJSON_IsString(desc)) {
            strncpy(out->description, desc->valuestring,
                    sizeof(out->description) - 1);
        }
        if (cJSON_IsString(main)) {
            strncpy(out->main, main->valuestring,
                    sizeof(out->main) - 1);
        }
    }

    /* "main" object – temperature, humidity & pressure */
    cJSON *main_obj = cJSON_GetObjectItem(root, "main");
    if (cJSON_IsObject(main_obj)) {
        cJSON *temp = cJSON_GetObjectItem(main_obj, "temp");
        cJSON *fl   = cJSON_GetObjectItem(main_obj, "feels_like");
        cJSON *hum  = cJSON_GetObjectItem(main_obj, "humidity");
        cJSON *pres = cJSON_GetObjectItem(main_obj, "pressure");

        if (cJSON_IsNumber(temp)) { out->temp_kelvin       = (float)temp->valuedouble; }
        if (cJSON_IsNumber(fl))   { out->feels_like_kelvin = (float)fl->valuedouble;   }
        if (cJSON_IsNumber(hum))  { out->humidity          = (float)hum->valuedouble;  }
        if (cJSON_IsNumber(pres)) { out->pressure_hpa      = (float)pres->valuedouble; }
    }

    /* "wind" object – speed */
    cJSON *wind = cJSON_GetObjectItem(root, "wind");
    if (cJSON_IsObject(wind)) {
        cJSON *spd = cJSON_GetObjectItem(wind, "speed");
        if (cJSON_IsNumber(spd)) { out->wind_speed_ms = (float)spd->valuedouble; }
    }

    /* City name */
    cJSON *name = cJSON_GetObjectItem(root, "name");
    if (cJSON_IsString(name)) {
        strncpy(out->city_name, name->valuestring, sizeof(out->city_name) - 1);
    }

    /* Timezone offset (seconds from UTC) */
    cJSON *tz = cJSON_GetObjectItem(root, "timezone");
    if (cJSON_IsNumber(tz)) {
        out->timezone_offset = (int32_t)tz->valueint;
    }

    /* sys – sunrise & sunset (Unix UTC timestamps) */
    cJSON *sys = cJSON_GetObjectItem(root, "sys");
    if (cJSON_IsObject(sys)) {
        cJSON *sr = cJSON_GetObjectItem(sys, "sunrise");
        cJSON *ss = cJSON_GetObjectItem(sys, "sunset");
        if (cJSON_IsNumber(sr)) { out->sunrise_utc = (int64_t)sr->valuedouble; }
        if (cJSON_IsNumber(ss)) { out->sunset_utc  = (int64_t)ss->valuedouble; }
    }

    cJSON_Delete(root);
    ret = ESP_OK;

cleanup:
    esp_http_client_cleanup(client);
    free(resp_buf);
    return ret;
}
