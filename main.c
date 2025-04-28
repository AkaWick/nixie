#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// Pin definíciók
#define NIXIE_1 PD2
#define NIXIE_2 PD3
#define NIXIE_3 PD4
#define NIXIE_4 PD5
#define BCD_A PB0
#define BCD_B PB1
#define BCD_C PB2
#define BCD_D PB3
#define LAMPA_PIN PC3
#define BUTTON_PWM PD7
#define BUTTON_ORA PC0
#define BUTTON_PERC PC1

// Háttérvilágítás szintek
const uint8_t brightnessLevels[4] = {0, 85, 170, 255};
volatile uint8_t currentLevel = 0;

// Idő változók
volatile uint8_t ora = 12;
volatile uint8_t perc = 0;
volatile uint8_t masodperc = 0;
volatile uint8_t lampa_allapot = 0;
volatile uint8_t roulette_active = 0;
volatile uint8_t display_visible = 1;

// Timer inicializálás
void idozito_init() {
    // Timer1 - időszámoláshoz (1Hz)
    TCCR1A = 0;
    TCCR1B = (1<<WGM12);
    OCR1A = 15624;
    TIMSK1 = (1<<OCIE1A);
    TCCR1B |= (1<<CS12) | (1<<CS10);
    
    // Timer0 - PWM háttérvilágításhoz
    TCCR0A = (1<<COM0A1)|(1<<WGM01)|(1<<WGM00);
    TCCR0B = (1<<CS00);
}

// GPIO inicializálás (belső pull-up)
void gpio_init() {
    // Nixie csövek
    DDRD |= (1<<NIXIE_1)|(1<<NIXIE_2)|(1<<NIXIE_3)|(1<<NIXIE_4);
    DDRB |= (1<<BCD_A)|(1<<BCD_B)|(1<<BCD_C)|(1<<BCD_D);
    
    // Lámpa és PWM kimenet
    DDRC |= (1<<LAMPA_PIN);
    DDRD |= (1<<PD6); // PWM kimenet
    
    // Gombok bemenetként belső pull-up-pal
    DDRD &= ~(1<<BUTTON_PWM);
    PORTD |= (1<<BUTTON_PWM);
    DDRC &= ~((1<<BUTTON_ORA)|(1<<BUTTON_PERC));
    PORTC |= (1<<BUTTON_ORA)|(1<<BUTTON_PERC);
}

// Háttérvilágítás frissítése
void update_backlight() {
    OCR0A = brightnessLevels[currentLevel];
    
    // Visszajelzés
    if (brightnessLevels[currentLevel] > 0) {
        PORTB |= (1<<PB4);
        _delay_ms(30);
        PORTB &= ~(1<<PB4);
    }
}

// BCD kód küldése
void bcd_kuldes(uint8_t szamjegy) {
    PORTB = (PORTB & 0xF0) | (szamjegy & 0x0F);
}

// Roulette effekt (egy ciklus, minden számjegy fél másodpercig)
void roulette_effekt() {
    for(uint8_t szamjegy = 0; szamjegy < 10; szamjegy++) {
        bcd_kuldes(szamjegy);
        // Minden Nixie cső bekapcsolása
        PORTD |= (1<<NIXIE_1) | (1<<NIXIE_2) | (1<<NIXIE_3) | (1<<NIXIE_4);
        _delay_ms(500); // fél másodpercig látható
        PORTD &= ~((1<<NIXIE_1) | (1<<NIXIE_2) | (1<<NIXIE_3) | (1<<NIXIE_4));
    }
    roulette_active = 0;
}

// Idő megjelenítése
void ido_megjelenites() {
    uint8_t szamjegyek[4] = {ora/10, ora%10, perc/10, perc%10};
    
    for(uint8_t i = 0; i < 4; i++) {
        bcd_kuldes(szamjegyek[i]);
        PORTD |= (1<<(NIXIE_1 + (3-i))); // Fordított sorrend
        _delay_us(800); // Megjelenítési idő
        PORTD &= ~(1<<(NIXIE_1 + (3-i)));
    }
    
    // Másodperc villogás
    PORTC = (PORTC & ~(1<<LAMPA_PIN)) | (lampa_allapot << LAMPA_PIN);
}

// Idő frissítése
void ido_frissites() {
    masodperc++;
    if(masodperc >= 60) {
        masodperc = 0;
        perc++;
        
        if(perc >= 60) {
            perc = 0;
            ora++;
            if(ora >= 24) ora = 0;
            roulette_active = 1;  // Minden óraváltáskor indul a roulette effekt
        }
    }
}

// Gomb kezelés
void gomb_kezeles() {
    // Háttérvilágítás gomb
    static uint8_t last_pwm = 1;
    uint8_t current_pwm = PIND & (1<<BUTTON_PWM);
    if (last_pwm && !current_pwm) { // Falling edge
        _delay_ms(20); // Debounce
        if (!(PIND & (1<<BUTTON_PWM))) {
            currentLevel = (currentLevel + 1) % 4;
            update_backlight();
        }
    }
    last_pwm = current_pwm;
    
    // Óra gomb - azonnali óra növelés
    if (!(PINC & (1<<BUTTON_ORA))) {
        _delay_ms(50);
        if (!(PINC & (1<<BUTTON_ORA))) {
            ora = (ora + 1) % 24;
            while(!(PINC & (1<<BUTTON_ORA))); // Vár a felengedésre
        }
    }
    
    // Perc gomb - azonnali perc növelés
    if (!(PINC & (1<<BUTTON_PERC))) {
        _delay_ms(50);
        if (!(PINC & (1<<BUTTON_PERC))) {
            perc = (perc + 1) % 60;
            while(!(PINC & (1<<BUTTON_PERC))); // Vár a felengedésre
        }
    }
}

// Timer1 megszakítás (1Hz)
ISR(TIMER1_COMPA_vect) {
    ido_frissites();
    lampa_allapot ^= 1;
}

int main(void) {
    gpio_init();
    idozito_init();
    sei();
    
    update_backlight();

    while(1) {
        if(roulette_active) {
            roulette_effekt();
        }
        ido_megjelenites();
        gomb_kezeles();
    }
}