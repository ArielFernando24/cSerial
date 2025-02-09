#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "hardware/uart.h"
#include "pio_matrix.pio.h"
#include "font.h"
#include "ssd1306.h"

#define NUM_PIXELS 25
#define OUT_PIN 7
#define LED_PIN1 11 // GPIO para o LED Verde
#define LED_PIN2 12 // GPIO para o LED Azul
#define RED_LED_PIN 13
#define BTN_A_PIN 5
#define BTN_B_PIN 6
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define UART_ID uart0
#define BAUD_RATE 115200
#define UART_TX_PIN 0
#define UART_RX_PIN 1

volatile int numero_atual = 0; // Número exibido na matriz
volatile bool estado_led_vermelho = false;
volatile bool estado_led_verde = false;
volatile bool estado_led_azul = false;

ssd1306_t ssd; // Estrutura do display

int mapa_leds[25] = {
    24, 23, 22, 21, 20, // Linha 1: direita -> esquerda
    15, 16, 17, 18, 19, // Linha 2: esquerda -> direita
    14, 13, 12, 11, 10, // Linha 3: direita -> esquerda
    5, 6, 7, 8, 9,      // Linha 4: esquerda -> direita
    4, 3, 2, 1, 0       // Linha 5: direita -> esquerda
};

// Função para converter RGB em formato aceito pela matriz
uint32_t matrix_rgb(double r, double g, double b)
{
    return ((uint8_t)(g * 255) << 24) | ((uint8_t)(r * 255) << 16) | ((uint8_t)(b * 255) << 8);
}

// Função para configurar a matriz de LEDs com um número específico
void exibir_numero(int numero, PIO pio, uint sm, double r, double g, double b)
{
    const int numeros[10][25] = {
        {1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // 0
        {0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0}, // 1
        {1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1}, // 2
        {1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // 3
        {1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, // 4
        {1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // 5
        {1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // 6
        {1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}, // 7
        {1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // 8
        {1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1}  // 9
    };

    for (int i = 0; i < NUM_PIXELS; i++)
    {
        int led_index = mapa_leds[i]; // Corrige a ordem dos LEDs com base no mapa
        uint32_t valor_led = matrix_rgb(numeros[numero][led_index] * r, numeros[numero][led_index] * g, numeros[numero][led_index] * b);
        pio_sm_put_blocking(pio, sm, valor_led);
    }
}

// Tratamento de debouncing para botões
volatile uint32_t ultimo_tempo_btn_a = 0;
volatile uint32_t ultimo_tempo_btn_b = 0;

void exibir_mensagem(const char* mensagem) {
    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, mensagem  , 0, 0);
    ssd1306_send_data(&ssd);
}

void gpio_callback(uint gpio, uint32_t events)
{
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());

    if (gpio == BTN_A_PIN && (tempo_atual - ultimo_tempo_btn_a > 200)) {
        estado_led_verde = !estado_led_verde;
        gpio_put(LED_PIN1, estado_led_verde);
        printf("Botão A pressionado: LED Verde %s\n", estado_led_verde ? "Ligado" : "Desligado");
        uart_puts(UART_ID, "Botão A pressionado\n");
        exibir_mensagem(estado_led_verde ? "LED Verde ON" : "LED Verde OFF");  // LED verde
        ultimo_tempo_btn_a = tempo_atual;
    } else if (gpio == BTN_B_PIN && (tempo_atual - ultimo_tempo_btn_b > 200)) {
        estado_led_azul = !estado_led_azul;
        gpio_put(LED_PIN2, estado_led_azul);
        printf("Botão B pressionado: LED Azul %s\n", estado_led_azul ? "Ligado" : "Desligado");
        uart_puts(UART_ID, "Botão B pressionado\n");
        exibir_mensagem(estado_led_azul ? "LED Azul ON" : "LED Azul OFF"); // LED azul
        ultimo_tempo_btn_b = tempo_atual;
    }
}

int main()
{
    stdio_init_all();

    // Configura GPIOs
    gpio_init(RED_LED_PIN);
    gpio_set_dir(RED_LED_PIN, GPIO_OUT);

    gpio_init(LED_PIN1);
    gpio_set_dir(LED_PIN1, GPIO_OUT);

    gpio_init(LED_PIN2);
    gpio_set_dir(LED_PIN2, GPIO_OUT);

    gpio_init(BTN_A_PIN);
    gpio_set_dir(BTN_A_PIN, GPIO_IN);
    gpio_pull_up(BTN_A_PIN);
    gpio_set_irq_enabled_with_callback(BTN_A_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    gpio_init(BTN_B_PIN);
    gpio_set_dir(BTN_B_PIN, GPIO_IN);
    gpio_pull_up(BTN_B_PIN);
    gpio_set_irq_enabled(BTN_B_PIN, GPIO_IRQ_EDGE_FALL, true);

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL);                                        // Pull up the clock line                                               // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd);                                         // Configura o display
    ssd1306_send_data(&ssd);                                      // Envia os dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Inicializa PIO para matriz
    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &pio_matrix_program);
    pio_matrix_program_init(pio, sm, offset, OUT_PIN);

    char c = '\0'; // Armazena o último caractere digitado

    bool cor = true;
    while (true)
    {                     
        // Se houver um caractere salvo, exiba-o na tela
    if (c != '\0') {
        ssd1306_draw_char(&ssd, c, 50, 25); // Mantém o caractere na posição central
    }

    ssd1306_send_data(&ssd); // Atualiza o display

    char novo_c;
    if (scanf("%c", &novo_c) == 1) { // Captura um novo caractere da UART
        printf("Caractere recebido: %c\n", novo_c);
        c = novo_c; // Atualiza o caractere salvo para exibição contínua

        // Se for um número, também exibe na matriz de LEDs
        if (c >= '0' && c <= '9') {
            exibir_numero(c - '0', pio, sm, 0.02, 0.02, 0.02); // Atualiza a matriz 5x5
        }
    }

    sleep_ms(200);
}
    

    return 0;
}