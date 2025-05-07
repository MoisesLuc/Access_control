#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "ws2818b.pio.h"

//pinos e módulos controlador i2c selecionado
#define I2C_PORT i2c1
#define PINO_SCL 14
#define PINO_SDA 15

//definição dos LEDs
#define BUTTON_A 5
#define BUTTON_B 6

// Definição do número de LEDs e pino.
#define LED_COUNT 25
#define LED_PIN 7

// Definição dos pinos usados para o joystick
const int VRY = 27;          // Pino de leitura do eixo Y do joystick (conectado ao ADC)
const int ADC_CHANNEL_1 = 1; // Canal ADC para o eixo Y do joystick
const int SW = 22;           // Botão do Joystick

ssd1306_t disp;


/**
 * INICIALIZAÇÃO 
 * MATRIZ DE LEDS
 */

// Definição de pixel GRB
struct pixel_t {
    uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.

// Declaração do buffer de pixels que formam a matriz.
npLED_t leds[LED_COUNT];

// Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;

/**
* Inicializa a máquina PIO para controle da matriz de LEDs.
*/
void npInit(uint pin) {
    // Cria programa PIO.
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;

    // Toma posse de uma máquina PIO.
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
    }

    // Inicia programa na máquina PIO obtida.
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

    // Limpa buffer de pixels.
    for (uint i = 0; i < LED_COUNT; ++i) {
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
}

/**
* Atribui uma cor RGB a um LED.
*/
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}

/**
* Limpa o buffer de pixels.
*/
void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i)
        npSetLED(i, 0, 0, 0);
}

/**
* Escreve os dados do buffer nos LEDs.
*/
void npWrite() {
    // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
    for (uint i = 0; i < LED_COUNT; ++i) {
    pio_sm_put_blocking(np_pio, sm, leds[i].G);
    pio_sm_put_blocking(np_pio, sm, leds[i].R);
    pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
}

/**
 * FIM DA
 * INICIALIZAÇÃO 
 * MATRIZ DE LEDS
 */



//função para inicialização de todos os recursos do sistema
void inicializa() {
    stdio_init_all();
    adc_init();
    adc_gpio_init(VRY);
    i2c_init(I2C_PORT, 400*1000);// I2C Inicialização. Usando 400Khz.
    gpio_set_function(PINO_SCL, GPIO_FUNC_I2C);
    gpio_set_function(PINO_SDA, GPIO_FUNC_I2C);
    gpio_pull_up(PINO_SCL);
    gpio_pull_up(PINO_SDA);
    disp.external_vcc=false;
    ssd1306_init(&disp, 128, 64, 0x3C, I2C_PORT);

    //inicialização dos botões
    gpio_init(BUTTON_A);
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_pull_up(BUTTON_B);

    // Botão do Joystick
    gpio_init(SW);             // Inicializa o pino do botão
    gpio_set_dir(SW, GPIO_IN); // Configura o pino do botão como entrada
    gpio_pull_up(SW);

    // Inicializa matriz de LEDs NeoPixel
    npInit(LED_PIN);
    npClear();
    npWrite();
}




/*
FUNÇÕES DO DISPLAY
*/

//variável para armazenar a posição do seletor do display
uint pos_y=12; // Começa em Y = 8

//função escrita no display.
void print_texto(char *msg, uint pos_x, uint pos_y, uint scale){
    ssd1306_draw_string(&disp, pos_x, pos_y, scale, msg); //desenha texto
    ssd1306_show(&disp); //apresenta no Oled
}

//o desenho do retangulo fará o papel de seletor
void print_retangulo(int x1, int y1, int x2, int y2){
    ssd1306_draw_empty_square(&disp,x1,y1,x2,y2);
    ssd1306_show(&disp);
}

void print_menu(void) {
    char *text = ""; //texto do menu
    
    //texto do Menu
    ssd1306_clear(&disp); //Limpa a tela
    print_texto(text="Menu", 52, 2, 1);
    print_retangulo(2, pos_y-1, 120, 8);
    print_texto(text="Giro Reconhecido", 6, 12, 1);
    print_texto(text="Horario Permitido", 6, 20, 1);
    print_texto(text="Dia Permitido", 6, 28, 1);
    print_texto(text="Portaria", 6, 36, 1);
}

// Função para ler os valores dos eixos do joystick (X e Y)
void joystick_read_axis(uint16_t *vrx_value, uint16_t *vry_value) {
    // Leitura do valor do eixo Y do joystick
    adc_select_input(ADC_CHANNEL_1); // Seleciona o canal ADC para o eixo Y
    sleep_us(2);                     // Pequeno delay para estabilidade
    *vry_value = adc_read();         // Lê o valor do eixo Y (0-4095)
}

/*
FIM DAS 
FUNÇÕES 
DO DISPLAY
*/


int variables[] = {0,0,0,1}; // Variável global contendo o valor padrão para as entradas

// Função de alternação do valor da entrada
void change_input(int input) {
    int Vpos = 4 - input; // Identifica a posição do valor da entrada no array

    // Inverte as entradas acende o LED de acordo com seu valor (0 - vermelho; 1 - verde)
    if(variables[input] == 0) {
        variables[input] = 1;
        npSetLED(Vpos, 0, 255, 0);
    } else if(variables[input] == 1) {
        variables[input] = 0;
        npSetLED(Vpos, 255, 0, 0);
    }

    npWrite();
}

// Função de verificação do valor de saída com base na expressão "~PT OR (GR AND HO AND DP)"
void enable_output() {
    if(variables[3] == 0) {
        npSetLED(12, 0, 255, 0);
    }else if (variables[0] == 1 && variables[1] == 1 && variables[2] == 1) {
        npSetLED(12, 0, 255, 0);
    }else {
        npSetLED(12, 255, 0, 0);
    }
    npWrite();
}



int main() {
    inicializa();
    print_menu();
    uint countdown = 0; //verificar seleções para baixo do joystick
    uint countup = 3; //verificar seleções para cima do joystick
    int button_release = 1; // Para verificação de soltura do botão
    int input[] = {0,0,0,1};

    // Define a posição das entradas na matriz de LEDs
    int GR = 0;
    int HO = 1;
    int DI = 2;
    int PT = 3;

    // Definindo os LEDs 00, 01, 02 como vermelho, e o LED 03 como verde
    npSetLED(1, 0, 255, 0);
    npSetLED(2, 255, 0, 0);
    npSetLED(3, 255, 0, 0);
    npSetLED(4, 255, 0, 0);
    npSetLED(12, 255, 0, 0);
    npWrite();
   
    while(true) {
        //trecho de código aproveitado de https://github.com/BitDogLab/BitDogLab-C/blob/main/joystick/joystick.c
        adc_select_input(0);
        uint adc_y_raw = adc_read();
        const uint bar_width = 40;
        const uint adc_max = (1 << 12) - 1;
        uint bar_y_pos = adc_y_raw * bar_width / adc_max; //bar_y_pos determinará se o Joystick foi pressionado para cima ou para baixo

        // printf("Valor de y e: %d\n", bar_y_pos);
        // o valor de 20 é o estado de repouso do Joystick
        if(bar_y_pos < 10 && countdown < 3) { // Valor algumas vezes menor que o de repouso para menor sensibilidade no joystick
            pos_y+=8;
            countdown+=1;
            countup-=1;
            print_menu();
        } else if(bar_y_pos > 30 && countup < 3) { // Valor algumas vezes maior que o de repouso para menor sensibilidade no joystick
            pos_y-=8;
            countup+=1;
            countdown-=1;
            print_menu();
        } else if (bar_y_pos < 10 && countdown == 3) { // Move o apontador para a primeira posição caso esteja na última e joystick seja acionado para baixo
            pos_y = 12;
            countdown = 0;
            countup = 3;
            print_menu();
        } else if (bar_y_pos > 30 && countup == 3) { // Move o apontador para a última posição caso esteja na primeira e joystick seja acionado para cima
            pos_y = 36;
            countdown = 3;
            countup = 0;
            print_menu();
        }
      
        sleep_ms(150);

        int button_state = gpio_get(SW);
        sleep_ms(20);

        //verifica se botão foi pressionado e solto. Se sim, entra no switch case para verificar posição do seletor e chama acionamento dos leds.
        if(gpio_get(SW) == 1 && button_release == 0){
            switch (pos_y){
                case 12:
                    change_input(GR);
                    print_menu();
                    break;
                case 20:
                    change_input(HO);
                    print_menu();
                    break;
                case 28:
                    change_input(DI);
                    print_menu();
                    break;
                case 36:
                    change_input(PT);
                    print_menu();
                    break;
            }
        enable_output();
        }

        button_release = button_state;

    }

    return 0;
}
