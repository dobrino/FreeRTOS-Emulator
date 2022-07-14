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
#define alien1_1_FILEPATH "resources/images/alien1_1.png"
#define alien1_2_FILEPATH "resources/images/alien1_2.png"
#define alien2_1_FILEPATH "resources/images/alien2_1.png"
#define alien2_2_FILEPATH "resources/images/alien2_2.png"
#define alien3_1_FILEPATH "resources/images/alien3_1.png"
#define alien3_2_FILEPATH "resources/images/alien3_2.png"
#define explosion_1_FILEPATH "resources/images/explosion1.png"
#define explosion_2_FILEPATH "resources/images/explosion2.png"

// Task Hanles
static TaskHandle_t DemoTask = NULL;
static TaskHandle_t ControlTask = NULL;
static TaskHandle_t AlienControlTask = NULL;

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

// Aliens 
struct alien{
    coord_t coord;
    int type; //1,2,3 corellating with the loaded images
    int alive; //1= alive, 2= hit phase 1, 3= hit phase 2, 0 = dead
    int frame; //selecting frame for aliens, 2 available 
};

static struct alien aliens[5][8];
static SemaphoreHandle_t alien_lock;

static coord_t alien_offset;
static char direction = 1;
static int alien_speed = 1; 
static image_handle_t explosion_img_1 = NULL;
static image_handle_t explosion_img_2 = NULL;
static image_handle_t alien_img[3][2];



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

void vCheckHit(){
    for(int row = 0; row < 5; row++){
        for(int col = 0; col < 8; col++){
            if((abs(bullet_coord.x - aliens[row][col].coord.x - 11) <= 11) && aliens[row][col].alive){
                if(abs(bullet_coord.y - aliens[row][col].coord.y) == 0){
                    aliens[row][col].alive = 2; 
                    score += 20;
                    alien_speed += 1;
                    printf("hit detected with bullet coord: x: %d y: %d\n",bullet_coord.x,bullet_coord.y);
                    bullet_active = NULL;
                    printf("%d = %d?\n", aliens[row][col].coord.x,bullet_coord.x);
                    bullet_coord.x = spaceship_coord.x;
                    bullet_coord.y = spaceship_coord.y;
                }
            }
        }
    }
}



void vShootBullet(){
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(
                                    S)] && !bullet_active){ //S for Shoot
                bullet_coord.y = spaceship_coord.y + 10; //offset to th front of the spacehsip
                bullet_coord.x = spaceship_coord.x + 18; //offset to the front of the spaceship
                bullet_active = 1;
            }
            xSemaphoreGive(buttons.lock);
        }
    if(bullet_coord.y > 0 && bullet_active){
        bullet_coord.y = bullet_coord.y - 5;
    }
    else{
    bullet_active = NULL;
    }

    vCheckHit();
}

void vKillAliens(){
    for(int row = 0; row < 5; row++){
        for(int col = 0; col < 8; col++){
           if(aliens[row][col].alive == 2){
               aliens[row][col].alive = 3;
               //break;
           }
           else if(aliens[row][col].alive == 3){
               aliens[row][col].alive = 0;
               //break;
           }
        }
    }
}



void vAlienControlTask(){


    alien_offset.x = 50;
    alien_offset.y = 50;

    //waking up the aliens
    for(int row = 0; row < 5; row++){
        for(int col = 0; col < 8; col++){
            aliens[row][col].alive = 1;
            aliens[row][col].frame = 2;
            switch(row){
                case 0: aliens[row][col].type = 1;break;

                case 1: aliens[row][col].type = 2;break;

                case 2: aliens[row][col].type = 2;break;

                case 3: aliens[row][col].type = 3;break;

                case 4: aliens[row][col].type = 3;break;
            }
        }
    }
    xSemaphoreGive(alien_lock);

    while(1){
        if (xSemaphoreTake(alien_lock, 0) == pdTRUE){
            //Motion
            if(direction){//moving to the right
                alien_offset.x = alien_offset.x + alien_speed;
                if(aliens[0][7].coord.x >= SCREEN_WIDTH-20){//hitting right wall
                    alien_offset.y = alien_offset.y+15; 
                    direction = NULL;
                }
            }   
            if(!direction){//moving to the left 
                alien_offset.x = alien_offset.x - alien_speed;
                if(alien_offset.x <= 0){//hitting left wall
                    alien_offset.y = alien_offset.y+15;
                    direction = 1;
                }
            }


            vKillAliens();
            xSemaphoreGive(alien_lock);
        }

        vTaskDelay(200);
    }        
}

void  vControlTask(){
    
    
    spaceship_coord.x = SCREEN_WIDTH/2;
    spaceship_coord.y = SCREEN_HEIGHT - 100;

    bullet_coord.x = spaceship_coord.x;
    bullet_coord.y = spaceship_coord.y;

    printf("Screen widt:%d\n", SCREEN_WIDTH);
    
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
                    spaceship_coord.x = spaceship_coord.x - 5;
            }
            xSemaphoreGive(buttons.lock);
        }

        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(
                                    D)]) { // D for steering to the right
                if(spaceship_coord.x < SCREEN_WIDTH - 40)
                    spaceship_coord.x = spaceship_coord.x + 5;
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
    sprintf(str, "%04d", score);
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

void vDrawBarriers(){

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
}

void vDrawAliens(){
    if (xSemaphoreTake(alien_lock, 0) == pdTRUE){
        for(int row = 0; row < 5; row++){
            for(int col = 0; col < 8; col++){

                switch(aliens[row][col].alive){
                    
                    //alien alive
                    case 1: aliens[row][col].coord.x = alien_offset.x + col*50; 
                            aliens[row][col].coord.y = alien_offset.y + row*50;
                            tumDrawLoadedImage(alien_img[aliens[row][col].type][1],aliens[row][col].coord.x, aliens[row][col].coord.y);
                            break;
                    //alien explosion phase 1
                    case 2: tumDrawLoadedImage(explosion_img_1,aliens[row][col].coord.x, aliens[row][col].coord.y);
                            break;
                    //alien explosion phase 2
                    case 3: tumDrawLoadedImage(explosion_img_2,aliens[row][col].coord.x, aliens[row][col].coord.y);
                            break;
                }
            }
        }
        tumDrawLine(aliens[4][7].coord.x,aliens[4][7].coord.y,aliens[4][7].coord.x+ 22,aliens[4][7].coord.y,5,0x0000FF);
        xSemaphoreGive(alien_lock);
    }
}

void vDrawObjects(){
        vDrawSpaceship();
        if(bullet_active){
            vDrawBullet();
        }
        vDrawAliens();
}

void vLoadImages(){
    // Load Images
    spaceship_img = tumDrawLoadImage(spaceship_FILEPATH);
    tumDrawSetLoadedImageScale(spaceship_img, 0.1);

    life_img = tumDrawLoadImage(spaceship_FILEPATH);
    tumDrawSetLoadedImageScale(life_img, 0.05);
  
    alien_img[1][1] = tumDrawLoadImage(alien1_1_FILEPATH);
    tumDrawSetLoadedImageScale(alien_img[1][1], 0.25);
    
    alien_img[1][2] = tumDrawLoadImage(alien1_2_FILEPATH);
    tumDrawSetLoadedImageScale(alien_img[1][2], 0.25);
    
    alien_img[2][1] = tumDrawLoadImage(alien2_1_FILEPATH);
    tumDrawSetLoadedImageScale(alien_img[2][1], 0.25);
    
    alien_img[2][2] = tumDrawLoadImage(alien2_2_FILEPATH);
    tumDrawSetLoadedImageScale(alien_img[2][2], 0.25);

    alien_img[3][1] = tumDrawLoadImage(alien3_1_FILEPATH);
    tumDrawSetLoadedImageScale(alien_img[3][1], 0.25);

     alien_img[3][2] = tumDrawLoadImage(alien3_2_FILEPATH);
    tumDrawSetLoadedImageScale(alien_img[3][2], 0.25);
    

    explosion_img_1 = tumDrawLoadImage(explosion_1_FILEPATH);
    tumDrawSetLoadedImageScale(explosion_img_1,2);

    explosion_img_2 = tumDrawLoadImage(explosion_2_FILEPATH);
    tumDrawSetLoadedImageScale(explosion_img_2,2);

}

void vDrawTask(void *pvParameters)
{
    // structure to store time retrieved from Linux kernel
    static struct timespec the_time;
    static char our_time_string[100];
    static int our_time_strings_width = 0;

    vLoadImages();

    tumDrawBindThread();



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

    alien_lock = xSemaphoreCreateMutex(); //locking mechanism 

    if (xTaskCreate(vAlienControlTask, "AlienControl", mainGENERIC_STACK_SIZE * 2, NULL,
                    mainGENERIC_PRIORITY, &AlienControlTask) != pdPASS) {
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
