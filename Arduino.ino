#include <SPI.h>
#include <MFRC522.h>
#include <SD.h>
#include "RTClib.h"

#define RFID_SS_PIN 10
#define RST_PIN 9
#define SD_SS_PIN 5
#define POWER_PIN 8
#define SD_POWER_PIN 7

#define OUTPUT_TIME 44000

#define TAG_ARRAY_SIZE 75
#define STRING_MAX 15

#define TIMEOUT_DURATION 60000

MFRC522 mfrc522(RFID_SS_PIN, RST_PIN);
RTC_DS3231 rtc;

byte allowedTags[TAG_ARRAY_SIZE][4];
const String userFileName = "users.csv";
const String eventFile = "events.log";
char inputBuffer[STRING_MAX];
int inputIndex = 0;
bool waiting = false;
bool waitingForAdd = false;
bool waitingForRemove = false;
unsigned long startTime = 0;

byte allowedTagsCount = 0;

void initSD() {
  digitalWrite(SD_POWER_PIN, HIGH);
  digitalWrite(SD_SS_PIN, LOW);
  digitalWrite(RFID_SS_PIN, HIGH);
  delay(50);
  if (!SD.begin(SD_SS_PIN))
  {
    Serial.println(F("SD initialization failed!"));
  } else {
    Serial.println(F("SD initialized successfully."));
  }
}

void checkRFIDReader() {
  bool status = mfrc522.PCD_PerformSelfTest();
  if (status) {
    Serial.println(F("RFID reader self-test passed."));
  } else {
    Serial.println(F("RFID reader self-test failed. Resetting..."));
    resetRFIDReader();
  }
}

void resetRFIDReader() {
  // Hardware reset
  digitalWrite(RST_PIN, LOW);
  delay(100);
  digitalWrite(RST_PIN, HIGH);
  delay(100);
  
  // Reinitialize and check the RFID reader
  mfrc522.PCD_Init();
  
  if (mfrc522.PCD_PerformSelfTest()) {
    Serial.println(F("RFID reader reset and self-test passed."));
  } else {
    Serial.println(F("RFID reader reset failed. Please check the hardware connections."));
  }
}

void endSD() {
  SD.end();
  digitalWrite(SD_SS_PIN, HIGH);
  digitalWrite(SD_POWER_PIN, LOW);
  digitalWrite(RFID_SS_PIN, LOW);
  delay(50);
}

void readTagsFromFile() {
  initSD();
  File file = SD.open(userFileName);
  if (!file) {
    Serial.println(F("Failed to open file for reading"));
    return;
  }

  while (file.available() && allowedTagsCount < TAG_ARRAY_SIZE) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) continue;

    // Check if the line starts with 'I' (indicating invalid)
    if (line.charAt(0) != 'I') {
      int startIdx = line.indexOf(';') + 1;
      int endIdx = line.indexOf(';', startIdx);
      String uidStr = line.substring(startIdx, endIdx);

      parseUID(uidStr, allowedTags[allowedTagsCount]);

      allowedTagsCount++;
    }
  }
  file.close();
  endSD();
}

void parseUID(String uidStr, byte* uidArray) {
  int byteIndex = 0;
  int strIndex = 0;

  while (strIndex < uidStr.length() && byteIndex < 4) {
    String byteStr = uidStr.substring(strIndex, strIndex + 2);
    uidArray[byteIndex] = (byte) strtol(byteStr.c_str(), NULL, 16);
    strIndex += 3;
    byteIndex++;
  }
}

byte getAllowedTagIndex(byte *uid, byte uidSize)
{

  for (byte i = 0; i < allowedTagsCount; i++)
  {
    bool match = true;

    for (byte j = 0; j < uidSize; j++)
    {
      if (allowedTags[i][j] != uid[j])
      {
        match = false;
        break;
      }
    }

    if (match)
    {
      return i;
    }
  }

  return byte(255);
}

void addTag(byte newTag[], char *owner)
{
  Serial.println(F("add tag"));
  if (allowedTagsCount == TAG_ARRAY_SIZE)
  {
    Serial.println(F("Can't add tag: array size reached"));
    return;
  }

  memcpy(allowedTags[allowedTagsCount], newTag, 4);

  initSD();
  File userFile = SD.open(userFileName, FILE_WRITE);
  if (!userFile)
  {
    Serial.println(F("Error opening user file!"));
    return;
  }

  userFile.println();
  userFile.print("VALID;");

  for (int i = 0; i < 4; i++)
  {
    userFile.print(allowedTags[allowedTagsCount][i], HEX);
    if (i < 3)
      userFile.print(":");
  }
  userFile.print(";");
  userFile.print(owner);
  userFile.flush();
  delay(100);
  userFile.close();
  endSD();

  allowedTagsCount++;
  Serial.print(F("Added Tag: "));
  for (int i = 0; i < 4; i++)
  {
    Serial.print(allowedTags[allowedTagsCount-1][i], HEX);
  }
  Serial.print(F(" for user: "));
  Serial.println(owner);
}

void removeTag(int index)
{
  if (index < 0 || index >= allowedTagsCount)
  {
    Serial.println(F("Invalid index"));
    return;
  }
  byte tagToRemove[4];
  memcpy(tagToRemove, allowedTags[index], 4);
  for (int i = index; i < allowedTagsCount; i++)
  {
    memcpy(allowedTags[i], allowedTags[i + 1], 4);
  }
  
  initSD();
  File userFile = SD.open(userFileName, FILE_READ);
  if (!userFile)
  {
    Serial.println(F("Error opening user file!"));
    return;
  }

  String userToDelete;
  uint32_t position = 0;
  for (int i = 0; i < allowedTagsCount; i++)
  {
    if (i == index)
    {
      position = userFile.position();
      String line = userFile.readStringUntil('\n');
      int commaIndex = line.indexOf(';');

      if (commaIndex != -1)
      {
        userToDelete = line.substring(commaIndex + 1);
      }
    } else {
      String line = userFile.readStringUntil('\n');
      while (line[0] == 'I') {
        line = userFile.readStringUntil('\n');
      }
    }
  }
  userFile.close();

  delay(100);

  userFile = SD.open(userFileName, O_RDWR);
  if (!userFile)
  {
    Serial.println(F("Error opening user file!"));
    return;
  }
  if (userFile.seek(position)) {
    userFile.print("INVAL");
    Serial.println(F("User marked as invalid."));
  } else {
    Serial.println(F("Failed to seek to position"));
  }
  
  userFile.flush();
  delay(100);
  userFile.close();
  endSD();

  allowedTagsCount--;

  Serial.print(F("Removed Tag for user: "));
  Serial.println(userToDelete);
}

void startWaiting()
{
  startTime = millis();
  waiting = true;
}

int strcmp_custom(const char* a, const char* b) {
    while (*b == *a) {
        if (*a == '\0') return 0;
        a++;
        b++;
    }
    return *b - *a;
}

void handleCommand(char* command)
{

  if (strcmp_custom(command, "Tags") == 0)
  {
    getTags();
  }
  else if (strcmp_custom(command, "Add") == 0)
  {
    Serial.print(F("Enter user name (max 15 characters): "));
    startWaiting();
    waitingForAdd = true;
  }
  else if (strcmp_custom(command, "Remove") == 0)
  {
    Serial.print(F("Enter user name or index:"));
    startWaiting();
    waitingForRemove = true;
  }
  else if (strcmp_custom(command, "Events") == 0)
  {
    getEvents();
  }
  else
  {
    Serial.println(F("Command not found. Possible Commands: Tags, Add, Remove, Events"));
  }
}

void getTags()
{
  initSD();
  File userFile = SD.open(userFileName, FILE_READ);
  if (!userFile)
  {
    Serial.println(F("Error opening user file!"));
    return;
  }

  while(userFile.available())
  {
    String line = userFile.readStringUntil('\n');
    Serial.println(line);
  }
  userFile.close();
  endSD();
  Serial.println(F("Enter your command: "));
}

void handleAdd(char* name)
{
  waitingForAdd = false;
  Serial.print(F("Scan new tag for user: "));
  Serial.println(name);
  startTime = millis();

  while ((millis() - startTime) < TIMEOUT_DURATION)
  {
    if (!mfrc522.PICC_IsNewCardPresent()) {
      continue;
    }

    if (!mfrc522.PICC_ReadCardSerial()) {
      continue;
    }

    Serial.print(F("Found card UID: "));
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? "0" : " ");
      Serial.print(mfrc522.uid.uidByte[i], HEX);
    }
    Serial.println();

    for (int i = 0; i < allowedTagsCount; i++)
    {
      if (getAllowedTagIndex(mfrc522.uid.uidByte, mfrc522.uid.size) != 255)
      {
        Serial.println(F("Can't add tag: already in list"));
        return;
      }
    }
    addTag(mfrc522.uid.uidByte, name);
    return;
  }
}

void handleRemove(char* user)
{
  
  int index = atoi(user);

  if (index > 0 || user[0] == '0')
  {
    if (index >= 0 && index < TAG_ARRAY_SIZE)
    {
      removeTag(index);
      waitingForRemove = false;
    }
  }
  else
  {
    initSD();
    File userFile = SD.open(userFileName, FILE_READ);
    if (!userFile)
    {
      Serial.println(F("Error opening user file!"));
      return;
    }

    char* userFromFile;

    for (int i = 0; i < allowedTagsCount; i++)
    {
      String line = userFile.readStringUntil('\n');
      int commaIndex = line.indexOf(';');

      if (commaIndex != -1 && line[0] != 'I')
      {
        String extractedString = line.substring(commaIndex + 1);
        commaIndex = extractedString.indexOf(';');
        extractedString = extractedString.substring(commaIndex + 1);
        int len = extractedString.length();

        userFromFile = new char[len + 1];

        extractedString.toCharArray(userFromFile, len + 1);
        userFromFile[len] = '\0';
      }

      int compareResult = strcmp_custom(user, userFromFile);

      if (compareResult == 0 || compareResult == 13)
      {
        removeTag(i);
        waitingForRemove = false;
        userFile.close();
        return;
      }
    }
    Serial.println(F("Invalid Input: Couldn't find matching user"));
    waitingForRemove = false;
    userFile.close();
    endSD();
  }
}

void getEvents()
{
  initSD();
  File logFile = SD.open(eventFile, FILE_READ);
  if (!logFile)
  {
    Serial.println(F("No events found. Enter your command:"));
    return;
  }

  while (logFile.available())
  {
    String line = logFile.readStringUntil('\n');
    Serial.println(line);
  }
  logFile.close();

  Serial.println(F("Do you want to delete the saved events? (y/n)"));
  startTime = millis();
  while ((millis() - startTime) <= TIMEOUT_DURATION)
  {
    if (Serial.available() > 0)
    {
      String input = Serial.readStringUntil('\n');
      if (input[0] == 'y')
      {
        SD.remove(eventFile);
        Serial.println(F("Events deleted."));
        break;
      }
      else if (input[0] == 'n')
      {
        Serial.println(F("Events retained."));
        break;
      }
      else
      {
        Serial.println(F("Invalid input. Please enter 'y' or 'n'."));
      }
    }
  }
  endSD();
  Serial.println(F("Enter your command: "));
}

void logEvent(byte tagIndex)
{
  initSD();

  byte tag[4];
  memcpy(tag, allowedTags[tagIndex], 4);
  File userFile = SD.open(userFileName, FILE_READ);
  if (!userFile)
  {
    Serial.println(F("Error opening user file!"));
    return;
  }
  String line;
  String owner = "";
  for (int i = 0; i <= tagIndex; i++)
  {
    line = userFile.readStringUntil('\n');
    while (line[0] == 'I') {
      line = userFile.readStringUntil('\n');
    }
  }

  userFile.close();

  int commaIndex = line.indexOf(';');

  if (commaIndex != -1)
  {
    line = line.substring(commaIndex + 1);
    commaIndex = line.indexOf(';');
    owner = line.substring(commaIndex + 1);
  }

  File logFile = SD.open(eventFile, FILE_WRITE);
  if (!logFile)
  {
    Serial.println(F("Error opening log file!"));
    return;
  }
  DateTime now = rtc.now();
  logFile.print(now.timestamp());
  logFile.print(";");
  for (int i = 0; i < 4; i++)
  {
    logFile.print(tag[i], HEX);
    if (i < 3)
      logFile.print(":");
  }
  logFile.print(";");
  logFile.println(owner);
  logFile.flush();
  delay(100);
  logFile.close();
  endSD();
  Serial.println(F("Event logged successfully."));
}

void setup()
{
  Serial.begin(9600);

  pinMode(SD_POWER_PIN, OUTPUT);
  pinMode(POWER_PIN, OUTPUT);
  pinMode(SD_SS_PIN, OUTPUT);
  pinMode(RFID_SS_PIN, OUTPUT);
  digitalWrite(POWER_PIN, LOW);
  digitalWrite(SD_SS_PIN, HIGH);
  digitalWrite(RFID_SS_PIN, HIGH);
  SPI.begin();

  initSD();
  /* only for testing
  File test = SD.open(userFileName, FILE_READ);
    if (test) {
      Serial.println(F("user file opened successfully."));
    } else {
      Serial.println(F("failed to open userfile."));
    }
    test.close();
    //SD.remove(userFileName);
    */
  endSD();

  mfrc522.PCD_Init();
  Serial.println(F("RFID reader initialized."));

  readTagsFromFile();

  if(rtc.begin()) {
    Serial.println(F("RTC initialized."));
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  } else {
    Serial.println(F("failed to init RTC"));
  }

  //byte newTag[4] = {0xE9, 0xB9, 0xA6, 0xE2};
  //char newTagOwner[15] = "Test";
  //addTag(newTag, newTagOwner); //Test user

  Serial.println(F("Enter your command: "));
}

void loop() {

  checkRFIDReader();

  if (waiting && (millis() - startTime) >= TIMEOUT_DURATION) {
    Serial.println(F("Waited too long: reset. Enter new command:"));
    inputIndex = 0;
    memset(inputBuffer, 0, STRING_MAX); 
    waiting = false;
    waitingForAdd = false;
    waitingForRemove = false;
  }

 while (Serial.available() > 0) {
    char incomingByte = Serial.read();
    
    if (!waiting) {
      startWaiting();
    }

    if (incomingByte == '\n') {
      waiting = false;
      inputBuffer[inputIndex] = '\0';

      if (waitingForAdd) {
        handleAdd(inputBuffer);
        Serial.println(F("Enter your command: "));
      } else if (waitingForRemove) {
        handleRemove(inputBuffer);
        Serial.println(F("Enter your command: "));
      } else {
        handleCommand(inputBuffer);
      }

      inputIndex = 0;
      memset(inputBuffer, 0, STRING_MAX);
    } else {
      if (inputIndex < STRING_MAX - 1) {
        inputBuffer[inputIndex++] = incomingByte;
      }
    }
  }

  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  Serial.print(F("Card UID:"));
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

  byte allowedTag = getAllowedTagIndex(mfrc522.uid.uidByte, mfrc522.uid.size);

  if (allowedTag != 255) {
    Serial.println(F("Access Granted"));
    digitalWrite(POWER_PIN, HIGH); // Power the USB output

    // TODO enable when functionality is tested
    //unsigned long beforeLogging = millis();
    logEvent(allowedTag);
    //unsigned long timeForLogging = millis() - beforeLogging;

    delay(OUTPUT_TIME); // Keep the power on for 30 seconds
    digitalWrite(POWER_PIN, LOW); // Turn off the power
  } else {
    Serial.println(F("Access Denied"));
  } 

}