#include <avr/io.h>
#include <avr/interrupt.h>

#define F_CPU       16000000UL

#define GREEN_LED   5
#define ORANGE_LED  4
#define RED_LED     3
#define BUTTON      8
#define WORK_LED    LED_BUILTIN
#define PROG_PERIOD 100
#define BUFFER_SIZE 10

typedef enum
{
  stage_0, // горит красный
  stage_1, // 2 секунды горит красный
  stage_2, // 3 секунды желтый
  stage_3, // 5 секунд зеленый
  stage_4  // 1 секунду желтый
}lightStage_t;

typedef enum
{
  WORK_MODE, // режим работы
  COMMAND    // либо команда на срабатывание, либо номер сигнала
}inputDataMap_t;

typedef enum
{
  normalWork,     // нормальная работа светофора
  terminalWork,   // управление из терминала 
}workMode_t;

lightStage_t lightStage = stage_0;
workMode_t workMode = normalWork;
int button = 0;
bool starProgramm = false;
uint8_t* inputData;
uint8_t* outputData;

void initPins()
{
  pinMode(WORK_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(ORANGE_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUTTON, INPUT);
}

void initTimer(uint16_t delay_ms)
{
  cli();

  TCCR1A = 0;   // установить регистры в 0
  TCCR1B = 0;

  OCR1A = (F_CPU / 1024 / (1000 / delay_ms)) - 1; // установка регистра совпадения

  TCCR1B |= (1 << WGM12);  // включить CTC режим 
  TCCR1B |= (1 << CS10); // Установить биты на коэффициент деления 1024
  TCCR1B |= (1 << CS12);

  TIMSK1 |= (1 << OCIE1A);  // включить прерывание по совпадению таймера 

  sei();
}

void blinkWorkLed()
{
  static bool flag = false;
  flag = !flag;
  flag ? digitalWrite(WORK_LED, HIGH) : digitalWrite(WORK_LED, LOW);
}

void lights()
{
  static int8_t ledStage = -1;
  ledStage++;

  switch (ledStage)
  {
  case 0:
    digitalWrite(GREEN_LED, HIGH);
    break;
  case 1:
    digitalWrite(ORANGE_LED, HIGH);
    break;
  case 2:
    digitalWrite(RED_LED, HIGH);
    break;
  case 3:
    digitalWrite(GREEN_LED, LOW);
    break;
  case 4:
    digitalWrite(ORANGE_LED, LOW);
    break;
  case 5:
    digitalWrite(RED_LED, LOW);
    ledStage = -1;
    break;
  }
}

void noSignal()
{
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(ORANGE_LED, LOW);
}

void stopSignal()
{
  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(ORANGE_LED, LOW);
}

void startSignal()
{
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED, LOW);
  digitalWrite(ORANGE_LED, LOW);
}

void prepareSignal()
{
  static bool signalBlink = true;
  static int counter = 800 / PROG_PERIOD;
  if (counter != 0)
  {
    counter--;
  }
  else
  {
    signalBlink = !signalBlink;
    counter = 500 / PROG_PERIOD;
  }

  signalBlink ? digitalWrite(ORANGE_LED, HIGH) : digitalWrite(ORANGE_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
}

void updateButton()
{
  button = digitalRead(BUTTON);
  if (button)
  {
    starProgramm = true;
  }
}

void signalController()
{
  static int counter = -1;
  counter++;
  switch (counter)
  {
  case (0):
    lightStage = stage_1;
    break;
  case (2000 / PROG_PERIOD):
    lightStage = stage_2;
    break;
  case (5000 / PROG_PERIOD):
    lightStage = stage_3;
    break;
  case (10000 / PROG_PERIOD):
    lightStage = stage_4;
    break;
  case (11000 / PROG_PERIOD):
    lightStage = stage_0;
    starProgramm = false;
    counter = -1;
    inputData[COMMAND] = 0;
    break;
  }
}

void normalWorkSignal()
{
  switch (lightStage)
  {
  case stage_0:
  case stage_1:
    stopSignal();
    break;
  case stage_2:
  case stage_4:
    prepareSignal();
    break;
  case stage_3:
    startSignal();
    break;
  }
}

void terminalWorkSignal()
{
  switch (inputData[COMMAND])
  {
  case 1:
    startSignal();
    break;
  case 2:
    prepareSignal();
    break;
  case 3:
    stopSignal();
    break;
  default:
    noSignal();
    break;
  }
}


// the setup function runs once when you press reset or power the board
void setup()
{
  initPins();
  initTimer(PROG_PERIOD);
  Serial.begin(9600);
  inputData = (uint8_t*)calloc(BUFFER_SIZE, sizeof(uint8_t));
  outputData = (uint8_t*)calloc(BUFFER_SIZE, sizeof(uint8_t));

  // Serial.print("hello\n");
}

// the loop function runs over and over again forever
void loop()
{
  if (Serial.available() > 0)
  {
    Serial.readBytes(inputData, BUFFER_SIZE);
    if  (inputData[WORK_MODE] == 1)
    {
      workMode = normalWork;
    }
    else if (inputData[WORK_MODE] == 3)
    {
      workMode = terminalWork;
    }
    if (inputData[COMMAND])
    {
      starProgramm = true;
    }
  }
}

ISR(TIMER1_COMPA_vect)
{
  updateButton();
  blinkWorkLed();
  
  switch (workMode)
  {
  case normalWork:
    normalWorkSignal();
    if (starProgramm)
    {
      signalController();
    }
    break;
  case terminalWork:
    terminalWorkSignal();
    break;
  }

}
