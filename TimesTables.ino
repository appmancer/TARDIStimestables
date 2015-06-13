/**
 * TARDIS Time's Tables
 * by Samuel Pickard
 * most of this code is cut and paste from examples by people more knowledgable than me.
 * ----------------------------------------------------------------------------
 * This implements the  MFRC522 library; see https://github.com/miguelbalboa/rfid
 * for further details and other examples.
 *
 * Typical pin layout used:
 * -----------------------------------------------------------------------------------------
 *             MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
 *             Reader/PCD   Uno           Mega      Nano v3    Leonardo/Micro   Pro Micro
 * Signal      Pin          Pin           Pin       Pin        Pin              Pin
 * -----------------------------------------------------------------------------------------
 * RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
 * SPI SS      SDA(SS)      10            53        D10        10               10
 * SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
 * SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
 * SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15
 * 
 
Many thanks to Fran for this fix
http://forum.arduino.cc/index.php?topic=168131.0

Wiring guide to match the video:

MOSI   ORANGE
MISO   GREEN
CLK    YELLOW

Bitmaps are Copyright (c) Tim Doyle 2014 and are used with permission. 
www.NakatomiInc.com

The Doctor Who series and characters appearing thereon are copyrighted by the BBC. The term "TARDIS" is trademarked by the BBC. The Daleks are trademarked by Terry Nation.
*/

#include <SPI.h>
#include <SD.h>
#include <TFT.h>
#include "MFRC522.h"

//Definitions for the screen pins
#define TFT_CS     10 
#define TFT_RST    8  // you can also connect this to the Arduino reset
                      // in which case, set this #define pin to 0!
#define TFT_DC     9  // Marked as A0 on the board

//Definitions for the card reader pins
#define RFID_SS    6
#define RFID_RESET 7

//Definitions for the SDCard reader
#define SD_CS      5

//The blue light on the roof
#define LED_PIN  3

//Start the tardis sound on this pin
#define TARDIS_PIN 4

//Game state definitions
#define STATE_WAITING_FOR_CARD        0

#define GAME_CALC    0
#define GAME_TEST    1
#define GAME_NONE    -1

#define COMMANDBYTE_CHANGE_GAME 0xA0 
#define COMMANDBYTE_PLAYING_CARD 0xB0

#define COMMANDBYTE_GAME_CALC 0x10
#define COMMANDBYTE_GAME_TEST 0x11

#define MAX_ROUND  10

#define MIN_FACTOR 2
#define MAX_FACTOR 12

int current_state;    
char current_command;
byte commandByte;
byte typeByte;
byte valueByte;

int firstNumber;
int gameRound;
int answer;

String lastMessage;

MFRC522 mfrc522(RFID_SS, RFID_RESET);   // Create MFRC522 instance.

MFRC522::MIFARE_Key key;

TFT lcd(TFT_CS,  TFT_DC, TFT_RST); // Create the lcd instance

PImage logo;

//Create an array of the filenames for the bitmaps.  Load these images onto your 
//SDCard
const char c1[] PROGMEM = "first.bmp";
const char c2[] PROGMEM = "second.bmp";
const char c3[] PROGMEM = "third.bmp";
const char c4[] PROGMEM = "fourth.bmp";
const char c5[] PROGMEM = "fifth.bmp";
const char c6[] PROGMEM = "sixth.bmp";
const char c7[] PROGMEM = "seventh.bmp";
const char c8[] PROGMEM = "eighth.bmp";
const char c9[] PROGMEM = "ninth.bmp";
const char c10[] PROGMEM = "tenth.bmp";
const char c11[] PROGMEM = "eleventh.bmp";
const char c12[] PROGMEM = "twelfth.bmp";

const char* const images[] PROGMEM =
{
      c1,
      c2,
      c3,
      c4,
      c5,
      c6,
      c7,
      c8,
      c9,
      c10,
      c11,
      c12
};
/**
 * Initialize.
 */
void setup() {
    Serial.begin(115200); // Initialize serial communications with the PC
    while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
    SPI.begin();        // Init SPI bus
    mfrc522.PCD_Init(); // Init MFRC522 card

    // Prepare the key (used both as key A and as key B)
    // using FFFFFFFFFFFFh which is the default at chip delivery from the factory
    for (byte i = 0; i < 6; i++) {
        key.keyByte[i] = 0xFF;
    }

    Serial.println(F("Scan a MIFARE Classic PICC to demonstrate read and write."));
    Serial.print(F("Using key (for A and B):"));
    dump_byte_array(key.keyByte, MFRC522::MF_KEY_SIZE);
    Serial.println();
    
    Serial.println(F("Starting screen"));
    lcd.initR(INITR_BLACKTAB);   // initialize a ST7735S chip, black tab 
    lcd.setRotation(2);
    clearScreen();
    
    Serial.print(F("Initializing SD card..."));
    if (!SD.begin(SD_CS)) 
    {
      Serial.println(F("failed!"));
      return;
    }

    resetState();
    
    //Run the intro sequence
    pinMode(LED_PIN, OUTPUT);
    pinMode(TARDIS_PIN, OUTPUT);
    digitalWrite(TARDIS_PIN, HIGH);
    
    intro();
}


/**
 * Main loop.
 */
void loop() 
{
   readBlock();
   
   if(commandByte == COMMANDBYTE_CHANGE_GAME && typeByte == COMMANDBYTE_GAME_CALC)
     startCalc();
   else if(commandByte == COMMANDBYTE_CHANGE_GAME && typeByte == COMMANDBYTE_GAME_TEST)
     startTest();
   else if(commandByte == COMMANDBYTE_PLAYING_CARD && current_command == GAME_CALC)
     playCardForCalc();
   else if(commandByte == COMMANDBYTE_PLAYING_CARD && current_command == GAME_TEST)
     playCardForTest();
}

void startCalc()
{
  Serial.println(F("Starting calc"));
  clearScreen();
  current_command = GAME_CALC;
  firstNumber = 0;
  
  displayMessage(F("Calculator"));
}

void playCardForCalc()
{
  if(firstNumber == 0)
  {
    clearScreen();
    displayCard(valueByte);
    firstNumber = valueByte;
  }
  else
  {
    displayCard(valueByte);
    displaySum(110);
    firstNumber = 0;
  }
}

void startTest()
{
  Serial.println(F("Starting test"));
  gameRound = 1;
  current_command = GAME_TEST; 
  nextQuestion();
}

//Think of an answer, not prime!
void nextQuestion()
{
  firstNumber = 0;
  answer = random(MIN_FACTOR, MAX_FACTOR) * random(MIN_FACTOR, MAX_FACTOR);
  
  Serial.print(F("Target is: "));
  Serial.println(answer);
  
  displayNextAnswer();
}

void playCardForTest()
{
  displayCard(valueByte);
  if(firstNumber == 0)
  {
    firstNumber = valueByte;
  }
  else
  {
    debugSum();
    if((firstNumber * valueByte) == answer)
    {
      //We've got a winner!
      correctAnswer();
      gameRound++;  
      nextQuestion();
    }
    else
    {
      wrongAnswer();
    }   

    if(gameRound > MAX_ROUND)
    {
      endTest();
    } 
  }
}

void correctAnswer()
{
  displayMessage("", F("Correct!"));
  displaySum(110);
  delay(2000);
}

void wrongAnswer()
{
  displayMessage("", F("Try again!"));
  displaySum(110);
  firstNumber = 0;
  delay(2000);
  displayNextAnswer();
}

void endTest()
{
  clearScreen();
  displayMessage(F("You have  beaten theDalek"));
  delay(2000);
  resetState();
}

void resetState()
{
  current_state = STATE_WAITING_FOR_CARD;
  current_command = GAME_NONE;
  
  randomSeed(analogRead(0));
  
  //print instructions
  printInstructions();  
}

/**
 * Helper routine to dump a byte array as hex values to Serial.
 */
void dump_byte_array(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
}


void readBlock()
{
    Serial.println(F("Present card"));
 // Look for new cards
    while(! mfrc522.PICC_IsNewCardPresent());
        

    // Select one of the cards
    while( ! mfrc522.PICC_ReadCardSerial());

    // In this sample we use the second sector,
    // that is: sector #1, covering block #4 up to and including block #7
    byte sector         = 1;
    byte blockAddr      = 4;                        
    byte trailerBlock   = 7;
    byte status;
    byte buffer[18];
    byte size = sizeof(buffer);

    // Authenticate using key A
    Serial.println(F("Authenticating using key A..."));
    status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("PCD_Authenticate() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
    }

    // Read data from the block
    Serial.print(F("Reading data from block ")); Serial.print(blockAddr);
    Serial.println(F(" ..."));
    status = mfrc522.MIFARE_Read(blockAddr, buffer, &size);
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("MIFARE_Read() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
    }

    commandByte = buffer[3];
    typeByte = buffer[4];
    valueByte = buffer[7];    
    
    // Halt PICC
    mfrc522.PICC_HaltA();
    // Stop encryption on PCD
    mfrc522.PCD_StopCrypto1();
}

void displayNextAnswer()
{
  clearScreen();
  lcd.print(F("Make: "));
  lcd.print(answer);
}

void printInstructions()
{
  Serial.println(F("Start by presenting a game card"));
  logo = lcd.loadImage("PHONE.BMP");
  if (!logo.isValid()) 
  {
    Serial.println(F("error while loading bitmap"));
  }
  lcd.image(logo, 0, 0);
  
  logo.close();
}

void displayCard(int number)
{
  //It is important that this function does not clear the screen
  char imageName[12];
  strcpy_P(imageName, (char*)pgm_read_word(&(images[number - 1])));  
  Serial.print(F("loading image "));
  Serial.println(imageName);
  logo = lcd.loadImage(imageName);
  if (!logo.isValid()) 
  {
    Serial.println(F("error while loading bitmap"));
  }
  if(firstNumber == 0)
    lcd.image(logo, 0, 20);
  else
    lcd.image(logo, 50, 30);
 
  logo.close();
}

void displayMessage(String message)
{
  displayMessage(message, "");
}

//THis shows a messages on the LCD screen
void displayMessage(String message1, String message2)
{
  Serial.println("Updating LCD screen");
  Serial.println(message1);
  Serial.println(message2);
  
  lcd.setCursor(0, 0);
  lcd.print(message1);
  lcd.setCursor(0, 20);
  lcd.print(message2);
}

void displaySum()
{
  displaySum(0);
}

void displaySum(int line)
{
  lcd.setCursor(0, line);
  lcd.print(firstNumber);
  lcd.print(" * ");
  lcd.print(valueByte);
  lcd.print(" = ");
  lcd.print(firstNumber * valueByte);
 }

void clearScreen()
{  
  lcd.fillScreen(ST7735_BLACK);
  lcd.setTextColor(ST7735_WHITE);
  lcd.setTextSize(2);
  lcd.setCursor(0, 0);
}

//Its time to start the music, its time to light the lights
void intro()
{
  //Start the tardis
  digitalWrite(TARDIS_PIN, LOW);
  delay(100);
  digitalWrite(TARDIS_PIN, HIGH);
  
  //Fade the light, just like the example ;-)
  int brightness = 0;    // how bright the LED is
  int fadeAmount = 10;    // how many points to fade the LED by
  
  int endTime = millis() + 15000;
  
  while(millis() < endTime)
  {
    // set the brightness of pin 9:
    analogWrite(LED_PIN, brightness);    
    
    // change the brightness for next time through the loop:
    brightness = brightness + fadeAmount;
    
    // reverse the direction of the fading at the ends of the fade: 
    if (brightness == 0 || brightness == 250) 
    {
      fadeAmount = -fadeAmount ; 
    }     
    // wait for 30 milliseconds to see the dimming effect    
    delay(30);                   
  }
}

void debugSum()
{
    Serial.print(firstNumber);
    Serial.print(" * ");
    Serial.print(valueByte);
    Serial.print(" = ");
    Serial.println(firstNumber * valueByte);
 }

