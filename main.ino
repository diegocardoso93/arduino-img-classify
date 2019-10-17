/*
 * Arduino Due
 * UTFT ITDB32S + SDCard
 *
 * Diego Cardoso
 * May 2016
 * https://www.facebook.com/diegocardoso.93/videos/10210381339962306/
 */

/* Cortex M3 large sequential memory */
#define PROGMEM

/* Serial Peripheral Interface Library */
#include <SPI.h>

/* SDCard library to read image files */
#include <SD.h>

/* UTFT display library  */
#include <UTFT.h>

/*
 * Bitmap format definitions
 * https://en.wikipedia.org/wiki/BMP_file_format
 */
struct bitmapHeader
{
   unsigned short int type;
   unsigned int size;
   unsigned short int reserved1, reserved2;
   unsigned int offset;
} bmpHeader;

struct bitmapInfoHeader
{
   unsigned int size;
   int width, height;
   unsigned short int planes;
   unsigned short int bits;
   unsigned int compression;
   unsigned int image_size;
   int x_res, y_res;
   unsigned int colors;
   unsigned int imp_colors;
} bmpInfoHeader;

/* Driver and pins definitions */
UTFT GLCD(ITDB32S, 38, 39, 40, 41);
File imgFile;
extern uint8_t SmallFont[];

/* Bottle counter variables */
long cv = 0, cp = 0, cn = 0;
int randNum;
unsigned char img[76800] PROGMEM;
unsigned char r, g, b;
int i, x;
int row_count = 0, highest_row = 0;
int INIx = 20, INIy = 140, FIMx = 300, FIMy = 220;
bool cmd_paused = false;
int threshold = 120;

/* Draw the count state into screen */
void drawScoreboard(long cv, long cp, long cn) {
  GLCD.setFont(SmallFont);
  GLCD.setColor(~0);
  GLCD.print("GLASS: ", 244, 0);
  GLCD.print("PET  : ", 244, 12);
  GLCD.print("NONE : ", 244, 24);
  GLCD.printNumI(cv, 290, 00);
  GLCD.printNumI(cp, 290, 12);
  GLCD.printNumI(cn, 290, 24);
}

void setup()
{
  GLCD.InitLCD();
  GLCD.clrScr();

  Serial.begin(9600);
  Serial.println("Initializing SD card...");
  if (!SD.begin(4)) {
    Serial.println("Initialization fail!");
    return;
  }
  Serial.println("SD card initialized.");

  /* Pseudorandom number seed, to randomize images for system reading */
  randomSeed(analogRead(0));
}

void loop()
{
  while (cmd_paused) serialEvent();

  /* Capture number between 0 and 2 */
  randNum = random(0, 11) % 3;

  /* Default image name */
  char strFileName[8] = {'0','0','1','.','b','m','p','\0'};
  /* the third char is raffled */

  /* Convert to ASCII char */
  strFileName[2] = randNum+48;

  /* debug */
  Serial.println(strFileName);

  imgFile = SD.open(strFileName);

  if (imgFile && imgFile.available()) {
    /* Capture bitmap headers */
    bmpHeader.type = imgFile.read() | (imgFile.read() << 8);
    bmpHeader.size = imgFile.read() | (imgFile.read() << 8) | (imgFile.read() << 16) | (imgFile.read() << 24);
    bmpHeader.reserved1 = imgFile.read() | (imgFile.read() << 8);
    bmpHeader.reserved2 = imgFile.read() | (imgFile.read() << 8);
    bmpHeader.offset = imgFile.read() | (imgFile.read() << 8) | (imgFile.read() << 16) | (imgFile.read() << 24);
    bmpInfoHeader.size = imgFile.read() | (imgFile.read() << 8) | (imgFile.read() << 16) | (imgFile.read() << 24);
    bmpInfoHeader.width = imgFile.read() | (imgFile.read() << 8) | (imgFile.read() << 16) | (imgFile.read() << 24);
    bmpInfoHeader.height = imgFile.read() | (imgFile.read() << 8) | (imgFile.read() << 16) | (imgFile.read() << 24);
    bmpInfoHeader.planes = imgFile.read() | (imgFile.read() << 8);
    bmpInfoHeader.bits = imgFile.read() | (imgFile.read() << 8);
    bmpInfoHeader.compression = imgFile.read() | (imgFile.read() << 8) | (imgFile.read() << 16) | (imgFile.read() << 24);
    bmpInfoHeader.image_size = imgFile.read() | (imgFile.read() << 8) | (imgFile.read() << 16) | (imgFile.read() << 24);
    bmpInfoHeader.x_res = imgFile.read() | (imgFile.read() << 8) | (imgFile.read() << 16) | (imgFile.read() << 24);
    bmpInfoHeader.y_res = imgFile.read() | (imgFile.read() << 8) | (imgFile.read() << 16) | (imgFile.read() << 24);
    bmpInfoHeader.colors = imgFile.read() | (imgFile.read() << 8) | (imgFile.read() << 16) | (imgFile.read() << 24);
    bmpInfoHeader.imp_colors = imgFile.read() | (imgFile.read() << 8) | (imgFile.read() << 16) | (imgFile.read() << 24);

    /* Passes through the image pixels and reads the RGB */
    for (i=76799; i >= 0 ; i--) {
        r = imgFile.read();
        g = imgFile.read();
        b = imgFile.read();
        x = ((r >> 3 ) << 11) | ((g >> 2 ) << 5) | (b >> 3 ); /* RGB 565 format */
        img[i] = ((r >> 5 ) << 5) | ((g >> 5 ) << 2) | (b >> 6 );
        GLCD.setColor(x); 
        GLCD.drawPixel(i%320, i/320);
    }

    /* Window to shrink analysis region */
    for (i = FIMy*320; i >= INIy*320; i--) {
        GLCD.setColor(((img[i] >> 3 ) << 11) | ((img[i] >> 2 ) << 5) | (img[i] >> 3 )); /* threshold/grayscale */
        GLCD.drawPixel(i%320, i/320);
    }

    /* Print the border indicating analysis region */
    GLCD.setColor(255, 255, 0);
    GLCD.drawRoundRect(20, 140, 300, 220);

    /* Identification critery: Larger line formed at base identifies glass */
    row_count = highest_row = 0;
    for (i = FIMy*320; i >= INIy*320 ; i--) {
      if (i%320 == 0) {
        if (row_count > highest_row) {
          highest_row = row_count;
        }
        Serial.println(row_count);
        row_count = 0;
      }
      if (img[i] < threshold) {
        row_count++;
      }
    }
    if (highest_row > 50) { /* if the line formed is greater than 50 identifies as glass */
      cv++;
    } else if (highest_row>5) {
      cp++;
    } else {
      cn++;
    }
    drawScoreboard(cv, cp, cn);

    imgFile.close();
  } else {
    Serial.println("Error opening file.");
  }
}

/* debug */
void serialEvent() {
  char buf[20];
  int it=0;
  for (; Serial.available(); it++) {
    buf[it] = (char) Serial.read();
  }
  if (it) {
    if (buf[0] == 'p') {
      Serial.println("pause on");
      cmd_paused = true;
    }
    if (buf[0] == 'd') {
      Serial.println("pause off");
      cmd_paused = false;
    }
    if (buf[0] == 's') {
      Serial.print("set threshold ");
      threshold = 0;
    }
  }
}
