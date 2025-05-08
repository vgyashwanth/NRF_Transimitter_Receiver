// Transimitter Code 
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include<stdint.h>


#define CE_PIN 2
#define CSN_PIN 4

#define DIGITAL_CH1 6
#define DIGITAL_CH2 7
#define DIGITAL_CH3 9
#define DIGITAL_CH4 8
#define THROTTLE A6
#define STEERING A7
#define HEAD_POT A5
#define TOP_TRIM_POT A4
#define DOWN_TRIM_POT A3

RF24 radio(CE_PIN, CSN_PIN); // CE, CS
// address of the nodes
const byte address[6] = "00001";


//Data packet
struct DataPacket
{
  uint16_t throttle;
  uint8_t streeing;
  uint8_t head_pot;
  uint8_t down_trim_pot;
  uint8_t top_trim_pot;
  uint8_t digital_ch1 : 1;
  uint8_t digital_ch2 : 1;
  uint8_t digital_ch3 : 1;
  uint8_t digital_ch4 : 1;
  uint8_t left_turn   : 1;
  uint8_t right_turn  : 1;
  uint8_t forward     : 1;
  uint8_t backward    : 1;
};

void ProcessData(DataPacket* Data);

void setup() 
{
  Serial.begin(9600);

  // !!!Warning!!! dont remove this 
  // lead to Hardware Error
  analogReference(EXTERNAL); // use AREF for reference voltage
  // NRF Module Settings
  radio.begin();
  // (2400MHz + 100MHz) Frequency Channel
  // both Transimitter and Receiver should be on same channel
  // helps when multiple NRF modules communicating in same space
  radio.setChannel(100); 
  radio.setDataRate(RF24_250KBPS);
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MIN);
  radio.setPayloadSize(sizeof(DataPacket));
  radio.stopListening();

  // Hardware Settings
  pinMode(DIGITAL_CH1,INPUT_PULLUP);
  pinMode(DIGITAL_CH2,INPUT_PULLUP);
  pinMode(DIGITAL_CH3,INPUT_PULLUP);
  pinMode(DIGITAL_CH4,INPUT_PULLUP);

  

}

void loop() 
{   
  DataPacket data;
  ProcessData(&data);
  radio.write(&data, sizeof(data));
  
}

void ProcessData(DataPacket* data)
{

    uint16_t AnalogValA6 = analogRead(THROTTLE);
    uint16_t AnalogValA7 = analogRead(STEERING);
    uint16_t AnalogValA5 = analogRead(HEAD_POT);
    bool DigitalValCH1 = digitalRead(DIGITAL_CH1);
    bool DigitalValCH3 = digitalRead(DIGITAL_CH3);

    
    // Throttle
    if(AnalogValA6 < 287)
    {
      data->forward = true;
      data->backward = false;
      data->throttle = (uint16_t)(map(AnalogValA6,155, 287, 2000, 1500));
    }
    else if(AnalogValA6 > 295)
    {
      data->forward = false;
      data->backward = true;
      data->throttle = (uint16_t)(map(AnalogValA6,295, 382, 1500, 1000));
    }
    else 
    {
      data->forward = false;
      data->backward = false;
      data->throttle = 1500; // stop the motor
    }

    // Steering 
    if(AnalogValA7 < 556)
    {
      data->right_turn = true;
      data->left_turn = false;
      uint8_t angle = (-0.189*AnalogValA7 + 195.315);
      data->streeing = angle;
    }
    else if(AnalogValA7 > 558)
    {
      data->right_turn = false;
      data->left_turn = true;
      uint8_t angle = (-0.25*AnalogValA7 + 229.25);
      data->streeing = angle;
    }
    else 
    {
      data->right_turn = false;
      data->left_turn = false;

       // Steering Trim
      if(((int16_t)(600-(int16_t)AnalogValA5)>0) && (AnalogValA5 > 200))
      {
        data->head_pot = (uint8_t)((600-AnalogValA5)/10);
        data->streeing = (90+data->head_pot);
      }
      else if(((int16_t)((int16_t)AnalogValA5-600)>0) && (AnalogValA5 < 1000))
      {
        data->head_pot = (uint8_t)((AnalogValA5-600)/10);
        data->streeing = (90-data->head_pot);
      }
      else
      {
        data->head_pot = (uint8_t)0; 
        data->streeing = (90+data->head_pot);
      }
      
    }

    data->digital_ch1 = DigitalValCH1;
    data->digital_ch3 = DigitalValCH3;
  
}




