#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sqlite3ext.h"
 
SQLITE_EXTENSION_INIT1

static char *ENCODE = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

static unsigned char DECODE[128] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
    0x11, 0xff, 0x12, 0x13, 0xff, 0x14, 0x15, 0xff,
    0x16, 0x17, 0x18, 0x19, 0x1a, 0xff, 0x1b, 0x1c,
    0x1d, 0x1e, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
    0x11, 0xff, 0x12, 0x13, 0xff, 0x14, 0x15, 0xff,
    0x16, 0x17, 0x18, 0x19, 0x1a, 0xff, 0x1b, 0x1c,
    0x1d, 0x1e, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static void ulid_new(sqlite3_context *context, int argc, sqlite3_value **argv) {
    unsigned long long timestamp;
    unsigned char randomness[10];

    if (argc > 0) {
        if (sqlite3_value_type(argv[0]) != SQLITE_INTEGER) {
            sqlite3_result_error(context, "INTEGER value expected for timestamp", -1);
            return;
        }

        timestamp = sqlite3_value_int64(argv[0]);
    } else {
#ifdef __linux__
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
            sqlite3_result_error(context, "Internal error: failed to get current time", -1);
            return;
        }
        timestamp = (unsigned long long)ts.tv_sec * 1000 + (unsigned long long)ts.tv_nsec / (1000 * 1000);
#else
        timestamp = (unsigned long long)time(NULL) * 1000;
#endif
    }

    if (argc > 1) {
        if (sqlite3_value_type(argv[1]) != SQLITE_BLOB) {
            sqlite3_result_error(context, "BLOB value expected for randomness", -1);
            return;
        }

        if (sqlite3_value_bytes(argv[1]) != 10) {
            sqlite3_result_error(context, "Invalid byte length for randomness", -1);
            return;
        }

        const unsigned char *value = sqlite3_value_blob(argv[1]);
        memcpy(randomness, value, 10);
    } else {
        int i;
        for (i = 0; i < 10; i++) {
            randomness[i] = rand() & 0xff;
        }
    }

    unsigned char result[16] = {
        (unsigned char)((timestamp >> 40) & 0xff),
        (unsigned char)((timestamp >> 32) & 0xff),
        (unsigned char)((timestamp >> 24) & 0xff),
        (unsigned char)((timestamp >> 16) & 0xff),
        (unsigned char)((timestamp >> 8) & 0xff),
        (unsigned char)((timestamp >> 0) & 0xff),
    };

    memcpy(result + 6, randomness, 10);

    sqlite3_result_blob(context, result, 16, SQLITE_TRANSIENT);
}

static void ulid_encode(sqlite3_context *context, int argc, sqlite3_value **argv) {
    if (argc == 0) {
        sqlite3_result_error(context, "Argument required", -1);
        return;
    }

    if (sqlite3_value_type(argv[0]) != SQLITE_BLOB) {
        sqlite3_result_error(context, "BLOB value expected", -1);
        return;
    }

    if (sqlite3_value_bytes(argv[0]) != 16) {
        sqlite3_result_error(context, "Invalid byte length", -1);
        return;
    }

    const unsigned char *value = sqlite3_value_blob(argv[0]);

    char result[26] = {
        // Timestamp
        ENCODE[(value[0] & 224) >> 5],
        ENCODE[(value[0] & 31)],
        ENCODE[(value[1] & 248) >> 3],
        ENCODE[((value[1] & 7) << 2) | ((value[2] & 192) >> 6)],
        ENCODE[((value[2] & 62) >> 1)],
        ENCODE[((value[2] & 1) << 4) | ((value[3] & 240) >> 4)],
        ENCODE[((value[3] & 15) << 1) | ((value[4] & 128) >> 7)],
        ENCODE[(value[4] & 124) >> 2],
        ENCODE[((value[4] & 3) << 3) | ((value[5] & 224) >> 5)],
        ENCODE[(value[5] & 31)],

        // Randomness
        ENCODE[(value[6] & 248) >> 3],
        ENCODE[((value[6] & 7) << 2) | ((value[7] & 192) >> 6)],
        ENCODE[(value[7] & 62) >> 1],
        ENCODE[((value[7] & 1) << 4) | ((value[8] & 240) >> 4)],
        ENCODE[((value[8] & 15) << 1) | ((value[9] & 128) >> 7)],
        ENCODE[(value[9] & 124) >> 2],
        ENCODE[((value[9] & 3) << 3) | ((value[10] & 224) >> 5)],
        ENCODE[(value[10] & 31)],
        ENCODE[(value[11] & 248) >> 3],
        ENCODE[((value[11] & 7) << 2) | ((value[12] & 192) >> 6)],
        ENCODE[(value[12] & 62) >> 1],
        ENCODE[((value[12] & 1) << 4) | ((value[13] & 240) >> 4)],
        ENCODE[((value[13] & 15) << 1) | ((value[14] & 128) >> 7)],
        ENCODE[(value[14] & 124) >> 2],
        ENCODE[((value[14] & 3) << 3) | ((value[15] & 224) >> 5)],
        ENCODE[(value[15] & 31)],
    };
 
    sqlite3_result_text(context, result, 26, SQLITE_TRANSIENT);
}

static void ulid_decode(sqlite3_context *context, int argc, sqlite3_value **argv) {
    if (argc == 0) {
        sqlite3_result_error(context, "Argument required", -1);
        return;
    }

    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT) {
        sqlite3_result_error(context, "TEXT value expected", -1);
        return;
    }

    if (sqlite3_value_bytes(argv[0]) != 26) {
        sqlite3_result_error(context, "Invalid text length", -1);
        return;
    }

    const char *value = sqlite3_value_text(argv[0]);

    unsigned char result[16] = {
        // Timestamp
        ((DECODE[value[0]] << 5) | DECODE[value[1]]) & 0xff,
        ((DECODE[value[2]] << 3) | (DECODE[value[3]] >> 2)) & 0xff,
        ((DECODE[value[3]] << 6) | (DECODE[value[4]] << 1) | (DECODE[value[5]] >> 4)) & 0xff,
        ((DECODE[value[5]] << 4) | (DECODE[value[6]] >> 1)) & 0xff,
        ((DECODE[value[6]] << 7) | (DECODE[value[7]] << 2) | (DECODE[value[8]] >> 3)) & 0xff,
        ((DECODE[value[8]] << 5) | (DECODE[value[9]])) & 0xff,

        // Randomness
        ((DECODE[value[10]] << 3) | (DECODE[value[11]] >> 2)) & 0xff,
        ((DECODE[value[11]] << 6) | (DECODE[value[12]] << 1) | (DECODE[value[13]] >> 4)) & 0xff,
        ((DECODE[value[13]] << 4) | (DECODE[value[14]] >> 1)) & 0xff,
        ((DECODE[value[14]] << 7) | (DECODE[value[15]] << 2) | (DECODE[value[16]] >> 3)) & 0xff,
        ((DECODE[value[16]] << 5) | (DECODE[value[17]])) & 0xff,
        ((DECODE[value[18]] << 3) | (DECODE[value[19]] >> 2)) & 0xff,
        ((DECODE[value[19]] << 6) | (DECODE[value[20]] << 1) | (DECODE[value[21]] >> 4)) & 0xff,
        ((DECODE[value[21]] << 4) | (DECODE[value[22]] >> 1)) & 0xff,
        ((DECODE[value[22]] << 7) | (DECODE[value[23]] << 2) | (DECODE[value[24]] >> 3)) & 0xff,
        ((DECODE[value[24]] << 5) | (DECODE[value[25]])) & 0xff,
    };

    sqlite3_result_blob(context, result, 16, SQLITE_TRANSIENT);
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_extension_init(
    sqlite3 *db,
    char **pzErrMsg,
    const sqlite3_api_routines *pApi
) {
    SQLITE_EXTENSION_INIT2(pApi);

    srand((unsigned int)time(NULL));

    int res;

    res = sqlite3_create_function(db, "ulid_new", -1, SQLITE_UTF8, NULL, ulid_new, NULL, NULL);
    if (res != SQLITE_OK) {
        *pzErrMsg = sqlite3_mprintf("Can't create ulid_new function.");
        return res;
    }

    res = sqlite3_create_function(db, "ulid_encode", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, NULL, ulid_encode, NULL, NULL);
    if (res != SQLITE_OK) {
        *pzErrMsg = sqlite3_mprintf("Can't create ulid_encode function.");
        return res;
    }

    res = sqlite3_create_function(db, "ulid_decode", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, NULL, ulid_decode, NULL, NULL);
    if (res != SQLITE_OK) {
        *pzErrMsg = sqlite3_mprintf("Can't create ulid_decode function.");
        return res;
    }

    return SQLITE_OK;
}
