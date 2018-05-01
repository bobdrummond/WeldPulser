#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ClickEncoder.h>
#include <TimerOne.h>


#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

uint8_t buttonState;
int16_t cursor;
char itval=0;

#define pinA 3
#define pinB 4
#define pinSw 2 //switch
#define STEPS 4
#define pinOut 11

ClickEncoder encoder(pinA, pinB, pinSw, STEPS);


void displayFlash(){
  display.invertDisplay(1);
  display.display();
  delay(100);
  display.invertDisplay(0);
  display.display();

}
class CItem {
protected:
  char const *name, *unit;
public:
  CItem(char const* n, char const* u): name(n), unit(u){  }
  virtual void changeValue(int v){
    displayFlash();
  }
  void renderName(char *buf) {
    sprintf(buf, "%-14s:", name);
  }
  virtual void renderValue(char *buf) {sprintf(buf, "<ERR>");}
};

class CIntItem : public CItem{
  int _min, _max;
public:
  int value;
  CIntItem(char const* n, char const* u, int v, int mi, int ma): CItem(n, u), _min(mi), _max(ma), value(v){  }
  void changeValue(int v){
    value += v;
    if (value > _max) {value = _max; displayFlash();}
    if (value < _min) {value = _min; displayFlash();}
  }
  void renderValue(char *buf) {
    sprintf(buf, "%-3d%s",value, unit);
  }
};

class CdoubleItem : public CItem {
  double _min, _max;
public:
  double value;
  CdoubleItem(char const* n, char const *u, double v, double mi, double ma):
  CItem(n, u), _min(mi), _max(ma), value(v) {}
  void changeValue(int v){
    value += v / 4.0;
    if (value > _max) {value = _max; displayFlash();}
    if (value < _min) {value = _min; displayFlash();}
  }
  void renderValue(char *buf) {
    // Arduino sprintf doesn't handle floats, use casting
    int whole = (int) value;
    int partial = (int)((value - whole) * 100);
    sprintf(buf, "%d.%02d%s",whole, partial, unit);
  }
};
class CPeriodItem : public CItem {
  CdoubleItem *other;
  double _min, _max;
public:
  CPeriodItem(char const* n, char const *u, CdoubleItem *o, double mi, double ma):
  CItem(n, u), other(o), _min(mi), _max(ma) {}
  double getValue() {
    return 1.0/other->value;
  }
  void setValue(double value) {
    other->value = 1.0/value;
  }
  void changeValue(int v){
    double value = getValue() + (v/10.0);
    if (value > _max) {value = _max; displayFlash();}
    if (value < _min) {value = _min; displayFlash();}
    setValue(value);
  }
  void renderValue(char *buf) {
    // Arduino sprintf doesn't handle floats, use casting
    int whole = (int) getValue();
    int partial = (int)((getValue() - whole) * 100);
    sprintf(buf, "%d.%02d%s",whole, partial, unit);
  }
};

CIntItem Low("Low", "%", 0, 0, 100);
CIntItem High("High", "%", 3, 0, 100);
CdoubleItem Pulses("Pulse/sec", "", .5, .1, 10);
CPeriodItem Period("Period", "s", &Pulses, .05, 10.0);
CIntItem Duty("Duty Cycle", "%", 50, 0,100);
CItem *menu[] = {
  &Period, &Pulses, &Duty, &Low, &High,
};

void setOutput() {
  unsigned long time = millis();
  unsigned long period = (1000/Pulses.value);
  time = time % period;
  if (time < (Duty.value * period / 100)){
    analogWrite(pinOut, High.value);
  } else {
    analogWrite(pinOut, Low.value);
  }
}
void displayTextInvert(int condition){
  if (condition){
    display.setTextColor(BLACK,WHITE);
  } else {
    display.setTextColor(WHITE,BLACK);
  }
}
void updateText(void) {
  char buf[32];
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  int16_t first = max(0, cursor - 2);
  int16_t last = min(cursor + 3, (int16_t)(sizeof(menu)/sizeof(CItem*)));
  for(int i = first; i< last; i++)
  {
    display.setCursor(0,8*(1 + i - first));
    displayTextInvert(!itval && cursor == i);
    menu[i]->renderName((char*)&buf);
    display.print(buf);
    display.setTextColor(WHITE,BLACK);
    display.print(" ");
    displayTextInvert(itval && cursor == i);
    menu[i]->renderValue((char*)&buf);
    display.print(buf);
  }
  display.display();
  delay(10);
}

void timerIsr() {
  encoder.service();
  setOutput();
}

void setup()   {
  Serial.begin(9600);
  pinMode(pinOut, OUTPUT);
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // initialize with the I2C addr 0x3D (for the 128x64)
  // init done

  display.clearDisplay();
  updateText();

  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);

  encoder.setAccelerationEnabled(true);

  Serial.print("Acceleration is ");
  Serial.println((encoder.getAccelerationEnabled()) ? "enabled" : "disabled");

  delay(1000);
}

void loop() {
  if (!itval){
    cursor += encoder.getValue();
    if (cursor < 0) cursor = 0;
    if (cursor >= (int16_t)(sizeof(menu)/sizeof(CItem*)))
    cursor = (sizeof(menu)/sizeof(CItem*)) - 1;
  }
  else {
    menu[cursor]->changeValue(encoder.getValue());
  }

  buttonState = encoder.getButton();

  if (buttonState != 0) {
    Serial.print("Button: "); Serial.println(buttonState);
    switch (buttonState) {
      case ClickEncoder::Open:          //0
      break;
      case ClickEncoder::Closed:        //1
      break;
      case ClickEncoder::Pressed:       //2
      break;
      case ClickEncoder::Held:          //3
      break;
      case ClickEncoder::Released:      //4
      break;
      case ClickEncoder::Clicked:       //5
      itval = !itval;
      break;
      case ClickEncoder::DoubleClicked: //6
      break;
    }
  }
  updateText();
}
