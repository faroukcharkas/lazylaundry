#include <Adafruit_TCS34725.h>
#include <Servo.h>

// Config
double LIGHT_DARK_LINE = 110;
double EMPTY_MARGIN = 10;
double REPEAT_VALUE_THRESHOLD = 10;
double MULTIPLIER = 1.2;

// Pin Map
int lightServoPin = 9;
int darkServoPin = 11;

// Closed = \/    Open = \ \ or / /
enum SorterState { Idle, Dispensing };
enum Color { Light, Dark, Empty };
char *ColorTypes[] =
{
  "Light",
  "Dark",
  "Empty"
};

// Global Definitions
Servo lightServo;
Servo darkServo;

// Servo on left Side
// 120 -> Closed
// 180 -> Open

// Light Servo
// 75 -> Open
// 15 -> Close

// Dark Servo
// 40 -> Open
// 105 -> Close


Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_154MS, TCS34725_GAIN_4X);

// Classes
class ColorSensor {
  private:
    double _ambientMean;
    double _ambientSampleSize;
    int _repeatValue;
    double _repeatCount;
    uint64_t _normalClearv;
    void _registerMean(double mean) {
    }
  public:
    ColorSensor() {
      _ambientMean = 175;
      _ambientSampleSize = 1;
      _repeatValue = 0;
      _repeatCount = 0;
      _normalClearv = 0;
    }
    void calibrate() {
      uint16_t red, green, blue, clearv;
      int checks = 0;
      while (checks < 10) {
        Serial.print("Calibrating");
        tcs.getRawData(&red, &green, &blue, &clearv);
        if (abs(clearv - _normalClearv) < 3) {
          checks += 1;
        } else {
          _normalClearv = clearv;
          checks = 0;
        }
      }
    }
    double readRGBMean(bool addToAmbient) {
      double mean;
      uint16_t red, green, blue, clearv;
      tcs.getRawData(&red, &green, &blue, &clearv);
      mean = red + green + blue;
      mean = mean / 3;
      if (mean > (EMPTY_MARGIN + _ambientMean) || mean < (EMPTY_MARGIN - _ambientMean)) {
        addToAmbient = false;
      }
      Serial.print("Ambient: ");
      Serial.print(_ambientMean);
      Serial.print("  Mean: ");
      Serial.print(mean);
      Serial.print("\n");
      this->_registerMean(mean);
      return mean;
    }
    Color getColor() {
      double mean = this->readRGBMean(true);
      if ((mean > (_ambientMean - EMPTY_MARGIN)) and (mean < (_ambientMean + EMPTY_MARGIN))) {
        return Empty;
      }
      if (mean < (_ambientMean - EMPTY_MARGIN)) {
        return Dark;
      }
      if (mean > (_ambientMean * MULTIPLIER)) {
        return Light;
      }
      if ((mean < (_ambientMean * MULTIPLIER) and (mean > (_ambientMean + EMPTY_MARGIN)))) {
        return Dark;
      }
      return Empty;
    }
    bool areClothesPresent(uint64_t threshold) {
      uint16_t red, green, blue, clearv;
      tcs.getRawData(&red, &green, &blue, &clearv);
      if (abs(clearv - _normalClearv) <= threshold) {
        return false;
      } else {
        return true;
      }
    }
};

class Flap {
  private:
    Color _serviceColor;
    bool _flapOpen;
    Servo _servo;
    int _position;
    int _direction;
    int _darkClosePosition = 105;
    int _lightClosePosition = 15;
    int _toggleDelta = 60;
  public:
    Flap(Color serviceColor, Servo servo)
    {
      _serviceColor = serviceColor;
      _position = 0;
      _servo = servo;
      _flapOpen = false;

      if (serviceColor == Light) {
        _direction = 1;
        setPosition(_lightClosePosition);
      } else if (serviceColor == Dark) {
        _direction = -1;
        setPosition(_darkClosePosition);
      }
    }
    int getPosition() {
      return _position;
    }
    int setPosition(int desiredPosition) {
      _position = desiredPosition;
      _servo.write(_position);
    }
    void toggleFlap() {
      // Reverse the flap variable
      _flapOpen = !_flapOpen;
      if (_flapOpen == true) {
        // Open the flaps
        if (_serviceColor == Light) {
          setPosition(getPosition() + (_toggleDelta * _direction));
        } else if (_serviceColor == Dark) {
          setPosition(getPosition() + (_toggleDelta * _direction));
        }
      } else {
        // Close the flaps
        setPosition(60 - (_toggleDelta * _direction));
        if (_serviceColor == Light) {
          setPosition(_lightClosePosition);
        } else if (_serviceColor == Dark) {
          setPosition(_darkClosePosition);
        }
      }
    }
};

class LaundrySorter {  
  private:
    SorterState _sorterState;
    int _waitMS;
    Flap _lightFlap = Flap(Light, lightServo);
    Flap _darkFlap = Flap(Dark, darkServo);
    ColorSensor _colorSensor = ColorSensor();
   public:
    LaundrySorter(int waitMS = 3000)
    {
      _sorterState = Idle;
      _waitMS = waitMS;
    }
    
    SorterState getSorterState()
    {
      return _sorterState;
    }

    void dispense(Color selectedDestination)
    {
      _sorterState = Dispensing;

      if (selectedDestination == Light) {
        _lightFlap.toggleFlap();
        delay(_waitMS);
        _lightFlap.toggleFlap();
      } else if (selectedDestination == Dark) {
        _darkFlap.toggleFlap();
        delay(_waitMS);
        _darkFlap.toggleFlap();
      }
      
    }
    void wiggleFlaps(int delayMS, int wiggleTimes) {
      int i = 0;
      while (i < wiggleTimes) {
        _lightFlap.toggleFlap();
        delay(delayMS);
        _darkFlap.toggleFlap();
        delay(delayMS);
        i += 1;
      }
    }
    void sort() {
      bool sortingOn = true;
      Color colorSensed;
      while (sortingOn == true) {
        delay(500);
        if (_colorSensor.areClothesPresent(50) == true) {
          // Clothes got added
          colorSensed = _colorSensor.getColor();
          this->dispense(colorSensed);
        }
      }
    }
    void calibrate() {
      Serial.print("Calibrating...");
      _colorSensor.calibrate();
      Serial.print("\n");
      Serial.print("Calibrated!");
      this->wiggleFlaps(500, 4);
    }

};


LaundrySorter lazyLaundry = LaundrySorter(5000);

void setup()
{
  Serial.begin(9600);
  Serial.println("Setting up...");
  lightServo.attach(lightServoPin,500,2500);
  darkServo.attach(darkServoPin,500,2500);
  if (tcs.begin()) {
    Serial.println("Found sensor!");
  } else {
    Serial.println("No TCS34725 found ... check your connections!");
    while (1); // halt!
  }
  Serial.println("Hello!!!");
  lazyLaundry.calibrate();
}

void loop()
{
  Serial.print("Sorting");
  lazyLaundry.sort();
}
