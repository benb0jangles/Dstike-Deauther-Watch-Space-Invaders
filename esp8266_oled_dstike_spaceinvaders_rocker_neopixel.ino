#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_NeoPixel.h>

// Pin definitions
#define OLED_SDA 5
#define OLED_SCL 4
#define NEOPIXEL_PIN 15
#define LEFT_BUTTON 12
#define RIGHT_BUTTON 13

// Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define i2c_Address 0x3c

// Game constants
#define PLAYER_WIDTH 8
#define PLAYER_HEIGHT 4
#define ENEMY_WIDTH 8
#define ENEMY_HEIGHT 4
#define BULLET_WIDTH 2
#define BULLET_HEIGHT 4
#define NUM_ENEMIES 6
#define ENEMY_ROWS 2
#define MOVE_DELAY 50
#define ENEMY_DROP_DISTANCE 5
#define ENEMY_BOTTOM_LIMIT (SCREEN_HEIGHT - PLAYER_HEIGHT - 10)

// Initialize display and LED
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_NeoPixel pixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Game objects
struct GameObject {
  int x;
  int y;
  bool active;
};

GameObject player;
GameObject bullet;
GameObject enemies[ENEMY_ROWS][NUM_ENEMIES];

int score = 0;
bool gameOver = false;
bool levelComplete = false;
unsigned long lastMoveTime = 0;
int enemyDirection = 1;
bool victoryDisplayed = false;

void setup() {
  pinMode(LEFT_BUTTON, INPUT_PULLUP);
  pinMode(RIGHT_BUTTON, INPUT_PULLUP);
  
  Wire.begin(OLED_SDA, OLED_SCL);
  
  display.begin(i2c_Address, true);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.display();
  
  pixel.begin();
  pixel.clear();
  pixel.show();
  
  initializeGame();
}

void initializeGame() {
  player.x = SCREEN_WIDTH / 2 - PLAYER_WIDTH / 2;
  player.y = SCREEN_HEIGHT - PLAYER_HEIGHT - 2;
  player.active = true;
  
  bullet.active = false;
  
  initializeEnemies();
  
  score = 0;
  gameOver = false;
  levelComplete = false;
  victoryDisplayed = false;
}

void loop() {
  if (!gameOver && !levelComplete) {
    handleInput();
    updateGame();
    drawGame();
  } else if (levelComplete) {
    displayLevelComplete();
  } else {
    displayGameOver();
  }
  delay(16);
}

void initializeEnemies() {
  for (int row = 0; row < ENEMY_ROWS; row++) {
    for (int i = 0; i < NUM_ENEMIES; i++) {
      enemies[row][i].x = i * (ENEMY_WIDTH + 8) + 8;
      enemies[row][i].y = row * (ENEMY_HEIGHT + 8) + 8;
      enemies[row][i].active = true;
    }
  }
}

void handleInput() {
  if (digitalRead(LEFT_BUTTON) == LOW) {
    player.x = max(0, player.x - 2);
  }
  
  if (digitalRead(RIGHT_BUTTON) == LOW) {
    player.x = min(SCREEN_WIDTH - PLAYER_WIDTH, player.x + 2);
  }
  
  if (!bullet.active && (digitalRead(LEFT_BUTTON) == LOW || digitalRead(RIGHT_BUTTON) == LOW)) {
    bullet.x = player.x + PLAYER_WIDTH / 2 - BULLET_WIDTH / 2;
    bullet.y = player.y;
    bullet.active = true;
    
    pixel.setPixelColor(0, pixel.Color(0, 32, 0));
    pixel.show();
    delay(50);
    pixel.clear();
    pixel.show();
  }
}

void updateGame() {
  // Update bullet
  if (bullet.active) {
    bullet.y -= 2;
    if (bullet.y < 0) {
      bullet.active = false;
    }
  }
  
  // Update enemies
  if (millis() - lastMoveTime > MOVE_DELAY) {
    bool needsToDropDown = false;
    
    // Find rightmost and leftmost active enemies
    int rightmostX = 0;
    int leftmostX = SCREEN_WIDTH;
    
    for (int row = 0; row < ENEMY_ROWS; row++) {
      for (int i = 0; i < NUM_ENEMIES; i++) {
        if (enemies[row][i].active) {
          rightmostX = max(rightmostX, enemies[row][i].x);
          leftmostX = min(leftmostX, enemies[row][i].x);
        }
      }
    }
    
    // Check if enemies need to change direction
    if ((enemyDirection > 0 && rightmostX >= SCREEN_WIDTH - ENEMY_WIDTH) ||
        (enemyDirection < 0 && leftmostX <= 0)) {
      needsToDropDown = true;
    }
    
    // Move enemies
    if (needsToDropDown) {
      enemyDirection *= -1;
      bool enemyReachedBottom = false;
      
      for (int row = 0; row < ENEMY_ROWS; row++) {
        for (int i = 0; i < NUM_ENEMIES; i++) {
          if (enemies[row][i].active) {
            enemies[row][i].y += ENEMY_DROP_DISTANCE;
            
            // Check if any enemy reached the bottom
            if (enemies[row][i].y >= ENEMY_BOTTOM_LIMIT) {
              enemyReachedBottom = true;
            }
          }
        }
      }
      
      if (enemyReachedBottom) {
        gameOver = true;
      }
    } else {
      for (int row = 0; row < ENEMY_ROWS; row++) {
        for (int i = 0; i < NUM_ENEMIES; i++) {
          if (enemies[row][i].active) {
            enemies[row][i].x += enemyDirection;
          }
        }
      }
    }
    
    lastMoveTime = millis();
  }
  
  // Check collisions
  if (bullet.active) {
    for (int row = 0; row < ENEMY_ROWS; row++) {
      for (int i = 0; i < NUM_ENEMIES; i++) {
        if (enemies[row][i].active) {
          if (checkCollision(bullet, enemies[row][i])) {
            enemies[row][i].active = false;
            bullet.active = false;
            score += 10;
            
            pixel.setPixelColor(0, pixel.Color(0, 0, 32));
            pixel.show();
            delay(50);
            pixel.clear();
            pixel.show();
            
            // Check if all enemies are destroyed
            bool allDestroyed = true;
            for (int r = 0; r < ENEMY_ROWS; r++) {
              for (int j = 0; j < NUM_ENEMIES; j++) {
                if (enemies[r][j].active) {
                  allDestroyed = false;
                  break;
                }
              }
            }
            if (allDestroyed) {
              levelComplete = true;
            }
          }
        }
      }
    }
  }
}

bool checkCollision(GameObject obj1, GameObject obj2) {
  return (obj1.x < obj2.x + ENEMY_WIDTH &&
          obj1.x + BULLET_WIDTH > obj2.x &&
          obj1.y < obj2.y + ENEMY_HEIGHT &&
          obj1.y + BULLET_HEIGHT > obj2.y);
}

void drawGame() {
  display.clearDisplay();
  
  // Draw player
  display.fillRect(player.x, player.y, PLAYER_WIDTH, PLAYER_HEIGHT, SH110X_WHITE);
  
  // Draw bullet
  if (bullet.active) {
    display.fillRect(bullet.x, bullet.y, BULLET_WIDTH, BULLET_HEIGHT, SH110X_WHITE);
  }
  
  // Draw enemies
  for (int row = 0; row < ENEMY_ROWS; row++) {
    for (int i = 0; i < NUM_ENEMIES; i++) {
      if (enemies[row][i].active) {
        display.fillRect(enemies[row][i].x, enemies[row][i].y, ENEMY_WIDTH, ENEMY_HEIGHT, SH110X_WHITE);
      }
    }
  }
  
  // Draw score
  display.setCursor(0, 0);
  display.print("Score: ");
  display.print(score);
  
  display.display();
}

void displayLevelComplete() {
  if (!victoryDisplayed) {
    // Rainbow animation on NeoPixel
    for (int i = 0; i < 32; i++) {
      pixel.setPixelColor(0, pixel.Color(i, 32-i, i));
      pixel.show();
      delay(10);
    }
    victoryDisplayed = true;
  }
  
  display.clearDisplay();
  display.setCursor(15, 20);
  display.print("Level Complete!");
  display.setCursor(20, 35);
  display.print("Score: ");
  display.print(score);
  display.setCursor(15, 50);
  display.print("Press any button");
  display.display();
  
  // Check for button press to restart
  if (digitalRead(LEFT_BUTTON) == LOW || digitalRead(RIGHT_BUTTON) == LOW) {
    delay(200); // Debounce
    initializeGame();
  }
}

void displayGameOver() {
  display.clearDisplay();
  display.setCursor(20, 20);
  display.print("Game Over!");
  display.setCursor(20, 35);
  display.print("Score: ");
  display.print(score);
  display.display();
  
  // Flash LED red on game over
  pixel.setPixelColor(0, pixel.Color(32, 0, 0));
  pixel.show();
  delay(500);
  pixel.clear();
  pixel.show();
  delay(500);
  
  // Check for button press to restart
  if (digitalRead(LEFT_BUTTON) == LOW || digitalRead(RIGHT_BUTTON) == LOW) {
    delay(200); // Debounce
    initializeGame();
  }
}