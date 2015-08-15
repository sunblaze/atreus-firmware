int fn_decay = 0;
int fn_active = 0;
int fn2_active = 0;

void activate_fn() {
  fn_decay = 20;
  fn_active = 1;
}

void activate_fn2() {
  fn_decay = 20;
  fn2_active = 1;
}

int mod4_key = 0;
int mod4_state = 0;
void mod4_any(int key_code) {
  if(mod4_state == 0){
    if(mod4_key == 0){
      mod4_key = key_code;
      mod4_state = 5;
    }else if(mod4_key == key_code){
      mod4_state = 1;
    }
  }
}

void mod4_q(){
  mod4_any(KEY_Q);
}

void mod4_w(){
  mod4_any(KEY_W);
}

void mod4_e(){
  mod4_any(KEY_E);
}

void mod4_r(){
  mod4_any(KEY_R);
}

void mod4_a(){
  mod4_any(KEY_A);
}

void mod4_s(){
  mod4_any(KEY_S);
}

void mod4_d(){
  mod4_any(KEY_D);
}

void mod4_f(){
  mod4_any(KEY_F);
}

void mod4_z(){
  mod4_any(KEY_Z);
}

void mod4_x(){
  mod4_any(KEY_X);
}

void mod4_c(){
  mod4_any(KEY_C);
}

void mod4_v(){
  mod4_any(KEY_V);
}

void mod4_g(){
  mod4_any(KEY_G);
}

void mod4_b(){
  mod4_any(KEY_B);
}

void mod4_1(){
  mod4_any(KEY_1);
}

void mod4_2(){
  mod4_any(KEY_2);
}
void mod4_enter(){
  mod4_any(KEY_ENTER);
}

void mod4_space(){
  mod4_any(KEY_SPACE);
}

void mod4_j(){
  mod4_any(KEY_J);
}

void mod4_l(){
  mod4_any(KEY_L);
}

void mod4_k(){
  mod4_any(KEY_K);
}

void mod4_i(){
  mod4_any(KEY_I);
}

void mod4_p(){
  mod4_any(KEY_P);
}

void mod4_semicolon(){
  mod4_any(KEY_SEMICOLON);
}

void (*layer_functions[])(void) = {reset, activate_fn, activate_fn2,
  mod4_q, mod4_w, mod4_e, mod4_r, mod4_a, mod4_s, mod4_d, mod4_f,
  mod4_z, mod4_x, mod4_c, mod4_v, mod4_g, mod4_b, mod4_1, mod4_2,
  mod4_enter, mod4_space, mod4_j, mod4_l, mod4_k, mod4_i, mod4_p, mod4_semicolon};

// When we are sending key combinations that include modifiers, the OS
// can do some level of error-correction to prevent this scenario:

// - shift down
// - a key down
// - A inserted
// - shift up
// - a inserted

// However, fn is unlike other modifiers since the OS knows nothing
// about it; from the OS's perspective the keycodes it gets before and
// after the release of fn are unrelated. Because of this, we must let
// fn apply a little after it's been released; this is what fn_decay
// does.

void per_cycle() {
  if(fn_decay > 0) {
    if(fn_active && fn2_active){
      current_layer = layers[3];
    } else if(fn2_active){
      current_layer = layers[2];
    } else {
      current_layer = layers[1];
    }
    fn_decay--;
  } else {
    fn_active = 0;
    fn2_active = 0;
  }

  if(mod4_state == 0){
    mod4_key = 0;
  }else{
    switch(mod4_state)
    {
      case 1:
        keyboard_keys[usb_presses++] = mod4_key;
      case 2:
        keyboard_modifier_keys |= KEY_RIGHT_SHIFT;
      case 3:
        keyboard_modifier_keys |= KEY_RIGHT_ALT;
      case 4:
        keyboard_modifier_keys |= KEY_RIGHT_CTRL;
      case 5:
        keyboard_modifier_keys |= KEY_RIGHT_GUI;
    }
    mod4_state--;
  }
}
