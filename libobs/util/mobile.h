//
// Created by Mob on 2023/5/30.
//

#ifndef OBS_STUDIO_230530_MOBILE_H
#define OBS_STUDIO_230530_MOBILE_H

#include <pthread.h>
#include "curl/curl-helper.h"
#include "dstr.h"
#ifdef __cplusplus
extern "C" {
#endif

struct gzb_stream_info {
    char *stream_code;
    char *stream_metadata;
    char *error_message;
    char *server;
    char *key;
    uint64_t last_time;
};
struct gzb_upgrade_info {
    int64_t focusUpgrade;
    int64_t needUpgrade;
    char *upgradeText;
    char *upgradeURL;
};

EXPORT CURLcode fetch_api(char *url, char*data, int aes, struct dstr *response_body);

EXPORT struct gzb_stream_info *fetch_stream_code(const char *stream_code);

EXPORT void free_stream_cache();

EXPORT void add_report_stream_code(const char *stream_code, int type);

EXPORT void check_gzb_upgrade(struct gzb_upgrade_info *upgrade);

EXPORT void gzb_upgrade_info_free(struct gzb_upgrade_info *dst);

EXPORT void exitApp();
#ifdef __cplusplus
}
#endif

#endif //OBS_STUDIO_230530_MOBILE_H
