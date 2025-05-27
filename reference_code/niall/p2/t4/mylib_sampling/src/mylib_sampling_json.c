#include "mylib_sampling_json.h"
#include "mylib_sampling_thread.h"

#include <mylib_shell_globalsh.h>
#include <mylib_clock_rtcc.h>
#include <mylib_sensors_bmp280.h>
#include <mylib_sensors_si1133.h>
#include <mylib_sensors_si7021.h>

#include <stdio.h>
#include <string.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/data/json.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(jsonifying);

#define JSON_BUFF_SIZE 1024
static char json_buffer[JSON_BUFF_SIZE];
// This needs a getter and setter with mutex locks but I dont have time rip... im not using it any tho lol.


// Defines describing structure of JSON format through object construction.
// Then you just have to populate the structs without memeory errors!
struct reading {
    const char* type;
    char* value;
    const char* units;
};

static const struct json_obj_descr reading_descr[] = {
    JSON_OBJ_DESCR_PRIM(struct reading, type, JSON_TOK_STRING),
    JSON_OBJ_DESCR_PRIM(struct reading, value, JSON_TOK_STRING),
    JSON_OBJ_DESCR_PRIM(struct reading, units, JSON_TOK_STRING),
};

struct sample {
    int did;
    struct reading reading;
};

static const struct json_obj_descr sample_descr[] = {
    JSON_OBJ_DESCR_PRIM(struct sample, did, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_OBJECT(struct sample, reading, reading_descr),
};

struct samples {
    char* time;
    struct sample sample[6];
    size_t sample_len;
};

static const struct json_obj_descr samples_descr[] = {
    JSON_OBJ_DESCR_PRIM(struct samples, time, JSON_TOK_STRING),
    JSON_OBJ_DESCR_OBJ_ARRAY_NAMED(
        struct samples,
        "samples",
        sample,
        6,
        sample_len,
        sample_descr,
        ARRAY_SIZE(sample_descr))
};

// Make the required device information and values and store them in the required structs.
static void make_reading(struct reading* out, struct sensor_q31_data data, int did) 
{
    // Select the sensor type based on the DID value.
    q31_t reading = 0;
    switch (did) {
        case TEMP:
            reading = data.readings[0].temperature;
            out->type = "temperature";
            out->units = "celsius";
            break;
        case HUME:
            reading = data.readings[0].humidity;
            out->type = "humidity";
            out->units = "%";
            break;
        case PRES:
            reading = data.readings[0].pressure;
            out->type = "pressure";
            out->units = "kPa";
            break;
        case LITE:
            reading = data.readings[0].light;
            out->type = "light";
            out->units = "lux";
            break;
    }

    // Convert from q31 values to integer and fraction unsigned integers.
    uint32_t integer = __PRIq_arg_get_int(reading, data.shift);
    uint32_t fraction = __PRIq_arg_get_frac(reading, 2, data.shift);

    // Make some string space and write value as a string so it can be in decimal form. 
    // Because floats dont work :(.
    out->value = k_malloc(16); // MALLOC
    if (out->value == NULL) { LOG_ERR("Malloc has failed us."); }
    snprintf(out->value, 16, "%u.%02u", integer, fraction);
}

// Each actively sampling sensor has its DID and info/values formatted.
static void make_sample(struct sample* out, struct sensor_q31_data data, int did)
{
    out->did = did;
    make_reading(&out->reading, data, did);
}

// The shell was cutting off the JSON format and it was all bunched up.
// So these make it easily readable.
void print_json_pretty(const char *json_buffer, int length) {
    printk("\n___________________ HI JSON ___________________\n");
    int printed_upto = 0;
    for (int i = 0; i < length - 1; i++) {
        if (
            ((json_buffer[i] == '}') && (json_buffer[i + 1] == ','))
            || ((json_buffer[i-1] == ':') && (json_buffer[i] == '['))
        ) {
            int chars_to_print = (i + 1) - printed_upto; 
            // Prints from printed_upto for chars_to_print.
            printk("%.*s\n", chars_to_print, &json_buffer[printed_upto]);
            printed_upto = i + 1;
        }
    }
    if (printed_upto < length) { // Reamining.
        printk("%.*s\n", length - printed_upto, &json_buffer[printed_upto]);
    }
    printk("------------------- BYE JSON ------------------\n\n");
}

void print_json_pretty_alt(const char *json_buffer, int length) {
    printk("\n___________________ HI JSON ___________________\n");
    int printed_upto = 0;
    for (int i = 0; i < length; i++) {
        if (json_buffer[i] == '{') {
            int chars_to_print = (i + 1) - printed_upto;
            printk("%.*s\n", chars_to_print, &json_buffer[printed_upto]);
            printed_upto = i + 1;
        }
    }
    if (printed_upto < length) {
        printk("%.*s\n", length - printed_upto, &json_buffer[printed_upto]);
    }
    printk("------------------- BYE JSON ------------------\n\n");
}

// Returns the final formatted JSON string.
void jsonify_samples(struct rtc_time* rtc_time_ptr, 
    struct sensor_q31_data *data_array, int *did_array, size_t count) 
{
    struct samples samples;

    // Moved the time up a level so now it just shows at the start of each sample set
    // Instead of for every sensor (because they all measure at the ~same time).
    samples.time = k_malloc(TIMESTRING_LENGTH); // MALLOC
    if (samples.time == NULL) { LOG_ERR("Malloc has failed us."); }
    rtcc_timestring_rtc_time(samples.time, rtc_time_ptr);

    // Use the input array to make the structs for each sensor.
    for (int i = 0; i < count; i++) {
        struct sample sample;
        make_sample(&sample, data_array[i], did_array[i]);
        samples.sample[i] = sample;
    }
    // Zephyr API said I needed this in a struct wrapper for my array.
    // Although I have now added time to the struct wrapper aswell.
    samples.sample_len = count;

    // Making the zephyr string global to allow access to most recent saple data from
    // other applications.
    //size_t json_buff_size = 1024;
    //uint8_t json_buffer[json_buff_size];
    
    
    // Encode the described and populated build into a buffer string. 
    json_obj_encode_buf(
        samples_descr,
        ARRAY_SIZE(samples_descr),
        &samples,
        json_buffer,
        JSON_BUFF_SIZE);
    // Have a look.
    print_json_pretty(json_buffer, strlen(json_buffer));
    //print_json_pretty_alt(json_buffer, strlen(json_buffer));

    // #FreeMyBoyMalloc (he does good work)
    k_free(samples.time); // Only one to free now.
    for (int i = 0; i < count; i++) {
        k_free(samples.sample[i].reading.value);
    }

    // Return the string to the main thread execution for use?
}