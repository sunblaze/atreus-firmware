#include <stdlib.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <string.h>
#include <util/delay.h>
#include "usb_keyboard.h"

#define DEBOUNCE_PASSES 10

#define CPU_PRESCALE(n) (CLKPR = 0x80, CLKPR = (n))

// Layout setup

void reset(void);
void fn_pressed();
void fn2_pressed();

#define ROW_COUNT 4
#define COL_COUNT 11
#define KEY_COUNT ROW_COUNT*COL_COUNT

volatile char * row_ports[ROW_COUNT] = {&PORTD, &PORTD, &PORTD, &PORTD};
volatile char * row_dirs[ROW_COUNT] = {&DDRD, &DDRD, &DDRD, &DDRD};
int row_pins[ROW_COUNT] = {0, 1, 3, 2};

#ifdef SWAPCOLUMNS
volatile char * col_ports[COL_COUNT] = {&PIND, &PINC, &PINB, &PINB, &PINE, \
                                        &PIND,                          \
                                        &PINB, &PINF, &PINF, &PIND, &PINB};
#else
volatile char * col_ports[COL_COUNT] = {&PINB, &PIND, &PINF, &PINF, &PINB, \
                                        &PIND,                          \
                                        &PINE, &PINB, &PINB, &PINC, &PIND};
#endif

#ifdef SWAPCOLUMNS
int col_pins[COL_COUNT] = {7, 6, 5, 4, 6, 4, 6, 6, 7, 6, 7};
#else
int col_pins[COL_COUNT] = {7, 6, 7, 6, 6, 4, 6, 4, 5, 6, 7};
#endif

// b7 b6 f7 f6 b6
// e6 b4 b5 c6 d7

int pressed_count = 0;
int presses[KEY_COUNT];
int last_pressed_count = 0;
int last_presses[KEY_COUNT];

#define FN_PRESSED 0x01
#define FN2_PRESSED 0x02
uint8_t fn_keys_pressed = 0;

int active_layer[KEY_COUNT];

#define CTRL(key)   (0x1000 + (key))
#define SHIFT(key)  (0x2000 + (key))
#define ALT(key)    (0x4000 + (key))
#define GUI(key)    (0x8000 + (key))

/* LAYERS and FUNCTIONS are pessimistic, there's 4095 unused numbers between
 * the USB_MAX_KEY and the CTRL mask bit.  */
#define LAYERS 64
#define FUNCTIONS 255

#define MIN_LAYER         (USB_MAX_KEY      + 1)
#define MAX_LAYER         (USB_MAX_KEY      + LAYERS)
#define MIN_FUNCTION      (MAX_LAYER        + 1)
#define MAX_FUNCTION      (MAX_LAYER        + FUNCTIONS)
#define MIN_PRE_FUNCTION  (MAX_FUNCTION     + 1)
#define MAX_PRE_FUNCTION  (MAX_FUNCTION     + FUNCTIONS)

#define LAYER(layer)          (MIN_LAYER        + (layer))
#define FUNCTION(number)      (MIN_FUNCTION     + (number))
#define PRE_FUNCTION(number)  (MIN_PRE_FUNCTION + (number))

// layout.h must define:
// * layers: array of int[KEY_COUNT]
// * layer_functions: array of void function pointers
// ... plus any functions included in layer_functions
// per_cycle void function callback
#include "layout.h"


// Matrix scanning logic

void record(int col, int row) {
  presses[pressed_count++] = (row * COL_COUNT) + col;
};

void deactivate_rows() {
  for (int r = 0; r < ROW_COUNT; r++) {
    *(row_ports[r]) |= (char)(1 << row_pins[r]);
  }
}

void activate_row(int row) {
  deactivate_rows();
  *(row_ports[row]) &= (char)(~(1 << row_pins[row]));
  _delay_us(50);
};

void scan_row(int row) {
  for(int col = 0; col < COL_COUNT; col++) {
    if(~(*(col_ports[col])) & (1 << col_pins[col])) {
      record(col, row);
    }
  }
};

void scan_rows() {
  pressed_count = 0;
  for(int i = 0; i < ROW_COUNT; i++) {
    activate_row(i);
    scan_row(i);
  };
};


// Cycle functions

void debounce(int passes_remaining) {
  while(passes_remaining) {
    last_pressed_count = pressed_count;
    for(int i = 0; i < last_pressed_count; i++) {
      last_presses[i] = presses[i];
    }

    scan_rows();

    if((pressed_count != last_pressed_count) || \
       memcmp(presses, last_presses, pressed_count)) {
      passes_remaining = DEBOUNCE_PASSES;
    } else {
      passes_remaining--;
    }
  }
};

void pre_invoke_functions() {
  for(int i = 0; i < pressed_count; i++) {
    // PRE_FUNCTIONS only work on first layer (layer 0)
    unsigned int keycode = layers[0][presses[i]];
    if(keycode >= MIN_PRE_FUNCTION && keycode <= MAX_PRE_FUNCTION) {
      (layer_functions[keycode - MIN_PRE_FUNCTION])();
    }
  }
  per_cycle();
};

void calculate_presses() {
  int usb_presses = 0;
  for(int i = 0; i < pressed_count; i++) {
    unsigned int keycode = active_layer[presses[i]];

    if(keycode >= MIN_FUNCTION && keycode <= MAX_FUNCTION) {
      // regular layout functions
      (layer_functions[keycode - MIN_FUNCTION])();
    } else if(keycode >= MIN_PRE_FUNCTION && keycode <= MAX_PRE_FUNCTION) {
      // pre-invoke functions have already been processed
    } else if(keycode >= MIN_LAYER && keycode <= MAX_LAYER) {
      //TODO MIN_LAYER and MAX_LAYER should be removed since I don't care for them
    } else if(keycode == KEYBOARD_LEFT_CTRL) {
      keyboard_modifier_keys |= KEY_LEFT_CTRL;
    } else if(keycode == KEYBOARD_RIGHT_CTRL) {
      keyboard_modifier_keys |= KEY_RIGHT_CTRL;
    } else if(keycode == KEYBOARD_LEFT_SHIFT) {
      keyboard_modifier_keys |= KEY_LEFT_SHIFT;
    } else if(keycode == KEYBOARD_RIGHT_SHIFT) {
      keyboard_modifier_keys |= KEY_RIGHT_SHIFT;
    } else if(keycode == KEYBOARD_LEFT_ALT) {
      keyboard_modifier_keys |= KEY_LEFT_ALT;
    } else if(keycode == KEYBOARD_RIGHT_ALT) {
      keyboard_modifier_keys |= KEY_RIGHT_ALT;
    } else if(keycode == KEYBOARD_LEFT_GUI) {
      keyboard_modifier_keys |= KEY_LEFT_GUI;
    } else if(keycode == KEYBOARD_RIGHT_GUI) {
      keyboard_modifier_keys |= KEY_RIGHT_GUI;
    } else if(keycode > 255 && usb_presses < 6) {
      // modifier plus keypress
      keyboard_modifier_keys |= (keycode >> 8);
      keyboard_keys[usb_presses++] = (keycode & 255);
    } else if(usb_presses < 6){
      // keypress
      keyboard_keys[usb_presses++] = keycode;
    };
  };
};


// Top level stuff

void init() {
  CPU_PRESCALE(0);
  DDRB = DDRC = DDRE = DDRF = DDRD = 0; // set everything to input
  PORTB = PORTC = PORTE = PORTF = PORTD = 255; // enable pullups
  // set the row pins as outputs
  for (int r = 0; r < ROW_COUNT; r++) {
    *(row_dirs[r]) |= (char)(1 << row_pins[r]);
  }

#ifdef SWAPCOLUMNS
// This swaps middle two keys in case PCB was flipped (since those are on the same column)
  layer0[27]^=layer0[38]^=layer0[27]^=layer0[38];
  layer1[27]^=layer1[38]^=layer1[27]^=layer1[38];
  layer2[27]^=layer2[38]^=layer2[27]^=layer2[38];
#endif

  deactivate_rows();
  usb_init();
  while (!usb_configured()) /* wait */ ;
  _delay_ms(500);
};

void clear_keys() {
  keyboard_modifier_keys = 0;
  for(int i = 0; i < 6; i++) {
    keyboard_keys[i] = 0;
  };
  fn_keys_pressed = 0;
};

void init_active_layer() {
  for(int i = 0; i < KEY_COUNT; ++i) {
    active_layer[i] = layers[0][i];
  }
}

void update_active_layer() {
  for(int active_index = 0; active_index < KEY_COUNT; ++active_index) {
    int is_pressed = 0;
    for(int pressed_index = 0; pressed_index < pressed_count; ++pressed_index) {
      if(presses[pressed_index] == active_index){
        is_pressed = 1;
      }
    }
    if(!is_pressed){
      active_layer[active_index] = layers[fn_keys_pressed][active_index];
    }
  }
}

int main() {
  init();
  init_active_layer();
  while(1) {
    clear_keys();
    debounce(DEBOUNCE_PASSES);
    pre_invoke_functions();
    calculate_presses();
    usb_keyboard_send();
    update_active_layer();
  };
};

void reset(void) {
  UDCON = 1; USBCON = (1<<FRZCLK); UCSR1B = 0;
  _delay_ms(5);
  EIMSK = 0; PCICR = 0; SPCR = 0; ACSR = 0; EECR = 0; ADCSRA = 0;
  TIMSK0 = 0; TIMSK1 = 0; TIMSK3 = 0; TIMSK4 = 0; UCSR1B = 0; TWCR = 0;
  DDRB = 0; DDRC = 0; DDRD = 0; DDRE = 0; DDRF = 0; TWCR = 0;
  PORTB = 0; PORTC = 0; PORTD = 0; PORTE = 0; PORTF = 0;
  *(uint16_t *)0x0800 = 0x7777;
  wdt_enable(WDTO_120MS);
};

void fn_pressed() {
  fn_keys_pressed |= FN_PRESSED;
}

void fn2_pressed() {
  fn_keys_pressed |= FN2_PRESSED;
}
