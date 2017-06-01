#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9340.h"

// _sclk = 13, _miso = 12, _mosi = 11
/* ========== Constants ========== */
const unsigned char CS_PIN = 10, DC_PIN = 9, RST_PIN = 8;
const unsigned char LEFT_SPEAKER_PIN = 3, RIGHT_SPEAKER_PIN = 2;
const unsigned char NUM_SENSORS = 2;
const unsigned char NUM_TURNS = 2;
const unsigned char THRESHHOLD = 12;
const unsigned char turnPins[NUM_TURNS] = {1,0};
const unsigned char echoPins[NUM_SENSORS] = {7,5};
const unsigned char trigPins[NUM_SENSORS] = {6,4};
// Each index corresponds to the X, Y, Width, Height of each region
const unsigned char carCoord[4] = {101, 121,38,78};
const unsigned char DISPLAY_REGIONS[8][4] =
                    {{0,120,100,80},
                    {140,120,100,80},
                    {0,0,100,120},
                    {100,0,40,120},
                    {140,0,100,120},
                    {0,200,100,120},
                    {100,200,40,120},
                    {140,200,100,120}};
                     

/* ========== Globals ========== */
unsigned long distance[NUM_SENSORS][3] = { 0 };
unsigned long turn[2][3] = { 0 };
bool leftObstacle = false, rightObstacle = false;
bool leftTurn = false, rightTurn = false;

Adafruit_ILI9340 tft = Adafruit_ILI9340(CS_PIN, DC_PIN, RST_PIN);

/* ========== State Machine Setup ========== */
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
/* ========================================= */

void fillDetectedRegions(uint16_t color) {
  unsigned long avg_distance;
  for(unsigned char i = 0; i < NUM_SENSORS; i++) {
    avg_distance = (distance[i][0] + distance[i][1] + distance[i][2]) / 3;
    if(avg_distance <= THRESHHOLD && avg_distance != 0) {
      tft.fillRect(DISPLAY_REGIONS[i][0], DISPLAY_REGIONS[i][1],
                    DISPLAY_REGIONS[i][2], DISPLAY_REGIONS[i][3],
                    color);
    }
  }
  if(leftTurn) {
    tft.fillRect(DISPLAY_REGIONS[2][0], DISPLAY_REGIONS[2][1],
                    DISPLAY_REGIONS[2][2], DISPLAY_REGIONS[2][3],
                    color);
  }
  if(rightTurn) {
    tft.fillRect(DISPLAY_REGIONS[3][0], DISPLAY_REGIONS[3][1],
                    DISPLAY_REGIONS[3][2], DISPLAY_REGIONS[3][3],
                    color);
  }
}

void clearDetectedRegions(uint16_t color) {
  for(unsigned char i = 0; i < NUM_SENSORS; i++) {
    tft.fillRect(DISPLAY_REGIONS[i][0], DISPLAY_REGIONS[i][1],
                  DISPLAY_REGIONS[i][2], DISPLAY_REGIONS[i][3],
                  color);
  }
}


void displayCar() {
  tft.drawRect(carCoord[0],carCoord[1], carCoord[2], carCoord[3], ILI9340_WHITE);
}

bool isObstacleDetected() {
  unsigned long avg_distance;
  for(unsigned char i = 0; i < NUM_SENSORS; i++) {
    avg_distance = (distance[i][0] + distance[i][1] + distance[i][2]) / 3;
    if(avg_distance <= THRESHHOLD && avg_distance != 0) {
      return true;
    }
  }
  
  return false;
}

void testDisplay() {
  tft.fillScreen(ILI9340_BLACK);
  displayCar();
  for(unsigned char i = 0; i < NUM_SENSORS; i++) {
    for(unsigned char j = i + 1; j < NUM_SENSORS; j++) {
      distance[i][0] = distance[i][1] = distance[i][2] = THRESHHOLD;
      distance[j][0] = distance[j][1] = distance[j][2] = distance[i][0];
      fillDetectedRegions(ILI9340_RED);
      delay(500);
      clearDetectedRegions(ILI9340_BLACK);
      delay(500);
      distance[i][0] = distance[i][1] = distance[i][2] = 0;
      distance[j][0] = distance[j][1] = distance[j][2] = distance[i][0];
    }
  }
}

enum USensor_States{Sample};
int ReadDistance(int state) {
  switch(state) {
    case Sample:
    unsigned long avg_distance;
    avg_distance = (distance[0][0]+distance[0][1]+distance[0][2]) / 3;
    if(avg_distance <= THRESHHOLD && avg_distance != 0) {
      leftObstacle = true;
    } else {
      leftObstacle = false;
    }
    avg_distance = (distance[1][0]+distance[1][1]+distance[1][2]) / 3;
    if(avg_distance <= THRESHHOLD && avg_distance != 0) {
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
       distance[i][2] = distance[i][1];
       distance[i][1] = distance[i][0];
       distance[i][0] = duration / 148;
    }
    for(unsigned char j = 0; j < NUM_TURNS; j++ ) {
      turn[j][2] = turn[j][1];
      turn[j][1] = turn[j][0];
      turn[j][0] = digitalRead(turnPins[j]);
      if(!turn[j][0] && !turn[j][1] && !turn[j][2]) {
        if(j == 0) {
          leftTurn = true;
        } else {
          rightTurn = true;
        }
      } else {
        if(j == 0) {
          leftTurn = false;
        } else { 
          rightTurn = false;
        }
      }
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
        digitalWrite(LEFT_SPEAKER_PIN, HIGH);
        delayMicroseconds(1000);
        digitalWrite(LEFT_SPEAKER_PIN, LOW);
        delayMicroseconds(1300);
      }
    delayMicroseconds(1000);
    }
    count = 0; 
    break;

    case A_Right:
    for(unsigned j = 0; j < 5; j++) {
      for(unsigned char i = 0; i < 5; i++) {
        digitalWrite(RIGHT_SPEAKER_PIN, HIGH);
        delayMicroseconds(900);
        digitalWrite(RIGHT_SPEAKER_PIN, LOW);
        delayMicroseconds(1200);
      }
    delayMicroseconds(1000);
    }
    count = 0;
    break;

    case A_Both:
    for(unsigned j = 0; j < 5; j++) {
      for(unsigned char i = 0; i < 5; i++) {
        digitalWrite(LEFT_SPEAKER_PIN, HIGH);
        digitalWrite(RIGHT_SPEAKER_PIN, HIGH);
        delayMicroseconds(500);
        digitalWrite(LEFT_SPEAKER_PIN, LOW);
        digitalWrite(RIGHT_SPEAKER_PIN, LOW);
        delayMicroseconds(900);
      }
    delayMicroseconds(1000);
    }
    break;
  }
  return state;
}

enum lcdStates{Safe, Display_On, Display_Off};
int DisplayLcd(int state) {
  static bool clear_screen = true;
  switch(state) {
    case Safe:
    if(clear_screen) {
      tft.fillScreen(ILI9340_BLACK);
      clear_screen = false;
      displayCar();
    }
    
    if(isObstacleDetected()) {
      state = Display_On;
    } else {
      state = Safe;
    }
    break;
    
    case Display_On:
    if(isObstacleDetected()) {
      state = Display_Off;
    } else {
      state = Safe;
    }
    break;
    
    case Display_Off:
    if(isObstacleDetected()) {
      state = Display_On;
    } else {
      state = Safe;
    } 
    break;
  }
  switch(state) {
    case Safe:
    break;

    case Display_On:
    fillDetectedRegions(ILI9340_RED);
    break;
    
    case Display_Off:
    clearDetectedRegions(ILI9340_BLACK);
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
  for(unsigned char j = 0; j < NUM_TURNS; j++) {
    pinMode(turnPins[j], INPUT);
  }
  pinMode(LEFT_SPEAKER_PIN, OUTPUT);
  pinMode(RIGHT_SPEAKER_PIN, OUTPUT);
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
  tft.begin();
}

void loop() {
  
  for (unsigned char z = 0; z < tasksNum; ++z) { // Heart of the scheduler code
    if ( tasks[z].elapsedTime >= tasks[z].period ) { // Ready
      tasks[z].state = tasks[z].TickFct(tasks[z].state);
      tasks[z].elapsedTime = 0;
    }
    tasks[z].elapsedTime += tasksPeriodGCD;
  }

  //testDisplay();
}
