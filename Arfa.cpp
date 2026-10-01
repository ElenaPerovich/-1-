#include <Wire.h>

#include <U8x8lib.h>

#include <SPI.h>

#include <SD.h>


// =====================================================

// OLED

// =====================================================

U8X8_SSD1306_128X64_NONAME_HW_I2C oled(

  U8X8_PIN_NONE

);

// =====================================================

// CD74HC4067

// =====================================================

const byte MUX_S0 = 2;

const byte MUX_S1 = 3;

const byte MUX_S2 = 4;

const byte MUX_S3 = 5;

const byte MUX_COM = A0;

const byte sensorChannels[7] = {

  9, 10, 11, 12, 13, 14, 15

};

// =====================================================

// 74HC595

// =====================================================

const byte DATA_PIN  = 6;

const byte CLOCK_PIN = 7;

const byte LATCH_PIN = 8;

// =====================================================

// BUZZER

// =====================================================

const byte BUZZER_PIN = 9;

// =====================================================

// microSD

// =====================================================

const byte SD_CS = 10;

// =====================================================

// BUTTONS

// =====================================================

// A1 теперь используется как аналоговый вход

// сразу для LEARN и SELECT.

const byte BUTTONS_A1 = A1;

const byte BTN_REC  = A2;

const byte BTN_PLAY = A3;

// =====================================================

// NOTES

// =====================================================

const uint16_t noteFrequencies[7] = {

  262,

  294,

  330,

  349,

  392,

  440,

  494

};

// =====================================================

// SENSORS

// =====================================================

const int SENSOR_THRESHOLD = 500;

bool blocked[7] = {

  false,

  false,

  false,

  false,

  false,

  false,

  false

};

// =====================================================

// MODES

// =====================================================

enum Mode : byte {

  NORMAL,

  LEARNING,

  RECORDING,

  PLAYBACK

};

Mode currentMode = NORMAL;

// =====================================================

// LEARNING SONG

// =====================================================

const byte learningSong[] = {

  0, 0, 4, 4, 5, 5, 4,

  3, 3, 2, 2, 1, 1, 0

};

const byte learningLength =

  sizeof(learningSong) / sizeof(learningSong[0]);

byte learningIndex = 0;

// =====================================================

// RECORDING

// =====================================================

File recordFile;

unsigned long lastRecordTime = 0;

unsigned int recordedNotes = 0;

// =====================================================

// MELODY FILES

// =====================================================

// Ограничим проект 20 сохранёнными мелодиями.

const byte MAX_MELODIES = 20;

byte melodyCount = 0;

byte selectedMelody = 1;

// =====================================================

// BUTTON STATES

// =====================================================

bool prevRec = HIGH;

bool prevPlay = HIGH;

byte previousA1Button = 0;

// =====================================================

// FILE NAME

// =====================================================

void makeMelodyName(

  byte number,

  char *name

) {

  // Получаем:

  // M001.TXT

  // M002.TXT

  // ...

  sprintf(

    name,

    "M%03d.TXT",

    number

  );

}

// =====================================================

// OLED

// =====================================================

void clearOLED() {

  oled.clearDisplay();

  oled.setCursor(0, 0);

  oled.print(F("LASER HARP"));

}

void showNormalScreen() {

  clearOLED();

  oled.setCursor(0, 2);

  oled.print(F("Mode: NORMAL"));

  oled.setCursor(0, 4);

  if (melodyCount == 0) {

    oled.print(F("No melodies"));

  }

  else {

    oled.print(F("Selected: M"));

    oled.print(selectedMelody);

  }

}

void showRecordingScreen() {

  clearOLED();

  oled.setCursor(0, 2);

  oled.print(F("RECORDING"));

  oled.setCursor(0, 4);

  oled.print(F("Melody: M"));

  oled.print(melodyCount);

  oled.setCursor(0, 6);

  oled.print(F("Notes: "));

  oled.print(recordedNotes);

}

void showLearningScreen() {

  clearOLED();

  oled.setCursor(0, 2);

  oled.print(F("LEARNING"));

  oled.setCursor(0, 4);

  oled.print(F("Next: "));

  oled.print(

    learningSong[learningIndex]

  );

}

void showPlaybackScreen() {

  clearOLED();

  oled.setCursor(0, 2);

  oled.print(F("PLAYBACK"));

  oled.setCursor(0, 4);

  oled.print(F("Melody: M"));

  oled.print(selectedMelody);

}

void showSDError() {

  clearOLED();

  oled.setCursor(0, 2);

  oled.print(F("SD ERROR"));

}

// =====================================================

// MULTIPLEXER

// =====================================================

void selectMuxChannel(byte channel) {

  digitalWrite(

    MUX_S0,

    bitRead(channel, 0)

  );

  digitalWrite(

    MUX_S1,

    bitRead(channel, 1)

  );

  digitalWrite(

    MUX_S2,

    bitRead(channel, 2)

  );

  digitalWrite(

    MUX_S3,

    bitRead(channel, 3)

  );

}

// =====================================================

// 74HC595 / LEDs

// =====================================================

void writeLeds(byte value) {

  digitalWrite(

    LATCH_PIN,

    LOW

  );

  shiftOut(

    DATA_PIN,

    CLOCK_PIN,

    MSBFIRST,

    value

  );

  digitalWrite(

    LATCH_PIN,

    HIGH

  );

}

void allLedsOff() {

  writeLeds(0);

}

void showLearningLed(byte note) {

  byte value = 0;

  bitSet(value, note);

  writeLeds(value);

}

// =====================================================

// SOUND

// =====================================================

void playNote(byte note) {

  if (note >= 7) {

    return;

  }

  tone(

    BUZZER_PIN,

    noteFrequencies[note],

    250

  );

}

// =====================================================

// COUNT MELODIES ON SD

// =====================================================

void findMelodies() {

  melodyCount = 0;

  char name[13];

  for (

    byte i = 1;

    i <= MAX_MELODIES;

    i++

  ) {

    makeMelodyName(

      i,

      name

    );

    if (SD.exists(name)) {

      melodyCount = i;

    }

    else {

      break;

    }

  }

  if (melodyCount > 0) {

    selectedMelody = 1;

  }

}

// =====================================================

// ANALOG BUTTONS A1

// =====================================================

// Возвращает:

//

// 0 = ничего

// 1 = LEARN

// 2 = SELECT

byte readA1Button() {

  int value =

    analogRead(BUTTONS_A1);

  // Значения могут немного отличаться.

// Поэтому используем диапазоны.

  if (value < 200) {

    return 1;

  }

  if (value < 700) {

    return 2;

  }

  return 0;

}

// =====================================================

// NORMAL

// =====================================================

void startNormal() {

  currentMode = NORMAL;

  allLedsOff();

  showNormalScreen();

}

// =====================================================

// LEARNING

// =====================================================

void startLearning() {

  currentMode = LEARNING;

  learningIndex = 0;

  showLearningLed(

    learningSong[learningIndex]

  );

  showLearningScreen();

}

void handleLearningNote(

  byte note

) {

  byte expected =

    learningSong[learningIndex];

  if (note != expected) {

    return;

  }

  playNote(note);

  learningIndex++;

  if (

    learningIndex >=

    learningLength

  ) {

    startNormal();

    return;

  }

  showLearningLed(

    learningSong[learningIndex]

  );

  showLearningScreen();

}

// =====================================================

// SELECT NEXT MELODY

// =====================================================

void selectNextMelody() {

  if (melodyCount == 0) {

    return;

  }

  selectedMelody++;

  if (

    selectedMelody >

    melodyCount

  ) {

    selectedMelody = 1;

  }

  showNormalScreen();

}

// =====================================================

// START RECORDING

// =====================================================

void startRecording() {

  if (

    melodyCount >=

    MAX_MELODIES

  ) {

    return;

  }

  // Следующая запись получает

  // следующий номер.

  melodyCount++;

  selectedMelody =

    melodyCount;

  char name[13];

  makeMelodyName(

    melodyCount,

    name

  );

  // На всякий случай удаляем файл,

  // если он уже существовал.

  if (SD.exists(name)) {

    SD.remove(name);

  }

  recordFile =

    SD.open(

      name,

      FILE_WRITE

    );

  if (!recordFile) {

    melodyCount--;

    showSDError();

    return;

  }

  recordedNotes = 0;

  lastRecordTime =

    millis();

  currentMode =

    RECORDING;

  allLedsOff();

  showRecordingScreen();

}

// =====================================================

// STOP RECORDING

// =====================================================

void stopRecording() {

  if (recordFile) {

    recordFile.close();

  }

  currentMode =

    NORMAL;

  selectedMelody =

    melodyCount;

  showNormalScreen();

}

// =====================================================

// SAVE NOTE TO SD

// =====================================================

void saveNote(byte note) {

  if (!recordFile) {

    return;

  }

  unsigned long now =

    millis();

  unsigned long delta =

    now - lastRecordTime;

  // Формат файла:

  //

  // note,delay

  //

  // например:

  // 0,300

  recordFile.print(note);

  recordFile.print(',');

  recordFile.println(delta);

  // Сразу физически отправляем

  // данные на SD.

  recordFile.flush();

  lastRecordTime =

    now;

  recordedNotes++;

  showRecordingScreen();

}

// =====================================================

// PLAY MELODY FROM SD

// =====================================================

void playSelectedMelody() {

  if (melodyCount == 0) {

    return;

  }

  char name[13];

  makeMelodyName(

    selectedMelody,

    name

  );

  File file =

    SD.open(name);

  if (!file) {

    showSDError();

    delay(700);

    showNormalScreen();

    return;

  }

  currentMode =

    PLAYBACK;

  allLedsOff();

  showPlaybackScreen();

  while (file.available()) {

    int note =

      file.parseInt();

    // Пропускаем запятую.

    if (file.peek() == ',') {

      file.read();

    }

    unsigned long pause =

      file.parseInt();

    // Пропускаем перевод строки.

    while (

      file.available() &&

      (

        file.peek() == '\n' ||

        file.peek() == '\r'

      )

    ) {

      file.read();

    }

    delay(pause);

    if (

      note >= 0 &&

      note < 7

    ) {

      playNote(

        (byte)note

      );

    }

  }

  file.close();

  // Даём последней ноте прозвучать.

  delay(300);

  startNormal();

}

// =====================================================

// STRING PRESSED

// =====================================================

void stringPressed(

  byte note

) {

  switch (

    currentMode

  ) {

    case NORMAL:

      playNote(note);

      break;

    case LEARNING:

      handleLearningNote(

        note

      );

      break;

    case RECORDING:

      playNote(note);

      saveNote(note);

      break;

    case PLAYBACK:

      break;

  }

}

// =====================================================

// SENSOR SCANNING

// =====================================================

void scanSensors() {

  for (

    byte i = 0;

    i < 7;

    i++

  ) {

    selectMuxChannel(

      sensorChannels[i]

    );

    delay(3);

    int value =

      analogRead(

        MUX_COM

      );

    bool active =

      value >

      SENSOR_THRESHOLD;

    if (

      active &&

      !blocked[i]

    ) {

      blocked[i] = true;

      stringPressed(i);

    }

    if (

      !active &&

      blocked[i]

    ) {

      blocked[i] = false;

    }

  }

}

// =====================================================

// CHECK BUTTONS

// =====================================================

void checkButtons() {

  // -------------------------

  // LEARN + SELECT on A1

  // -------------------------

  byte a1Button =

    readA1Button();

  if (

    previousA1Button == 0 &&

    a1Button != 0

  ) {

    if (

      a1Button == 1

    ) {

      // LEARN

      if (

        currentMode ==

        LEARNING

      ) {

        startNormal();

      }

      else if (

        currentMode ==

        NORMAL

      ) {

        startLearning();

      }

    }

    if (

      a1Button == 2 &&

      currentMode ==

      NORMAL

    ) {

      selectNextMelody();

    }

  }

  previousA1Button =

    a1Button;

  // -------------------------

  // REC

  // -------------------------

  bool rec =

    digitalRead(

      BTN_REC

    );

  if (

    prevRec == HIGH &&

    rec == LOW

  ) {

    if (

      currentMode ==

      RECORDING

    ) {

      stopRecording();

    }

    else if (

      currentMode ==

      NORMAL

    ) {

      startRecording();

    }

  }

  prevRec = rec;

  // -------------------------

  // PLAY

  // -------------------------

  bool play =

    digitalRead(

      BTN_PLAY

    );

  if (

    prevPlay == HIGH &&

    play == LOW &&

    currentMode ==

    NORMAL

  ) {

    playSelectedMelody();

  }

  prevPlay = play;

  delay(20);

}

// =====================================================

// SETUP

// =====================================================

void setup() {

  Serial.begin(9600);

  pinMode(MUX_S0, OUTPUT);

  pinMode(MUX_S1, OUTPUT);

  pinMode(MUX_S2, OUTPUT);

  pinMode(MUX_S3, OUTPUT);

  pinMode(DATA_PIN, OUTPUT);

  pinMode(CLOCK_PIN, OUTPUT);

  pinMode(LATCH_PIN, OUTPUT);

  pinMode(

    BUZZER_PIN,

    OUTPUT

  );

  // A1 НЕ ставим INPUT_PULLUP.

  // Там теперь наша резисторная схема.

  pinMode(

    BTN_REC,

    INPUT_PULLUP

  );

  pinMode(

    BTN_PLAY,

    INPUT_PULLUP

  );

  allLedsOff();

  // OLED

  oled.begin();

  oled.setFont(

    u8x8_font_chroma48medium8_r

  );

  // SD

  if (!SD.begin(SD_CS)) {

    showSDError();

    Serial.println(

      F("SD ERROR")

    );

    while (true) {

    }

  }

  // Ищем уже сохранённые мелодии.

  findMelodies();

  showNormalScreen();

  Serial.println(

    F("LASER HARP READY")

  );

}

// =====================================================

// LOOP

// =====================================================

void loop() {

  checkButtons();

  if (currentMode != PLAYBACK) {
    scanSensors();
  }
}