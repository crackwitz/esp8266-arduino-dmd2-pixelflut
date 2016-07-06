/*
  Game of Life display

  Simulates Conway's Game of Life
  https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life
 */

#include <SPI.h>
#include <DMD2.h>

bool do_gameoflife = true;
bool autonoise = true;
uint8_t noise_amount = 0;

#define NOISEHIST 8
uint16_t noisehist[NOISEHIST] = {0};

// How many displays do you have?
const int WIDTH = 2;
const int HEIGHT = 1;

SPIDMD dmd(WIDTH,HEIGHT);

void init_cells(uint8_t value) {
  for(int x = 0; x < dmd.width; x++) {
    for(int y = 0; y < dmd.height; y++) {
      dmd.setPixel(x, y, value ? GRAPHICS_ON : GRAPHICS_OFF);
    }
  }
}

// the setup routine runs once when you press reset:
void setup() {
  Serial.begin(115200);
  dmd.setBrightness(5);
  dmd.begin();

  randomSeed(analogRead(0));
  init_cells(0);
}

void iterate_gameoflife()
{
  uint16_t curcount = 0;
  
  // Store the current generation by copying the current DMD frame contents
  DMDFrame current_generation(dmd);

  // noise
  for (uint8_t k = noise_amount; k > 0; k -= 1)
    current_generation.setPixel(random(dmd.width), random(dmd.height), GRAPHICS_ON);

  // Update next generation of every pixel
  bool change = false;
  for(int x = 0; x < dmd.width; x++) {
    for(int y = 0; y < dmd.height; y++) {
      bool state = current_generation.getPixel(x,y);
      int live_neighbours = 0;

      // Count how many live neighbours we have in the current generation
      for(int nx = x - 1; nx < x + 2; nx++) {
        for(int ny = y - 1; ny < y + 2; ny++) {
          if(nx == x && ny == y)
            continue;
          if(current_generation.getPixel(nx,ny))
            live_neighbours++;
        }
      }

      // Update pixel count for the next generation
      if(state && (live_neighbours < 2 || live_neighbours > 3)) {
        state = false;
        change = true;
      }
      else if(!state && (live_neighbours == 3)) {
        state = true;
        change = true;
      }
      curcount += state;
      dmd.setPixel(x,y,state ? GRAPHICS_ON : GRAPHICS_OFF);
    }
  }

  if (autonoise)
  {
    bool match = false;
    for (int8_t i = NOISEHIST-1; i >= 0; i -= 1)
      if (noisehist[i] == curcount)
        match = true;

    if (match)
    {
      if (noise_amount < 0xff)
      {
        noise_amount += 1;
        //Serial.print("Noise ");Serial.println(noise_amount);
      }
    }
    else
    {
      if (noise_amount > 0x00)
      {
        noise_amount -= 1;
        //Serial.print("Noise ");Serial.println(noise_amount);
      }
    }
  }

  for (int8_t i = NOISEHIST-1; i >= 1; i -= 1)
    noisehist[i] = noisehist[i-1];
  noisehist[0] = curcount;
}

void spool_bitmap()
{
  
  uint8_t const left = 0, top = 0, width = dmd.width, height = dmd.height;
  //while (Serial.available() < 4) delay(1);
  //left   = Serial.read() ?: left;
  //top    = Serial.read() ?: top;
  //width  = Serial.read() ?: width;
  //height = Serial.read() ?: height;

  //int lastrecv = millis();
  
  for (uint8_t y = 0; y < height; y += 1)
  for (uint8_t x = 0; x < width; x += 8)
  {
    /*
    while (true)
    {
      if (Serial.available())
        break;
      if (millis() - lastrecv > 1000)
        return;
    }
    lastrecv = millis();
    //*/

    while (!Serial.available()) {}
    uint8_t octet = Serial.read();

    for (uint8_t k = 0; k < 8; k += 1)
      dmd.setPixel((dmd.width-1)  - (left+x+k), (dmd.height-1) - (top+y), ((octet >> k) & 1) ? GRAPHICS_ON : GRAPHICS_OFF);
  }
}

void process_uart()
{
  static uint8_t prev_available = 0;
  const uint8_t cur_available = Serial.available();

  if (cur_available > prev_available)
  {
    prev_available = cur_available;
    switch (Serial.peek())
    {
      case 'D':
      {
        if (cur_available >= 2)
        {
          Serial.read();
          dmd.setBrightness(Serial.read());
          prev_available = 0;
        }
        break;
      }
      case 'G':
      {
        if (cur_available >= 2)
        {
          Serial.read();
          uint8_t value = Serial.read();
          if (value >= 0xfe)
          {
            init_cells(0xff - value);
            do_gameoflife = 0;
          }
          else if (value <= 1)
          {
            do_gameoflife = value;
          }
          /*
          Serial.print("~");
          Serial.println(do_gameoflife);
          */
          prev_available = 0;
        }
        break;
      }
      case 'A':
      {
        if (cur_available >= 2)
        {
          Serial.read();
          autonoise = Serial.read();
          prev_available = 0;
        }
        break;
      }
      
      case 'N':
      {
        if (cur_available >= 2)
        {
          Serial.read();
          noise_amount = Serial.read();
          /*
          Serial.print("#");
          Serial.println(noise_amount);
          */
          prev_available = 0;
        }
        break;
      }
      case '!':
      {
        if (cur_available >= 4)
        {
          Serial.read();
          const uint8_t x = (dmd.width-1)  - (uint8_t)Serial.read();
          const uint8_t y = (dmd.height-1) - (uint8_t)Serial.read();
          const uint8_t v = Serial.read();
          dmd.setPixel(x, y, v ? GRAPHICS_ON : GRAPHICS_OFF);
          /*
          Serial.print("PX ");
          Serial.print(x);
          Serial.print(" ");
          Serial.print(y);
          Serial.print(" ");
          Serial.println(v);
          */
          prev_available = 0;
        }
        break;
      }

      case 'B':
      {
        Serial.read(); // discard command 'B'
        spool_bitmap();
        prev_available = 0;
        break;
      }

      default:
      {
        Serial.read();
        prev_available = 0;
        break;
      }
    }
  }
}

// the loop routine runs over and over again forever:
void loop() {
  process_uart();

  if (do_gameoflife)
    iterate_gameoflife();
}
