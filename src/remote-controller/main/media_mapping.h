#ifndef MEDIA_MAPPING_H
#define MEDIA_MAPPING_H

#include "rc522.h"

/**
 * Get the media ID for a given RFID UID.
 * 
 * @param uid Pointer to the rc522_picc_uid_t structure from the RFID tag
 * @return The media ID string if found, NULL if UID is not in the mapping
 */
const char* media_mapping_get_media_id(const rc522_picc_uid_t *uid);

#endif // MEDIA_MAPPING_H
