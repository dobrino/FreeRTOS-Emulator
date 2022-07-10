#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>

#include <SDL2/SDL_scancode.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "TUM_Ball.h"
#include "TUM_Draw.h"
#include "TUM_Event.h"
#include "TUM_Sound.h"
#include "TUM_Utils.h"
#include "TUM_Font.h"

#include "AsyncIO.h"

#define mainGENERIC_PRIORITY (tskIDLE_PRIORITY)
#define mainGENERIC_STACK_SIZE ((unsigned short)2560)
#define spaceship_FILEPATH "resources/images/spaceship.PNG"

// Task Hanles
static TaskHandle_t DemoTask = NULL;
static TaskHandle_t ControlTask = NULL;

// Score Board
static int score = 0;
static int lives = 3;
static image_handle_t life_img = NULL;

// Space ship
static image_handle_t spaceship_img = NULL;
static coord_t spaceship_coord;
// Bullet
static char bullet_active = NULL;
static coord_t bullet_coord;

typedef struct buttons_buffer {
    unsigned char buttons[SDL_NUM_SCANCODES];
    SemaphoreHandle_t lock;
} buttons_buffer_t;

static buttons_buffer_t buttons = { 0 };

void xGetButtonInput(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        xQueueReceive(buttonInputQueue, &buttons.buttons, 0);
        xSemaphoreGive(buttons.lock);
    }
}

#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR

void vShootBullet(){
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(
                                    S)]) { //SPACE for Shoot
                bullet_coord.y = spaceship_coord.y + 10; //offset to th front of the spacehsip
                bullet_coord.x = spaceship_coord.x + 18; //offset to the front of the spaceship
                bullet_active = 1;
                printf("Bullet shot\n");
            }
            xSemaphoreGive(buttons.lock);
        }
    if(bullet_coord.y > 0){
        bullet_coord.y = bullet_coord.y - 5;
    }
    else{
       bullet_active = NULL;
    }
}

void  vControlTask(){
    
    
    spaceship_coord.x = SCREEN_WIDTH/2;
    spaceship_coord.y = SCREEN_HEIGHT - 100;
    
    while(1){

        // Exit Mode
        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(
                                    Q)]) { // Equiv to SDL_SCANCODE_Q
                exit(EXIT_SUCCESS);
            }
            xSemaphoreGive(buttons.lock);
        }

        // Spaceship Control
        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(
                                    A)]) { // A for steering to the left
                if(spaceship_coord.x > 0)    
                    spaceship_coord.x = spaceship_coord.x - 2;
            }
            xSemaphoreGive(buttons.lock);
        }

        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(
                                    D)]) { // D for steering to the right
                if(spaceship_coord.x < SCREEN_WIDTH - 40)
                    spaceship_coord.x = spaceship_coord.x + 2;
            }
            xSemaphoreGive(buttons.lock);
        }
        // shooting 
        vShootBullet();

        vTaskDelay(20);
    }
}

void vDrawScore(){
    char str[15];
    sprintf(str, "%03d", score);
    tumDrawText(str, 10,10,0xFF0000);
}

void vDrawLives(){
    char str[3];
    sprintf(str,"%d", lives); 
    tumDrawText(str, 10,SCREEN_HEIGHT-20,0xFF0000); //drawing the number of lives left

    //drawing spaceshipbsto visuliz the number of lives left 
    for(int i = 0; i < lives; i++){
        tumDrawLoadedImage(life_img,30 + i*30,SCREEN_HEIGHT-20);
    } 

}

void vDrawStatcItems(){
    vDrawScore();
    vDrawLives();

    // Draw bottom cave
    tumDrawLine(0,SCREEN_HEIGHT-30,SCREEN_WIDTH,SCREEN_HEIGHT-30,2,0x00FF00);
}

void vDrawSpaceship(){
    tumDrawLoadedImage(spaceship_img,spaceship_coord.x,spaceship_coord.y);
}

void vDrawBullet(){
    tumDrawLine(bullet_coord.x,bullet_coord.y,bullet_coord.x,bullet_coord.y - 5,1, 0x0000FF);
    printf("shooting\n");
}

void vDrawObjects(){
        vDrawSpaceship();
        if(bullet_active){
            vDrawBullet();
        }
}





void vDrawTask(void *pvParameters)
{
    // structure to store time retrieved from Linux kernel
    static struct timespec the_time;
    static char our_time_string[100];
    static int our_time_strings_width = 0;

    // Needed such that Gfx library knows which thread controlls drawing
    // Only one thread can call tumDrawUpdateScreen while and thread can call
    // the drawing functions to draw objects. This is a limitation of the SDL
    // backend.
    tumDrawBindThread();

    // Load Images
    spaceship_img = tumDrawLoadImage(spaceship_FILEPATH);
    tumDrawSetLoadedImageScale(spaceship_img, 0.1);

    life_img = tumDrawLoadImage(spaceship_FILEPATH);
    tumDrawSetLoadedImageScale(life_img, 0.05);

    while (1) {
        tumEventFetchEvents(FETCH_EVENT_NONBLOCK); // Query events backend for new events, ie. button presses
        xGetButtonInput(); // Update global input
        
        //Draw Static Items (Background and Scoreboard)
        tumDrawClear(0x000000); // Clear screen
        vDrawStatcItems();

        //Draw Moving Objects (Monsters Bullet)
        vDrawObjects();

        tumDrawUpdateScreen(); // Refresh the screen to draw string

        // Basic sleep of 1000 milliseconds
        vTaskDelay(20);
    }
}

int main(int argc, char *argv[])
{
    char *bin_folder_path = tumUtilGetBinFolderPath(argv[0]);

    printf("Initializing: ");

    if (tumDrawInit(bin_folder_path)) {
        PRINT_ERROR("Failed to initialize drawing");
        goto err_init_drawing;
    }

    if (tumEventInit()) {
        PRINT_ERROR("Failed to initialize events");
        goto err_init_events;
    }

    if (tumSoundInit(bin_folder_path)) {
        PRINT_ERROR("Failed to initialize audio");
        goto err_init_audio;
    }

    buttons.lock = xSemaphoreCreateMutex(); // Locking mechanism
    if (!buttons.lock) {
        PRINT_ERROR("Failed to create buttons lock");
        goto err_buttons_lock;
    }

    if (xTaskCreate(vDrawTask, "DemoTask", mainGENERIC_STACK_SIZE * 2, NULL,
                    mainGENERIC_PRIORITY, &DemoTask) != pdPASS) {
        goto err_demotask;
    }

    if (xTaskCreate(vControlTask, "ControlTask", mainGENERIC_STACK_SIZE * 2, NULL,
                    mainGENERIC_PRIORITY, &ControlTask) != pdPASS) {
        goto err_controltask;
    }

    vTaskStartScheduler();

    return EXIT_SUCCESS;

err_demotask:
    vSemaphoreDelete(buttons.lock);
err_controltask:
    vSemaphoreDelete(buttons.lock);   
err_buttons_lock:
    tumSoundExit();
err_bullet_lock:
    tumSoundExit();
err_init_audio:
    tumEventExit();
err_init_events:
    tumDrawExit();
err_init_drawing:
    return EXIT_FAILURE;
}

// cppcheck-suppress unusedFunction
__attribute__((unused)) void vMainQueueSendPassed(void)
{
    /* This is just an example implementation of the "queue send" trace hook. */
}

// cppcheck-suppress unusedFunction
__attribute__((unused)) void vApplicationIdleHook(void)
{
#ifdef __GCC_POSIX__
    struct timespec xTimeToSleep, xTimeSlept;
    /* Makes the process more agreeable when using the Posix simulator. */
    xTimeToSleep.tv_sec = 1;
    xTimeToSleep.tv_nsec = 0;
    nanosleep(&xTimeToSleep, &xTimeSlept);
#endif
}
