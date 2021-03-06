//Переключение громкости звука и треков
//модифицированными кнопками tiptronic на руле
//или любыми другими резистивными кнопками
//в магнитоле Gamma V
//
//в зависимости от сопротивления на аналоговом пине A0 посылает цифровую последовательность на D9
//позже на пине D10 планируется реализоватьактивацию режима Mute
//
// За раскодировку сигналов на магнитоле спасибо dmitryshagin (https://github.com/dmitryshagin)
//
//© Sergey S,2022
#include <EEPROM.h> // подключаем библиотеку работы с памятью
float Sr=0.15;  //дисперсия (погрешность определения) сопротивления %
int shortDelay = 100; //время задержки для долгого нажатия, ms
int longDelay = 1200; //время задержки для долгого нажатия, ms

int analogPin= 0; //пин управляющего входа A0, analogPin-R2-GND
float R1= 550;    //контрольное сопротивление, +5V-R1-analogPin
int ResistValues[]= {0, 376, 100, 0}; //начальные сопротивления кнопок: none, up (vol+), down (vol-), mute (или нажание двух кнопок разом)

#define LED_ON      (PORTB |=  (1<<5))
#define LED_OFF     (PORTB &= ~(1<<5))

#define OUT_ON      (PORTB |=  (1<<1)) //D9 +5V
#define OUT_OFF     (PORTB &= ~(1<<1))

#define MUTE_ON      (PORTB |=  (1<<2)) //D10 +5V
#define MUTE_OFF     (PORTB &= ~(1<<2))

int Button_Now = 0;
int Pressed_Button = 0;
float Pressed_Time = 0;
int R2min[] = {0,0,0,0};
int R2max[] = {0,0,0,0};

int raw= 0;

char* ButtonNames[]= {"NaN", "Vol +", "Vol -", "Mute", "Next", "Prev"};
static const uint8_t C[6][15] = {
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //----
      {0x00, 0x00, 0xFF, 0x5D, 0x55, 0x77, 0x77, 0x5D, 0x57, 0x55, 0x55, 0x77, 0x77, 0x77, 0x77}, //vol+
      {0x00, 0x00, 0xFF, 0x5D, 0x55, 0x77, 0x77, 0x5D, 0x55, 0x55, 0x57, 0x77, 0x77, 0x77, 0x77}, //vol-
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //mute
      {0x00, 0x00, 0xFF, 0x5D, 0x55, 0x77, 0x77, 0x5D, 0x57, 0x75, 0xD5, 0x55, 0xD7, 0x77, 0x77}, //up
      {0x00, 0x00, 0xFF, 0x5D, 0x55, 0x77, 0x77, 0x5D, 0x55, 0xD7, 0x55, 0x75, 0xD7, 0x77, 0x77}};//down 
      

void send_byte(uint8_t byte){
  uint8_t bit;
  for(bit=0;bit<8;bit++){
    if((byte >> (7-bit))  & 0x01){
      OUT_ON;
      LED_ON;
    }else{
      OUT_OFF;
      LED_OFF;
    }
    delayMicroseconds(444);
  }
}

void send_repeat(){
  send_byte(0x00);
  send_byte(0x00);
  send_byte(0xF7);
  Serial.println("& repeat");  
}

void send_command(uint8_t number){
  uint8_t byte=0;
  for(byte=0;byte<15;byte++){
    send_byte(C[number][byte]);
  }
  LED_ON;
  Serial.print(ButtonNames[number]);
  Serial.print(" pressed, t=");
  Serial.println(millis() - Pressed_Time);
}

void resistWrite(byte paramA, int paramB){ //записать значения сопротивлений из EEPROM
  byte hi = highByte(paramB); // старший байт
  byte low = lowByte(paramB); // младший байт
  EEPROM.write(2*paramA, hi);  // записываем в ячейку 1 старший байт
  EEPROM.write(2*paramA+1, low); // записываем в ячейку 2 младший байт
}

int resistRead(byte paramA) { //прочитать значения сопротивлений из EEPROM
    byte hi = EEPROM.read(2*paramA); // старший байт
    byte low = EEPROM.read(2*paramA+1); // младший байт      
    return word(hi, low);
}

void resistDefaults(){ //запустить чтобы установить дефолтные значения
  for (int i=0; i<4; i++) { 
    resistWrite(i,ResistValues[i]);
    Serial.print(i);
    Serial.print("=");
    Serial.println(ResistValues[i]);
  }
}

int R2Count(int Rraw){   //определить сопротивление кнопки
  float Vin= 5;
  float Vout= (Rraw * Vin)/1024.0;;
  return (R1 * Vout)/(Vin-Vout);
}

void learn(byte count){ //определение и сохранение сопротивлений кнопок (в основном цикле закомменчена)
  Serial.println("Setup mode");
  delayMicroseconds(300); 
  LED_ON;
  delayMicroseconds(1500);
  LED_OFF; 
  LED_ON;
  delayMicroseconds(1500);
  LED_OFF; 
  for (int i=0; i<4; i++) { 
      Serial.print("Setting ");
      Serial.print(ButtonNames[i]);
      Serial.println(". Hold it down");
      for (int j=0; j<i; j++) {
        LED_ON;
        delayMicroseconds(500);
        LED_OFF;
        delayMicroseconds(500);     
   }
  raw= analogRead(analogPin);
  float R2= R2Count(raw);
  if ((raw>1024*Sr)&&(raw<1024*(1-Sr))){        
        Serial.print(R2);
      //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        resistWrite(i,R2);
        Serial.println(" Ohms now. Set is Ok!");
        Serial.println();
      }else{
        Serial.print(R2);
        //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        resistWrite(i,0);
        Serial.println(" Ohms now. NOT set and resetted to 0!");
        Serial.println();
      }
   delayMicroseconds(300); 
  }
} 

void commandActivate(int button){ //send command to gamma
    if (button != 3){ //проверка на нажатие вызывающее mute
        send_command(button);   
    }else{
      //sendMute
    }     
}

int detectButton(){    //проверка нажата ли кнопка или просто наводка
  raw= analogRead(analogPin);
  float R2= R2Count(raw);

  int res= 0;
  for(int i=0;i<3;i++){  //
    if ((R2 > R2min[i])&&(R2 < R2max[i])){//проверка на соответствие сохранённым кнопкам
      res = i;
    }
  }   
  return res;  
}
  
void setup(){
  Serial.begin(9600);
  Serial.println("Tiptronic2Gamma resistance remote");
  //resistDefaults();
  //Serial.println("Send +5V to D12 for button setup");
  DDRB  = (1<<1)|(1<<2)|(0<<4)|(1<<5); //D14 input D9, D10, LED output
  //resistDefaults();
  //learn(); //переобучить кнопки

  //читаем сопротивления из памяти
  Serial.println("Resistance values for buttons:");
  for (int i=1; i<4; i++) { 
    ResistValues[i]=resistRead(i);
    Serial.print("R");
    Serial.print(i);
    Serial.print("=");
    Serial.println(ResistValues[i]);
  }

//определяем диапазоны сопротивлений
  for (int i=0; i<3; i++) { 
    R2min[i] = ResistValues[i]*(1-Sr);
    R2max[i] = ResistValues[i]*(1+Sr);
 }  
}

void loop(){
  Pressed_Button = Button_Now; //прошлая кнопка
  Button_Now = detectButton(); //новая кнопка

  //сбрасываем таймер времени, в момент нажатия
  if ((Button_Now != Pressed_Button)&&(Button_Now != 0)){
    Pressed_Time = millis();
  }

  //долгие нажатия кнопок
  if ((millis() > Pressed_Time+longDelay)&&(Button_Now != 0)){ 
    commandActivate(Button_Now+3); //запускаем команду
    while (Button_Now != 0){
      Button_Now = detectButton();
      delayMicroseconds(shortDelay);   
    }
  }

  //обработка коротких нажатий
  if ((Button_Now != Pressed_Button)&&(Button_Now != 0)){
    for (int i=0; i<3; i++) {//запускаем коротку команду несколько раз чтобы громкость регулировалась быстрее
      commandActivate(Button_Now);
      delayMicroseconds(shortDelay);  
    }
         
  }

  LED_OFF;
}
