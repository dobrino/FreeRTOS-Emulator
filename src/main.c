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

#define STATE_QUEUE_LENGTH 1

#define STATE_COUNT 2

#define STATE_ONE 0
#define STATE_TWO 1

#define NEXT_TASK 0
#define PREV_TASK 1

#define STATE_DEBOUNCE_DELAY 300

#define STARTING_STATE STATE_ONE

static QueueHandle_t StateQueue = NULL;

const unsigned char next_state_signal = NEXT_TASK;
const unsigned char prev_state_signal = PREV_TASK;

static TaskHandle_t DemoTask1 = NULL;
static TaskHandle_t DemoTask2 = NULL;
static TaskHandle_t StateMachine = NULL;

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

void vEx2_2(void *pvParameters)
{
    // structure to store time retrieved from Linux kernel
    static struct timespec the_time;
    static char our_time_string[100];
    static int our_time_strings_width = 0;

    // Needed such that Gfx library knows which thread controlls drawing
    // Only one thread can call tumDrawUpdateScreen while and thread can call
    // the drawing functions to draw objects. This is a limitation of the SDL
    // backend.

    //Instanciating and Initialiying Coordinates and parameters for objects//
    //CIRCLE
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
    // int a_counter_en = 0;
    // int b_counter_en = 0;
    // int c_counter_en = 0;
    // int d_counter_en = 0;
    // MOUSE COORD
    coord_t mouse_coord;
    mouse_coord.x = 0;
    mouse_coord.y = 0;
    
    tumDrawBindThread();

    while (1) {
        tumEventFetchEvents(FETCH_EVENT_NONBLOCK); // Query events backend for new events, ie. button presses
        xGetButtonInput(); // Update global input

        // `buttons` is a global shared variable and as such needs to be
        // guarded with a mutex, mutex must be obtained before accessing the
        // resource and given back when you're finished. If the mutex is not
        // given back then no other task can access the reseource.
        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(
                                    Q)]) { // Equiv to SDL_SCANCODE_Q
                exit(EXIT_SUCCESS);
            }
            xSemaphoreGive(buttons.lock);
        }

        tumDrawClear(White); // Clear screen

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

        clock_gettime(CLOCK_REALTIME,
                      &the_time); // Get kernel real time

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


        tumDrawUpdateScreen(); // Refresh the screen to draw string

        // Get input and check for state change
        //vCheckStateInput();

        // Basic sleep of 20 milliseconds
        vTaskDelay((TickType_t)100);
    }
}

void vDemoTask2(void *pvParameters)
{
    while(1)
    {   
        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(
                                    Q)]) { // Equiv to SDL_SCANCODE_Q
                exit(EXIT_SUCCESS);
            }
            xSemaphoreGive(buttons.lock);
        }
        
        tumDrawClear(White);
        // Get input and check for state change
        vCheckStateInput();

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

//STATE MACHINE

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

int vCheckStateInput(void)
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
        //Handle current state
        if (state_changed) {
            switch (current_state) {
                case STATE_ONE:
                    if (DemoTask2) {
                        vTaskSuspend(DemoTask2);
                    }
                    if (DemoTask1) {
                        vTaskResume(DemoTask1);
                    }
                    break;
                case STATE_TWO:
                    if (DemoTask1) {
                        vTaskSuspend(DemoTask1);
                    }
                    if (DemoTask2) {
                        vTaskResume(DemoTask2);
                    }
                    break;
                default:
                    break;
            }
            state_changed = 0;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
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

    // if(xTaskCreate(basicSequentialStateMachine, "StateMachine",
    //                   mainGENERIC_STACK_SIZE * 2, NULL,
    //                   configMAX_PRIORITIES - 1, &StateMachine) != pdPASS){
    //      printf("Error Statemachine");
    //      goto err_statemachine;
    // }

    if (xTaskCreate(vEx2_2, "Button Task", mainGENERIC_STACK_SIZE * 2, NULL, 
                    mainGENERIC_PRIORITY + 1, &DemoTask1) != pdPASS) {
        printf("Error DemoTask1");
        goto err_buttontask;
    }
    // if(xTaskCreate(vDemoTask2, "Demo Task 2",
    //                  mainGENERIC_STACK_SIZE * 2, NULL,
    //                  mainGENERIC_PRIORITY, &DemoTask2) != pdPASS){
    //     printf("Error DemoTask2");         
    // }


    vTaskSuspend(DemoTask1);
    vTaskSuspend(DemoTask2);   



    vTaskStartScheduler();

    return EXIT_SUCCESS;

err_buttontask:
    vSemaphoreDelete(buttons.lock);
    vTaskDelete(DemoTask1);
err_statemachine:
    vTaskDelete(StateMachine);
err_buttons_lock:
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