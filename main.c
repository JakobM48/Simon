#include <xc.h>
#include <stdlib.h>
#include "pic18f4321-Config.h"
#include "lcd.h"

#define _XTAL_FREQ 4000000

/* 
 * lcd pinout:
 *    d2 d3 d4 d5 d6 d7
 *    rs en d4 d5 d6 d7
 */

//configuration bits
#pragma config OSC = INTIO2
#pragma config WDT = OFF
#pragma config LVP = OFF

//tris defs
#define redButton TRISBbits.TRISB4
#define greenButton TRISBbits.TRISB5
#define yellowButton TRISBbits.TRISB6
#define blueButton TRISBbits.TRISB7
#define redLED TRISBbits.TRISB0
#define greenLED TRISBbits.TRISB1
#define yellowLED TRISBbits.TRISB2
#define blueLED TRISBbits.TRISB3

//button inputs and LED outputs
#define redValue PORTBbits.RB4
#define greenValue PORTBbits.RB5
#define yellowValue PORTBbits.RB6
#define blueValue PORTBbits.RB7
#define redOn LATBbits.LATB0
#define greenOn LATBbits.LATB1
#define yellowOn LATBbits.LATB2
#define blueOn LATBbits.LATB3

//buzzer logic
#define BUZZER_TRIS TRISDbits.TRISD0
#define BUZZER_LAT  LATDbits.LATD0

// Half-periods in microseconds for Simon tones
//                           Chord  Bb7
#define TONE_G_NOTE 86   // (Green)  A
#define TONE_R_NOTE 110  // (Red)    F
#define TONE_Y_NOTE 166  // (Yellow) Bb
#define TONE_B_NOTE 132  // (Blue)   D

//printing vs reading
#define WATCHING 0
#define PLAYING 1
#define WELCOME 3 
#define GAME_OVER 4 
#define BUFFER 5
#define READY 6
#define SOUND_TEST 7

//initialize gamemode
uint8_t MODE= WATCHING;

//variable for timer
volatile uint32_t ms_count = 0;

//loop durations
const uint32_t cycle_time = 100;
const uint32_t blink_time = 50;
const uint32_t buffer_max = 1500;
const uint32_t welcome_switch = 2000;
const uint32_t game_over_switch = 2000;
const uint32_t ready_cycle = 500;

//prototypes
void all_off(void);
void all_on(void);
void save_high_score(uint8_t score);
uint8_t read_high_score(void);
void play_tone(uint16_t half_period, uint16_t duration_ms);
void play_color(int color_index, int duration);

//interrupt service block
void __interrupt() high_isr(void)
{
    if (INTCONbits.TMR0IF)
    {
        TMR0H = 0xFF;
        TMR0L = 0x06;
        
        ms_count++;
        INTCONbits.TMR0IF = 0;
    }
}

//initial setup function
void setup(void)
{   
    //internal osc setup
    OSCCONbits.IRCF = 0b110;
    
    //slight delay to wake up lcd
    __delay_ms(100);
    
    //initialize lcd
    LCD_init();
    
    //sets pins to digital
    ADCON1 = 0x0F;
    
    //I/O setup
    redButton = 1;
    greenButton = 1;
    yellowButton = 1;
    blueButton = 1;
    
    redLED = 0;
    greenLED = 0;
    yellowLED = 0;
    blueLED = 0;
    
    //sets welcome game mode
    MODE = WELCOME;
    
    //timer0 setup
    T0CONbits.T08BIT = 0;
    T0CONbits.T0CS = 0;
    T0CONbits.PSA = 0;
    T0CONbits.T0PS = 0b001;
    
    TMR0H = 0xFF;
    TMR0L = 0x06;
    
    
    //enable interrupts
    INTCONbits.TMR0IE = 1;
    INTCONbits.GIE = 1;
    
    T0CONbits.TMR0ON = 1;
    
    //resets high score if default is 255
    if (read_high_score() == 255) 
    {
    save_high_score(0); 
    }
}

void main(void)
{
    setup();
    all_off();
            
    //initialize button states (active high)
    uint8_t last_red_state = 1;
    uint8_t last_green_state = 1;
    uint8_t last_yellow_state = 1;
    uint8_t last_blue_state = 1;
    
    //default seed for randomness
    int seed = 123;
    int random = 0;
    
    //sequence to store random buttons
    int sequence[100];
    int current_length = 0;
    
    //cycle for welcome screen
    uint32_t last_cycle = 0;
    
    //variable to keep track of player input
    int current_index = 0;
    
    //keeps track if LED's are on
    uint8_t led_on = 0;
    
    //timer trackers
    uint32_t buffer = 0;
    uint32_t welcome_cycle = 0;
    uint32_t ready_time = 0;
    uint32_t game_over_cycle = 0;
    uint32_t last_tone = 0;
    
    //screen toggles
    uint8_t welcome_screen = 0;
    uint8_t game_over_toggle = 0;
    
    //countdown tracker
    int ready_state = 0;
    
    //game over state
    int game_over = 0;
    
    //new best condition
    uint8_t new_best = 0;
    
    //set all press conditions to false
    uint8_t redPressed = 0, greenPressed = 0, yellowPressed = 0, bluePressed = 0;
    
    while(1)
    {
        //update press conditions every loop
        redPressed = (last_red_state == 1 && redValue == 0);
        greenPressed = (last_green_state == 1 && greenValue == 0);
        yellowPressed = (last_yellow_state == 1 && yellowValue == 0);
        bluePressed = (last_blue_state == 1 && blueValue == 0);
        
        //welcome mode block
        if (MODE == WELCOME)
        {   
            //admin controls 
            if (redValue == 0)
            {              
                if (last_red_state == 1)
                {
                    LCD_clear();
                    LCD_cursor_set(1, 1);
                    LCD_write_string("r+g = reset");
                    LCD_cursor_set(2, 1);
                    LCD_write_string("r+y = sound test");
                }          
                //reset high score
                if (greenPressed)
                {
                    save_high_score(0);
                    LCD_cursor_set(2, 1);
                    LCD_write_string("Score wiped!");                    
                }
                //sound test function
                else if(yellowPressed)
                {
                    MODE = SOUND_TEST;
                    LCD_clear();
                    LCD_cursor_set(1, 1);
                    LCD_write_string("Sound Test");
                    last_tone = ms_count;
                }
            }    
            //game start
            else if (greenPressed)
            {
                LCD_clear();
                all_off();
                ready_time = ms_count;
                MODE = READY;
                
                //sets random seed on game start
                srand((unsigned int)ms_count);
                
                //variable defaults
                current_length = 0;
                ready_state = 0;
                game_over = 0;
                new_best = 0;
            }
            
            //welcome cycle
            else
            {
                if (ms_count - welcome_cycle >= welcome_switch)
                {
                    welcome_cycle = ms_count;
                    
                    //instructions
                    if (welcome_screen == 0)
                    {
                        all_on();
                        LCD_clear();
                        LCD_cursor_set(1, 1);
                        LCD_write_string("Welcome to SIMON");
                        LCD_cursor_set(2, 1);
                        LCD_write_string("Green = start");
                    }
                    //display high score
                    else
                    {
                        all_off();
                        LCD_clear();
                        LCD_cursor_set(1, 2);
                        LCD_write_string("Highscore: ");
                        LCD_write_variable((int32_t)read_high_score(), 2);
                    }
                    //toggle screen
                    welcome_screen = !welcome_screen;
                }
            }
        }
        //countdown sequence
        else if (MODE == READY)
        {
            //blink every half cycle
            if (ms_count - ready_time >= (ready_cycle / 2))
            {
                all_off();
            }
            if (ms_count - ready_time >= ready_cycle)
            {
                ready_time = ms_count;                
                switch (ready_state)
                {
                    case 0:
                        all_on();
                        LCD_clear();
                        LCD_cursor_set(1, 1);
                        LCD_write_string("3...");
                        break;
                    case 1:
                        all_on();
                        LCD_clear();
                        LCD_cursor_set(1, 1);
                        LCD_write_string("2...");
                        break;
                    case 2:
                        all_on();
                        LCD_clear();
                        LCD_cursor_set(1, 1);
                        LCD_write_string("1...");
                        break;
                    case 3:
                        LCD_clear();
                        LCD_cursor_set(1, 1);
                        LCD_write_string("GO!");
                        break;
                    default:
                        ready_state = 0;
                        all_off();
                        LCD_clear();
                        
                        LCD_cursor_set(1, 1);
                        LCD_write_string("Current Score: ");
                        LCD_cursor_set(2, 1);
                        LCD_write_variable((int32_t)(current_length), 1);
                        
                        //populates first sequence
                        sequence[current_length] = rand() % 4; 
                        current_length++;                      
                        current_index = 0;
                        MODE = WATCHING;
                        break;
                }
                ready_state++;
            }
        }
        else if (MODE == WATCHING)
        {
            if (ms_count - last_cycle >= cycle_time)
            {
                //cycles through sequence
                play_color(sequence[current_index], blink_time);
                current_index++;
                
                last_cycle = ms_count;

                //when sequence is done, go to playing phase
                if (current_index >= current_length)
                {
                    MODE = PLAYING;
                    current_index = 0;
                    last_cycle = ms_count;
                }
            }
        }
        else if (MODE == PLAYING)
        {
            //checks if a button is pressed
            if (redPressed || greenPressed || yellowPressed || bluePressed) 
            {
                uint8_t pressed = 0;
                if (redPressed)         { pressed = 0; }
                else if (greenPressed)  { pressed = 1; }
                else if (yellowPressed) { pressed = 2; }
                else if (bluePressed)   { pressed = 3; }

                if (sequence[current_index] == pressed) 
                {
                    // Valid press: provide audio/visual feedback
                    play_color(pressed, blink_time);

                    current_index++;
                    last_cycle = ms_count; // Reset timeout/idle timers

                    //if matched the current sequence, switch to buffer
                    if (current_index == current_length) {
                        buffer = ms_count;
                                 
                        //updates current score
                        LCD_clear();
                        LCD_cursor_set(1, 1);
                        LCD_write_string("Current Score: ");
                        LCD_cursor_set(2, 1);
                        LCD_write_variable((int32_t)(current_length), 1);

                        MODE = BUFFER;
                    }
                } 
                else 
                {
                    // WRONG: Play a very low frequency 'buzz'
                    play_tone(500, 100); 
                    MODE = GAME_OVER;
                    game_over_cycle = ms_count;
                }
            }
        }
        //slight buffer between user input and playing next cycle
        else if (MODE == BUFFER)
        {
            if (ms_count - buffer >= buffer_max)
            {              
                all_off();
                
                //populates next num in sequence before playing sequence
                sequence[current_length] = rand() % 4; 
                current_length++;                      
                current_index = 0;
                MODE = WATCHING;
            }
        }
        //game over state
        else if (MODE == GAME_OVER)
        {     
            uint8_t current_score = (uint8_t)current_length - 1;
            if (game_over == 0)
            {
                //checks if new score is better than eeprom memory
                uint8_t old_record = read_high_score();
                
                //if so, replaces eeprom score
                if (current_score > old_record)
                {
                    save_high_score(current_score);
                    new_best = 1;
                }
                game_over = 1;
            }
            //green press goes back to welcome
            if (greenPressed)
            {
                MODE = WELCOME;
            }
            //screen cycle
            else
            {
                if (ms_count - game_over_cycle >= game_over_switch)
                {          
                    game_over_cycle = ms_count;
                    //displays score
                    if (game_over_toggle == 0)
                    {
                        all_on();
                        LCD_clear();
                        LCD_cursor_set(1, 1);
                        LCD_write_string("Game over!");
                        LCD_cursor_set(2, 1);
                        if (new_best == 1)
                        {
                            LCD_write_string("New Best: ");
                            LCD_write_variable((int32_t)current_score, 2);
                        }
                        else
                        {
                            LCD_write_string("Score: ");
                            LCD_write_variable((int32_t)current_score, 2);
                        }
                    }
                    //instructions
                    else
                    {
                        all_off();
                        LCD_clear();
                        LCD_cursor_set(1, 1);
                        LCD_write_string("Game over!");
                        LCD_cursor_set(2, 1);
                        LCD_write_string("Green = Again");
                    }
                    game_over_toggle = !game_over_toggle;
                }
            }
        }
        //sound testing block
        else if(MODE == SOUND_TEST)
        {
            int8_t pressed = -1;
            if (redValue == 0)         { pressed = 0; }
            else if (greenValue == 0)  { pressed = 1; }
            else if (yellowValue == 0) { pressed = 2; }
            else if (blueValue == 0)   { pressed = 3; }
            
            //if pressed, play for a second
            if (pressed != -1)
            {
                play_color(pressed, 100);
                last_tone = ms_count;              
            }  
            //if nothing pressed in 5 sec, go back to welcome
            else if (ms_count - last_tone >= 5000)
            {MODE = WELCOME; LCD_clear(); }
        }
        
        //updating last state for buttons for state logic
        last_red_state = redValue;
        last_green_state = greenValue;
        last_yellow_state = yellowValue;
        last_blue_state = blueValue;  
        
        __delay_ms(10);
    }
}

//all led off
void all_off(void)
{
    redOn = 0;
    greenOn = 0;
    yellowOn = 0;
    blueOn = 0;
}
//all led on
void all_on(void)
{
    redOn = 1;
    greenOn = 1;
    yellowOn = 1;
    blueOn = 1;
}
//saving high score to eeprom
void save_high_score(uint8_t score) {
    // 1. READ FIRST
    uint8_t existing_score = read_high_score();
    
    // 2. ONLY WRITE IF DIFFERENT
    if (existing_score == score) 
    {
        return; // Exit early, no write cycle used!
    }
    EEADR = 0x00;           // High score location (Address 0)
    EEDATA = score;         // The value to write (0-255)
    
    EECON1bits.EEPGD = 0;   // Point to Data EEPROM
    EECON1bits.CFGS = 0;    // Access EEPROM, not Config registers
    EECON1bits.WREN = 1;    // Enable write cycles
    
    // Save interrupt state and disable them
    uint8_t gie_status = INTCONbits.GIE; 
    INTCONbits.GIE = 0;     
    
    // Required Unlock Sequence (Exact timing required)
    EECON2 = 0x55; 
    EECON2 = 0xAA;
    EECON1bits.WR = 1;      // Start the write
    
    // Restore interrupts
    INTCONbits.GIE = gie_status; 
    
    // Wait for the hardware to finish writing (Self-timed)
    while(EECON1bits.WR);   
    
    EECON1bits.WREN = 0;    // Disable writes (Safety first!)
}

//reading score from eeprom
uint8_t read_high_score(void) {
    EEADR = 0x00;           // Address we want to read
    EECON1bits.EEPGD = 0;   // Point to Data EEPROM
    EECON1bits.CFGS = 0;    // Access EEPROM
    EECON1bits.RD = 1;      // Trigger the read
    
    return EEDATA;          // Result is available in EEDATA immediately
}

//playing pitch with pwm
void play_tone(uint16_t half_period, uint16_t duration_ms) 
{
    BUZZER_TRIS = 0;
    // Pre-calculate the number of cycles so the loop is fast
    // Each cycle is 2 * half_period.
    uint32_t total_us = (uint32_t)duration_ms * 1000;
    uint32_t cycles = total_us / (half_period * 2);

    for (uint32_t i = 0; i < cycles; i++) {
        BUZZER_LAT = 1;
        // Use the built-in delay but keep it tight
        for(uint16_t d = 0; d < half_period; d++) __delay_us(1); 
        BUZZER_LAT = 0;
        for(uint16_t d = 0; d < half_period; d++) __delay_us(1);
    }
}

//playing color and pitch associated with color
void play_color(int color_index, int duration) 
{
    all_off();
    uint16_t pitch = 0;
    
    switch(color_index) {
        case 0: redOn = 1;    pitch = TONE_R_NOTE; break;
        case 1: greenOn = 1;  pitch = TONE_G_NOTE; break;
        case 2: yellowOn = 1; pitch = TONE_Y_NOTE; break;
        case 3: blueOn = 1;   pitch = TONE_B_NOTE; break;
    }
    
    play_tone(pitch, duration);
    all_off();
}