/*
  A formula usada para fazer os calculo e encontrar os valores das temperaturas em graus C e F
  eu encontrei no site: https://portal.vidadesilicio.com.br/sensor-temperatura-ntc10k-com-esp32/
  
  As implementações relacionadas as threads eu fiz seguindo a documentação do Freertos
*/

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C LCD = LiquidCrystal_I2C(0x27, 16, 2);

#define TEMPO_DEBOUNCE 500 //ms
volatile unsigned long timestamp_ultimo_acionamento = 0;

TaskHandle_t tarefaTemperatura;
TaskHandle_t tarefaExibicaoDisplay;
TaskHandle_t tarefaVerificarSemaforos;

SemaphoreHandle_t SMF1;
SemaphoreHandle_t SMF2;
SemaphoreHandle_t SMF3;

volatile double fahrenheit;
volatile double celsius;
volatile boolean exibirTemperatura = false;
volatile boolean selecionadoCelsius = false;
volatile boolean limparDisplay = false;

void verificarTemperatura(void *arg){
  // Tensão de saida do ESP32
  double Vs = 3.3;

  // Resistor utilizado no divisor de tensão
  double R1 = 10000;

  // Valor de Beta
  double Beta = 3950;

  // Valor em Kelvin referente a 25º Celsius
  double To=298.15;

  double Ro=10000;
  double adcMax = 4095.0;
  double Vout, Rt;
  double T, Tc, Tf, adc;

  while(true){
    // GARANTE QUE AS INFORMAÇÕES SERÃO RESETADAS APÓS CADA LEITURA
    Vout = 0;
    Rt = 0;
    T =0;
    Tc = 0;
    Tf = 0;
    adc = 0;

    // variavel que recebe leitura do sensor = analogRead(numero do pino)
    adc = analogRead(4);

    // CALCULOS PARA CONVERSÃO DA LEITURA RECEBIDA PELO ESP32 EM TEMPERATURA EM °C
    Vout = adc * Vs/adcMax;
    Rt = R1 * Vout / (Vs - Vout);
    T = 1/(1/To + log(Rt/Ro)/Beta);
    Tc = T - 273.15;
    Tf = Tc * 9 / 5 + 32;
    celsius = Tc;
    fahrenheit = Tf;
    vTaskDelay(1000);
  }
}

void exibicaoDisplay(void *arg){
  while(true){

    if(limparDisplay){
      LCD.clear();
      limparDisplay = false;
    }

    if(exibirTemperatura) {
      if(selecionadoCelsius){
        LCD.setCursor(0, 0);
        LCD.println("Temperatura C");
        LCD.setCursor(0, 1);
        LCD.println(celsius);
      } else{
        LCD.setCursor(0, 0);
        LCD.println("Temperatura F");
        LCD.setCursor(0, 1);
        LCD.println(fahrenheit);
      }
    }
    
    if(!exibirTemperatura){
      LCD.setCursor(0, 0);
      LCD.println("Bem vindo");
    }
    vTaskDelay(500);
  }
}

void verificarSemaforos(void *arg){
  while(true){
    if (xSemaphoreTake(SMF1, pdMS_TO_TICKS(200)) == true) {

      xSemaphoreTake(SMF3, portMAX_DELAY );

        selecionadoCelsius = !selecionadoCelsius;
        exibirTemperatura = true;
        limparDisplay = true;

      xSemaphoreGive(SMF3);
    }
    if (xSemaphoreTake(SMF2, pdMS_TO_TICKS(200)) == true) {
      xSemaphoreTake(SMF3, portMAX_DELAY );

        exibirTemperatura = false;
        selecionadoCelsius = false;
        limparDisplay = true;

      xSemaphoreGive(SMF3);
    }
    vTaskDelay(250);
  }
}

void IRAM_ATTR isr() {
  if ( (millis() - timestamp_ultimo_acionamento) >= TEMPO_DEBOUNCE ){
    timestamp_ultimo_acionamento = millis();
    xSemaphoreGiveFromISR(SMF1, NULL);
  }
}

void IRAM_ATTR isr2(){
  if ( (millis() - timestamp_ultimo_acionamento) >= TEMPO_DEBOUNCE ){
    timestamp_ultimo_acionamento = millis();
    xSemaphoreGiveFromISR(SMF2, NULL);
  }
}

void setup() {
  attachInterrupt(2, isr, RISING);
  attachInterrupt(0, isr2, RISING);

  SMF1 = xSemaphoreCreateBinary();
  SMF2 = xSemaphoreCreateBinary();
  SMF3 = xSemaphoreCreateMutex();
  xSemaphoreGive(SMF3);
  
  LCD.init();
  LCD.backlight();
  LCD.clear();
  
  xTaskCreatePinnedToCore(exibicaoDisplay, "tarefaExibicaoDisplay" , 4096 , NULL, 1 , NULL,0);
  xTaskCreatePinnedToCore(verificarTemperatura, "tarefaTemperatura" , 4096 , NULL, 1 , NULL,0);
  xTaskCreatePinnedToCore(verificarSemaforos, "tarefaVerificarSemaforos" , 4096 , NULL, 1 , NULL,0);
}

void loop() {
  vTaskDelete(NULL);
}
