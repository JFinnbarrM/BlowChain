
#include <stdbool.h>

// Fetch the node identifiers from /aliases.
#define RC DT_ALIAS(r_channel)
#define GC DT_ALIAS(g_channel)
#define BC DT_ALIAS(b_channel)
#define PWM DT_ALIAS(rgb_pwm)

// For code readability.
#define ON  true
#define OFF false

// Allows for simplified and more readable code in the set_colour_to function.
typedef struct {
    bool red;
    bool green;
    bool blue;
} rgb_state;

// Provides an interface for readable message interpretation between threads.
// Also defines the desired order of colours.
typedef enum { // Assigns integer values to symbolic names.
    BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, YELLOW, WHITE, NUM_COLOURS
    //0    //1   //2    //3   //4  //5      //6     //7    //8
} rgb_colour;

// Functions.
void configure_pins(void);
void set_pwm_pin(void);
void set_colour_to(rgb_colour);
void rgb_led_tfun(void);
void control_tfun(void);

