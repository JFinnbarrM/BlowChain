
/*
References:
1:  zephyr/tests/subsys/random/rng/src/main.c
2:  https://docs.zephyrproject.org/latest/kernel/services/index.html
3:  https://docs.zephyrproject.org/latest/doxygen/html/group__msgq__apis.html#ga54a5cdcaea2236c383ace433fedc0d39
4:  zephyr/doc/kernel/services/data_passing/message_queues.rst
5:  https://docs.zephyrproject.org/latest/kernel/services/threads/index.html
6:  https://docs.zephyrproject.org/latest/doxygen/html/group__random__api.html#details
7:  https://docs.zephyrproject.org/latest/services/crypto/random/index.html
*/

#include <sys/_intsup.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/printk.h>
#include <stdbool.h>
#include <stdint.h>

#include "headers.h"
#include "zephyr/kernel.h"

/*
Declare a message passing primitive to communicate information between threads.
*/
K_MSGQ_DEFINE(          // Compile time definition of a msgq which is used threads to share fix sized data items.
    rng_msgq,           // Creates the k_msgq struct with this name.
    sizeof(uint32_t),   // The message queue is storing unsigned 32 bit integers.
    1,                  // Only need 1 random number to be stored at a time.
    4                   // 32 bit integers use 4 bytes, this helps to efficiently align the data in memory.
);                      // (2)(3)(4)

/*
Declare a thread for generation of 8 digit random numbers.
*/
K_THREAD_DEFINE(            // Static threads are simpler to declare at compile time.
    rng_thread_id,          // Set a variable to store this threads ID in.
    1024,                   // This thread uses a memory stack allocation of 1KB to manage itself.
    rng_thread_function,    // Entry point is the rng_thread_function.
    NULL, NULL, NULL,       // This threads function does not need any parameter values.
    1,                      // Give this thread execution the HIGHER priority.
    0,                      // No special options are required.
    2000                    // Start this thread after 2 seconds.
);                          // (5)

/*
Declare a thread for display of the 8 digit random numbers.
*/
K_THREAD_DEFINE(            // Static threads are simpler to declare at compile time.
    display_thread_id,      // Set a variable to store this threads ID in.
    1024,                   // This thread uses a memory stack allocation of 1KB to manage itself.
    display_thread_function,// Entry point is the dsiplay_thread_function.
    NULL, NULL, NULL,       // This threads function does not need any parameter values.
    2,                      // Give this thread execution the LOWER priority.
    0,                      // No special options are required.
    2000                    // Start this thread after 2 seconds.
);                          // (5)

/*
Entry point of the program.
- Assigns thread names.
- Logs program initialisation. 
*/
int main(void)
{
    // Assign thread names for identification with thread_analyzer.
    k_thread_name_set(rng_thread_id, "RNG-Thread");
    k_thread_name_set(display_thread_id, "Display-Thread");

    // Log that the program has started.
    printk("\n\nITS ENTROPYING TIME !!!\n\n");

    return 0; // Return from the main function and allow the 
}

/*
Function run by the RNG thread.
- Generates an 8 digit random number.
- Adds the random number to the message queue.
- Checks that the random number was added succsessfully.
- Sleeps for two seconds before repeating.
*/
void rng_thread_function(void)
{   
    while (true)
    {
        // Get the random 8 digit number.
        uint32_t randint_8digit = get_randint_8digit();

        // Put the random number into the shared primitive message queue.
        int result = k_msgq_put    // Store the result for error checking.
        (
            &rng_msgq,        // Memory reference to the target message queue.  
            &randint_8digit,  // Memory reference to the target variable holding the message.
            K_MSEC(100)    // Keep trying for 100ms (While there is no space to put).
        );                         // (2)(3)(4)

        if (result != 0) { // The message was not successfully added to the message queue.
            printk("RNG THREAD FAILED"); // Then log a warning.
        }

        k_sleep(K_MSEC(2000)); // Wait 2 seconds before repeating.
    }
}

/*
Function run by the display thread.
- Retrieves (and removes) the random number from the shared message queue.
- Checks that a number was successfully retrieved.
- Logs the number to the terminal.
- Sleeps for two seconds before repeating.
*/
void display_thread_function(void)
{
    while (true)
    {
        uint32_t randint_8digit; // Initialise a variable to store the 8 digit random integer in.

        // Take the random number out of the shared primitive message queue.
        int result = k_msgq_get     // Store the result for error checking.
        (
            &rng_msgq,         // Memory reference to the target messsage queue.
            &randint_8digit,   // Memory reference to the target variable to hold the data.
            K_MSEC(100)     // Keep trying forever 100ms (While there is no message to get).
        );                          // (2)(3)(4)

        if (result != 0) { // The message was not successfully recieved and removed from the message queue.
            printk("DISPLAY THREAD FAILED"); // Then log a warning.
        } 
        else { // The 8 digit random number was successfully retrieved.
            // Display the number with '0' extensions to ensure 8 digits.
            printk("The random number is... %08u\n", randint_8digit); 
        }

        k_sleep(K_MSEC(2000)); // Wait 2 seconds before repeating.
    }
}

/*
Generates and returns a random 8 digit integer.
*/
int get_randint_8digit(void)
{
    // Randomly generate a 32 bit integer (8 digit numbers require 27 bits).
    uint32_t randint_32bit = sys_rand32_get(); //(1)(6)(6)
    // Because the entropy device random generator device is enabled in prj.conf this will use it.
    // # CONFIG_TEST_RANDOM_GENERATOR is not set and so the system clock will not be used. (7)

    // Restrict the 32 bit number to a maximum of 8 display digits.
    uint32_t randint_8digit = randint_32bit % 100000000;

    // Return the 8 digit random number.
    return randint_8digit;
}