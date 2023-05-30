//
// Created by Mob on 2023/5/30.
//

#include <pthread.h>
#include <jansson.h>
#include <obsversion.h>
#include <curl/curl.h>

#include "platform.h"
#include "obs-config.h"
#include "mobile.h"
#include "base.h"
#include "dstr.h"
#include "darray.h"
#include "wrapper_aes.h"

#ifndef SEC_TO_NSEC
#define SEC_TO_NSEC 1000000000ULL
#endif
static struct dstr repot_codes = {0};
static int report_running = 0;
static struct dstr LIVE_SERVER_URL = {0};
static DARRAY(struct gzb_stream_info) gzb_streams = {0};


void gzb_upgrade_info_free(struct gzb_upgrade_info *dst)
{
    bfree(dst->upgradeText);
    bfree(dst->upgradeURL);
    dst->focusUpgrade = 0;
    dst->needUpgrade = 0;
}
static size_t fetch_api_response(void *data, size_t size, size_t nmemb, void *user_pointer)
{
    struct dstr *json = user_pointer;
    size_t realsize = size * nmemb;
    dstr_ncat(json, data, realsize);
    return realsize;
}

CURLcode fetch_get(char *url, struct dstr *response_body) {
    CURL *curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, fetch_api_response);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, response_body);
    curl_obs_set_revoke_setting(curl_handle);
#if LIBCURL_VERSION_NUM >= 0x072400
    curl_easy_setopt(curl_handle, CURLOPT_SSL_ENABLE_ALPN, 0);
#endif
    CURLcode res = curl_easy_perform(curl_handle);
    curl_easy_cleanup(curl_handle);
    return res;
}

void fetch_api_host_config() {
    if (LIVE_SERVER_URL.len > 0) {
        return;
    }
    struct dstr response_body = {0};
    CURLcode res = fetch_get("https://config.ganzb.com.cn/oss/config.json", &response_body);
    if (res == CURLE_OK) {
        json_error_t error;
        json_t *root = json_loads(response_body.array, JSON_REJECT_DUPLICATES, &error);
        if (root != NULL) {
            const char *obsHost = json_string_value(json_object_get(json_object_get(root, "obs"), "api"));
            dstr_cat(&LIVE_SERVER_URL, obsHost);
            dstr_cat(&LIVE_SERVER_URL, "/api/mobile/stream/obs.do");
            
            blog(LOG_WARNING, "GZB LIVE_SERVER_URL.array %s", LIVE_SERVER_URL.array);
        }
    }
    dstr_free(&response_body);
}
void check_gzb_upgrade(struct gzb_upgrade_info *upgrade) {
    struct dstr response_body = {0};
    CURLcode res = fetch_get("https://config.ganzb.com.cn/oss/config.json", &response_body);
    if (res == CURLE_OK) {
        json_error_t error;
        json_t *root = json_loads(response_body.array, JSON_REJECT_DUPLICATES, &error);
        json_t *obs = json_object_get(root, "obs");
        const char* obsVersion = json_string_value(json_object_get(obs, "version"));
        int64_t obsFocus = json_integer_value(json_object_get(obs, "focus"));
        const char* obsText = json_string_value(json_object_get(obs, "text"));
        const char* obsURL = json_string_value(json_object_get(obs, "url"));
        upgrade->focusUpgrade = obsFocus;
        upgrade->upgradeURL = bstrdup(obsURL);
        upgrade->upgradeText = bstrdup(obsText);
        if (strcmp(OBS_VERSION_CANONICAL, obsVersion) >= 0) {
            upgrade->needUpgrade = 0;
        } else {
            upgrade->needUpgrade = 1;
        }
    }
    dstr_free(&response_body);
}
CURLcode fetch_api(char *url, char*data, int aes, struct dstr *response_body) {
    struct timeval ts;
    gettimeofday(&ts, NULL);
    char timestamp[22];
    sprintf(timestamp, "%ld", ts.tv_sec);
    
    struct dstr requsetURL = {0};
    struct dstr form_data = {0};
    struct dstr aes_data = {0};
    struct dstr aes_res_data = {0};
    dstr_cat(&requsetURL, url);
    dstr_cat(&requsetURL, "?from=obs&sign=");
    dstr_cat(&requsetURL, timestamp);
    dstr_cat(&requsetURL, "&version=");
    dstr_cat(&requsetURL, OBS_VERSION_CANONICAL);
    if (aes == 1) {
        encode_aes_wrapper(data, &aes_data);
        dstr_copy(&form_data, "data=");
        dstr_cat(&form_data, aes_data.array);
    } else {
        dstr_cat(&form_data, data);
    }
//    blog(LOG_WARNING, "GZB fetch_api_req url : %s", url);
//    blog(LOG_WARNING, "GZB fetch_api_req form: %s", data);
//    blog(LOG_WARNING, "GZB fetch_api_req aes : %s", form_data.array);

    
    CURL *curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, requsetURL.array);
    curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, form_data.array);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, fetch_api_response);
    if (aes == 0) {
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, response_body);
    } else {
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &aes_res_data);
    }
    curl_obs_set_revoke_setting(curl_handle);
#if LIBCURL_VERSION_NUM >= 0x072400
    curl_easy_setopt(curl_handle, CURLOPT_SSL_ENABLE_ALPN, 0);
#endif
    CURLcode res = curl_easy_perform(curl_handle);
    if (res == CURLE_OK && aes == 1) {
        decode_aes_wrapper(aes_res_data.array, response_body);
    }
    dstr_free(&aes_res_data);
    dstr_free(&form_data);
    dstr_free(&aes_data);
    dstr_free(&requsetURL);

//    blog(LOG_WARNING, "GZB fetch_api_res");
//    blog(LOG_WARNING, "GZB fetch_api_res %d-%s", res, response_body->array);
    curl_easy_cleanup(curl_handle);
    return res;
}

struct gzb_stream_info *find_gzb_stream(const char *stream_code) {
    for (size_t i=0; i< gzb_streams.num; i++) {
        struct gzb_stream_info *info = &gzb_streams.array[i];
        if (info->stream_code != NULL && strcmp(info->stream_code, stream_code) == 0) {
            return info;
        }
    }
    return NULL;
}

void free_stream_cache(void) {
//    blog(LOG_WARNING, "GZB free_stream_cache");
    for (size_t i=0; i< gzb_streams.num; i++) {
        struct gzb_stream_info *info = &gzb_streams.array[i];
        bfree(info->server);
        bfree(info->key);
        bfree(info->error_message);
        bfree(info->stream_code);
        bfree(info->stream_metadata);
    }
    da_free(gzb_streams);
    dstr_free(&repot_codes);
}
struct gzb_stream_info *fetch_stream_code(const char *stream_code) {
    fetch_api_host_config();
//    blog(LOG_WARNING, "GZB fetch_stream_code: %s", stream_code);
    struct gzb_stream_info *info = find_gzb_stream(stream_code);
    if (info != NULL) {
//        blog(LOG_WARNING, "GZB fetch_stream_code cache result: %s-%s%s", info->stream_code, info->server, info->key);
        uint64_t ts_sec = os_gettime_ns() / SEC_TO_NSEC;
        if (info->server != NULL) {
            return info;
        } else if (info->error_message != NULL && ts_sec - info->last_time < 5) {
            return info;
        }
    }
    if (info == NULL) {
        info = da_push_back_new(gzb_streams);
    }
    struct dstr form = {0};
    struct dstr response_string = {0};
    dstr_copy(&form, "action=fetch&code=");
    dstr_cat(&form, stream_code);
    CURLcode response_code = fetch_api(LIVE_SERVER_URL.array, form.array, 0, &response_string);
    if (response_code == CURLE_OK) {
        json_error_t error;
        json_t *root = json_loads(response_string.array, JSON_REJECT_DUPLICATES, &error);
        if (root) {
            long long code = json_integer_value(json_object_get(root, "code"));
            if (code == 0) {
                json_t *data = json_object_get(root, "data");
                const char *url_str = json_string_value(json_object_get(data, "server"));
                const char *key_str = json_string_value(json_object_get(data, "key"));
                json_t *metadata = json_object_get(data, "metadata");
                
                info->server = bstrdup(url_str);
                info->key = bstrdup(key_str);
                info->stream_code = bstrdup(stream_code);
                info->stream_metadata = bstrdup(json_dumps(metadata, JSON_MAX_INDENT));
            } else {
                const char *error_message = json_string_value(json_object_get(json_object_get(root, "error"), "message"));
                info->error_message = bstrdup(error_message);
            }
            info->last_time = os_gettime_ns() / SEC_TO_NSEC;
        } else {
            info->error_message = "Need Check Your Timer";
        }
        json_decref(root);
    } else {
        char str[255];
        sprintf(str, "Error code: %d", response_code);
        info->error_message = bstrdup(str);
    }
    dstr_free(&form);
    dstr_free(&response_string);
//    blog(LOG_WARNING, "GZB fetch_stream_code api result: %s-%s-%s%s", info->stream_code , info->error_message, info->server, info->key);
    return info;
}

void * stop_stream_heat_report_thread(void *args) {
    const char *stream_code = (const char *) args;
    struct dstr form = {0};
    struct dstr response_string = {0};
    dstr_copy(&form, "action=heat_stop&code=");
    dstr_cat(&form, stream_code);
    fetch_api(LIVE_SERVER_URL.array, form.array, 0, &response_string);
    dstr_free(&form);
    dstr_free(&response_string);
    return NULL;
}

void * start_stream_heat_report_thread(void *args) {
    const char *stream_code = (const char *) args;
    struct dstr form = {0};
    struct dstr response_string = {0};
    dstr_copy(&form, "action=heat_start&code=");
    dstr_cat(&form, stream_code);
    fetch_api(LIVE_SERVER_URL.array, form.array, 0, &response_string);
    dstr_free(&form);
    dstr_free(&response_string);
    return NULL;
}

void *start_stream_heat_report_timer_thread() {
    do {
//        blog(LOG_WARNING, "GZB start_stream_heat_report_thread");
        if (&repot_codes.len > 0) {
            struct dstr form = {0};
            dstr_copy(&form, "action=heat_start&code=");
            dstr_cat(&form, repot_codes.array);

            struct dstr response_string = {0};
            fetch_api(LIVE_SERVER_URL.array, form.array, 0, &response_string);
            dstr_free(&form);
            dstr_free(&response_string);
        }
        os_sleep_ms(60000);
    } while (report_running == 1);
    return NULL;
}

void add_report_stream_code(const char *stream_code, int type) {
    if (stream_code == NULL) {
        return;
    }
    if (type == 1 && !dstr_find_i(&repot_codes, stream_code)) {
        dstr_cat(&repot_codes, ",");
        dstr_cat(&repot_codes, stream_code);
    } else if (type == 0 && dstr_find_i(&repot_codes, stream_code)) {
        struct dstr replace_code = {0};
        dstr_cat(&replace_code, ",");
        dstr_cat(&replace_code, stream_code);
        dstr_replace(&repot_codes, replace_code.array, "");
        dstr_free(&replace_code);
        pthread_t ntid;
        pthread_create(&ntid, NULL, stop_stream_heat_report_thread, (void *) stream_code);
    }
    if (report_running == 0) {
        report_running = 1;
        pthread_t ntid;
        pthread_create(&ntid, NULL, start_stream_heat_report_timer_thread, NULL);
    } else if (type == 1) {
        pthread_t ntid;
        pthread_create(&ntid, NULL, start_stream_heat_report_thread, (void *) stream_code);
    }
}

void exitApp() {
    exit(0);
}
