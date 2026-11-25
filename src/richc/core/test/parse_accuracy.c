// parse_accuracy.c
//
// Checks rc_str_parse_f32 and _f64 against glibc's strtof and strtod, which round correctly, bit
// for bit, including where each stops reading and which values overflow. It takes minutes, so it
// is a tool to run after changing the parser, not one of the tests that run after every build:
//
//   richc_parse_accuracy [f32 | f64] [samples]
//
// f32 tries every float, printed as %.9g (which reads back exactly), %.6g and %a. f64 tries random
// samples: doubles printed as %.17g, with fewer digits, and as %a; random digit strings, short and
// up to 1100 digits long, with exponents across the range and past it; and the exact halfway point
// between neighbouring doubles and floats, where rounding is hardest.
//
// Native and POSIX only: it leans on libc for the references and pthreads to use every core.

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "richc/str.h"

enum {
    THREAD_COUNT = 16,
    DEFAULT_SAMPLES = 10000000,
    MAX_TEXT = 4096,
    MAX_REPORTS = 10
};

// What a thread checked and found.
typedef struct parse_job {
    uint64_t first;
    uint64_t count;
    uint64_t checked;
    uint64_t failures;
    bool all_floats;
} parse_job;

// Failures are reported by several threads at once
static pthread_mutex_t parse_report_lock = PTHREAD_MUTEX_INITIALIZER;
static uint32_t parse_reports;


// The next of a splitmix64 sequence.
static uint64_t parse_random(uint64_t *state)
{
    uint64_t z = (*state += 0x9e3779b97f4a7c15ull);

    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}


// Print a failure, if not too many have been already.
static void parse_report(const char *type, const char *text, double expected, double got, uint8_t error)
{
    pthread_mutex_lock(&parse_report_lock);
    if (parse_reports < MAX_REPORTS) {
        printf("%s %.80s: expected %a, got %a, error %u\n", type, text, expected, got, error);
    }
    parse_reports++;
    pthread_mutex_unlock(&parse_report_lock);
}


// True if text, parsed as a double, matches strtod: the same bits, the same text read, and
// RC_PARSE_ERROR_RANGE where strtod overflows to infinity from finite digits.
static bool parse_matches_f64(const char *text)
{
    char *end = NULL;
    double expected = strtod(text, &end);
    rc_parsed_f64 parsed = rc_str_parse_f64(rc_str_from_cstr(text), 0);
    bool overflowed = isinf(expected) && strchr(text, 'n') == NULL && strchr(text, 'N') == NULL;
    uint64_t expected_bits = 0;
    uint64_t got_bits = 0;

    if (overflowed) {
        return parsed.error == RC_PARSE_ERROR_RANGE;
    }

    if (isnan(expected)) {
        return isnan(parsed.value) && parsed.rest.data == end;
    }

    memcpy(&expected_bits, &expected, sizeof expected);
    memcpy(&got_bits, &parsed.value, sizeof parsed.value);
    return parsed.error == RC_PARSE_ERROR_NONE && got_bits == expected_bits && parsed.rest.data == end;
}


// The same for a float, against strtof.
static bool parse_matches_f32(const char *text)
{
    char *end = NULL;
    float expected = strtof(text, &end);
    rc_parsed_f32 parsed = rc_str_parse_f32(rc_str_from_cstr(text), 0);
    bool overflowed = isinf(expected) && strchr(text, 'n') == NULL && strchr(text, 'N') == NULL;
    uint32_t expected_bits = 0;
    uint32_t got_bits = 0;

    if (overflowed) {
        return parsed.error == RC_PARSE_ERROR_RANGE;
    }

    if (isnan(expected)) {
        return isnan(parsed.value) && parsed.rest.data == end;
    }

    memcpy(&expected_bits, &expected, sizeof expected);
    memcpy(&got_bits, &parsed.value, sizeof parsed.value);
    return parsed.error == RC_PARSE_ERROR_NONE && got_bits == expected_bits && parsed.rest.data == end;
}


// Check text as a double, counting it in job.
static void parse_check_f64(parse_job *job, const char *text)
{
    rc_parsed_f64 parsed = rc_str_parse_f64(rc_str_from_cstr(text), 0);

    job->checked++;
    if (!parse_matches_f64(text)) {
        job->failures++;
        parse_report("f64", text, strtod(text, NULL), parsed.value, parsed.error);
    }
}


// Check text as a float, counting it in job.
static void parse_check_f32(parse_job *job, const char *text)
{
    rc_parsed_f32 parsed = rc_str_parse_f32(rc_str_from_cstr(text), 0);

    job->checked++;
    if (!parse_matches_f32(text)) {
        job->failures++;
        parse_report("f32", text, (double)strtof(text, NULL), (double)parsed.value, parsed.error);
    }
}


// Random decimal text: a sign, digits digits with a point somewhere among them, and an exponent.
static void parse_random_digits(uint64_t *state, char *text, uint32_t digits, int32_t exponent)
{
    uint32_t len = 0;
    uint32_t point = (uint32_t)(parse_random(state) % (digits + 1));

    if (parse_random(state) & 1) {
        text[len++] = '-';
    }

    for (uint32_t i = 0; i < digits; i++) {
        if (i == point && i > 0) {
            text[len++] = '.';
        }
        text[len++] = (char)('0' + parse_random(state) % 10);
    }
    snprintf(text + len, MAX_TEXT - len, "e%d", exponent);
}


// Every float in the job's range, printed three ways.
static void parse_run_all_floats(parse_job *job)
{
    char text[MAX_TEXT] = {0};

    for (uint64_t i = job->first; i < job->first + job->count; i++) {
        uint32_t bits = (uint32_t)i;
        float value = 0.0f;

        memcpy(&value, &bits, sizeof value);
        if (isnan(value)) {
            continue;
        }

        snprintf(text, sizeof text, "%.9g", (double)value);
        parse_check_f32(job, text);
        snprintf(text, sizeof text, "%.6g", (double)value);
        parse_check_f32(job, text);
        snprintf(text, sizeof text, "%a", (double)value);
        parse_check_f32(job, text);
    }
}


// Random samples of every kind, as doubles and floats.
static void parse_run_samples(parse_job *job)
{
    uint64_t state = job->first * 0x9e3779b97f4a7c15ull + 1;
    char text[MAX_TEXT] = {0};

    for (uint64_t i = 0; i < job->count; i++) {
        uint64_t bits = parse_random(&state);
        double value = 0.0;
        double low = 0.0;
        float low_f32 = 0.0f;
        uint32_t bits_f32 = (uint32_t)parse_random(&state) & 0x7f7fffffu;

        memcpy(&value, &bits, sizeof value);
        if (!isnan(value)) {
            snprintf(text, sizeof text, "%.17g", value);
            parse_check_f64(job, text);
            snprintf(text, sizeof text, "%.*g", 1 + (int)(parse_random(&state) % 16), value);
            parse_check_f64(job, text);
            snprintf(text, sizeof text, "%a", value);
            parse_check_f64(job, text);
        }

        // Short and long digit strings, with exponents past both ends of the range
        parse_random_digits(&state, text, 1 + (uint32_t)(parse_random(&state) % 40), (int32_t)(parse_random(&state) % 800) - 400);
        parse_check_f64(job, text);
        parse_check_f32(job, text);
        if (i % 64 == 0) {
            parse_random_digits(&state, text, 700 + (uint32_t)(parse_random(&state) % 400), (int32_t)(parse_random(&state) % 1500) - 1150);
            parse_check_f64(job, text);
            parse_check_f32(job, text);
        }

        // The exact halfway point between neighbours: in long double for doubles, which holds it
        bits &= 0x7fefffffffffffffull;
        memcpy(&low, &bits, sizeof low);
        snprintf(text, sizeof text, "%.800Lg", ((long double)low + (long double)nextafter(low, (double)INFINITY)) / 2);
        parse_check_f64(job, text);
        memcpy(&low_f32, &bits_f32, sizeof low_f32);
        snprintf(text, sizeof text, "%.200g", ((double)low_f32 + (double)nextafterf(low_f32, INFINITY)) / 2);
        parse_check_f32(job, text);
    }
}


static void *parse_thread(void *context)
{
    parse_job *job = context;

    if (job->all_floats) {
        parse_run_all_floats(job);
    }
    else {
        parse_run_samples(job);
    }
    return NULL;
}


int main(int argc, char **argv)
{
    bool all_floats = argc > 1 && strcmp(argv[1], "f32") == 0;
    uint64_t samples = argc > 2 ? strtoull(argv[2], NULL, 10) : DEFAULT_SAMPLES;
    uint64_t total = all_floats ? (UINT64_C(1) << 32) : samples;
    parse_job jobs[THREAD_COUNT] = {0};
    pthread_t threads[THREAD_COUNT] = {0};
    uint64_t checked = 0;
    uint64_t failures = 0;

    for (uint32_t i = 0; i < THREAD_COUNT; i++) {
        uint64_t first = total / THREAD_COUNT * i;
        uint64_t end = i + 1 == THREAD_COUNT ? total : total / THREAD_COUNT * (i + 1);

        jobs[i] = (parse_job) {.first = first, .count = end - first, .all_floats = all_floats};
        pthread_create(&threads[i], NULL, parse_thread, &jobs[i]);
    }

    for (uint32_t i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
        checked += jobs[i].checked;
        failures += jobs[i].failures;
    }

    printf("%s: %llu texts checked, %llu differ from %s\n", all_floats ? "every float" : "samples", (unsigned long long)checked,
           (unsigned long long)failures, all_floats ? "strtof" : "strtod and strtof");
    return failures == 0 ? 0 : 1;
}
