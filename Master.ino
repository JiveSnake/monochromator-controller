#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal.h>

LiquidCrystal lcd(11, 10, A0, A1, A2, A3);

const int stop_switches_pin = 13;
const int encoder_pin = 12;
const double encoder_range = 2000; //Encoder count from switch to switch
const double wavelength_range = 2500; //Wavelength from swtich to switch
const double steps_per_encoder = 500;

const byte ROWS = 4; // Four rows
const byte COLS = 4; // Three columns
char keys[ROWS][COLS] = { // Define the Keymap
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = { 6, 7, 8, 9 }; // Keypad ROW0, ROW1, ROW2 and ROW3 pins.
byte colPins[COLS] = { 2, 3, 4, 5 }; // Keypad COL0, COL1, COL2 and COL3 pins.

Keypad kpd = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS ); // Create the Keypad

class Display {
  public:
    void updateDisplay(String text, int new_position) {
      lcd.setCursor(0,0);
      lcd.print(text);
      setCursorPosition(new_position);
    }
    void inputKey(char input_key) {
      if (cursor_position - cursor_start < 3){
        lcd.print(input_key);
        cursor_position++;
      } else if (cursor_position - cursor_start == 3) {
        lcd.print(input_key);
        lcd.noBlink();
        cursor_position++;
      }
    }
    void reset(void){
      lcd.clear();
      updateDisplay("Start Wavelength", 0);
    }
  private:
    int cursor_position;
    int cursor_start;

    void setCursorPosition(int new_position) {
      lcd.setCursor(new_position,1);
      lcd.blink();
      cursor_position = new_position;
      cursor_start = new_position;
    }
};

class Outputs {
  public:
    bool error = false;
    int raw_outputs[3] = {0, 0, 0};
    int start_position = 0;
    int end_position = 0;
    int sweep_vector[2] = {0, 0};
    
    void convertInput(char input_key) {
      if(digit == 4) {
      } else {
        input_number_string[digit] = input_key;
        input_number = atoi(input_number_string);
        digit++;
      }
    }
    void confirm(void) {
      raw_outputs[current_input] = input_number;
      memset(input_number_string,0,sizeof(input_number_string));
      digit = 0;
      current_input++;
    }
    void calculate(void) {
      calculateStartPosition(raw_outputs[0]);
      calculateEndPosition(raw_outputs[1]);
      calculateSweepVector(raw_outputs[0], raw_outputs[1], raw_outputs[2]);
    }
    void reset(void) {
      error = false;
      digit = 0;
      memset(raw_outputs, 0, sizeof(raw_outputs));
      current_input = 0;
    }
  private:
    int digit = 0;
    int current_input = 0;
    char input_number_string[4];
    int input_number;
    
    void calculateStartPosition(int start_wavelength) {
      start_position = (start_wavelength * encoder_range) / wavelength_range;
    }
    
    void calculateEndPosition(int end_wavelength) {
      end_position = (end_wavelength * encoder_range) / wavelength_range;
    }
    
    void calculateSweepVector(int end_wavelength, int start_wavelength, int sweep_time) {
      double sweep_nanometers = abs(end_wavelength - start_wavelength);
      double wavelength_per_encoder = wavelength_range / encoder_range;
      double microseconds_per_second = 1000000;
      double microseconds_per_steps_per_encoder = microseconds_per_second / steps_per_encoder;
      double steps_per_microsecond = (sweep_time/sweep_nanometers) * microseconds_per_steps_per_encoder * wavelength_per_encoder;

      if(start_wavelength - end_wavelength >= 0) {
        sweep_vector[0] = -1;
      } else {
        sweep_vector[0] = 1;
      }
      sweep_vector[1] = steps_per_microsecond/2;
    }
};

class Slave {
  public:
    bool initializing = false;
    bool initialized = false;
    bool halted = false;
    bool moving_to_start = false;
    bool at_start = false;
    bool sweeping = false;
    bool sweep_finished = false;
    int target;

    void initialize(void){
      if (digitalRead(stop_switches_pin) == HIGH) {
        halt();
        initialized = true;
        halted = true;
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Start Wavelength");
        lcd.setCursor(0,1);
        lcd.blink();
      }
      else if (!initializing) {
        transmit_vector[0] = 1;
        transmit_vector[1] = 29999;
        initializing = true;
        transmit(transmit_vector);
        halted = false;
      }
    }
    void halt(void) {
      int stop_code[2] = {19999, 19999};
      transmit(stop_code);
      halted = true;
    }
    void reset(void) {
      moving_to_start = false;
      at_start = false;
      sweeping = false;
      sweep_finished = false;
    }
    
    void moveToStart(int start_position) {
      Serial.print("At Position: ");
      Serial.println(current_position);
      Serial.print("Moving to: ");
      Serial.println(start_position);
      if (current_position - start_position < 0) {
        transmit_vector[0] = -1;
      }
      else if (current_position - start_position > 0) {
        transmit_vector[0] = 1;
      } else {
        transmit_vector[0] = 19999;
      }
      transmit_vector[1] = 29999;
      transmit(transmit_vector);
      target = start_position;
      halted = false;
      moving_to_start = true;
      lcd.setCursor(0,0);
      lcd.print("Moving to Start");
      lcd.setCursor(0,1);
      lcd.print("    Cancel-C    ");
      lcd.noBlink();
    }
    void sweep(int input_vector[], int end_position) {
      Serial.print("Sweeping to: ");
      Serial.println(end_position);
      transmit_vector[0] = input_vector[0];
      transmit_vector[1] = input_vector[1];
      transmit(transmit_vector);
      target = end_position;
      halted = false;
      at_start = false;
      sweeping = true;
      lcd.setCursor(0,0);
      lcd.print("  Sweeping....  ");
      lcd.setCursor(0,1);
      lcd.print("    Cancel-C    ");
      lcd.noBlink();
    }
    int currentPosition(void) {
      current_encoder_read = digitalRead(encoder_pin);
      if (current_encoder_read != previous_encoder_read) {
        if (transmit_vector[0] < 0) {
          current_position++;
        } else if (transmit_vector[0] > 0) {
          current_position--;
        }
      }
      previous_encoder_read = current_encoder_read;
      return current_position;
    }
    private:
      int transmit_vector[2] = {0, 0};
      int current_position = 0;
      int previous_encoder_read = 0;
      int current_encoder_read;

      void transmit(int output[]) {
        for(int i = 0; i < 2; i++) {
          Wire.beginTransmission(8);
          Wire.write(lowByte(output[i]));
          Wire.endTransmission();
          Wire.beginTransmission(8);
          Wire.write(highByte(output[i]));
          Wire.endTransmission();
        }
    }
};

void setup(){
  pinMode(stop_switches_pin, INPUT);
  pinMode(encoder_pin, INPUT);
  lcd.begin(16, 2);
  lcd.setCursor(0,0);
  lcd.print("Initializing...");
  lcd.setCursor(0,1);
  lcd.print(" Please Wait... ");
  lcd.noBlink();
  Serial.begin(9600);
  Wire.begin(); // join i2c bus (address optional for master)
}

Display LCDDisplay;
Outputs MasterOutputs;
Slave StepperController;

void loop() {
  /*if (digitalRead(stop_switches_pin) == HIGH) {
    if (!StepperController.halted) {
      StepperController.halt();
    }
  }*/
  char key = kpd.getKey();
  if (!StepperController.initialized) {
    StepperController.initialize();
  }
  else if (StepperController.sweep_finished) {
    if (key) {
      switch(key){
        case 'A':
          StepperController.reset();
          StepperController.moveToStart(MasterOutputs.start_position);
          break;
        case 'C':
          LCDDisplay.reset();
          MasterOutputs.reset();
          StepperController.reset();
          break;
         default:
          break;
      }
    }
  }
  else if (StepperController.at_start) {
    if (key) {
      switch (key) {
        case 'A':
          StepperController.sweep(MasterOutputs.sweep_vector, MasterOutputs.end_position);
          break;
        case 'C':
          LCDDisplay.reset();
          MasterOutputs.reset();
          StepperController.reset();
          break;
        default:
          break;
      }
    }
  }
  else if (StepperController.moving_to_start == true or StepperController.sweeping == true) {
    Serial.print(StepperController.currentPosition());
    if (StepperController.currentPosition() == StepperController.target and StepperController.moving_to_start == true) {
      Serial.print("Arrived at: ");
      Serial.println(StepperController.target);
      StepperController.halt();
      StepperController.moving_to_start = false;
      StepperController.at_start = true;
      lcd.setCursor(0,0);
      lcd.print("     Ready     ");
      lcd.setCursor(0,1);
      lcd.print("Run-A   Cancel-C");
    }
    else if (StepperController.currentPosition() == StepperController.target and StepperController.sweeping == true) {
      Serial.print("Finished at: ");
      Serial.println(StepperController.target);
      StepperController.halt();
      StepperController.sweeping == false;
      StepperController.sweep_finished = true;
      lcd.setCursor(0,0);
      lcd.print(" Sweep Finished ");
      lcd.setCursor(0,1);
      lcd.print("Rerun-A  Reset-C");
    }
    if (key) {
      switch (key) {
        case 'C':
          StepperController.halt();
          LCDDisplay.reset();
          MasterOutputs.reset();
          StepperController.reset();
          break;
        default:
          break;
      }
    }
  }
  else if (MasterOutputs.error) {
    if (key == 'C') {
      LCDDisplay.reset();
      MasterOutputs.reset();
    }
  }
  else {
    if (MasterOutputs.raw_outputs[2] > 0) {
      lcd.setCursor(0,0);
      lcd.print(" OK-A   Clear-C ");
      if(key) {
      switch(key){
        case 'A': {
          MasterOutputs.calculate();
          if (MasterOutputs.sweep_vector[1] < 330) {
            lcd.setCursor(0,0);
            lcd.print("*Time too small*");
            lcd.setCursor(0,1);
            lcd.print("   Cancel-C   ");
            lcd.noBlink();
            MasterOutputs.error = true;
          } else {
            StepperController.moveToStart(MasterOutputs.start_position);
          }
          break;
        }
        case 'C': {
          LCDDisplay.reset();
          MasterOutputs.reset();
          break;
        }
        default:
          break;
        }
      }
    }
    else if(key) {
      switch (key) {
        case 'A':
          MasterOutputs.confirm();
          if (MasterOutputs.raw_outputs[1] > 0) {
            LCDDisplay.updateDisplay("            Time", 12);
          }
          else if (MasterOutputs.raw_outputs[0] > 0) {
            LCDDisplay.updateDisplay(" End Wavelength ", 6);
          }
          break;
        case 'B':
          break;
        case 'C':
          LCDDisplay.reset();
          MasterOutputs.reset();
          break;
        case 'D':
          break;
        case '*':
          break;
        case '#':
          break;
        default:
          MasterOutputs.convertInput(key);
          LCDDisplay.inputKey(key);
          break;
      }
    }
  }
}


