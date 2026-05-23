/*
 * PIR-Based Home Security System
 * ATmega328P-Xplained Mini
 *
 * PIN MAP:
 * PD0 -> HC-05 TX (RX Xmini)
 * PD1 -> HC-05 RX (TX Xmini)
 * PD2 -> PIR OUT (INT0)
 * PD3 -> Keypad R1
 * PD4 -> Keypad R2
 * PD5 -> LED rosu
 * PD6 -> Buzzer
 * PD7 -> LED verde
 * PB0 -> Keypad C1
 * PB1 -> Keypad C2
 * PB2 -> Keypad C3
 * PB3 -> Keypad R3
 * PB4 -> Keypad R4
 * PC0 -> Keypad C4
 * PC1 -> MQ-2 DO (Digital)
 * PC2 -> MQ-2 AO (Digital)
 * PC4 -> LCD SDA (I2C)
 * PC5 -> LCD SCL (I2C)
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>

// DEFINIRI PINI

#define LED_RED     PD5
#define LED_GREEN   PD7
#define BUZZER      PD6
#define PIR         PD2

#define MQ2_DO      PC1

// Keypad rows
#define R1          PD3
#define R2          PD4
#define R3          PB3
#define R4          PB4

// Keypad cols
#define C1          PB0
#define C2          PB1
#define C3          PB2
#define C4          PC0

// I2C LCD
#define I2C_LCD_ADDR    0x27
#define LCD_BACKLIGHT   0x08
#define LCD_EN          0x04
#define LCD_RW          0x02
#define LCD_RS          0x01

// UART
#define BAUD        9600
#define UBRR_VAL    ((F_CPU / (16UL * BAUD)) - 1)

// Debounce gaz
#define GAS_THRESHOLD   10

// STARI SISTEM

typedef enum {
    STATE_DISARMED,
    STATE_EXIT_DELAY,
    STATE_ARMED,
    STATE_ENTRY_DELAY,
    STATE_ALARM
} SystemState;

volatile SystemState alarm_state = STATE_DISARMED;
volatile uint8_t pir_flag = 0;

const char CORRECT_PIN[] = "1234";
char entered_pin[5] = "";
uint8_t pin_index = 0;

volatile uint32_t millis_count = 0;
uint32_t delay_start = 0;
#define EXIT_DELAY_MS   10000
#define ENTRY_DELAY_MS  15000

// MILLIS

void timer1_init() {
    TCCR1B |= (1 << WGM12) | (1 << CS11) | (1 << CS10);
    OCR1A = (F_CPU / 64 / 1000) - 1;
    TIMSK1 |= (1 << OCIE1A);
}

ISR(TIMER1_COMPA_vect) {
    millis_count++;
}

uint32_t millis() {
    uint32_t m;
    cli();
    m = millis_count;
    sei();
    return m;
}

// UART
void uart_init() {
    UBRR0H = (UBRR_VAL >> 8);
    UBRR0L = UBRR_VAL;
    UCSR0B = (1 << TXEN0) | (1 << RXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_send_char(char c) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

void uart_send_string(const char *str) {
    while (*str) uart_send_char(*str++);
}

// I2C (TWI)

void i2c_init() {
    TWSR = 0x00;
    TWBR = ((F_CPU / 100000UL) - 16) / 2;
    TWCR = (1 << TWEN);
}

void i2c_start() {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}

void i2c_stop() {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    _delay_us(10);
}

void i2c_write(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}

void i2c_send_byte(uint8_t addr, uint8_t data) {
    i2c_start();
    i2c_write(addr << 1);
    i2c_write(data);
    i2c_stop();
}

// LCD I2C

void lcd_send_nibble(uint8_t nibble, uint8_t rs) {
    uint8_t data = (nibble << 4) | LCD_BACKLIGHT | (rs ? LCD_RS : 0);
    i2c_send_byte(I2C_LCD_ADDR, data | LCD_EN);
    _delay_us(1);
    i2c_send_byte(I2C_LCD_ADDR, data & ~LCD_EN);
    _delay_us(50);
}

void lcd_send_byte(uint8_t byte, uint8_t rs) {
    lcd_send_nibble(byte >> 4, rs);
    lcd_send_nibble(byte & 0x0F, rs);
}

void lcd_cmd(uint8_t cmd) { lcd_send_byte(cmd, 0); }
void lcd_char(char c)     { lcd_send_byte(c, 1); }

void lcd_init() {
    _delay_ms(100);
    lcd_send_nibble(0x03, 0); _delay_ms(10);
    lcd_send_nibble(0x03, 0); _delay_ms(5);
    lcd_send_nibble(0x03, 0); _delay_ms(5);
    lcd_send_nibble(0x02, 0); _delay_ms(5);
    lcd_cmd(0x28);
    lcd_cmd(0x0C);
    lcd_cmd(0x06);
    lcd_cmd(0x01);
    _delay_ms(5);
}

void lcd_clear() {
    lcd_cmd(0x01);
    _delay_ms(2);
}

void lcd_set_cursor(uint8_t row, uint8_t col) {
    uint8_t addr = (row == 0) ? 0x00 : 0x40;
    lcd_cmd(0x80 | (addr + col));
}

void lcd_print(const char *str) {
    while (*str) lcd_char(*str++);
}

void lcd_print_number(uint16_t num) {
    char buf[6];
    itoa(num, buf, 10);
    lcd_print(buf);
}

// BUZZER

void beep(uint16_t ms) {
    for (uint16_t i = 0; i < ms; i++) {
        PORTD |= (1 << BUZZER);
        _delay_us(500);
        PORTD &= ~(1 << BUZZER);
        _delay_us(500);
    }
}

void beep_alarm() {
    for (uint8_t i = 0; i < 5; i++) {
        beep(100);
        _delay_ms(50);
    }
}

// KEYPAD

const char keys[4][4] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};

void keypad_init() {
    DDRD  |=  (1 << R1) | (1 << R2);
    PORTD |=  (1 << R1) | (1 << R2);
    DDRB  |=  (1 << R3) | (1 << R4);
    PORTB |=  (1 << R3) | (1 << R4);

    DDRB  &= ~((1 << C1) | (1 << C2) | (1 << C3));
    PORTB |=  (1 << C1) | (1 << C2) | (1 << C3);
    DDRC  &= ~(1 << C4);
    PORTC |=  (1 << C4);
}

static void row_low(int r) {
    if      (r == 0) PORTD &= ~(1 << R1);
    else if (r == 1) PORTD &= ~(1 << R2);
    else if (r == 2) PORTB &= ~(1 << R3);
    else             PORTB &= ~(1 << R4);
}

static void row_high(int r) {
    if      (r == 0) PORTD |= (1 << R1);
    else if (r == 1) PORTD |= (1 << R2);
    else if (r == 2) PORTB |= (1 << R3);
    else             PORTB |= (1 << R4);
}

static char read_cols(int r) {
    if      (!(PINB & (1 << C1))) return keys[r][0];
    else if (!(PINB & (1 << C2))) return keys[r][1];
    else if (!(PINB & (1 << C3))) return keys[r][2];
    else if (!(PINC & (1 << C4))) return keys[r][3];
    return 0;
}

char keypad_scan() {
    for (int r = 0; r < 4; r++) {
        row_low(r);
        _delay_us(50);
        char found = read_cols(r);
        row_high(r);

        if (found) {
            _delay_ms(20);
            row_low(r);
            _delay_us(50);
            char confirm = read_cols(r);
            row_high(r);

            if (found == confirm) {
                while (1) {
                    row_low(r);
                    _delay_us(50);
                    char still = read_cols(r);
                    row_high(r);
                    if (!still) break;
                    _delay_ms(10);
                }
                _delay_ms(50);
                return found;
            }
        }
    }
    return 0;
}

// PIR INTERRUPT 

void pir_init() {
    DDRD  &= ~(1 << PIR);
    PORTD &= ~(1 << PIR);
    EICRA |= (1 << ISC01) | (1 << ISC00);
    EIMSK |= (1 << INT0);
}

ISR(INT0_vect) {
    if (alarm_state == STATE_ARMED) {
        pir_flag = 1;
    }
}

// MQ-2

void mq2_init() {
    DDRC  &= ~(1 << MQ2_DO);
    PORTC &= ~(1 << MQ2_DO);
}

uint8_t mq2_detected() {
    return (PINC & (1 << MQ2_DO));
}

// PIN HANDLING

void reset_pin() {
    pin_index = 0;
    memset(entered_pin, 0, sizeof(entered_pin));
}

uint8_t check_pin() {
    return strcmp(entered_pin, CORRECT_PIN) == 0;
}

void add_digit(char c) {
    if (pin_index < 4) {
        entered_pin[pin_index++] = c;
        entered_pin[pin_index] = '\0';
    }
}

// DISPLAY STARE

void display_state() {
    lcd_clear();
    switch (alarm_state) {
        case STATE_DISARMED:
            lcd_set_cursor(0, 0);
            lcd_print("System disarmed");
            lcd_set_cursor(1, 0);
            lcd_print("* to arm");
            PORTD &= ~(1 << LED_RED);
            PORTD |=  (1 << LED_GREEN);
            break;

        case STATE_EXIT_DELAY:
            lcd_set_cursor(0, 0);
            lcd_print("Arming in:");
            PORTD |= (1 << LED_RED);
            PORTD |= (1 << LED_GREEN);
            break;

        case STATE_ARMED:
            lcd_set_cursor(0, 0);
            lcd_print("System ARMED");
            lcd_set_cursor(1, 0);
            lcd_print("Monitoring...");
            PORTD |=  (1 << LED_RED);
            PORTD &= ~(1 << LED_GREEN);
            break;

        case STATE_ENTRY_DELAY:
            lcd_set_cursor(0, 0);
            lcd_print("MOTION detected!");
            PORTD |=  (1 << LED_RED);
            PORTD &= ~(1 << LED_GREEN);
            break;

        case STATE_ALARM:
            lcd_set_cursor(0, 0);
            lcd_print("!! ALARM !!");
            lcd_set_cursor(1, 0);
            lcd_print("Enter PIN:");
            PORTD |=  (1 << LED_RED);
            PORTD &= ~(1 << LED_GREEN);
            break;
    }
}

// MAIN

int main(void) {

    DDRD  |=  (1 << LED_RED) | (1 << LED_GREEN) | (1 << BUZZER);
    PORTD &= ~((1 << LED_RED) | (1 << LED_GREEN) | (1 << BUZZER));

    DDRD  &= ~(1 << PIR);
    PORTD &= ~(1 << PIR);

    i2c_init();
    lcd_init();
    keypad_init();
    mq2_init();
    pir_init();
    uart_init();
    timer1_init();
    sei();

    // Warm-up MQ-2
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print("MQ-2 Warm-up...");
    PORTD |= (1 << LED_RED) | (1 << LED_GREEN);
    for (uint8_t i = 0; i < 30; i++) {
        lcd_set_cursor(1, 0);
        lcd_print("Wait: ");
        lcd_print_number(30 - i);
        lcd_print("s  ");
        _delay_ms(1000);
    }

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print("PIR Alarm System");
    lcd_set_cursor(1, 0);
    lcd_print("Initializing...");
    beep(200);
    _delay_ms(500);
    beep(200);
    _delay_ms(1000);
    PORTD &= ~((1 << LED_RED) | (1 << LED_GREEN));

    display_state();
    uart_send_string("System ready\r\n");

    uint32_t last_beep       = 0;
    uint32_t last_alarm_beep = 0;
    uint8_t  wrong_attempts  = 0;
    uint8_t  gas_count       = 0;

    while (1) {

        // MQ-2 debounce
        if (mq2_detected()) {
            gas_count++;
            if (gas_count >= GAS_THRESHOLD) {
                gas_count = 0;
                lcd_clear();
                lcd_set_cursor(0, 0);
                lcd_print("!! GAS/SMOKE !!");
                lcd_set_cursor(1, 0);
                lcd_print("EVACUATE NOW!");
                uart_send_string("ALERT: Gas/Smoke!\r\n");
                beep_alarm();
                _delay_ms(2000);
                display_state();
            }
        } else {
            gas_count = 0;
        }

        // STATE MACHINE
        switch (alarm_state) {

            case STATE_DISARMED: {
                char key = keypad_scan();
                if (key == '*') {
                    reset_pin();
                    alarm_state = STATE_EXIT_DELAY;
                    delay_start = millis();
                    display_state();
                    uart_send_string("System arming...\r\n");
                    beep(100);
                }
                break;
            }

            case STATE_EXIT_DELAY: {
                uint32_t elapsed   = millis() - delay_start;
                uint32_t remaining = (EXIT_DELAY_MS - elapsed) / 1000;

                lcd_set_cursor(1, 0);
                lcd_print("Leaving: ");
                lcd_print_number(remaining);
                lcd_print("s  ");

                if (millis() - last_beep > 2000) {
                    beep(100);
                    last_beep = millis();
                }

                if (elapsed >= EXIT_DELAY_MS) {
                    alarm_state = STATE_ARMED;
                    pir_flag    = 0;
                    display_state();
                    uart_send_string("System ARMED\r\n");
                    beep(500);
                }
                break;
            }

            case STATE_ARMED: {
                if (pir_flag) {
                    pir_flag    = 0;
                    alarm_state = STATE_ENTRY_DELAY;
                    delay_start = millis();
                    display_state();
                    uart_send_string("MOTION DETECTED!\r\n");
                    reset_pin();
                }
                break;
            }

            case STATE_ENTRY_DELAY: {
                uint32_t elapsed   = millis() - delay_start;
                uint32_t remaining = (ENTRY_DELAY_MS - elapsed) / 1000;

                lcd_set_cursor(1, 0);
                lcd_print("PIN: ");
                for (uint8_t i = 0; i < pin_index; i++) lcd_char('*');
                lcd_print(" T:");
                lcd_print_number(remaining);
                lcd_print("  ");

                uint16_t beep_interval = (remaining > 5) ? 1000 : 300;
                if (millis() - last_beep > beep_interval) {
                    beep(50);
                    last_beep = millis();
                }

                char key = keypad_scan();
                if (key >= '0' && key <= '9') {
                    add_digit(key);
                    beep(30);
                    if (pin_index == 4) {
                        if (check_pin()) {
                            wrong_attempts = 0;
                            alarm_state    = STATE_DISARMED;
                            display_state();
                            uart_send_string("PIN correct. Disarmed.\r\n");
                            beep(200); _delay_ms(100); beep(200);
                        } else {
                            wrong_attempts++;
                            uart_send_string("Wrong PIN!\r\n");
                            lcd_clear();
                            lcd_set_cursor(0, 0);
                            lcd_print("Wrong PIN!");
                            beep_alarm();
                            _delay_ms(1000);
                            reset_pin();
                            if (wrong_attempts >= 3) {
                                alarm_state = STATE_ALARM;
                                uart_send_string("3 wrong attempts!\r\n");
                            }
                            display_state();
                        }
                    }
                } else if (key == '#') {
                    reset_pin();
                }

                if (elapsed >= ENTRY_DELAY_MS) {
                    alarm_state = STATE_ALARM;
                    display_state();
                    uart_send_string("Time expired! ALARM!\r\n");
                    reset_pin();
                }
                break;
            }

            case STATE_ALARM: {
                if (millis() - last_alarm_beep > 500) {
                    beep(200);
                    PORTD ^= (1 << LED_RED);
                    last_alarm_beep = millis();
                    uart_send_string("ALARM ACTIVE\r\n");
                }

                char key = keypad_scan();
                if (key >= '0' && key <= '9') {
                    add_digit(key);
                    beep(30);
                    lcd_set_cursor(1, 0);
                    lcd_print("PIN: ");
                    for (uint8_t i = 0; i < pin_index; i++) lcd_char('*');
                    lcd_print("     ");

                    if (pin_index == 4) {
                        if (check_pin()) {
                            wrong_attempts = 0;
                            alarm_state    = STATE_DISARMED;
                            display_state();
                            uart_send_string("PIN correct. Disarmed.\r\n");
                            beep(200); _delay_ms(100); beep(200);
                        } else {
                            reset_pin();
                            uart_send_string("Wrong PIN!\r\n");
                        }
                    }
                } else if (key == '#') {
                    reset_pin();
                    lcd_set_cursor(1, 0);
                    lcd_print("Enter PIN:     ");
                }
                break;
            }
        }

        _delay_ms(10);
    }
}