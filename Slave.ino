#include <Wire.h>

const int step_pin = 3;
const int direction_pin = 2;

class Carriage {
  public:
    bool halted = true;
    bool setting_up = false;

    void revUp() {
      digitalWrite(direction_pin, move_direction);
      for(int x = 0; x < 200; x++) {
        digitalWrite(step_pin, HIGH);
        delayMicroseconds(move_speed_delay);
        digitalWrite(step_pin, LOW);
        delayMicroseconds(move_speed_delay);
      }
      while(move_speed_delay > 230) {
        move_speed_delay = move_speed_delay - 50;
      }
    }
    void sweep() {
      digitalWrite(direction_pin, move_direction);
      for(int x = 0; x < 200; x++) {
        digitalWrite(step_pin, HIGH);
        delayMicroseconds(move_speed_delay);
        digitalWrite(step_pin, LOW);
        delayMicroseconds(move_speed_delay);
      }
    }
    void receive(int received) {
      if(receiving_high_byte) {
        high_byte = received;
        velocity[index] = high_byte * 256 + low_byte;
        receiving_high_byte = false;
        index++;
      } else {
        low_byte = received;
        receiving_high_byte = true;
      }
      if (index == 2) {
        index = 0;
        if (velocity[0] == 19999){
          halted = true;
          setting_up = false;
        } else if (velocity[1] == 29999) {
          halted = false;
          setting_up = true;
          move_direction = velocity[0] > 0;
          move_speed_delay = 330;
        } else {
          halted = false;
          setting_up = false;
          move_direction = velocity[0] > 0;
          move_speed_delay = velocity[1];
        }
      }
    }
  private:
    int raw_input[2];
    bool receiving_high_byte = false;
    int high_byte;
    int low_byte;
    int velocity[2];
    bool move_direction;
    int move_speed_delay;
    int index = 0;
};

void setup() {

  pinMode(step_pin,OUTPUT); //Stepper STEP pin
  pinMode(direction_pin,OUTPUT);  //Stepper DIRECTION pin

  Wire.begin(8);                // join i2c bus with address #8
  Wire.onReceive(receiveEvent); // register event
  Serial.begin(9600);           // start serial for output
}

Carriage MonoChrome;

void loop() {
  if (!MonoChrome.halted) {
    if (MonoChrome.setting_up) {
      MonoChrome.revUp();
    } else {
      MonoChrome.sweep();
    }
  }
}

void receiveEvent() {
  int received_byte = Wire.read();    // receive byte as an integer
  MonoChrome.receive(received_byte);
}
