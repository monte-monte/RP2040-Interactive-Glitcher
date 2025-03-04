#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "build/glitch.pio.h"

#define GLITCH_MOD 0

#define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))

#ifndef SYSTEM_FREQ
// #define SYSTEM_FREQ 125000
#define SYSTEM_FREQ 200000
#endif

#ifndef OUT_GPIO
#define OUT_GPIO 8
#endif

#ifndef IN_GPIO
#define IN_GPIO 26
#endif

#ifndef LED_GPIO
#define LED_GPIO 0
#endif

#ifndef RESET_OUT
#define RESET_OUT 1
#endif

#ifndef VCC_CTL
#define VCC_CTL 28
#endif

#ifndef GND_CTL
#define GND_CTL 29
#endif

#ifndef PWR_CYCLE_DELAY
#define PWR_CYCLE_DELAY 50
#endif

bool reset_armed = false;

typedef enum {
  NORMAL = 0,
  LOW = 1,
  FALLING = 2,
  LOW_FALLING = 3,
  MOD = 4,
} GlitcherType;

typedef struct {
  uint in_pin;
  uint out_pin;
  uint reset_out_pin;
  uint vcc_ctl_pin;
  uint gnd_ctl_pin;
  GlitcherType glitcher_type;
  uint32_t clock;
  uint32_t ns_per_clock;
  bool reset_out;
  uint32_t power_delay;
} Config;

Config config = {
  .in_pin = IN_GPIO,
  .out_pin = OUT_GPIO,
  .reset_out_pin = RESET_OUT,
  .vcc_ctl_pin = VCC_CTL,
  .gnd_ctl_pin = GND_CTL,
  .glitcher_type = GLITCH_MOD,
  .clock = SYSTEM_FREQ,
  .reset_out = false,
  .power_delay = PWR_CYCLE_DELAY,
};

PIO pio[2];
uint sm[2];
uint offset[2];
char cmd_buffer[64];
uint ns_per_clock;

void reboot() {
  AIRCR_Register = 0x5FA0004;
}

void get_command() {
  uint pos = 0;
  memset(cmd_buffer, 0, sizeof(cmd_buffer));
  while (pos < 64) {
    cmd_buffer[pos] = getchar();
    if (cmd_buffer[pos] == '\r' || cmd_buffer[pos] == '\n') break;
    pos++;
  }
}

void glitch_init(PIO pio, uint sm, uint offset, uint out_pin, uint in_pin) {
  glitch_program_init(pio, sm, offset, out_pin, in_pin);
  pio_sm_set_enabled(pio, sm, true);
  printf("Glitchin pin %d\n", out_pin);
}

void glitch_mod_init(PIO pio, uint sm, uint offset, uint out_pin, uint in_pin) {
  glitch_nop_program_init(pio, sm, offset, out_pin, in_pin);
  pio_sm_set_enabled(pio, sm, true);
  printf("Glitchin pin %d\n", out_pin);
}

void glitch_pulse(PIO pio, uint sm, uint delay1, uint delay2) {
  uint d1, d2;
  reset_armed = true;
  d1 = delay1 / config.ns_per_clock;
  if (d1 > 1) d1--; // Minimum delay would be 16ns
  d2 = delay2 / config.ns_per_clock;
  if (d2 > 1) d2--;
  pio->txf[sm] = d1;
  sleep_ms(1);
  pio->txf[sm] = d2;
}

uint find_ns_coef(char* string) {
  uint ret = 1;
  if (string[0] == 'M' || string[0] == 'm') {
    string[0] = '0';
    ret = 1000000;
  } else if (string[0] == 'U' || string[0] == 'u') {
    string[0] = '0';
    ret = 1000;
  } else if (string[0] == 'N' || string[0] == 'n') {
    string[0] = '0';
  }
  return ret;
}

void reset_glitch(PIO pio, uint sm, uint offset) {
  pio_sm_exec(pio, sm, pio_encode_jmp(offset));
  sleep_ms(1);
}

void reset_callback(uint gpio, uint32_t events) {
    if (reset_armed) {
      if (gpio_get(config.in_pin)) {
        gpio_put(config.reset_out_pin, 1);
        reset_armed = 0;
      } else {
        gpio_put(config.reset_out_pin, 0);
      }
    }
}

void trigger_glitch(PIO pio, uint sm, uint offset) {
  pio_sm_exec(pio, sm, pio_encode_jmp(offset+6));
}

void get_init_command(Config *config) {
  uint pos = 0;
  memset(cmd_buffer, 0, sizeof(cmd_buffer));
  while (pos < 64) {
    cmd_buffer[pos] = getchar();
    if (cmd_buffer[pos] == 'r') reboot();
    if (cmd_buffer[pos] == '\r' || cmd_buffer[pos] == '\n') break;
    pos++;
  }
  // printf("recieved a string: %s\n", cmd_buffer);
  char * pch;
  pch = strtok(cmd_buffer, ";");
  
  while (pch != NULL) {
    int value = atoi(pch+1);
    switch (pch[0]) {
    case 'i':
    case 'I':
      if (value < NUM_BANK0_GPIOS) config->in_pin = value;
      break;
    
    case 'o':
    case 'O':
      if (value < NUM_BANK0_GPIOS) config->out_pin = value;
      break;

    case 'e':
    case 'E':
      if (value < NUM_BANK0_GPIOS) {
        config->reset_out_pin = value;
        config->reset_out = true;
      }
      break;
    
    case 'v':
    case 'V':
      if (value < NUM_BANK0_GPIOS) config->vcc_ctl_pin = value;
      break;

    case 'g':
    case 'G':
      if (value < NUM_BANK0_GPIOS) config->gnd_ctl_pin = value;
      break;
    
    case 'm':
    case 'M':
      if ((GlitcherType)value > MOD) config->glitcher_type = MOD;
      else if (value < 0) config->glitcher_type = NORMAL;
      else config->glitcher_type = (GlitcherType)value;
      break;

    case 'c':
    case 'C':
      config->clock = value;
      break;

    case 'd':
    case 'D':
      config->power_delay = value;
      break;
    }
    pch = strtok(NULL, ";");
  } 
}

void init(Config * config) {
  hard_assert(config->out_pin < 31);

  // printf("Settings pins\n");

  gpio_init(config->vcc_ctl_pin);
  gpio_init(config->gnd_ctl_pin);
  gpio_set_dir(config->vcc_ctl_pin, 1);
  gpio_set_dir(config->gnd_ctl_pin, 1);
  gpio_put(config->vcc_ctl_pin, 0);
  gpio_put(config->gnd_ctl_pin, 1);

  if (config->reset_out) {
    // printf("Settings reset\n");
    gpio_init(config->reset_out_pin);
    gpio_set_dir(config->reset_out_pin, 1);
    gpio_put(config->reset_out_pin, 0);
    gpio_set_irq_enabled_with_callback(config->in_pin, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &reset_callback);
  }

  // printf("Settings clock\n");
  set_sys_clock_khz(config->clock, 1);
  config->ns_per_clock = (float)1 / ((float)clock_get_hz(clk_sys) / 1000000000);

  bool rc;
  
  switch (config->glitcher_type) {
    
  case NORMAL:
    rc = pio_claim_free_sm_and_add_program_for_gpio_range(&glitch_program, &pio[0], &sm[0], &offset[0], 0, 3, true);
    glitch_program_init(pio[0], sm[0], offset[0], config->out_pin, config->in_pin);
    // printf("Glitcher type NORMAL\n");
    break;

  case FALLING:
    rc = pio_claim_free_sm_and_add_program_for_gpio_range(&glitch_falling_program, &pio[0], &sm[0], &offset[0], 0, 2, true);
    glitch_falling_program_init(pio[0], sm[0], offset[0], config->out_pin, config->in_pin);
    // printf("Glitcher type FALLING\n");
    break;

  case LOW:
    rc = pio_claim_free_sm_and_add_program_for_gpio_range(&glitch_low_program, &pio[0], &sm[0], &offset[0], 0, 2, true);
    glitch_low_program_init(pio[0], sm[0], offset[0], config->out_pin, config->in_pin);
    // printf("Glitcher type LOW\n");
    break;

  case LOW_FALLING:
    rc = pio_claim_free_sm_and_add_program_for_gpio_range(&glitch_low_falling_program, &pio[0], &sm[0], &offset[0], 0, 2, true);
    glitch_low_falling_program_init(pio[0], sm[0], offset[0], config->out_pin, config->in_pin);
    // printf("Glitcher type LOW_FALLING\n");
    break;
  
  case MOD:
    rc = pio_claim_free_sm_and_add_program_for_gpio_range(&glitch_nop_program, &pio[0], &sm[0], &offset[0], 0, 2, true);
    glitch_nop_program_init(pio[0], sm[0], offset[0], config->out_pin, config->in_pin);
    // printf("Glitcher type MOD\n");
    break;
  }
  hard_assert(rc);
  
  printf("Running at %uMHz. Resolution: %uns. Smallest delay: %uns. Power cycle delay: %ums\n", (clock_get_hz(clk_sys) / 1000000), config->ns_per_clock, config->ns_per_clock*2, config->power_delay);
  if (!config->reset_out) printf("Trigger pin: %u, VCC control: %u, GND control: %u Glitching pin: %u\n", config->in_pin, config->vcc_ctl_pin, config->gnd_ctl_pin, config->out_pin);
  else printf("Trigger pin: %u, VCC control: %u, GND control: %u Reset pin: %u Glitching pin: %u\n", config->in_pin, config->vcc_ctl_pin, config->gnd_ctl_pin, config->reset_out_pin, config->out_pin);
  printf("Loaded glitcher at %u offset on PIO #%u\n", offset[0], PIO_NUM(pio[0]));
  printf("Ready\n");
}

void cycle_vcc() {
  // gpio_put(config.out_pin, 1);
  gpio_put(config.vcc_ctl_pin, 1);
  sleep_ms(config.power_delay);
  // gpio_put(config.out_pin, 0);
  gpio_put(config.vcc_ctl_pin, 0);
}

void cycle_gnd() {
  gpio_put(config.gnd_ctl_pin, 0);
  sleep_ms(config.power_delay);
  gpio_put(config.gnd_ctl_pin, 1);
}

void cycle_power() {
  gpio_put(config.vcc_ctl_pin, 1);
  gpio_put(config.gnd_ctl_pin, 0);
  sleep_ms(config.power_delay);
  gpio_put(config.vcc_ctl_pin, 0);
  gpio_put(config.gnd_ctl_pin, 1);
}

int main() {
  // setup_default_uart();
  // stdio_init_all();
  stdio_usb_init();
  gpio_init(LED_GPIO);
  gpio_set_dir(LED_GPIO, GPIO_OUT);
  gpio_put(LED_GPIO, 1);
  sleep_ms(1000);
  gpio_put(LED_GPIO, 0);

  int init_char = getchar();
  if (init_char == 'r') reboot();
  if (init_char == '#') get_init_command(&config);

  init(&config);
  
  while (1) {
    char c = getchar();
    switch (c)
    {
    case 'r':
    case 'R':
      reboot();
      break;

    case 'f':
    case 'F':
      reset_glitch(pio[0], sm[0], offset[0]);
      continue;
    
    case 'v':
    case 'V':
      cycle_vcc();
      reset_glitch(pio[0], sm[0], offset[0]);
      continue;

    case 'g':
    case 'G':
      cycle_gnd();
      reset_glitch(pio[0], sm[0], offset[0]);
      continue;

    case 'p':
    case 'P':
      cycle_power();
      reset_glitch(pio[0], sm[0], offset[0]);
      continue;

    case 't':
    case 'T':
      trigger_glitch(pio[0], sm[0], offset[0]);
      continue;
    }

    if (pio_sm_get_pc(pio[0], sm[0]) != offset[0]) {
      printf("PIO is still running, please wait. %d\n", pio_sm_get_pc(pio[0], sm[0]));
    } else if (c == '#') {
      uint d1, d2;
      uint ns_coef = 1;
      char * pch;
      get_command();
      pch = strtok(cmd_buffer, "-");
      ns_coef = find_ns_coef(pch);
      d1 = atoi(pch) * ns_coef;
      pch = strtok(NULL, "-");
      ns_coef = find_ns_coef(pch);
      d2 = atoi(pch) * ns_coef;
      if (d1 || d2) {
        printf("Glitching with delays: %uns - %uns\n", d1, d2);
        glitch_pulse(pio[0], sm[0], d1, d2);
      }
    } else if (c == '?') {
      printf("OK\n");
    }
  }
}
