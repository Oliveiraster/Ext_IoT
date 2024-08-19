#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// Definindo o ID máximo e a quantidade de usuários que podem ser cadastrados
#define MAX_USERS 60
#define EEPROM_SIZE 2048 // Tamanho da EEPROM (2KB)
#define USER_DATA_SIZE 40 // Tamanho máximo para cada usuário (40 bytes)

// Definição dos pinos do ESP32 conectados aos módulos e componentes
#define LedVerde 26
#define LedVermelho 12
#define tranca 2
#define buzzer 15
#define SS_PIN 14
#define RST_PIN 27
#define SENSOR_PORTA 13

// Informações da rede Wi-Fi
const char* ssid = "NomeDaRedeWiFi";
const char* password = "SenhaDaRedeWiFi";
const char* apiURL = "http://api-seu-servidor.com/cadastrar-acesso"; // URL da API 

MFRC522 mfrc522(SS_PIN, RST_PIN);   
LiquidCrystal_I2C lcd(0x27, 16, 2);

struct Usuario {
  char tagID[12];
  char nome[28];
};

void inicializarEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
}

bool cadastrarUsuario(String tagID, String nome) {
  int usuariosCadastrados = obterQuantidadeUsuariosCadastrados();
  if (usuariosCadastrados >= MAX_USERS) {
    Serial.println("Limite de usuários atingido.");
    return false;
  }

  if (verificarUsuarioExistente(tagID)) {
    Serial.println("Usuário já cadastrado.");
    return false;
  }

  Usuario novoUsuario;
  tagID.toCharArray(novoUsuario.tagID, sizeof(novoUsuario.tagID));
  nome.toCharArray(novoUsuario.nome, sizeof(novoUsuario.nome));

  int endereco = usuariosCadastrados * USER_DATA_SIZE;
  for (int i = 0; i < sizeof(Usuario); i++) {
    EEPROM.write(endereco + i, *((char*)&novoUsuario + i));
  }
  EEPROM.commit();
  return true;
}

bool verificarUsuarioExistente(String tagID) {
  int usuariosCadastrados = obterQuantidadeUsuariosCadastrados();
  for (int i = 0; i < usuariosCadastrados; i++) {
    Usuario usuario;
    lerUsuarioDaEEPROM(i, usuario);
    if (tagID.equals(usuario.tagID)) {
      return true;
    }
  }
  return false;
}

int obterQuantidadeUsuariosCadastrados() {
  for (int i = 0; i < MAX_USERS; i++) {
    Usuario usuario;
    lerUsuarioDaEEPROM(i, usuario);
    if (strlen(usuario.tagID) == 0) {
      return i;
    }
  }
  return MAX_USERS;
}

void lerUsuarioDaEEPROM(int indice, Usuario &usuario) {
  int endereco = indice * USER_DATA_SIZE;
  for (int i = 0; i < sizeof(Usuario); i++) {
    *((char*)&usuario + i) = EEPROM.read(endereco + i);
  }
}

void enviarDadosAPI(String tagID, String nomeUsuario) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(apiURL);
    http.addHeader("Content-Type", "application/json");

    String json = "{\"tagID\":\"" + tagID + "\",\"nome\":\"" + nomeUsuario + "\",\"horario\":\"" + millis() + "\"}";

    int httpResponseCode = http.POST(json);

    if (httpResponseCode > 0) {
      Serial.println("Dados enviados para API com sucesso.");
    } else {
      Serial.printf("Erro ao enviar para API: %d\n", httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi desconectado.");
  }
}

void setup() {
  Serial.begin(115200);

  lcd.begin();
  lcd.print("Conectando WiFi");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(0, 1);
    lcd.print("Tentando...");
  }

  lcd.clear();
  lcd.print("Conectado WiFi");
  Serial.println("WiFi conectado!");
  Serial.println(WiFi.localIP());

  SPI.begin();
  mfrc522.PCD_Init();

  pinMode(LedVerde, OUTPUT);
  pinMode(LedVermelho, OUTPUT);
  pinMode(tranca, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(SENSOR_PORTA, INPUT_PULLUP);

  inicializarEEPROM();
}

void loop() {
  int estadoPorta = digitalRead(SENSOR_PORTA);
  digitalWrite(buzzer, estadoPorta == HIGH ? HIGH : LOW);

  lcd.home();
  lcd.print("Aguardando");
  lcd.setCursor(0, 1);
  lcd.print("Leitura RFID");

  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  String conteudo = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    conteudo.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  conteudo.toUpperCase();

  if (verificarUsuarioExistente(conteudo)) {
    Serial.println("Acesso permitido.");
    digitalWrite(LedVerde, HIGH);
    lcd.clear();
    lcd.print("Acesso Liberado");

    digitalWrite(tranca, HIGH);

    for (byte s = 5; s > 0; s--) {
      lcd.setCursor(8, 1);
      lcd.print(s);
      delay(1000);
    }

    digitalWrite(tranca, LOW);
    digitalWrite(LedVerde, LOW);

    Usuario usuario;
    lerUsuarioDaEEPROM(0, usuario); 
    enviarDadosAPI(conteudo, usuario.nome);

    lcd.clear();
  } else {
    digitalWrite(LedVermelho, HIGH);
    lcd.clear();
    lcd.print("Acesso Negado");
    for (byte s = 5; s > 0; s--) {
      lcd.setCursor(8, 1);
      lcd.print(s);
      delay(800);
      digitalWrite(buzzer, HIGH);
      delay(200);
      digitalWrite(buzzer, LOW);
    }
    digitalWrite(LedVermelho, LOW);
    lcd.clear();
  }
}