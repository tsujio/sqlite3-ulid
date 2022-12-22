#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __linux__
#include <sys/random.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <bcrypt.h>
#endif

#include "sqlite3ext.h"
 
SQLITE_EXTENSION_INIT1

#define TIMESTAMP_BYTE_LEN 6
#define RANDOMNESS_BYTE_LEN 10
#define ULID_BYTE_LEN (TIMESTAMP_BYTE_LEN + RANDOMNESS_BYTE_LEN)
#define ULID_STR_LEN 26

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
    unsigned char randomness[RANDOMNESS_BYTE_LEN];

    if (argc > 0) {
        int type = sqlite3_value_type(argv[0]);
        if (type == SQLITE_NULL) {
            sqlite3_result_null(context);
            return;
        } else if (type != SQLITE_INTEGER) {
            sqlite3_result_error(context, "[ULID_NEW] INTEGER value expected for timestamp", -1);
            return;
        }

        timestamp = sqlite3_value_int64(argv[0]);
    } else {
#ifdef __linux__
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
            sqlite3_result_error(context, "[ULID_NEW] Internal error: failed to get current time", -1);
            return;
        }
        timestamp = (unsigned long long)ts.tv_sec * 1000 + (unsigned long long)ts.tv_nsec / (1000 * 1000);
#elif defined(_WIN32) || defined(_WIN64)
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        ULARGE_INTEGER t;
        t.HighPart = ft.dwHighDateTime;
        t.LowPart = ft.dwLowDateTime;
        timestamp = (t.QuadPart - 116444736000000000ULL) / (10 * 1000);
#else
        timestamp = (unsigned long long)time(NULL) * 1000;
#endif
    }

    if (argc > 1) {
        int type = sqlite3_value_type(argv[1]);
        if (type == SQLITE_NULL) {
            sqlite3_result_null(context);
            return;
        } else if (type != SQLITE_BLOB) {
            sqlite3_result_error(context, "[ULID_NEW] BLOB value expected for randomness", -1);
            return;
        }

        if (sqlite3_value_bytes(argv[1]) < RANDOMNESS_BYTE_LEN) {
            sqlite3_result_error(context, "[ULID_NEW] Invalid byte length for randomness", -1);
            return;
        }

        const unsigned char *value = sqlite3_value_blob(argv[1]);
        memcpy(randomness, value, RANDOMNESS_BYTE_LEN);
    } else {
#ifdef __linux__
        if (getrandom(randomness, RANDOMNESS_BYTE_LEN, GRND_RANDOM) != RANDOMNESS_BYTE_LEN) {
            sqlite3_result_error(context, "[ULID_NEW] Internal error: failed to get random bytes", -1);
            return;
        }
#elif defined(_WIN32) || defined(_WIN64)
        if (!BCRYPT_SUCCESS(BCryptGenRandom(NULL, randomness, RANDOMNESS_BYTE_LEN, BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
            sqlite3_result_error(context, "[ULID_NEW] Internal error: failed to get random bytes", -1);
            return;
        }
#else
        sqlite3_randomness(RANDOMNESS_BYTE_LEN, randomness);
#endif
    }

    unsigned char result[ULID_BYTE_LEN] = {
        (unsigned char)((timestamp >> 40) & 0xff),
        (unsigned char)((timestamp >> 32) & 0xff),
        (unsigned char)((timestamp >> 24) & 0xff),
        (unsigned char)((timestamp >> 16) & 0xff),
        (unsigned char)((timestamp >> 8) & 0xff),
        (unsigned char)((timestamp >> 0) & 0xff),
    };

    memcpy(result + TIMESTAMP_BYTE_LEN, randomness, RANDOMNESS_BYTE_LEN);

    sqlite3_result_blob(context, result, ULID_BYTE_LEN, SQLITE_TRANSIENT);
}

static void ulid_encode(sqlite3_context *context, int argc, sqlite3_value **argv) {
    if (argc == 0) {
        sqlite3_result_error(context, "[ULID_ENCODE] No argument", -1);
        return;
    }

    int type = sqlite3_value_type(argv[0]);
    if (type == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    } else if (type != SQLITE_BLOB) {
        sqlite3_result_error(context, "[ULID_ENCODE] BLOB value expected", -1);
        return;
    }

    if (sqlite3_value_bytes(argv[0]) != ULID_BYTE_LEN) {
        sqlite3_result_error(context, "[ULID_ENCODE] Invalid byte length", -1);
        return;
    }

    const unsigned char *value = sqlite3_value_blob(argv[0]);

    char result[ULID_STR_LEN] = {
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
 
    sqlite3_result_text(context, result, ULID_STR_LEN, SQLITE_TRANSIENT);
}

static void ulid_decode(sqlite3_context *context, int argc, sqlite3_value **argv) {
    if (argc == 0) {
        sqlite3_result_error(context, "[ULID_DECODE] No argument", -1);
        return;
    }

    int type = sqlite3_value_type(argv[0]);
    if (type == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    } else if (type != SQLITE_TEXT) {
        sqlite3_result_error(context, "[ULID_DECODE] TEXT value expected", -1);
        return;
    }

    if (sqlite3_value_bytes(argv[0]) != ULID_STR_LEN) {
        sqlite3_result_error(context, "[ULID_DECODE] Invalid text length", -1);
        return;
    }

    const char *value = sqlite3_value_text(argv[0]);

    unsigned char result[ULID_BYTE_LEN] = {
        // Timestamp
        ((DECODE[value[0] & 127] << 5) | DECODE[value[1] & 127]) & 0xff,
        ((DECODE[value[2] & 127] << 3) | (DECODE[value[3] & 127] >> 2)) & 0xff,
        ((DECODE[value[3] & 127] << 6) | (DECODE[value[4] & 127] << 1) | (DECODE[value[5] & 127] >> 4)) & 0xff,
        ((DECODE[value[5] & 127] << 4) | (DECODE[value[6] & 127] >> 1)) & 0xff,
        ((DECODE[value[6] & 127] << 7) | (DECODE[value[7] & 127] << 2) | (DECODE[value[8] & 127] >> 3)) & 0xff,
        ((DECODE[value[8] & 127] << 5) | (DECODE[value[9] & 127])) & 0xff,

        // Randomness
        ((DECODE[value[10] & 127] << 3) | (DECODE[value[11] & 127] >> 2)) & 0xff,
        ((DECODE[value[11] & 127] << 6) | (DECODE[value[12] & 127] << 1) | (DECODE[value[13] & 127] >> 4)) & 0xff,
        ((DECODE[value[13] & 127] << 4) | (DECODE[value[14] & 127] >> 1)) & 0xff,
        ((DECODE[value[14] & 127] << 7) | (DECODE[value[15] & 127] << 2) | (DECODE[value[16] & 127] >> 3)) & 0xff,
        ((DECODE[value[16] & 127] << 5) | (DECODE[value[17] & 127])) & 0xff,
        ((DECODE[value[18] & 127] << 3) | (DECODE[value[19] & 127] >> 2)) & 0xff,
        ((DECODE[value[19] & 127] << 6) | (DECODE[value[20] & 127] << 1) | (DECODE[value[21] & 127] >> 4)) & 0xff,
        ((DECODE[value[21] & 127] << 4) | (DECODE[value[22] & 127] >> 1)) & 0xff,
        ((DECODE[value[22] & 127] << 7) | (DECODE[value[23] & 127] << 2) | (DECODE[value[24] & 127] >> 3)) & 0xff,
        ((DECODE[value[24] & 127] << 5) | (DECODE[value[25] & 127])) & 0xff,
    };

    sqlite3_result_blob(context, result, ULID_BYTE_LEN, SQLITE_TRANSIENT);
}

static void ulid_to_timestamp(sqlite3_context *context, int argc, sqlite3_value **argv) {
    if (argc == 0) {
        sqlite3_result_error(context, "[ULID_TO_TIMESTAMP] No argument", -1);
        return;
    }

    int type = sqlite3_value_type(argv[0]);
    if (type == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    } else if (type != SQLITE_BLOB) {
        sqlite3_result_error(context, "[ULID_TO_TIMESTAMP] BLOB value expected", -1);
        return;
    }

    if (sqlite3_value_bytes(argv[0]) != ULID_BYTE_LEN) {
        sqlite3_result_error(context, "[ULID_TO_TIMESTAMP] Invalid byte length", -1);
        return;
    }

    const unsigned char *value = sqlite3_value_blob(argv[0]);

    unsigned long long result = 0;

    result += ((unsigned long long)value[0]) << 40;
    result += ((unsigned long long)value[1]) << 32;
    result += ((unsigned long long)value[2]) << 24;
    result += ((unsigned long long)value[3]) << 16;
    result += ((unsigned long long)value[4]) << 8;
    result += ((unsigned long long)value[5]) << 0;

    sqlite3_result_int64(context, result);
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

    res = sqlite3_create_function(db, "ulid_to_timestamp", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, NULL, ulid_to_timestamp, NULL, NULL);
    if (res != SQLITE_OK) {
        *pzErrMsg = sqlite3_mprintf("Can't create ulid_to_timestamp function.");
        return res;
    }

    return SQLITE_OK;
}
