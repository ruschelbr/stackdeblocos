#include <MD_MAX72xx.h>
#include <SPI.h>

// Define as configurações do MAX7219
#define MAX_DEVICES 4  // Display 4 em 1
#define CLK_PIN 11
#define DATA_PIN 12
#define CS_PIN 10

MD_MAX72XX matrix = MD_MAX72XX(MD_MAX72XX::FC16_HW, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// Pino do botão
#define BUTTON_PIN 2  // Pino do botão (usando pull-up interno)

// Tamanho da grade
#define WIDTH 8    // 8 colunas (largura de um único display 8x8)
#define HEIGHT 32  // 32 linhas (4 módulos x 8 linhas)

// Variáveis do jogo
int blockPos = 0;                  // Posição x atual do bloco (LED mais à esquerda)
const int INITIAL_BLOCK_WIDTH = 4; // Largura inicial do bloco em LEDs
int currentBlockWidth = INITIAL_BLOCK_WIDTH; // Largura ATUAL do bloco (diminui se desalinhar)
int blockRow = 0;              // Linha atual do bloco (0 = base)
bool blockMoving = true;       // O bloco está se movendo?
int stack[HEIGHT][WIDTH];      // Estado da pilha (1 = LED aceso, 0 = apagado)
int stackHeight = 0;           // Linha mais alta ocupada
bool gameOver = false;         // Flag de fim de jogo
unsigned long lastMove = 0;    // Momento do último movimento do bloco
int moveDelay = 200;           // Atraso inicial do movimento (ms)
bool moveRight = true;         // Direção do movimento do bloco
bool testMode = false;         // Modo de teste para o mapeamento do display
int testRow = 0;               // Linha atual no modo de teste
const bool ROTATE_DISPLAY = true; // Ajuste para a rotação do FC-16 (linhas/colunas trocadas)

void setup() {
  Serial.begin(9600);
  Serial.println("Setup started");

  matrix.begin(); // Inicializa o display
  matrix.clear();

  pinMode(BUTTON_PIN, INPUT_PULLUP); // Botão com pull-up interno
  randomSeed(analogRead(A0));        // Semente aleatória usando pino analógico não utilizado
  resetGame();
  Serial.println("Game initialized. Send 't' in Serial Monitor to enter test mode.");
}

void resetGame() {
  // Limpa o array da pilha
  for (int row = 0; row < HEIGHT; row++) {
    for (int col = 0; col < WIDTH; col++) {
      stack[row][col] = 0;
    }
  }
  currentBlockWidth = INITIAL_BLOCK_WIDTH; // Reseta a largura do bloco para o valor inicial
  blockPos = random(0, WIDTH - currentBlockWidth + 1); // Início aleatório
  blockRow = 0;
  stackHeight = 0;
  gameOver = false;
  blockMoving = true;
  moveDelay = 200;
  moveRight = true;
  testMode = false;
  testRow = 0;
  updateDisplay();
  Serial.println("Game reset");
}

void updateDisplay() {
  matrix.clear();

  if (testMode) {
    // Modo de teste: acende uma linha por vez
    for (int col = 0; col < WIDTH; col++) {
      if (ROTATE_DISPLAY) {
        matrix.setPoint(col, testRow, true); // Rotaciona 90° para display vertical
      } else {
        matrix.setPoint(testRow, col, true);
      }
    }
    Serial.print("Test mode: Lighting row ");
    Serial.println(testRow);
    return;
  }

  // Desenha a pilha
  for (int row = 0; row < HEIGHT; row++) {
    for (int col = 0; col < WIDTH; col++) {
      if (stack[row][col]) {
        if (ROTATE_DISPLAY) {
          matrix.setPoint(col, row, true); // Rotaciona 90°
        } else {
          matrix.setPoint(row, col, true);
        }
      }
    }
  }

  // Desenha o bloco em movimento se o jogo não acabou
  if (!gameOver && blockMoving) {
    for (int col = blockPos; col < blockPos + currentBlockWidth; col++) {
      if (col >= 0 && col < WIDTH) {
        if (ROTATE_DISPLAY) {
          matrix.setPoint(col, blockRow, true); // Rotaciona 90°
        } else {
          matrix.setPoint(blockRow, col, true);
        }
      }
    }
    Serial.print("Block at row ");
    Serial.print(blockRow);
    Serial.print(", cols ");
    Serial.print(blockPos);
    Serial.print("-");
    Serial.println(blockPos + currentBlockWidth - 1);
  }
}

void dropBlock() {
  blockMoving = false; // Para o movimento do bloco
  int alignedCount = 0; // Quantos LEDs do bloco realmente alinharam

  // Verifica se o bloco caiu sobre a pilha ou na base
  for (int col = blockPos; col < blockPos + currentBlockWidth; col++) {
    if (col >= 0 && col < WIDTH) {
      // Se for a linha 0 ou houver sobreposição com a pilha abaixo, posiciona o bloco
      if (blockRow == 0 || stack[blockRow - 1][col] == 1) {
        stack[blockRow][col] = 1;
        alignedCount++;
      }
    }
  }

  // Se nenhum LED alinhou (errou a pilha completamente), fim de jogo
  if (alignedCount == 0 && blockRow > 0) {
    gameOver = true;
    Serial.println("Game over: Missed stack");
    return;
  }

  // Atualiza a largura do bloco para a próxima rodada com base no que alinhou
  currentBlockWidth = alignedCount;

  // Se por algum motivo a largura zerar (não deveria, já tratado acima), fim de jogo
  if (currentBlockWidth <= 0) {
    gameOver = true;
    Serial.println("Game over: Block width reached 0");
    return;
  }

  // Atualiza a altura da pilha
  if (blockRow >= stackHeight) {
    stackHeight = blockRow + 1;
  }

  // Verifica se a pilha chegou ao topo
  if (stackHeight >= HEIGHT) {
    gameOver = true;
    Serial.println("Game over: Stack reached top");
    return;
  }

  // Move para a próxima linha
  blockRow++;
  blockPos = random(0, WIDTH - currentBlockWidth + 1);
  blockMoving = true;

  // Acelera o jogo
  moveDelay = max(50, moveDelay - 10); // Atraso mínimo de 50ms
  Serial.print("Dropped block at row ");
  Serial.print(blockRow - 1);
  Serial.print(", new width ");
  Serial.print(currentBlockWidth);
  Serial.print(", new block at row ");
  Serial.println(blockRow);
  Serial.println("Stack after drop:");
  for (int col = 0; col < WIDTH; col++) {
    Serial.print(stack[blockRow - 1][col]);
  }
  Serial.println();
}

void loop() {
  // Verifica entrada Serial para alternar o modo de teste
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 't') {
      testMode = !testMode;
      if (testMode) {
        testRow = 0;
        Serial.println("Entered test mode. Send 'n' or press button to advance row, 't' to exit.");
      } else {
        Serial.println("Exited test mode");
        resetGame();
      }
    } else if (testMode && c == 'n') {
      testRow = (testRow + 1) % HEIGHT;
      Serial.print("Test mode: Advanced to row ");
      Serial.println(testRow);
    }
    updateDisplay();
  }

  if (testMode) {
    // No modo de teste, avança a linha com o botão
    if (digitalRead(BUTTON_PIN) == LOW) {
      delay(50); // Debounce
      if (digitalRead(BUTTON_PIN) == LOW) {
        testRow = (testRow + 1) % HEIGHT;
        Serial.print("Test mode: Advanced to row ");
        Serial.println(testRow);
        updateDisplay();
        while (digitalRead(BUTTON_PIN) == LOW); // Espera o botão ser solto
      }
    }
    return;
  }

  if (gameOver) {
    // Pisca o display e espera o botão para reiniciar
    matrix.clear();
    delay(500);
    updateDisplay();
    delay(500);
    if (digitalRead(BUTTON_PIN) == LOW) {
      delay(50); // Debounce
      if (digitalRead(BUTTON_PIN) == LOW) {
        resetGame();
        Serial.println("Game restarted");
        while (digitalRead(BUTTON_PIN) == LOW); // Espera o botão ser solto
      }
    }
    return;
  }

  // Move o bloco para esquerda/direita
  if (blockMoving && millis() - lastMove >= moveDelay) {
    blockPos += moveRight ? 1 : -1;
    if (blockPos <= 0) {
      blockPos = 0;
      moveRight = true;
    } else if (blockPos >= WIDTH - currentBlockWidth) {
      blockPos = WIDTH - currentBlockWidth;
      moveRight = false;
    }
    lastMove = millis();
    updateDisplay();
  }

  // Verifica o botão pressionado para soltar o bloco
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("Button pressed");
      dropBlock();
      updateDisplay();
      while (digitalRead(BUTTON_PIN) == LOW); // Espera o botão ser solto
    }
  }
}
