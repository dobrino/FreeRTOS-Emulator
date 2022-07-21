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
#define mothership_FILEPATH "resources/images/mothership.png"
#define alien1_1_FILEPATH "resources/images/alien1_1.png"
#define alien1_2_FILEPATH "resources/images/alien1_2.png"
#define alien2_1_FILEPATH "resources/images/alien2_1.png"
#define alien2_2_FILEPATH "resources/images/alien2_2.png"
#define alien3_1_FILEPATH "resources/images/alien3_1.png"
#define alien3_2_FILEPATH "resources/images/alien3_2.png"
#define explosion_1_FILEPATH "resources/images/explosion1.png"
#define explosion_2_FILEPATH "resources/images/explosion2.png"
#define block1_1_FILEPATH "resources/images/block1_1.png"
#define block1_2_FILEPATH "resources/images/block1_2.png"
#define block2_1_FILEPATH "resources/images/block2_1.png"
#define block2_2_FILEPATH "resources/images/block2_2.png"
#define block3_1_FILEPATH "resources/images/block3_1.png"
#define block3_2_FILEPATH "resources/images/block3_2.png"
#define logo_FILEPATH "resources/images/logo.jpg"
#define start_game_FILEPATH "resources/images/start_game.png"
#define controls_FILEPATH "resources/images/controls.png"

#define STATE_COUNT 4
#define GAME_STARTING 0
#define GAME_IN_PROGESS 1
#define GAME_PAUSE 2
#define GAME_END 3
#define NEXT_TASK 0
#define END_GAME 1
#define PAUSE 2

#define STATE_DEBOUNCE_DELAY 300
#define STATE_QUEUE_LENGTH 1

//Screen
static SemaphoreHandle_t DrawSignal = NULL;
static SemaphoreHandle_t ScreenLock = NULL;

//Intro 
static image_handle_t logo_img;
static image_handle_t controls_img;
static image_handle_t start__game_img;
static int logo_y = 0;
static int spaceships_x = 0;

//Barriers
struct bunker_block{
    coord_t coord;
    int hits;
    int type;
};
static image_handle_t block_img[4][3];
static struct bunker_block bunker_blocks[4][3][3];
const int block_width = 16;
const int bunker_x_offset = 32;
const int bunker_y_offset = 320;


// Task Hanles
static TaskHandle_t DrawTask = NULL;
static TaskHandle_t ControlTask = NULL;
static TaskHandle_t AlienControlTask = NULL;
static TaskHandle_t MothershipConrol = NULL;
static TaskHandle_t StateMachine = NULL;
static TaskHandle_t GameControl = NULL;
static TaskHandle_t IntroTask = NULL;
static TaskHandle_t BufferSwap = NULL;

// Queue Hanles and signals
static QueueHandle_t StateQueue;
const unsigned char next_state_signal = NEXT_TASK;
const unsigned char end_game_signal = END_GAME;
const unsigned char pause_signal = PAUSE;

// Score Board
static int score = 0;
static int high_score = 0;
static int level = 1;
static int lives = 3;
static image_handle_t life_img = NULL;

// Bullet
struct bullet{
    char active;
    coord_t coord;
};

// Space ship
struct spaceship{
    coord_t coord;
    struct bullet bullet;
    coord_t bullet_coord;
    SemaphoreHandle_t lock;
};
static struct spaceship spaceship;
static image_handle_t spaceship_img = NULL;

// Mothership
static struct spaceship mothership;


// Aliens
static SemaphoreHandle_t alien_lock; //to lock acess to the alien variables

struct alien{
    coord_t coord;
    int type; //1,2,3 corellating with the loaded images
    int alive; //1= alive, 2= hit phase 1, 3= hit phase 2, 0 = dead
    int frame; //selecting frame for aliens, 2 available
};
static int global_frame;
static int current_last_row = 4;
static int current_alien_count = 5;
static struct alien aliens[5][8];

static coord_t alien_offset;
static char direction = 1;
static int alien_speed = 1;
struct bomb{
    char active;
    coord_t coord;
};
static struct bomb bomb = {NULL, {0,0}};


static image_handle_t explosion_img_1 = NULL;
static image_handle_t explosion_img_2 = NULL;
static image_handle_t alien_img[4][3];
static image_handle_t mothership_img = NULL;


// Buttonsad
typedef struct buttons_buffer {
    unsigned char buttons[SDL_NUM_SCANCODES];
    SemaphoreHandle_t lock;
} buttons_buffer_t;

static buttons_buffer_t buttons = { 0 };

#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR

void xGetButtonInput(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        xQueueReceive(buttonInputQueue, &buttons.buttons, 0);
        xSemaphoreGive(buttons.lock);
    }
}

void vBufferSwap(void *pvParameters)
{
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    const TickType_t frameratePeriod = 20;

    tumDrawBindThread(); // Setup Rendering handle with correct GL context

    while (1) {
        if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
            tumDrawUpdateScreen();
            tumEventFetchEvents(FETCH_EVENT_BLOCK);
            xSemaphoreGive(ScreenLock);
            xSemaphoreGive(DrawSignal);
            vTaskDelayUntil(&xLastWakeTime,
                            pdMS_TO_TICKS(frameratePeriod));
        }
    }
}

void vRestartGame(){
    vInitGame();
    lives = 3;
    if(score > high_score)
        high_score = score;
    score = 0;
}

static int vCheckStateInput(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        if (buttons.buttons[KEYCODE(SPACE)]) {
            buttons.buttons[KEYCODE(SPACE)] = 0;
            printf("Change state was init\n");
            if (StateQueue) {
                xSemaphoreGive(buttons.lock);
                xQueueSend(StateQueue, &next_state_signal, 0);
                return 0;
            }
            return -1;
        }
        xSemaphoreGive(buttons.lock);
    }
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        if (buttons.buttons[KEYCODE(R)] && lives == 0) {
            buttons.buttons[KEYCODE(R)] = 0;
            printf("Change state was init\n");
            if (StateQueue) {
                vRestartGame();           
                xSemaphoreGive(buttons.lock);
                xQueueSend(StateQueue, &end_game_signal, 0);
                return 0;
            }
            return -1;
        }
        xSemaphoreGive(buttons.lock);
    }

    return 0;
}


// STATE MACHINE
void changeState(volatile unsigned char *state, unsigned char forwards)
{
    
    switch (forwards) {
        case NEXT_TASK:
            if (*state <= 1) {
                (*state)++;
            }
            else {
                (*state)--;
            }  
            break;
        case END_GAME:
            if (*state == STATE_COUNT - 1) {
                *state = STATE_COUNT - 3; //Restarting Game
            }
            else {
                *state = STATE_COUNT - 1; //Ending Game 
            }
            break;
        default:
            break;
    }
    printf("current state: %d \n",*state);
}

/*
 * Example basic state machine with sequential states
 */

void vPauseControl(){

}

void vStateMachine(void *pvParameters)
{
    unsigned char current_state = GAME_STARTING; // Default state
    unsigned char state_changed =
        1; // Only re-evaluate state if it has changed
    unsigned char input = 0;

    const int state_change_period = STATE_DEBOUNCE_DELAY;

    TickType_t last_change = xTaskGetTickCount();

    while (1) {
        if (state_changed) {
            goto initial_state;
        }

        // Handle state machine input
        if (StateQueue)
            if (xQueueReceive(StateQueue, &input, portMAX_DELAY) ==
                pdTRUE)
                if (xTaskGetTickCount() - last_change >
                    state_change_period) {
                    changeState(&current_state, input);
                    state_changed = 1;
                    last_change = xTaskGetTickCount();
                }

initial_state:
        // Handle current state
        if (state_changed) {
            switch (current_state) {
                case GAME_STARTING:
                    if(AlienControlTask)
                        vTaskSuspend(AlienControlTask);
                    if(DrawTask)
                        vTaskSuspend(DrawTask);
                    if(MothershipConrol)
                        vTaskSuspend(MothershipConrol);
                    if(ControlTask)
                        vTaskSuspend(ControlTask);
                    if(IntroTask)
                        vTaskResume(IntroTask);                    
                    break;
                case GAME_IN_PROGESS:
                    if(AlienControlTask)
                        vTaskResume(AlienControlTask);
                    if(DrawTask)
                        vTaskResume(DrawTask);
                    if(MothershipConrol)
                        vTaskResume(MothershipConrol);
                    if(ControlTask)
                        vTaskResume(ControlTask);
                    if(IntroTask)
                        vTaskSuspend(IntroTask);
                    break;
                case GAME_PAUSE:
                    if(AlienControlTask)
                        vTaskSuspend(AlienControlTask);
                    if(DrawTask)
                        vTaskResume(DrawTask);
                    if(MothershipConrol)
                        vTaskSuspend(MothershipConrol);
                    if(ControlTask)
                        vTaskSuspend(ControlTask);
                    if(IntroTask)
                        vTaskSuspend(IntroTask);
                    break;
                case GAME_END:
                    if(AlienControlTask)
                        vTaskSuspend(AlienControlTask);
                    if(DrawTask)
                        vTaskResume(DrawTask);
                    if(MothershipConrol)
                        vTaskSuspend(MothershipConrol);
                    if(ControlTask)
                        vTaskSuspend(ControlTask);
                    if(IntroTask)
                        vTaskSuspend(IntroTask);
                    break;
                default:
                    break;
            }
            state_changed = 0;
            printf("state changed\n");
        }
    }
}

void vLoadImages(){
    // Load Images
    spaceship_img = tumDrawLoadImage(spaceship_FILEPATH);
    tumDrawSetLoadedImageScale(spaceship_img, 0.2);

    life_img = tumDrawLoadImage(spaceship_FILEPATH);
    tumDrawSetLoadedImageScale(life_img, 0.05);

    mothership_img = tumDrawLoadImage(mothership_FILEPATH);
    tumDrawSetLoadedImageScale(mothership_img,0.16);

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

    block_img[1][1] = tumDrawLoadImage(block1_1_FILEPATH);
    tumDrawSetLoadedImageScale(block_img[1][1],2);

    block_img[1][2] = tumDrawLoadImage(block1_2_FILEPATH);
    tumDrawSetLoadedImageScale(block_img[1][2],2);

    block_img[2][1] = tumDrawLoadImage(block2_1_FILEPATH);
    tumDrawSetLoadedImageScale(block_img[2][1],2);

    block_img[2][2] = tumDrawLoadImage(block2_2_FILEPATH);
    tumDrawSetLoadedImageScale(block_img[2][2],2);

    block_img[3][1] = tumDrawLoadImage(block3_1_FILEPATH);
    tumDrawSetLoadedImageScale(block_img[3][1],2);

    block_img[3][2] = tumDrawLoadImage(block3_2_FILEPATH);
    tumDrawSetLoadedImageScale(block_img[3][2],2);

    logo_img = tumDrawLoadImage(logo_FILEPATH);
    tumDrawSetLoadedImageScale(logo_img,0.5);

    controls_img = tumDrawLoadImage(controls_FILEPATH);
    tumDrawSetLoadedImageScale(controls_img,0.25);

    start__game_img = tumDrawLoadImage(start_game_FILEPATH);
    tumDrawSetLoadedImageScale(start__game_img,0.25);
}
const alien_position_y = 300; 
void vDrawScores(int tick_counter){ //Explaining score counts for different alien types in Start Screen
    if(tick_counter >= 60){
        tumDrawLoadedImage(alien_img[1][1],20,alien_position_y);//alien1
        tumDrawText("30 Points", 50, alien_position_y,0xFFFFFF); //alienscore1> 30 Points
        tumDrawLoadedImage(alien_img[2][1],20,alien_position_y+40);//alien2
        tumDrawText("20 Points", 50, alien_position_y+40,0xFFFFFF);//alienscore2:
        tumDrawLoadedImage(alien_img[3][1],20,alien_position_y+80);//alien3
        tumDrawText("10 Points", 50, alien_position_y+80,0xFFFFFF);//alienscore3
    }
}

int drawIntro(int tick_counter){
    tumDrawText("These are the rules, obey them!",300,300,0x00FFF);

    if(logo_y > -100){
        logo_y = 500 - tick_counter*10;
        spaceships_x = -550 + 10*tick_counter;
        printf("%d \n", logo_y);
        printf("%d \n", tick_counter);
    }

    tumDrawLoadedImage(logo_img,100,logo_y);
    tumDrawLoadedImage(controls_img,200,logo_y+350);
    tumDrawLoadedImage(mothership_img,spaceships_x,60);
    tumDrawLoadedImage(spaceship_img,550-spaceships_x, 30);
    if((tick_counter%50)<25) //Modulo Toggle Operation - patented by C. Dobra - Apple INC., Period: 50 ticks Duty Cycle: 50%
        tumDrawLoadedImage(start__game_img,200,logo_y+530);
    
    vDrawScores(tick_counter);

    return logo_y;   
}


void vIntroTask(void *pvParameters)
{
    int tick_counter = 0;
    vLoadImages();



    while (1) {
         if (DrawSignal)
            if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
                pdTRUE) {
                    //tumEventFetchEvents(FETCH_EVENT_NONBLOCK); // Query events backend for new events, ie. button presses
                    xGetButtonInput(); // Update global input

                    //Draw Static Items (Background and Scoreboard)
                    tumDrawClear(0x000000); // Clear screen

                    logo_y = drawIntro(tick_counter);

                    vCheckStateInput();

                    tick_counter++;

                    xSemaphoreGive(ScreenLock);
                    // Basic sleep of 1000 milliseconds
                    vTaskDelay(20);
                }
    }
}


void vMothershipControl(){

    //initing spaeship
    xSemaphoreTake(mothership.lock,0);
    int tick_counter = 0;
    char direction = NULL; //move right by default

    while(1){
        tick_counter++;
        printf("motership_tick: %d \n", tick_counter);
        if (xSemaphoreTake(mothership.lock, 0) == pdTRUE){
            if(tick_counter > 20){
                if(!direction){
                    if(mothership.coord.x < SCREEN_WIDTH + 30){
                        mothership.coord.x =+ tick_counter;
                    }
                    else{
                        direction = 1;
                        tick_counter = 0;
                    }
                }
                else{
                    if(mothership.coord.x > -30){
                        mothership.coord.x =- 10*tick_counter;
                    }
                    else{
                        direction = NULL;
                        tick_counter = 0;
                    }
                }
            }
            xSemaphoreGive(mothership.lock);
        }
        vTaskDelay(40);
    }
}

void vCheckHit(){
    for(int row = 0; row < 5; row++){
        for(int col = 0; col < 8; col++){
            if((abs(spaceship.bullet.coord.x - aliens[row][col].coord.x - 10) <= 10) && aliens[row][col].alive){
                if(abs(spaceship.bullet.coord.y - aliens[row][col].coord.y - 10) <= 10){
                    aliens[row][col].alive = 2;
                    score += 10 * (4-aliens[row][col].type);
                    alien_speed += 1;
                    current_alien_count--;
                    printf("hit detected with bullet coord: x: %d y: %d\n",spaceship.bullet.coord.x,spaceship.bullet.coord.y);
                    spaceship.bullet.active = NULL;
                    printf("%d = %d?\n", aliens[row][col].coord.x,spaceship.bullet.coord.x);
                    spaceship.bullet.coord.x = spaceship.coord.x;
                    spaceship.bullet.coord.y = spaceship.coord.y;
                }
            }
        }
    }
}



void vShootBullet(){
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(
                                    S)] && !spaceship.bullet.active){ //S for Shoot
                buttons.buttons[KEYCODE(S)] = 0;
                spaceship.bullet.coord.y = spaceship.coord.y + 10; //offset to th front of the spacehsip
                spaceship.bullet.coord.x = spaceship.coord.x + 18; //offset to the front of the spaceship
                spaceship.bullet.active = 1;
            }
            xSemaphoreGive(buttons.lock);
        }
    if(spaceship.bullet.coord.y > 0 && spaceship.bullet.active){
        spaceship.bullet.coord.y = spaceship.bullet.coord.y - 10;
    }
    else{
    spaceship.bullet.active = NULL;
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

void vWakeUpAliens(){
    if (xSemaphoreTake(alien_lock, 0) == pdTRUE){
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
        alien_offset.x = 50;
        alien_offset.y = 50;
        xSemaphoreGive(alien_lock);
    }
}

void vToggleFrame(){
    if(global_frame == 1)
        global_frame = 2;
    else
        global_frame = 1;
}

void vInitGame(){
    vInitSpaceship();
    vInitBunkers();
    vWakeUpAliens(); 
    bomb.active = NULL;
}

void vDeathroutine(){
    printf("Death detected\n");
    lives--;
    if(lives == 0)
        xQueueSend(StateQueue, &end_game_signal, 0);
    vInitGame();

}


void vDetectHits(){
    //Check hits
    if((abs(spaceship.coord.y - bomb.coord.y + 20) <= 20) && bomb.active && (abs(spaceship.coord.x - bomb.coord.x + 20) <= 20)){
        vDeathroutine();
    }

    //Check if aliens reached bottom
    for(int col = 0; col < 8; col++){
        if(aliens[current_last_row][col].coord.y == spaceship.coord.y-20){
            vDeathroutine();
        }
    }
    
    //Level Up
    if(current_alien_count == 0){
        level++;
        current_alien_count = 5;
        vInitGame();
    }

    //Check Bunkers 
    for(int i = 0;i<4;i++){
        for(int row = 0;row<3;row++){
            for(int col = 0; col<3;col++){
                if(bomb.active && (bunker_blocks[i][row][col].hits < 2) && (abs(bunker_blocks[i][row][col].coord.x - bomb.coord.x + block_width/2) <= block_width/2)){
                    if(abs(bunker_blocks[i][row][col].coord.y - bomb.coord.y + block_width/2) <= block_width/2){
                        bunker_blocks[i][row][col].hits++;
                        bomb.active = NULL;
                    }
                }
                if(spaceship.bullet.active && (bunker_blocks[i][row][col].hits < 2) && (abs(bunker_blocks[i][row][col].coord.x - spaceship.bullet.coord.x + block_width/2) <= block_width/2)){
                    if(abs(bunker_blocks[i][row][col].coord.y - spaceship.bullet.coord.y + block_width/2) <= block_width/2){
                        bunker_blocks[i][row][col].hits++;
                        spaceship.bullet.active = NULL;
                        printf("block %d, row %d, col %d \n", i, row,col);
                    }
                }
            }
        }
    }

}

void vDropBomb(){
    int random_number;

    if(!bomb.active && aliens[1][1].coord.x){ //if bomb not active, shoot bomb
        random_number = rand()%7;
        if(aliens[current_last_row][random_number].alive){
            bomb.active = 1;
            printf("bomb shooting\n");
            bomb.coord.y = aliens[current_last_row][random_number].coord.y + 15;
            bomb.coord.x = aliens[current_last_row][random_number].coord.x + 15;
        }
    }

    if(bomb.active) //if bomb active, let bomb fall
        bomb.coord.y += 10; //bomb moving downwards
        if(bomb.coord.y >= SCREEN_HEIGHT)
            bomb.active = NULL;


}


void vAlienControlTask(){


    

    printf("Aliens Control Task started\n");

    vWakeUpAliens();


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

            vToggleFrame();

            vDropBomb();

            vKillAliens();
            xSemaphoreGive(alien_lock);
        }

        vTaskDelay(200);
    }
}

void vInitBunkers(){
    for(int i = 0;i<4;i++){
        for(int row = 0;row<3;row++){
            bunker_blocks[i][row][0].coord.x = block_width+(i+1)*SCREEN_WIDTH/5 - bunker_x_offset;
            bunker_blocks[i][row][0].coord.y = bunker_y_offset+row*block_width;
            bunker_blocks[i][row][1].coord.x = block_width*2+(i+1)*SCREEN_WIDTH/5 - bunker_x_offset;
            bunker_blocks[i][row][1].coord.y = bunker_y_offset+row*block_width;
            bunker_blocks[i][row][2].coord.x = block_width*3+(i+1)*SCREEN_WIDTH/5 - bunker_x_offset;
            bunker_blocks[i][row][2].coord.y = bunker_y_offset+row*block_width;

            //Initialisation
            for(int col = 0;col<3;col++){
                bunker_blocks[i][row][col].hits = 0;
            }
            switch(row){
                case 0: bunker_blocks[i][row][0].type = 3;
                        bunker_blocks[i][row][1].type= 1;
                        bunker_blocks[i][row][2].type= 2;break;\

                case 1: bunker_blocks[i][row][0].type= 1;
                        bunker_blocks[i][row][1].type= 1;
                        bunker_blocks[i][row][2].type= 1;break;

                case 2: bunker_blocks[i][row][0].type= 1;
                        bunker_blocks[i][row][1].hits= 2;
                        bunker_blocks[i][row][2].type= 1;break;
            }
        }
    }
}

void vInitSpaceship(){
    spaceship.coord.x = SCREEN_WIDTH/2;
    spaceship.coord.y = SCREEN_HEIGHT - 100;

    spaceship.bullet.coord.x = spaceship.coord.x;
    spaceship.bullet.coord.y = spaceship.coord.y;

    xSemaphoreGive(spaceship.lock);  
}

void  vControlTask(){


    vInitSpaceship();

    vInitBunkers();

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
                if(spaceship.coord.x > 0)
                    spaceship.coord.x = spaceship.coord.x - 5;
            }
            xSemaphoreGive(buttons.lock);
        }

        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(
                                    D)]) { // D for steering to the right
                if(spaceship.coord.x < SCREEN_WIDTH - 40)
                    spaceship.coord.x = spaceship.coord.x + 5;
            }
            xSemaphoreGive(buttons.lock);
        }
        // shooting
        vShootBullet();

        vDetectHits();

        vTaskDelay(20);
    }
}

void vDrawScore(){
    char str[15];
    sprintf(str, "%04d", score);
    tumDrawText(str, 10,10,0xFF0000);
    sprintf(str, "high score: %04d", high_score);
    tumDrawText(str, 10,40,0xFF0000);
    sprintf(str, "Level : %d", level);
    tumDrawText(str, SCREEN_WIDTH-100,10,0xFF0000);
}

void vDrawLives(){
    char str[3];
    sprintf(str,"%d", lives);
    tumDrawText(str, 10,SCREEN_HEIGHT-20,0xFF0000); //drawing the number of lives left

    //drawing spaceshipbsto visuliz the number of lives left
    for(int i = 0; i < lives; i++){
        tumDrawLoadedImage(life_img,30 + i*30,SCREEN_HEIGHT-20);
    }

    //Insert GAME OVER Text here
}


void vDrawBunkers(){
    for(int i = 0;i<4;i++){
        for(int row = 0;row<3;row++){
            for(int col = 0; col<3;col++){
                if(bunker_blocks[i][row][col].hits < 2){
                    tumDrawLoadedImage(block_img[bunker_blocks[i][row][col].type][1 + bunker_blocks[i][row][col].hits],bunker_blocks[i][row][col].coord.x,bunker_blocks[i][row][col].coord.y);
                }
            }
        }
    }
}

void vDrawStatcItems(){
    vDrawScore();
    vDrawLives();
    vDrawBunkers();
    // Draw bottom cave
    tumDrawLine(0,SCREEN_HEIGHT-30,SCREEN_WIDTH,SCREEN_HEIGHT-30,2,0x00FF00);
}

void vDrawSpaceship(){
    tumDrawLoadedImage(spaceship_img,spaceship.coord.x,spaceship.coord.y);
    tumDrawBox(spaceship.coord.x,spaceship.coord.y,35,40,0x00FF00);
}

void vDrawBullet(){
    tumDrawLine(spaceship.bullet.coord.x,spaceship.bullet.coord.y,spaceship.bullet.coord.x,spaceship.bullet.coord.y - 5,10, 0x0000FF);
}

void vDrawBomb(){
    tumDrawCircle(bomb.coord.x,bomb.coord.y,3,0xFF0000);
}

void vDrawAliens(){
    if (xSemaphoreTake(alien_lock, 0) == pdTRUE){
        int row = 0;
        for(row; row < 5; row++){
            for(int col = 0; col < 8; col++){

                switch(aliens[row][col].alive){

                    //alien alive
                    case 1: aliens[row][col].coord.x = alien_offset.x + col*50;
                            aliens[row][col].coord.y = alien_offset.y + row*40;
                            aliens[row][col].frame = global_frame;
                            current_last_row = row;
                            tumDrawLoadedImage(alien_img[aliens[row][col].type][aliens[row][col].frame],aliens[row][col].coord.x, aliens[row][col].coord.y);
                            tumDrawBox(aliens[row][col].coord.x, aliens[row][col].coord.y,20,20,0x00FFFF);
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
        xSemaphoreGive(alien_lock);
    }
}

void vDrawMothership(){
    if (xSemaphoreTake(spaceship.lock, 0) == pdTRUE){
        tumDrawLoadedImage(mothership_img,mothership.coord.x,mothership.coord.y);
        xSemaphoreGive(spaceship.lock);
    }
}

void vDrawObjects(){
    if (xSemaphoreTake(spaceship.lock, 0) == pdTRUE) {
        vDrawSpaceship();
        if(spaceship.bullet.active)
            vDrawBullet();
        xSemaphoreGive(spaceship.lock);
    }

    vDrawMothership();

    vDrawAliens();
    if(bomb.active)
        vDrawBomb();
}


void vDrawTask(void *pvParameters)
{
    // structure to store time retrieved from Linux kernel
    static struct timespec the_time;
    static char our_time_string[100];
    static int our_time_strings_width = 0;

    //downscaling ships for gamemode
    tumDrawSetLoadedImageScale(spaceship_img, 0.1);
    tumDrawSetLoadedImageScale(mothership_img,0.08);

    


    while (1) {
         if (DrawSignal)
            if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
                pdTRUE) {
                    //tumEventFetchEvents(FETCH_EVENT_NONBLOCK); // Query events backend for new events, ie. button presses
                    xGetButtonInput(); // Update global input

                    //Draw Static Items (Background and Scoreboard)
                    tumDrawClear(0x000000); // Clear screen
                    vDrawStatcItems();

                    //Draw Moving Objects (Monsters Bullet)
                    vDrawObjects();
                    vCheckStateInput();

                    xSemaphoreGive(ScreenLock);
                    // Basic sleep of 1000 milliseconds
                    vTaskDelay(20);
                }
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

    if (xTaskCreate(vDrawTask, "DrawTask", mainGENERIC_STACK_SIZE * 2, NULL,
                    mainGENERIC_PRIORITY, &DrawTask) != pdPASS) {
        goto err_demotask;
    }

    spaceship.lock = xSemaphoreCreateMutex();
    if (xTaskCreate(vControlTask, "ControlTask", mainGENERIC_STACK_SIZE * 2, NULL,
                    5, &ControlTask) != pdPASS) {
        goto err_controltask;
    }

    alien_lock = xSemaphoreCreateMutex(); //locking mechanism

    if (xTaskCreate(vAlienControlTask, "AlienControl", mainGENERIC_STACK_SIZE * 2, NULL,
                    mainGENERIC_PRIORITY, &AlienControlTask) != pdPASS) {
        goto err_controltask;
    }

    mothership.lock = xSemaphoreCreateMutex();
    if (xTaskCreate(vMothershipControl, "MothershipControl", mainGENERIC_STACK_SIZE * 2, NULL,
                    mainGENERIC_PRIORITY, &MothershipConrol) != pdPASS) {
        goto err_controltask;
    }

    StateQueue = xQueueCreate(STATE_QUEUE_LENGTH, sizeof(unsigned char));

    if (xTaskCreate(vStateMachine, "StateMachine", mainGENERIC_STACK_SIZE * 2, NULL,
                    mainGENERIC_PRIORITY+3, &StateMachine) != pdPASS) {
        goto err_controltask;
    }

    if (xTaskCreate(vIntroTask, "IntroTask", mainGENERIC_STACK_SIZE * 2, NULL,
                    mainGENERIC_PRIORITY+3, &IntroTask) != pdPASS) {
        goto err_controltask;
    }

    DrawSignal = xSemaphoreCreateBinary();
    ScreenLock = xSemaphoreCreateMutex();
    if (xTaskCreate(vBufferSwap, "BufferSwapTask",
                    mainGENERIC_STACK_SIZE * 2, NULL, configMAX_PRIORITIES,
                    &BufferSwap) != pdPASS) {
        goto err_controltask;
    }

    vTaskSuspend(DrawTask);
    vTaskSuspend(IntroTask);
    vTaskSuspend(MothershipConrol);
    vTaskSuspend(AlienControlTask);
    vTaskSuspend(ControlTask);





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
