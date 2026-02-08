#include "media_mapping.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "MEDIA_MAPPING";

/* UID to Media ID mapping */
typedef struct {
    const char *uid_string;
    const char *media_id;
} uid_media_entry_t;

static const uid_media_entry_t uid_media_map[] = {
    {
        .uid_string = "B9 83 53 97",
        .media_id = "radiobrowser://radio/0669aea3-e2ec-11e9-a8ba-52543be04c81"
    },
    {
        .uid_string = "E6 2C 6F 04",
        .media_id = "radiobrowser://radio/82ebafb0-e192-40c5-abea-02834259f01d"
    }
};

static const size_t uid_media_map_size = sizeof(uid_media_map) / sizeof(uid_media_map[0]);

/**
 * Compare a hex string (e.g., "B9 83 53 97") with actual UID bytes
 * @return true if they match, false otherwise
 */
static bool uid_string_matches(const char *uid_string, const rc522_picc_uid_t *uid)
{
    if (!uid_string || !uid) {
        return false;
    }

    char uid_hex[RC522_PICC_UID_STR_BUFFER_SIZE_MAX] = {0};
    if (rc522_picc_uid_to_str(uid, uid_hex, sizeof(uid_hex)) != ESP_OK) {
        return false;
    }

    return strcmp(uid_string, uid_hex) == 0;
}

const char* media_mapping_get_media_id(const rc522_picc_uid_t *uid)
{
    if (!uid) {
        return NULL;
    }

    for (size_t i = 0; i < uid_media_map_size; i++) {
        if (uid_string_matches(uid_media_map[i].uid_string, uid)) {
            ESP_LOGI(TAG, "Found media ID for UID");
            return uid_media_map[i].media_id;
        }
    }

    ESP_LOGW(TAG, "No media ID mapping found for UID");
    return NULL;
}
