#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include "SSD1306.h"

//Deixe esta linha descomentada para compilar o Master
//Comente ou remova para compilar o Slave

//#define MASTER

#define SCK 5   // GPIO5  SCK
#define MISO 19 // GPIO19 MISO
#define MOSI 27 // GPIO27 MOSI
#define SS 18   // GPIO18 CS
#define RST 14  // GPIO14 RESET
#define DI00 26 // GPIO26 IRQ(Interrupt Request)

#define BAND 868E6 //Frequência do radio - exemplo : 433E6, 868E6, 915E6

char FrameMaster[7] = {0x1, 0, 0x1, 0, 0, 0, 0x14};
char FrameSlave[7] = {0x1, 0, 0x1, 0, 0, 0, 0x32};
char received[7] = {0, 0, 0, 0, 0, 0, 0};

//Variável para controlar o display
SSD1306 display(0x3c, 4, 15, 16);

//Tempo do último envio
long lastSendTime = 0;

//Intervalo entre os envios
#define INTERVAL 500

void receive(int packetSize){
  if (packetSize){
    char i = 0;
    //Armazena os dados do pacote em uma array
    while(LoRa.available()){
      received[i] = (char) LoRa.read();
      i++;
    }
    for(uint8_t j = 0; j < sizeof(received); j++){
      Serial.print((uint8_t)received[j]);
      if(j == 6){
        Serial.print('\t');
      }
    }
      vTaskDelay(10 / portTICK_PERIOD_MS);
      String waiting = String(millis() - lastSendTime);
      //Mostra no display os dados e o tempo que a operação demorou
      display.clear();
      display.drawString(0, 0, "Recebeu: " + String((uint8_t)received[6]));
      display.drawString(0, 10, "Tempo: " + waiting + "ms");
      display.display();
    }
  }

void send(){
  //beginPacket : abre um pacote para adicionarmos os dados para envio
  LoRa.beginPacket();
  //print: adiciona os dados no pacote
  for (int i = 0; i < sizeof(FrameMaster); i++) {
          LoRa.write((uint8_t)FrameMaster[i]);
  }
  //endPacket : fecha o pacote e envia
  LoRa.endPacket(); //retorno= 1:sucesso | 0: falha
}

//Função onde se faz a leitura dos dados que queira enviar
//Poderia ser o valor lido por algum sensor por exemplo
//Você pode alterar a função para fazer a leitura de algum sensor

void Task1Code( void * pvParameters ){
  for(;;){
    //Tenta ler o pacote
    int packetSize = LoRa.parsePacket();
    char k = 0;
    //Verifica se o pacote possui a quantidade de caracteres que esperamos
    if (packetSize != 0){

      //Armazena os dados do pacote em uma string
      while(LoRa.available()){
        received[k] = (char) LoRa.read();
        k++;
      }

      vTaskDelay(10 / portTICK_PERIOD_MS);

      if(memcmp(received, FrameMaster, sizeof(FrameMaster)) == 0){
        //Cria o pacote para envio
        LoRa.beginPacket();

        for (int i = 0; i < sizeof(FrameSlave); i++) {
        LoRa.write((uint8_t)FrameSlave[i]);
        }
        //Finaliza e envia o pacote
        LoRa.endPacket();
        //Mostra no display
        display.clear();
        display.drawString(0, 0, "Enviou: " + String((int)FrameSlave[6]));
        display.display();
      }
    }
  }
}

void Task2Code( void * pvParameters){
  for(;;){
    //Se passou o tempo definido em INTERVAL desde o último envio
    if (millis() - lastSendTime > INTERVAL){
      //Marcamos o tempo que ocorreu o último envio
      lastSendTime = millis();
      //Envia o pacote para informar ao Slave que queremos receber os dados
      send();
    }
    while(1){
      //Verificamos se há pacotes para recebermos
      int packetSize = LoRa.parsePacket();
      if(packetSize != 0){
        //caso tenha recebido pacote chama a função para configurar os dados que serão mostrados em tela
        receive(packetSize);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        break;
      }
      else if(millis() - lastSendTime > 5000){
        break;
      }
    }
  } 
}

void setupDisplay(){
  //O estado do GPIO16 é utilizado para controlar o display OLED
  pinMode(16, OUTPUT);
  //Reseta as configurações do display OLED
  digitalWrite(16, LOW);
  delay(50);
  //Para o OLED permanecer ligado, o GPIO16 deve permanecer HIGH
  //Deve estar em HIGH antes de chamar o display.init() e fazer as demais configurações,
  //não inverta a ordem
  digitalWrite(16, HIGH);

  //Configurações do display
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  //display.setTextAlignment(TEXT_ALIGN_LEFT);
}

//Configurações iniciais do LoRa
void setupLoRa(){ 
  //Inicializa a comunicação
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DI00);

  //Inicializa o LoRa
  if (!LoRa.begin(BAND, true)){
    //Se não conseguiu inicializar, mostra uma mensagem no display
    display.drawString(0, 0, "Erro ao inicializar o LoRa!");
    display.display();
    while (1);
  }

  //Ativa o crc
  LoRa.enableCrc();
  //Ativa o recebimento de pacotes
  LoRa.receive();
}

//Compila apenas se MASTER estiver definido no arquivo principal
#ifdef MASTER

void setup(){
  Serial.begin(9600);
  Serial.println("Iniciei");
  //Chama a configuração inicial do display
  setupDisplay();
  //Chama a configuração inicial do LoRa
  setupLoRa();
  display.clear();
  display.drawString(0, 0, "Master");
  display.display();
  xTaskCreatePinnedToCore(Task2Code, "Task2", 10000, NULL, 1, NULL, 0);
  delay(500);
}

void loop(){
}

#endif

//Compila apenas se MASTER não estiver definido no arquivo principal
#ifndef MASTER

void setup(){
    Serial.begin(9600);
    //Chama a configuração inicial do display
    setupDisplay();
    //Chama a configuração inicial do LoRa
    setupLoRa();
    display.clear();
    display.drawString(0, 0, "Slave esperando...");
    display.display();
    xTaskCreatePinnedToCore(Task1Code, "Task1", 10000, NULL, 1, NULL, 0);
    delay(500);
}

void loop(){
}

#endif