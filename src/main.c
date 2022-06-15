#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL_scancode.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "TUM_Ball.h"
#include "TUM_Draw.h"
#include "TUM_Font.h"
#include "TUM_Event.h"
#include "TUM_Sound.h"
#include "TUM_Utils.h"
#include "TUM_FreeRTOS_Utils.h"
#include "TUM_Print.h"

#include "AsyncIO.h"

#define mainGENERIC_PRIORITY (tskIDLE_PRIORITY)
#define mainGENERIC_STACK_SIZE ((unsigned short)2560)

#define STATE_QUEUE_LENGTH 1

#define STATE_COUNT 3

#define STATE_ONE 0
#define STATE_TWO 1
#define STATE_THREE 2

#define NEXT_TASK 0
#define PREV_TASK 1

#define STARTING_STATE STATE_ONE

#define STATE_DEBOUNCE_DELAY 300

#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR
#define CAVE_SIZE_X SCREEN_WIDTH / 2
#define CAVE_SIZE_Y SCREEN_HEIGHT / 2
#define CAVE_X CAVE_SIZE_X / 2
#define CAVE_Y CAVE_SIZE_Y / 2
#define CAVE_THICKNESS 25
#define LOGO_FILENAME "freertos.jpg"
#define UDP_BUFFER_SIZE 2000
#define UDP_TEST_PORT_1 1234
#define UDP_TEST_PORT_2 4321
#define MSG_QUEUE_BUFFER_SIZE 1000
#define MSG_QUEUE_MAX_MSG_COUNT 10
#define TCP_BUFFER_SIZE 2000
#define TCP_TEST_PORT 2222

#ifdef TRACE_FUNCTIONS
#include "tracer.h"
#endif

static char *mq_one_name = "FreeRTOS_MQ_one_1";
static char *mq_two_name = "FreeRTOS_MQ_two_1";
aIO_handle_t mq_one = NULL;
aIO_handle_t mq_two = NULL;
aIO_handle_t udp_soc_one = NULL;
aIO_handle_t udp_soc_two = NULL;
aIO_handle_t tcp_soc = NULL;

const unsigned char next_state_signal = NEXT_TASK;
const unsigned char prev_state_signal = PREV_TASK;

static TaskHandle_t StateMachine = NULL;
static TaskHandle_t BufferSwap = NULL;
static TaskHandle_t DemoTask1 = NULL;
static TaskHandle_t Exercise4 = NULL;
static TaskHandle_t UDPDemoTask = NULL;
static TaskHandle_t TCPDemoTask = NULL;
static TaskHandle_t MQDemoTask = NULL;
static TaskHandle_t DemoSendTask = NULL;
static TaskHandle_t Exercise3 = NULL;
static TaskHandle_t Circle1 = NULL;
static TaskHandle_t Circle2 = NULL;
static TaskHandle_t RandTask1 = NULL;
static TaskHandle_t RandTask2 = NULL;
static TaskHandle_t timedreset = NULL;
static TaskHandle_t increment = NULL;
static TaskHandle_t Ex4_1 = NULL;
static TaskHandle_t Ex4_2 = NULL;
static TaskHandle_t Ex4_3 = NULL;
static TaskHandle_t Ex4_4 = NULL;
static TaskHandle_t TickMaster = NULL;

 
static QueueHandle_t StateQueue = NULL;
static SemaphoreHandle_t DrawSignal = NULL;
static SemaphoreHandle_t ScreenLock = NULL;
static SemaphoreHandle_t counterlock  = NULL;
static SemaphoreHandle_t task4_3  = NULL;

static image_handle_t logo_image = NULL;

static int task1_counter = 0;
static int task2_counter = 0;
static int task3_counter = 0; 
static SemaphoreHandle_t Task1Trigger=NULL;

static char circle1_en = NULL;
static char circle2_en = NULL;

static int tickcounter = 0;

static char Ex4[16][100];

typedef struct buttons_buffer {
    unsigned char buttons[SDL_NUM_SCANCODES];
    SemaphoreHandle_t lock;
} buttons_buffer_t;

static buttons_buffer_t buttons = { 0 };

void checkDraw(unsigned char status, const char *msg)
{
    if (status) {
        if (msg)
            fprints(stderr, "[ERROR] %s, %s\n", msg,
                    tumGetErrorMessage());
        else {
            fprints(stderr, "[ERROR] %s\n", tumGetErrorMessage());
        }
    }
}

/*
 * Changes the state, either forwards of backwards
 */
void changeState(volatile unsigned char *state, unsigned char forwards)
{
    switch (forwards) {
        case NEXT_TASK:
            if (*state == STATE_COUNT - 1) {
                *state = 0;
            }
            else {
                (*state)++;
            }
            break;
        case PREV_TASK:
            if (*state == 0) {
                *state = STATE_COUNT - 1;
            }
            else {
                (*state)--;
            }
            break;
        default:
            break;
    }
}

/*
 * Example basic state machine with sequential states
 */
void basicSequentialStateMachine(void *pvParameters)
{
    unsigned char current_state = STARTING_STATE; // Default state
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
                case STATE_ONE:
                    if (Exercise4) {
                        vTaskSuspend(Exercise4);
                    }
                    if (DemoTask1) {
                        vTaskResume(DemoTask1);
                    }
                    if(Circle1){
                        vTaskSuspend(Circle1);
                    }
                    if(Circle2){
                        vTaskSuspend(Circle2);
                    }
                    if(Exercise3){
                        vTaskSuspend(Exercise3);
                    }
                    printf("We are in Statge 1");
                    break;
                case STATE_TWO:
                    printf("We are in Statge 3");
                    if (DemoTask1) {
                        vTaskSuspend(DemoTask1);
                    }
                    if (Exercise4) {
                        vTaskSuspend(Exercise4);
                    }
                    if(Circle1){
                        vTaskResume(Circle1);
                    }
                    if(Circle2){
                        vTaskResume(Circle2);
                    }
                    if(Exercise3){
                        vTaskResume(Exercise3);
                    }
                    printf("We are in State 3");
                    break;
                case STATE_THREE:
                    if (DemoTask1) {
                        vTaskSuspend(DemoTask1);
                    }
                    if (Exercise4) {
                        vTaskResume(Exercise4);
                    }
                    if(Circle1){
                        vTaskSuspend(Circle1);
                    }
                    if(Circle2){
                        vTaskSuspend(Circle2);
                    }
                    if(Exercise3){
                        vTaskSuspend(Exercise3);
                    }
                    printf("We are in Statge 2");
                    break;
  
                default:
                    break;
            }
            state_changed = 0;
        }
    }
}

void vSwapBuffers(void *pvParameters)
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

void xGetButtonInput(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        xQueueReceive(buttonInputQueue, &buttons.buttons, 0);
        xSemaphoreGive(buttons.lock);
    }
}


void vDrawHelpText(void)
{
    static char str[100] = { 0 };
    static int text_width;
    ssize_t prev_font_size = tumFontGetCurFontSize();

    tumFontSetSize((ssize_t)30);

    sprintf(str, "[Q]uit, [E]xhange State");

    if (!tumGetTextSize((char *)str, &text_width, NULL))
        checkDraw(tumDrawText(str, SCREEN_WIDTH - text_width - 10,
                              DEFAULT_FONT_SIZE * 0.5, Black),
                  __FUNCTION__);

    tumFontSetSize(prev_font_size);
}

#define FPS_AVERAGE_COUNT 50
#define FPS_FONT "IBMPlexSans-Bold.ttf"

void vDrawFPS(void)
{
    static unsigned int periods[FPS_AVERAGE_COUNT] = { 0 };
    static unsigned int periods_total = 0;
    static unsigned int index = 0;
    static unsigned int average_count = 0;
    static TickType_t xLastWakeTime = 0, prevWakeTime = 0;
    static char str[10] = { 0 };
    static int text_width;
    int fps = 0;
    font_handle_t cur_font = tumFontGetCurFontHandle();

    if (average_count < FPS_AVERAGE_COUNT) {
        average_count++;
    }
    else {
        periods_total -= periods[index];
    }

    xLastWakeTime = xTaskGetTickCount();

    if (prevWakeTime != xLastWakeTime) {
        periods[index] =
            configTICK_RATE_HZ / (xLastWakeTime - prevWakeTime);
        prevWakeTime = xLastWakeTime;
    }
    else {
        periods[index] = 0;
    }

    periods_total += periods[index];

    if (index == (FPS_AVERAGE_COUNT - 1)) {
        index = 0;
    }
    else {
        index++;
    }

    fps = periods_total / average_count;

    tumFontSelectFontFromName(FPS_FONT);

    sprintf(str, "FPS: %2d", fps);

    if (!tumGetTextSize((char *)str, &text_width, NULL))
        checkDraw(tumDrawText(str, SCREEN_WIDTH - text_width - 10,
                              SCREEN_HEIGHT - DEFAULT_FONT_SIZE * 1.5,
                              Skyblue),
                  __FUNCTION__);

    tumFontSelectFontFromHandle(cur_font);
    tumFontPutFontHandle(cur_font);
}

void vDrawLogo(void)
{
    static int image_height;

    if ((image_height = tumDrawGetLoadedImageHeight(logo_image)) != -1)
        checkDraw(tumDrawLoadedImage(logo_image, 10,
                                     SCREEN_HEIGHT - 10 - image_height),
                  __FUNCTION__);
    else {
        fprints(stderr,
                "Failed to get size of image '%s', does it exist?\n",
                LOGO_FILENAME);
    }
}

void vDrawStaticItems(void)
{
    vDrawHelpText();
    vDrawLogo();
}


static int vCheckStateInput(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        if (buttons.buttons[KEYCODE(E)]) {
            buttons.buttons[KEYCODE(E)] = 0;
            if (StateQueue) {
                xSemaphoreGive(buttons.lock);
                xQueueSend(StateQueue, &next_state_signal, 0);
                return 0;
            }
            return -1;
        }
        xSemaphoreGive(buttons.lock);
    }

    return 0;
}


void vExercise2(void *pvParameters)
{
    static coord_t circ_coord;
    circ_coord.x = 150;
    circ_coord.y = 250;
    //TRIANGLE
    static coord_t tria_coord[3];
    tria_coord[0].x = 300;
    tria_coord[0].y = 200;
    tria_coord[1].x = 250;
    tria_coord[1].y = 300;
    tria_coord[2].x = 350; 
    tria_coord[2].y = 300;
    //SQAURE
    static coord_t squ_coord;
    squ_coord.x = 400;
    squ_coord.y = 200;
    const int squ_width = 100;
    const int squ_height = 100;

    //EXECUTION COUNTER
    int exec_counter = 0;
    const float rot_speed = 0.01;
    const int rot_radius = 150;

    //TEXT
    const char subtitle_bottom[] = "Max Herbst, der High-Performer";
    const char subtitle_top[] = "PYX rules";
    static coord_t sub_top_coord;
    static coord_t sub_bottom_coord;
    sub_top_coord.x = 0;
    sub_top_coord.y = 100;
    sub_bottom_coord.x = 150;
    sub_bottom_coord.y = 400;

    //BUTTONS COUNTER
    int a_counter = 0;
    int b_counter = 0;
    int c_counter = 0;
    int d_counter = 0;
    int a_counter_en = 0;
    int b_counter_en = 0;
    int c_counter_en = 0;
    int d_counter_en = 0;
    // MOUSE COORD
    coord_t mouse_coord;
    mouse_coord.x = 0;
    mouse_coord.y = 0;
    TickType_t xLastFrameTime = xTaskGetTickCount();

    while (1) {
        if (DrawSignal){
            if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
                pdTRUE) {
                tumEventFetchEvents(FETCH_EVENT_BLOCK |
                                    FETCH_EVENT_NO_GL_CHECK);
                xGetButtonInput(); // Update global input

                xSemaphoreTake(ScreenLock, portMAX_DELAY);

                // Clear screen
                checkDraw(tumDrawClear(White), __FUNCTION__);
                vDrawStaticItems();
                
                xLastFrameTime = xTaskGetTickCount();
          

                //EXERCISE 2.1//
                //DRAWING OBJECTS//
                //CIRCLE
                tumDrawCircle(circ_coord.x,circ_coord.y,50,0x00FF00);
                
                //TRIANLGE
                tumDrawTriangle(tria_coord,0xFF0000);//use pointer here 
                //Triangle is used as the center for all the other objects
                //SQUARE
                tumDrawFilledBox(squ_coord.x,squ_coord.y,squ_width, squ_height,0x0000FF);

                //TEXT
                tumDrawText(subtitle_bottom,sub_bottom_coord.x, sub_bottom_coord.y, 0x000000);
                tumDrawText(subtitle_top,sub_top_coord.x, sub_top_coord.y, 0x000000);

                //MOTION//
                exec_counter +=1;
                //movement with mouse:
                tria_coord[0].x = mouse_coord.x;
                tria_coord[0].y = mouse_coord.y - 50;
                tria_coord[1].x = mouse_coord.x + 50;
                tria_coord[1].y = mouse_coord.y + 50;
                tria_coord[2].x = mouse_coord.x - 50; 
                tria_coord[2].y = mouse_coord.y + 50;

                // CIRCLE
                circ_coord.x =  tria_coord[0].x + rot_radius * cos(rot_speed * exec_counter + M_PI); //triangle center + radius * rotation(wt)
                circ_coord.y = (tria_coord[0].y + tria_coord[1].y)/2 + rot_radius * sin(rot_speed * exec_counter); //triangle center + radius * rotation(wt)

                //FILLED BOX
                squ_coord.x =  -squ_width/2 + tria_coord[0].x + rot_radius * cos(rot_speed * exec_counter); //offset to squre center + triangle center + radius * rotation(wt)
                squ_coord.y = -squ_width/2 + (tria_coord[0].y + tria_coord[1].y)/2 + rot_radius * sin(rot_speed * exec_counter + M_PI); //offset to squre center + triangle center + radius * rotation(wt)
                
                //SUBTITLE TOP
                sub_top_coord.x = mouse_coord.x * sin(rot_speed * exec_counter); //screen center + sin motion, left <-> right
                sub_top_coord.y = mouse_coord.y - 100;

                //SUBTITLE BOTTOM 
                sub_bottom_coord.x = mouse_coord.x - 150;
                sub_bottom_coord.y = mouse_coord.y + 150;

                //EXERCISE 2.2//
                if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
                    if (buttons.buttons[KEYCODE(A)]) { // Equiv to SDL_SCANCODE_A
                        buttons.buttons[KEYCODE(A)] = 0;
                        a_counter += 1;
                        printf("a was pressed %d times \n", a_counter); 
                    }
                    if (buttons.buttons[KEYCODE(B)]) { // Equiv to SDL_SCANCODE_B
                        buttons.buttons[KEYCODE(B)] = 0;
                        b_counter += 1;
                        printf("b was pressed %d times \n", b_counter);
                    }
                    if (buttons.buttons[KEYCODE(C)]) { // Equiv to SDL_SCANCODE_C
                        buttons.buttons[KEYCODE(C)] = 0;
                        c_counter += 1;
                        printf("c was pressed %d times \n", c_counter);
                    }
                    if (buttons.buttons[KEYCODE(D)]) { // Equiv to SDL_SCANCODE_D
                        buttons.buttons[KEYCODE(D)] = 0;
                        d_counter += 1;
                        printf("d was pressed %d times \n", d_counter);
                    }
                    if (tumEventGetMouseLeft()) { //Mouse Click Right
                        //buttons.buttons[KEYCODE(D)] = 0;
                        a_counter = 0;
                        b_counter = 0;
                        c_counter = 0;
                        d_counter = 0;
                        printf("All values have been reset \n"); 
                    }
                    if(tumEventGetMouseX()){
                        mouse_coord.x = tumEventGetMouseX();
                    }
                    if(tumEventGetMouseY()){
                        mouse_coord.y = tumEventGetMouseY();
                    }
                    sprintf(subtitle_bottom,"A: %d | B: %d | C: %d | D: %d  Mouse_x : %d Mouse_y : %d", //printing string to subtitle bottom do be drawn on the bottom 
                        a_counter,b_counter,c_counter,d_counter,mouse_coord.x,mouse_coord.y);

                    xSemaphoreGive(buttons.lock);
                }
            }
        }
                        
        // Get input and check for state change
        xSemaphoreGive(ScreenLock);
        vCheckStateInput();
    }
}


void vExercise4(void *pvParameters)
{   
    TickType_t xLastWakeTime, prevWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    prevWakeTime = xLastWakeTime;

    

    prints("Task 1 init'd\n");
    
    char en_tasks = 1;

    while (1) {
        if (DrawSignal){
            if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
                pdTRUE) {
                xLastWakeTime = xTaskGetTickCount();

                xGetButtonInput(); // Update global button data

                xSemaphoreTake(ScreenLock, portMAX_DELAY);
                // Clear screen
                checkDraw(tumDrawClear(White), __FUNCTION__);

                vDrawStaticItems();

            
                // Draw FPS in lower right corner
                vDrawFPS();
                if(en_tasks){ 
                    vTaskResume(Ex4_1);
                    vTaskResume(Ex4_2);
                    vTaskResume(Ex4_3);
                    vTaskResume(Ex4_4);
                    vTaskResume(TickMaster);

                    vTaskDelay(15);

                    vTaskSuspend(Ex4_1);
                    vTaskSuspend(Ex4_2);
                    vTaskSuspend(Ex4_3);
                    vTaskSuspend(Ex4_4);
                    vTaskSuspend(TickMaster);
                    
                    en_tasks = NULL;
                    for(int i = 1; i < 16; i++){
                        printf("%s",Ex4[i]);
                        printf("\n");
                    }
                }
                

                xSemaphoreGive(ScreenLock);

                // Check for state change
                vCheckStateInput();
                // Keep track of when task last ran so that you know how many ticks
                //(in our case miliseconds) have passed so that the balls position
                // can be updated appropriatley
                prevWakeTime = xLastWakeTime;
            }
        }
    }
}

void vCircle1(void *pvParameters)
{
    prints("Task 1 init'd\n");

    while (1) {
        if(circle1_en){
            circle1_en = NULL;
        }
        else{
            circle1_en = 1;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
            
    }
}
void vCircle2(void *pvParameters)
{
    char circle_en = NULL;

    prints("Task 1 init'd\n");

    while (1) {
        if(circle2_en){
            
            circle2_en = NULL;
        }
        else{
            circle2_en = 1;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
            
    }
}


void vRandTask1(void *pvParameters)
{   

    char text[100];
    while (1) {
        if(Task1Trigger){ 
            if (xSemaphoreTake(Task1Trigger,portMAX_DELAY) == pdTRUE){
                if (xSemaphoreTake(counterlock,portMAX_DELAY) == pdTRUE){ 
                    task1_counter++;
                    printf("Task1 triggerd");
                    xSemaphoreGive(counterlock);
                }          
            }
        }    
    }
} 

#define LBI 0x001
void vRandTask2(void *pvParameters)
{    
    uint32_t NotificationBuffer;
    
    while (1) {
        NotificationBuffer = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (NotificationBuffer & LBI){
            if (xSemaphoreTake(counterlock,portMAX_DELAY) == pdTRUE){ 
                task2_counter++;
                xSemaphoreGive(counterlock);
            }
        }
    }
}

void vTimedreset(void *pvParameters)
{    
    while (1) {
        if (xSemaphoreTake(counterlock,portMAX_DELAY) == pdTRUE){ 
            task1_counter = 0;
            task2_counter = 0;
            xSemaphoreGive(counterlock);
            vTaskDelay(15000); //waiting for 15 seconds 
        } 
    }
}
void vIncremet(void *pvParameters)
{    
    while (1) {
        task3_counter++;
        vTaskDelay(0); //waiting for 1 seconds 
    }
}
void v4_1(void *pvParameters)
{    
    while (1) {
        sprintf(Ex4[tickcounter] + strlen(Ex4[tickcounter]),"1 ");
        vTaskDelay(1);
    }
}
void v4_2(void *pvParameters)
{    
    while (1) {
        sprintf(Ex4[tickcounter] + strlen(Ex4[tickcounter]),"2 ");
        vTaskDelay(2); //waiting for one tick
        xSemaphoreGive(task4_3);
    }
}
void v4_3(void *pvParameters)
{    
    while (1) {
        if(xSemaphoreTake(task4_3,portMAX_DELAY)){     
            sprintf(Ex4[tickcounter] + strlen(Ex4[tickcounter]),"3 ");
            vTaskDelay(3); //waiting for one tick
        }
    }
}
void v4_4(void *pvParameters)
{    
    while (1) {
        sprintf(Ex4[tickcounter] + strlen(Ex4[tickcounter]),"4 ");
        vTaskDelay(4); //waiting for one tick 
    }
}
void vTickMaster(void *pvParameters)
{    
    while (1) {
        printf("Tick\n");
        tickcounter++;
        printf(Ex4[tickcounter]);
        vTaskDelay(1); //waiting for one tick 
    }
}

void vExercise3(void *pvParameters)
{
    TickType_t xLastWakeTime, prevWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    prevWakeTime = xLastWakeTime; 
    char str[100];
    xSemaphoreGive(counterlock);
    char toggleCounter = 1;
    
    while (1) {
        if (DrawSignal)
            if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
                pdTRUE) {
                xLastWakeTime = xTaskGetTickCount();

                xGetButtonInput(); // Update global button data
                xSemaphoreTake(ScreenLock, portMAX_DELAY);
                // Clear screen
                checkDraw(tumDrawClear(White), __FUNCTION__);

                vDrawStaticItems();

                if(circle1_en){
                    tumDrawCircle(400,300,100,0x00FF00);
                }
                if(circle2_en){
                    tumDrawCircle(200,300,100,0xFF00FF);
                }

                //3_2_3
                if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
                    if (buttons.buttons[KEYCODE(A)]) { // Equiv to SDL_SCANCODE_A
                        buttons.buttons[KEYCODE(A)] = 0;
                        printf("a was pressed\n"); 
                        xSemaphoreGive(Task1Trigger);
                    }
                    if (buttons.buttons[KEYCODE(S)]) { // Equiv to SDL_SCANCODE_A
                        buttons.buttons[KEYCODE(S)] = 0;
                        printf("s was pressed\n"); 
                        xTaskNotify(RandTask2,LBI,eSetBits);
                    }if (buttons.buttons[KEYCODE(C)]) { // Equiv to SDL_SCANCODE_A
                        buttons.buttons[KEYCODE(C)] = 0;
                        if(toggleCounter){
                            vTaskResume(increment);
                            toggleCounter = NULL;
                        }
                        else{
                            vTaskSuspend(increment);
                            toggleCounter = 1;
                        }

                    }
                    xSemaphoreGive(buttons.lock);
                }

                sprintf(str, "Task 1 was triggered %d times.", task1_counter);
                tumDrawText(str, 20,20,0x000000);
                
                sprintf(str, "Task 2 was triggered %d times.", task2_counter);
                tumDrawText(str, 20,50,0x000000);
                sprintf(str, "It has beem %d seconds since the lastm incident", task3_counter);
                tumDrawText(str, 20,80,0x000000);
                vDrawFPS();
                xSemaphoreGive(buttons.lock);
                xSemaphoreGive(ScreenLock);

                // Check for state change
                vCheckStateInput();

                // Keep track of when task last ran so that you know how many ticks
                //(in our case miliseconds) have passed so that the balls position
                // can be updated appropriatley
                prevWakeTime = xLastWakeTime;
            }
    }
}



#define PRINT_TASK_ERROR(task) PRINT_ERROR("Failed to print task ##task");

int main(int argc, char *argv[])
{
    char *bin_folder_path = tumUtilGetBinFolderPath(argv[0]);

    prints("Initializing: ");

    //  Note PRINT_ERROR is not thread safe and is only used before the
    //  scheduler is started. There are thread safe print functions in
    //  TUM_Print.h, `prints` and `fprints` that work exactly the same as
    //  `printf` and `fprintf`. So you can read the documentation on these
    //  functions to understand the functionality.

    if (tumDrawInit(bin_folder_path)) {
        PRINT_ERROR("Failed to intialize drawing");
        goto err_init_drawing;
    }
    else {
        prints("drawing");
    }

    if (tumEventInit()) {
        PRINT_ERROR("Failed to initialize events");
        goto err_init_events;
    }
    else {
        prints(", events");
    }

    if (tumSoundInit(bin_folder_path)) {
        PRINT_ERROR("Failed to initialize audio");
        goto err_init_audio;
    }
    else {
        prints(", and audio\n");
    }

    if (safePrintInit()) {
        PRINT_ERROR("Failed to init safe print");
        goto err_init_safe_print;
    }

    logo_image = tumDrawLoadImage(LOGO_FILENAME);

    atexit(aIODeinit);

    //Load a second font for fun
    tumFontLoadFont(FPS_FONT, DEFAULT_FONT_SIZE);

    buttons.lock = xSemaphoreCreateMutex(); // Locking mechanism
    if (!buttons.lock) {
        PRINT_ERROR("Failed to create buttons lock");
        goto err_buttons_lock;
    }

    DrawSignal = xSemaphoreCreateBinary(); // Screen buffer locking
    if (!DrawSignal) {
        PRINT_ERROR("Failed to create draw signal");
        goto err_draw_signal;
    }
    ScreenLock = xSemaphoreCreateMutex();
    if (!ScreenLock) {
        PRINT_ERROR("Failed to create screen lock");
        goto err_screen_lock;
    }

    // Message sending
    StateQueue = xQueueCreate(STATE_QUEUE_LENGTH, sizeof(unsigned char));
    if (!StateQueue) {
        PRINT_ERROR("Could not open state queue");
        goto err_state_queue;
    }

    if (xTaskCreate(basicSequentialStateMachine, "StateMachine",
                    mainGENERIC_STACK_SIZE * 2, NULL,
                    configMAX_PRIORITIES - 1, &StateMachine) != pdPASS) {
        PRINT_TASK_ERROR("StateMachine");
        goto err_statemachine;
    }
    if (xTaskCreate(vSwapBuffers, "BufferSwapTask",
                    mainGENERIC_STACK_SIZE * 2, NULL, configMAX_PRIORITIES,
                    &BufferSwap) != pdPASS) {
        PRINT_TASK_ERROR("BufferSwapTask");
        goto err_bufferswap;
    }

    /** Demo Tasks */
    if (xTaskCreate(vExercise2, "Exercise 2", mainGENERIC_STACK_SIZE * 2,
                    NULL, mainGENERIC_PRIORITY, &DemoTask1) != pdPASS) {
        PRINT_TASK_ERROR("DemoTask1");
        goto err_demotask1;
    }
    if (xTaskCreate(vExercise4, "Exercise 4", mainGENERIC_STACK_SIZE * 2,
                    NULL, 10, &Exercise4) != pdPASS) {
        PRINT_TASK_ERROR("DemoTask2");
        goto err_demotask2;
    }
    if (xTaskCreate(vCircle1, "Circle1",  mainGENERIC_STACK_SIZE * 2,
                    NULL, mainGENERIC_PRIORITY+1, &Circle1) != pdPASS) {
        PRINT_TASK_ERROR("DemoTask2");
        goto err_circle1;
    }
    if (xTaskCreate(vCircle2, "Circle2", mainGENERIC_STACK_SIZE * 2,
                    NULL, mainGENERIC_PRIORITY, &Circle2) != pdPASS) {
        PRINT_TASK_ERROR("DemoTask2");
        goto err_circle1;
    }
    if (xTaskCreate(vExercise3, "Exercise3", mainGENERIC_STACK_SIZE * 2,
                    NULL, mainGENERIC_PRIORITY+5, &Exercise3) != pdPASS) {
        PRINT_TASK_ERROR("DemoTask2");
    }      
    Task1Trigger = xSemaphoreCreateBinary();
    counterlock = xSemaphoreCreateBinary();
    if (xTaskCreate(vRandTask1, "RandTask1", mainGENERIC_STACK_SIZE * 2,
                    NULL, mainGENERIC_PRIORITY+1, &RandTask1) != pdPASS) {
        PRINT_TASK_ERROR("DemoTask2");
        goto err_circle1;
    }
    if (xTaskCreate(vRandTask2, "RandTask2", mainGENERIC_STACK_SIZE * 2,
                    NULL, mainGENERIC_PRIORITY, &RandTask2) != pdPASS) {
        PRINT_TASK_ERROR("DemoTask2");
        goto err_circle1;
    }
    if (xTaskCreate(vTimedreset, "timed reset", mainGENERIC_STACK_SIZE * 2,
                    NULL, mainGENERIC_PRIORITY, &timedreset) != pdPASS) {
        PRINT_TASK_ERROR("DemoTask2");
        goto err_circle1;
    }
    if (xTaskCreate(vIncremet, "Task Incriment", mainGENERIC_STACK_SIZE * 2,
                    NULL, mainGENERIC_PRIORITY, &increment) != pdPASS) {
        PRINT_TASK_ERROR("DemoTask2");
        goto err_circle1;
    }
    if (xTaskCreate(v4_1, "Task Ex4.1", mainGENERIC_STACK_SIZE * 2,
                    NULL, 1, &Ex4_1) != pdPASS) {
        PRINT_TASK_ERROR("4.1");
        goto err_circle1;
    }
    if (xTaskCreate(v4_2, "Task Ex4.2", mainGENERIC_STACK_SIZE * 2,
                    NULL, 2, &Ex4_2) != pdPASS) {
        PRINT_TASK_ERROR("4.2");
        goto err_circle1;
    }
    task4_3 = xSemaphoreCreateBinary();
    if (xTaskCreate(v4_3, "Task Ex4.3", mainGENERIC_STACK_SIZE * 2,
                    NULL, 3, &Ex4_3) != pdPASS) {
        PRINT_TASK_ERROR("4.3");
        goto err_circle1;
    }
    if (xTaskCreate(v4_4, "Task Ex4.4", mainGENERIC_STACK_SIZE * 2,
                    NULL, 4, &Ex4_4) != pdPASS) {
        PRINT_TASK_ERROR("4.4");
        goto err_circle1;
    }if (xTaskCreate(vTickMaster, "Task Tick Master", mainGENERIC_STACK_SIZE * 2,
                    NULL, 5, &TickMaster) != pdPASS) {
        PRINT_TASK_ERROR("4.4");
        goto err_circle1;
    }

    vTaskSuspend(DemoTask1);
    vTaskSuspend(Exercise4);
    vTaskSuspend(Circle1);
    vTaskSuspend(increment);
    vTaskSuspend(Ex4_1);
    vTaskSuspend(Ex4_2);
    vTaskSuspend(Ex4_3);
    vTaskSuspend(Ex4_4);
    vTaskSuspend(TickMaster);
    //vTaskSuspend(RandTask1);

    tumFUtilPrintTaskStateList();

    vTaskStartScheduler();

    return EXIT_SUCCESS;

err_demotask2:
    vTaskDelete(DemoTask1);
err_demotask1:
    vTaskDelete(BufferSwap);
err_circle1:
    vTaskDelete(Circle1);
err_bufferswap:
    vTaskDelete(StateMachine);
err_statemachine:
    vQueueDelete(StateQueue);
err_state_queue:
    vSemaphoreDelete(ScreenLock);
err_screen_lock:
    vSemaphoreDelete(DrawSignal);
err_draw_signal:
    vSemaphoreDelete(buttons.lock);
err_buttons_lock:
    tumSoundExit();
err_init_audio:
    tumEventExit();
err_init_events:
    tumDrawExit();
err_init_drawing:
    safePrintExit();
err_init_safe_print:
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