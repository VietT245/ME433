#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"

#include "usb_descriptors.h"

#include "math.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

// I2C settings
#define I2C_SDA 4
#define I2C_SCL 5

// Heartbeat LED
#define HEARTBEAT_LED 7
#define BUTTON_PIN 0

// MPU6050
#define MPU6050_ADDR  0x68
#define CONFIG        0x1A
#define GYRO_CONFIG   0x1B
#define ACCEL_CONFIG  0x1C
#define PWR_MGMT_1    0x6B
#define WHO_AM_I      0x75
#define ACCEL_XOUT_H  0x3B

// Modes
#define MODE_IMU    0
#define MODE_CIRCLE 1

// MPU6050 Helpers
void i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t data) {
    uint8_t buf[2] = {reg, data};
    i2c_write_blocking(i2c_default, addr, buf, 2, false);
}

void i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len) {
    i2c_write_blocking(i2c_default, addr, &reg, 1, true);
    i2c_read_blocking(i2c_default, addr, buf, len, false);
}

void mpu6050_init() {
    uint8_t who;
    i2c_read_reg(MPU6050_ADDR, WHO_AM_I, &who, 1);
    if (who != 0x68 && who != 0x98) {
        gpio_put(HEARTBEAT_LED, 1);
        while (1) { tight_loop_contents(); }
    }
    i2c_write_reg(MPU6050_ADDR, PWR_MGMT_1,  0x00);
    i2c_write_reg(MPU6050_ADDR, ACCEL_CONFIG, 0x00); // +/- 2g
    i2c_write_reg(MPU6050_ADDR, GYRO_CONFIG,  0x18); // +/- 2000 dps
}

void mpu6050_read(float *ax, float *ay) {
    uint8_t buf[4];
    i2c_read_reg(MPU6050_ADDR, ACCEL_XOUT_H, buf, 4); // just X and Y accel
    int16_t raw_ax = (int16_t)(buf[0] << 8 | buf[1]);
    int16_t raw_ay = (int16_t)(buf[2] << 8 | buf[3]);
    *ax = raw_ax * 0.000061f; // g
    *ay = raw_ay * 0.000061f; // g
}

// Maps acceleration to one of 4 discrete speed levels
int8_t accel_to_delta(float a) {
    float abs_a = fabsf(a);
    int sign    = a >= 0 ? 1 : -1;
    if      (abs_a > 0.5f) return sign * 5;
    else if (abs_a > 0.3f) return sign * 3;
    else if (abs_a > 0.1f) return sign * 1;
    else                   return 0;
}

// Mode and button state
static uint8_t mode        = MODE_IMU;
static bool    last_button = true; // pull-up: idle = high
static float   circle_angle = 0.0f;

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */

enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);
void hid_task(void);

/*------------- MAIN -------------*/
int main(void)
{
  board_init();

  // Initialize heartbeat LED
  gpio_init(HEARTBEAT_LED);
  gpio_set_dir(HEARTBEAT_LED, GPIO_OUT);
  gpio_put(HEARTBEAT_LED, 0); // off = IMU mode

  // Initialize button
  gpio_init(BUTTON_PIN);
  gpio_set_dir(BUTTON_PIN, GPIO_IN);
  gpio_pull_up(BUTTON_PIN);

  // Initialize I2C
  i2c_init(i2c_default, 400000);
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA);
  gpio_pull_up(I2C_SCL);

  // Initialize MPU6050
  mpu6050_init();

  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  while (1)
  {
    tud_task(); // tinyusb device task
    led_blinking_task();
    hid_task();
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

static void send_hid_report(uint8_t report_id, uint32_t btn)
{
  // skip if hid is not ready yet
  if ( !tud_hid_ready() ) return;

  switch(report_id)
  {
    case REPORT_ID_MOUSE:
    {
      int8_t dx = 0, dy = 0;

      // Check button for mode toggle (active low, edge detect)
      bool btn_state = gpio_get(BUTTON_PIN);
      if (!btn_state && last_button) {
        mode = (mode == MODE_IMU) ? MODE_CIRCLE : MODE_IMU;
        gpio_put(HEARTBEAT_LED, mode == MODE_CIRCLE ? 1 : 0);
      }
      last_button = btn_state;

      if (mode == MODE_IMU) {
        // IMU mode: tilt controls cursor
        float ax, ay;
        mpu6050_read(&ax, &ay);
        dx = -accel_to_delta(ax);
        dy = accel_to_delta(ay);
      } else {
        // Circle mode: slow circle, ~4 second full rotation
        circle_angle += 0.016f;
        if (circle_angle > 2.0f * (float)M_PI) circle_angle -= 2.0f * (float)M_PI;
        dx = (int8_t)(3.0f * cosf(circle_angle));
        dy = (int8_t)(3.0f * sinf(circle_angle));
      }

      tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, dx, dy, 0, 0);
    }
    break;

    default: break;
  }
}

// Every 10ms, we will sent 1 report for each HID profile (keyboard, mouse etc ..)
// tud_hid_report_complete_cb() is used to send the next report after previous one is complete
void hid_task(void)
{
  // Poll every 10ms
  const uint32_t interval_ms = 10;
  static uint32_t start_ms = 0;

  if ( board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  uint32_t const btn = board_button_read();

  // Remote wakeup
  if ( tud_suspended() && btn )
  {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    tud_remote_wakeup();
  }else
  {
    // Send the 1st of report chain, the rest will be sent by tud_hid_report_complete_cb()
    send_hid_report(REPORT_ID_MOUSE, btn);
  }
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) instance;
  (void) len;

  uint8_t next_report_id = report[0] + 1u;

  if (next_report_id < REPORT_ID_COUNT)
  {
    send_hid_report(next_report_id, board_button_read());
  }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  (void) instance;

  if (report_type == HID_REPORT_TYPE_OUTPUT)
  {
    // Set keyboard LED e.g Capslock, Numlock etc...
    if (report_id == REPORT_ID_KEYBOARD)
    {
      // bufsize should be (at least) 1
      if ( bufsize < 1 ) return;

      uint8_t const kbd_leds = buffer[0];

      if (kbd_leds & KEYBOARD_LED_CAPSLOCK)
      {
        // Capslock On: disable blink, turn led on
        blink_interval_ms = 0;
        board_led_write(true);
      }else
      {
        // Caplocks Off: back to normal blink
        board_led_write(false);
        blink_interval_ms = BLINK_MOUNTED;
      }
    }
  }
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // blink is disabled
  if (!blink_interval_ms) return;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}