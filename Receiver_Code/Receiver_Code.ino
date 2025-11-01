// Receiver Code
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <stdint.h>
#include <Servo.h>

#define CE_PIN         2
#define CSN_PIN        4

#define MOTOR_PIN        3
#define SERVO_PIN       10
#define SPOILER_PIN      9
#define HEAD_LIGHTS     A1
#define HEAD_BAR_LIGHTS A2
#define BOOSTER_LIGHTS  A3
#define RED_LIGHTS      A4


// Spolier related variables
#define SPOILER_UP_ANGLE (90-4)
#define SPOILER_DOWN_ANGLE (90+21)

RF24 radio(CE_PIN, CSN_PIN); // CE, CSN
Servo servo;  // create servo object to control a servo
Servo motor;  // for controlling the esc of the motor
Servo spoiler; // for Spoiler

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

void Control(DataPacket received_data);
void ResetSystem(void);
void SpoilerOn(bool On);



void setup() {

  Serial.begin(9600);

  // NRF Module Settings
  radio.begin();

  // (2400MHz + 100MHz) Frequency Channel
  // both Transimitter and Receiver should be on same channel
  // helps when multiple NRF modules communicating in same space
  radio.setChannel(100); 
  radio.setDataRate(RF24_250KBPS);
  radio.openReadingPipe(1, address);
  radio.setPALevel(RF24_PA_MIN);
  radio.setPayloadSize(sizeof(DataPacket));
  radio.startListening();
 
  // Hardware Setup
  servo.attach(SERVO_PIN);
  motor.attach(MOTOR_PIN);
  spoiler.attach(SPOILER_PIN);

  pinMode(HEAD_LIGHTS, OUTPUT);
  pinMode(HEAD_BAR_LIGHTS, OUTPUT);
  pinMode(BOOSTER_LIGHTS, OUTPUT);
  pinMode(RED_LIGHTS, OUTPUT);

  // Spoiler Initial Vlaue
  spoiler.write(SPOILER_DOWN_ANGLE);

  // Motor ESC Calibration
  motor.writeMicroseconds(1500);
  delay(2000);
  
}

void loop()
{
  
  DataPacket data;
  if (radio.available()) 
  {
    radio.read(&data, sizeof(data));
    Control(data);
  }
  else
  { // 
    Serial.println("System reset");
    ResetSystem();
  }
}

void Control(DataPacket received_data)
{
  // Throttle
  if(received_data.forward && !(received_data.backward))
  {
  
    // Setting up the Blue Lights
    if(received_data.throttle > 1900)
    {
        digitalWrite(BOOSTER_LIGHTS,HIGH);
        // SpoilerOn(true);
    }
    else
    {
        digitalWrite(BOOSTER_LIGHTS,LOW);
        // SpoilerOn(false);
    }
    // supply PWM to motor
    motor.writeMicroseconds(received_data.throttle);

    // Turn off the red lights if it is on already
    digitalWrite(RED_LIGHTS,LOW);
    Serial.print("Throttle : ");
    Serial.println(received_data.throttle);
  }
  else if (!(received_data.forward) && received_data.backward)
  {
   
    motor.writeMicroseconds(received_data.throttle);
    // Turn off the booster lights if it is on already
    // Turn on the red lights
    digitalWrite(BOOSTER_LIGHTS,LOW);
    digitalWrite(RED_LIGHTS,HIGH);
    Serial.print("Throttle : ");
    Serial.println(received_data.throttle);

  }
  else
  {
    motor.writeMicroseconds(1500); // stop the motor
    // Turn off the booster lights if it is on already
    // Turn off the red lights if it is on already
    digitalWrite(BOOSTER_LIGHTS,LOW);
    digitalWrite(RED_LIGHTS,LOW);
    Serial.print("Throttle : ");
    Serial.println(received_data.throttle);
  }
  
  // Steering
  if(received_data.right_turn || received_data.left_turn)
  {
    servo.write(received_data.streeing);

    // if ((received_data.right_turn && (received_data.streeing > 115)) &&
    //           (received_data.digital_ch3 == HIGH))
    // {
    //   ToggleRightIndicator();
    // }
    // else if(received_data.digital_ch3 == HIGH)
    // {
    //   digitalWrite(RIGHT_INDICATOR, LOW);
    // }
    // if ((received_data.left_turn && (received_data.streeing < 65)) &&
    //         (received_data.digital_ch3 == HIGH))
    // { 
    //   ToggleLeftIndicator();
    // }
    // // if all indicators blinking turned off 
    // else if(received_data.digital_ch3 == HIGH)
    // {
    //   digitalWrite(LEFT_INDICATOR, LOW);
    // }
    Serial.print("Steering: ");
    Serial.println(received_data.streeing);
  }
  else
  {
    servo.write(received_data.streeing);
    Serial.print("Steering: ");
    Serial.println(received_data.streeing);
  }
  if((received_data.digital_ch1 == LOW) && (received_data.digital_ch2 == HIGH))
  {
    digitalWrite(HEAD_LIGHTS,HIGH);
    digitalWrite(HEAD_BAR_LIGHTS,LOW);
    Serial.print("Head Lights: ");
    Serial.println(received_data.digital_ch1);

  }
  else if((received_data.digital_ch1 == HIGH) && (received_data.digital_ch2 == LOW))
  {
    digitalWrite(HEAD_LIGHTS,HIGH);
    digitalWrite(HEAD_BAR_LIGHTS,HIGH);
    Serial.print("Head Lights: ");
    Serial.println(received_data.digital_ch1);
  }
  else
  {
    // Turn off Both Head Lights
    digitalWrite(HEAD_LIGHTS,LOW);
    digitalWrite(HEAD_BAR_LIGHTS,LOW);
    Serial.println("Both Head Lights Turned Off");

  }
  // Turn on/off the Spolier

  if(received_data.digital_ch3 == LOW)
  {
    // Turn on the Spoiler
    Serial.print("Spoiler Turned On: ");
    Serial.println(!received_data.digital_ch3);

  }
  else
  {
    // Turn off the Spoiler
    Serial.print("Spoiler Turned Off: ");
    Serial.println(!received_data.digital_ch3);
  }

  SpoilerOn(!(received_data.digital_ch3));

}
void ResetSystem(void)
{
 
  motor.writeMicroseconds(1500); // stop the motor

  // Resetting steering
  servo.write(90);

  // Resetting all Lights
  digitalWrite(HEAD_LIGHTS,LOW);
  digitalWrite(HEAD_BAR_LIGHTS, LOW);
  digitalWrite(BOOSTER_LIGHTS,LOW);
  digitalWrite(RED_LIGHTS,LOW);
  // Turn off the Spolier
  SpoilerOn(false);

}

void SpoilerOn(bool On)
{   
  // Read the Current Spoiler Postion
  uint8_t current_angle = spoiler.read();
  Serial.print("Current Angle: ");
  Serial.println(current_angle);
  
  if(On)
  {

    for(uint8_t angle = current_angle; angle>=SPOILER_UP_ANGLE; angle--)
    {
        spoiler.write(angle);
        delay(5);
    }

  }
  else
  {
    for(uint8_t angle = current_angle; angle<=SPOILER_DOWN_ANGLE; angle++)
    {
        spoiler.write(angle);
        delay(5);
    }
  }
  
}
