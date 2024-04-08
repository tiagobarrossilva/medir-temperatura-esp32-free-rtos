#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C LCD = LiquidCrystal_I2C(0x27, 16, 2);

#define TEMPO_DEBOUNCE 500 //ms
unsigned long timestamp_ultimo_acionamento = 0;

int PinoNTC = 4; // PINO DO NTC10K
double Vs = 3.3; // TENSÃO DE SAIDA DO ESP32
double R1 = 10000; //RESISTOR UTILIZADO NO DIVISOR DE TENSÃO
double Beta = 3950; // VALOR DE BETA
double To=298.15; // VALOR EM KELVIN REFERENTE A 25° CELSIUS
double Ro=10000;
double adcMax = 4095.0;

TaskHandle_t tarefaTemperatura;
TaskHandle_t tarefaExibicaoDisplay;
TaskHandle_t tarefaVerificarSemaforos;

SemaphoreHandle_t SMF1;
SemaphoreHandle_t SMF2;
SemaphoreHandle_t SMF3;

volatile float fahrenheit;
volatile float celsius;
volatile boolean exibirTemperatura = false;
volatile boolean selecionadoCelsius = false;
volatile boolean limparDisplay = false;

void verificarTemperatura(void *arg){
  double Vout, Rt;
  double T, Tc, Tf, adc;

  while(true){
    //GARANTE QUE AS INFORMAÇÕES SERÃO RESETADAS APÓS CADA LEITURA
    Vout, Rt = 0;
    T, Tc, Tf, adc = 0;

    // VARIÁVEL QUE RECEBE A LEITURA DO NTC10K
    adc = analogRead(PinoNTC);

    //CALCULOS PARA CONVERSÃO DA LEITURA RECEBIDA PELO ESP32 EM TEMPERATURA EM °C
    Vout = adc * Vs/adcMax;
    Rt = R1 * Vout / (Vs - Vout);
    T = 1/(1/To + log(Rt/Ro)/Beta);
    Tc = T - 273.15;
    Tf = Tc * 9 / 5 + 32;
      
    celsius = Tc;
    fahrenheit = Tf;
    vTaskDelay(200);
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
    vTaskDelay(400);
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
    vTaskDelay(200);
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
  pinMode(36,INPUT);
  attachInterrupt(2, isr, RISING);
  attachInterrupt(0, isr2, RISING);

  SMF1 = xSemaphoreCreateBinary();
  SMF2 = xSemaphoreCreateBinary();
  SMF3 = xSemaphoreCreateMutex();
  xSemaphoreGive(SMF3);

  //Serial.begin(115200);
  
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
