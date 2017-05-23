#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9340.h"

// _sclk = 13, _miso = 12, _mosi = 11
/* ========== Constants ========== */
const unsigned char CS_PIN = 10, DC_PIN = 9, RST_PIN = 8;
const unsigned char LEFT_SPEAKER = 3, RIGHT_SPEAKER = 2;
const unsigned char NUM_SENSORS = 2;
const unsigned char echoPins[NUM_SENSORS] = {7,5};
const unsigned char trigPins[NUM_SENSORS] = {6,4};
const unsigned short LCD_WIDTH = 240, LCD_HEIGHT = 320;
const unsigned short DISPLAY_REGIONS[8][4] =
                    {0,0,100,120},
                    {100,0,40,120},
                     

/* ========== Globals ========== */
unsigned long distance[NUM_SENSORS][3] = {{0,0,0},{0,0,0}};
unsigned char lcd_grid[LCD_HEIGHT][LCD_WIDTH];
bool leftObstacle = false, rightObstacle = false;

Adafruit_ILI9340 tft = Adafruit_ILI9340(CS_PIN, DC_PIN, RST_PIN);

typedef struct task {
  int state;                  // Task's current state
  unsigned long period;       // Task period
  unsigned long elapsedTime;  // Time elapsed since last task tick
  int (*TickFct)(int);        // Task tick function
} task;
task tasks[3];
const unsigned char tasksNum = 3;
const unsigned long tasksPeriodGCD = 10;
const unsigned long periodReadDistance = 40;
const unsigned long periodRelayAudio = 10;
const unsigned long periodDisplayLcd = 100;

enum USensor_States{Sample};
int ReadDistance(int state) {
  switch(state) {
    case Sample:
    unsigned long average;
    average = (distance[0][0]+distance[0][1]+distance[0][2]) / 3;
    if(average <= 12 && average != 0) {
      leftObstacle = true;
    } else {
      leftObstacle = false;
    }
    average = (distance[1][0]+distance[1][1]+distance[1][2]) / 3;
    if(average <= 12 && average != 0) {
      rightObstacle = true;
    } else {
      rightObstacle = false;
    }
    state = Sample;
    break;
  }
  switch(state) {
    case Sample:
    for(unsigned char i = 0; i < NUM_SENSORS; i++) {
       digitalWrite(trigPins[i], LOW);
       delayMicroseconds(2);
    
       digitalWrite(trigPins[i], HIGH);
       delayMicroseconds(10);
       digitalWrite(trigPins[i], LOW);

       unsigned long duration;
       duration = pulseIn(echoPins[i], HIGH);
       distance[i][1] = distance[i][2];
       distance[i][0] = distance[i][1];
       distance[i][0] = duration / 148;
       
       Serial.print("Distance_");
       Serial.print(i);
       Serial.print(": ");
       Serial.print((distance[i][0]+distance[i][1]+distance[i][2]) / 3);
       Serial.println(" in");
    }
    break;
  }
  return state;
}

unsigned char count = 0;
enum Audio_States{Idle, A_Left, A_Right, A_Both};
int RelayAudio(int state) {
  switch(state) {
    case Idle:
    if(leftObstacle && rightObstacle) {
      state = A_Both;
    } else if(leftObstacle) {
      state = A_Left;
    } else if(rightObstacle) {
      state = A_Right;
    } else {
      state = Idle;
    }
    count++;
    break;

    case A_Left:
    if(leftObstacle && rightObstacle) {
      state = A_Both;
    } else if(rightObstacle && !leftObstacle) {
      state = A_Right;
    } else if(!rightObstacle && !leftObstacle) {
      state = Idle;
    } else {
      state = A_Left;
    }
    count++;
    break;

    case A_Right:
    if(leftObstacle && rightObstacle) {
      state = A_Both;
    } else if(leftObstacle && !rightObstacle) {
      state = A_Left;
    } else if(!rightObstacle && !leftObstacle) {
      state = Idle;
    } else {
      state = A_Right;
    }
    ++count;
    break;

    case A_Both:
    if(leftObstacle && !rightObstacle) {
      state = A_Left;
    } else if(rightObstacle && !leftObstacle) {
      state = A_Right;
    } else if(!rightObstacle && !leftObstacle) {
      state = Idle;
    } else {
      state = A_Both;
    }
    ++count;
    break;
  }
  switch(state) {
    case Idle:
    break;

    case A_Left:
    for(unsigned j = 0; j < 5; j++) {
      for(unsigned char i = 0; i < 5; i++) {
        digitalWrite(LEFT_SPEAKER, HIGH);
        delayMicroseconds(1000);
        digitalWrite(LEFT_SPEAKER, LOW);
        delayMicroseconds(1300);
      }
    delayMicroseconds(1000);
    }
    count = 0; 
    break;

    case A_Right:
    for(unsigned j = 0; j < 5; j++) {
      for(unsigned char i = 0; i < 5; i++) {
        digitalWrite(RIGHT_SPEAKER, HIGH);
        delayMicroseconds(900);
        digitalWrite(RIGHT_SPEAKER, LOW);
        delayMicroseconds(1200);
      }
    delayMicroseconds(1000);
    }
    count = 0;
    break;

    case A_Both:
    for(unsigned j = 0; j < 5; j++) {
      for(unsigned char i = 0; i < 5; i++) {
        digitalWrite(LEFT_SPEAKER, HIGH);
        digitalWrite(RIGHT_SPEAKER, HIGH);
        delayMicroseconds(500);
        digitalWrite(LEFT_SPEAKER, LOW);
        digitalWrite(RIGHT_SPEAKER, LOW);
        delayMicroseconds(900);
      }
    delayMicroseconds(1000);
    }
    break;
  }
  return state;
}

enum lcdStates{Safe, D_Left, D_Right, D_Both};
int DisplayLcd(int state) {
  switch(state) {
    case Safe:
    if(leftObstacle && rightObstacle) {
      state = D_Both;
    } else if(leftObstacle) {
      state = D_Left;
    } else if(rightObstacle) {
      state = D_Right;
    } else {
      state = Safe;
    }
    break;

    case D_Left:
    if(leftObstacle && rightObstacle) {
      state = D_Both;
    } else if(rightObstacle && !leftObstacle) {
      state = D_Right;
    } else if(!rightObstacle && !leftObstacle) {
      state = Safe;
    } else {
      state = D_Left;
    }
    break;

    case D_Right:
    if(leftObstacle && rightObstacle) {
      state = D_Both;
    } else if(leftObstacle && !rightObstacle) {
      state = D_Left;
    } else if(!rightObstacle && !leftObstacle) {
      state = Safe;
    } else {
      state = D_Right;
    }
    break;

    case D_Both:
    if(leftObstacle && !rightObstacle) {
      state = D_Left;
    } else if(rightObstacle && !leftObstacle) {
      state = D_Right;
    } else if(!rightObstacle && !leftObstacle) {
      state = Safe;
    } else {
      state = D_Both;
    }
    break;
  }
  switch(state) {
    case Safe:
    tft.fillScreen(ILI9340_BLACK);
    tft.setCursor(70, 100);
    tft.setTextColor(ILI9340_WHITE);  tft.setTextSize(5);
    tft.println("ALL");
    tft.setCursor(40, 145);
    tft.println("CLEAR");
    break;

    case D_Left:
    tft.fillScreen(ILI9340_BLACK);
    tft.setCursor(40, 100);
    tft.setTextColor(ILI9340_RED);  tft.setTextSize(3);
    tft.println("!CAUTION!");
    tft.setTextSize(5);
    tft.setCursor(62, 130);
    tft.println("LEFT");
    tft.setCursor(82, 175);
    tft.setTextSize(3);
    tft.println("SIDE");
    break;

    case D_Right:
    tft.fillScreen(ILI9340_BLACK);
    tft.setCursor(40, 100);
    tft.setTextColor(ILI9340_RED);  tft.setTextSize(3);
    tft.println("!CAUTION!");
    tft.setTextSize(5);
    tft.setCursor(47, 130);
    tft.println("RIGHT");
    tft.setCursor(82, 175);
    tft.setTextSize(3);
    tft.println("SIDE");
    break;

    case D_Both:
    tft.fillScreen(ILI9340_BLACK);
    tft.setCursor(40, 80);
    tft.setTextColor(ILI9340_RED);  tft.setTextSize(3);
    tft.println("!CAUTION!");
    tft.setTextSize(5);
    tft.setCursor(62, 110);
    tft.println("LEFT");
    tft.setCursor(105, 150);
    tft.setTextSize(3);
    tft.println("&");
    tft.setCursor(47, 175);
    tft.setTextSize(5);
    tft.println("RIGHT");
    tft.setCursor(82, 215);
    tft.setTextSize(3);
    tft.println("SIDE");
    break;
  }
  return state;
}

void setup() {
  // put your setup code here, to run once:
  for(unsigned char i = 0; i < NUM_SENSORS; i++) {
    pinMode(trigPins[i], OUTPUT);
    pinMode(echoPins[i], INPUT);
  }
  pinMode(LEFT_SPEAKER, OUTPUT);
  pinMode(RIGHT_SPEAKER, OUTPUT);
  unsigned char i=0;
  tasks[i].state = Sample;
  tasks[i].period = periodReadDistance;
  tasks[i].elapsedTime = tasks[i].period;
  tasks[i].TickFct = &ReadDistance;
  i++;
  tasks[i].state = Idle;
  tasks[i].period = periodRelayAudio;
  tasks[i].elapsedTime = tasks[i].period;
  tasks[i].TickFct = &RelayAudio;
  i++;
  tasks[i].state = Safe;
  tasks[i].period = periodDisplayLcd;
  tasks[i].elapsedTime = tasks[i].period;
  tasks[i].TickFct = &DisplayLcd;
  Serial.begin(9600);
  tft.begin();
}

void loop() {
  // put your main code here, to run repeatedly:

  for (unsigned char z = 0; z < tasksNum; ++z) { // Heart of the scheduler code
    if ( tasks[z].elapsedTime >= tasks[z].period ) { // Ready
      tasks[z].state = tasks[z].TickFct(tasks[z].state);
      tasks[z].elapsedTime = 0;
    }
    tasks[z].elapsedTime += tasksPeriodGCD;
  }
}
