#include "adhan.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_crt_bundle.h"

static const char *TAG = "adhan";

/* Maximum size of the HTTP response body we'll buffer.
 * The AlAdhan timings response is ~2 KB; 4 KB gives comfortable headroom. */
#define RESPONSE_BUFFER_SIZE  4096

/* Base URL for the AlAdhan timings endpoint */
#define ADHAN_API_BASE  "https://api.aladhan.com/v1/timings"

/* Static copy of the user configuration */
static adhan_config_t s_config      = {0};
static bool           s_initialised = false;

/* -------------------------------------------------------------------------
 * HTTP event callback – accumulates the body into a flat buffer
 * ---------------------------------------------------------------------- */
typedef struct {
    char *buf;
    int   len;
    int   cap;
} http_buf_t;

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
 * Helper – safely copy an "HH:MM" string from a cJSON string node
 * ---------------------------------------------------------------------- */
static void copy_time_str(char *dst, size_t dst_sz, cJSON *node)
{
    if (cJSON_IsString(node) && node->valuestring != NULL) {
        /* The API may append " (UTC+8)" or similar – take only "HH:MM" */
        strncpy(dst, node->valuestring, dst_sz - 1);
        dst[dst_sz - 1] = '\0';
        /* Truncate at first space in case the API appends extra info */
        char *sp = strchr(dst, ' ');
        if (sp) *sp = '\0';
    }
}

/* =========================================================================
 * Public API
 * ====================================================================== */
esp_err_t adhan_init(const adhan_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid configuration.");
        return ESP_ERR_INVALID_ARG;
    }

    s_config.lat  = config->lat;
    s_config.lon  = config->lon;
    s_initialised = true;

    ESP_LOGI(TAG, "Initialised. lat=%.7f lon=%.7f", s_config.lat, s_config.lon);
    return ESP_OK;
}

esp_err_t adhan_fetch(adhan_timings_t *out)
{
    if (!s_initialised) {
        ESP_LOGE(TAG, "Call adhan_init() first.");
        return ESP_ERR_INVALID_STATE;
    }
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* ------------------------------------------------------------------
     * Build the date string "DD-MM-YYYY" from the SNTP-synchronised
     * system clock.
     * ---------------------------------------------------------------- */
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);

    static char date_str[30]; /* "DD-MM-YYYY\0" – static: lives in BSS, not on the task stack */
    snprintf(date_str, sizeof(date_str), "%02d-%02d-%04d",
             tm_info.tm_mday,
             tm_info.tm_mon + 1,
             tm_info.tm_year + 1900);

    /* ------------------------------------------------------------------
     * AlAdhan API query parameters.
     * Each is stored as a variable so callers can override them easily.
     * ---------------------------------------------------------------- */
    int         param_method          = ADHAN_METHOD;
    const char *param_shafaq          = ADHAN_SHAFAQ;
    const char *param_tune            = ADHAN_TUNE;
    const char *param_timezonestring  = ADHAN_TIMEZONE;
    const char *param_calendar_method = ADHAN_CALENDAR_METHOD;

    /* ------------------------------------------------------------------
     * Assemble the full request URL.
     * ---------------------------------------------------------------- */
    static char url[512];
    snprintf(url, sizeof(url),
             "%s/%s"
             "?latitude=%.7f"
             "&longitude=%.7f"
             "&method=%d"
             "&shafaq=%s"
             "&tune=%s"
             "&timezonestring=%s"
             "&calendarMethod=%s",
             ADHAN_API_BASE,
             date_str,
             s_config.lat,
             s_config.lon,
             param_method,
             param_shafaq,
             param_tune,
             param_timezonestring,
             param_calendar_method);

    ESP_LOGI(TAG, "Fetching prayer times for %s", date_str);
    ESP_LOGD(TAG, "URL: %s", url);

    /* ------------------------------------------------------------------
     * HTTP GET
     * ---------------------------------------------------------------- */
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

    esp_http_client_config_t http_cfg = {
        .url               = url,
        .event_handler     = http_event_handler,
        .user_data         = &rb,
        .transport_type    = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms        = 10000,
    };

    esp_err_t ret = ESP_FAIL;
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

    /* ------------------------------------------------------------------
     * Parse JSON:  root -> data -> timings -> { Imsak, Fajr, … }
     * ---------------------------------------------------------------- */
    memset(out, 0, sizeof(*out));

    cJSON *root = cJSON_Parse(resp_buf);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON parse error.");
        ret = ESP_FAIL;
        goto cleanup;
    }

    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (!cJSON_IsObject(data)) {
        ESP_LOGE(TAG, "Unexpected JSON structure: missing 'data'.");
        cJSON_Delete(root);
        ret = ESP_FAIL;
        goto cleanup;
    }

    cJSON *timings = cJSON_GetObjectItem(data, "timings");
    if (!cJSON_IsObject(timings)) {
        ESP_LOGE(TAG, "Unexpected JSON structure: missing 'timings'.");
        cJSON_Delete(root);
        ret = ESP_FAIL;
        goto cleanup;
    }

    copy_time_str(out->imsak,   sizeof(out->imsak),   cJSON_GetObjectItem(timings, "Imsak"));
    copy_time_str(out->fajr,    sizeof(out->fajr),    cJSON_GetObjectItem(timings, "Fajr"));
    copy_time_str(out->dhuhr,   sizeof(out->dhuhr),   cJSON_GetObjectItem(timings, "Dhuhr"));
    copy_time_str(out->asr,     sizeof(out->asr),     cJSON_GetObjectItem(timings, "Asr"));
    copy_time_str(out->maghrib, sizeof(out->maghrib), cJSON_GetObjectItem(timings, "Maghrib"));
    copy_time_str(out->isha,    sizeof(out->isha),    cJSON_GetObjectItem(timings, "Isha"));

    ESP_LOGI(TAG, "Imsak=%s  Fajr=%s  Dhuhr=%s  Asr=%s  Maghrib=%s  Isha=%s",
             out->imsak, out->fajr, out->dhuhr, out->asr, out->maghrib, out->isha);

    cJSON_Delete(root);
    ret = ESP_OK;

cleanup:
    esp_http_client_cleanup(client);
    free(resp_buf);
    return ret;
}
