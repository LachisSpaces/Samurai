#define C_SETUP   makeColorRGB(  0, 128,   0)
#define C_PLAYER1 makeColorRGB( 96,   0,  96)
#define C_WEAPON1 makeColorRGB(255,   0, 255)
#define C_PLAYER2 makeColorRGB(  0,  96,  96)
#define C_WEAPON2 makeColorRGB(  0, 255, 255)
#define C_REFEREE makeColorRGB(255, 255,   0)

#define PREPARE_DURATION 3000
#define FIGHT_DURATION 4000
#define ATTACK_DURATION 400

enum debugStates {NONE, BITS, SIGNALSTATE, BITS_SIGNAL, GAMESTATE};
byte debugState = BITS; //SIGNALSTATE | GAMESTATE;

enum signalStates {WAIT, COMMUNICATE, FINISH};
byte signalState = WAIT; // the default signal state when the game starts

enum gameStates {SETUP, READY, PREPARE, FIGHT, ATTACK, WINNER, LOSER, GAMEOVER};
byte gameState = SETUP; // the default game state when the game begins
byte result = GAMEOVER;

enum blinkRoles {UNDEFINED, PLAYER, WEAPON, REFEREE};
byte blinkRole = UNDEFINED; // the default role when the game begins

byte playerId = 0;
byte faceIdToPlayer = 6;

byte playerCount = 0;
byte attackerId = 2;
Timer waitTimer;
Timer prepareTimer;
Timer fightTimer;
Timer attackTimer;

void setup() {
  resetMe();
}

void loop() {
  if (signalState == WAIT) {
    handleUserInteraction();
    handleBlinkSignals();
    actAsRole();
  }
  else if (signalState == COMMUNICATE) {
    if (neighborsDontHaveSignalState(WAIT)) {
      signalState = FINISH;
    }
  }
  else if (signalState == FINISH) {
    if (neighborsDontHaveSignalState(COMMUNICATE)) {
      signalState = WAIT;
    }
  }
  refreshFaces();
  sendSignal();
  // Debugging only, see debugState
  debugShowInfo();
}

// Handle interactions from the users like clicks
void handleUserInteraction() {
  // Process the click event
  if (buttonSingleClicked()) {
    if (gameState < PREPARE) { //SETUP + READY
      if (blinkRole == UNDEFINED) {
        playerCount++;
        playerId = playerCount;
        blinkRole = PLAYER;
        changeGameState(READY);
      }
    }
    else if (gameState < WINNER) { //PREPARE + FIGHT + ATTACK
      if (blinkRole == PLAYER) {
        attackerId = playerId;
        changeGameState(ATTACK);
      }
    }
  }
  else if (buttonDoubleClicked()) {
    if (blinkRole == REFEREE) {
      resetMe();
      changeGameState(SETUP);
    }
  }
}

// Handle signals from other blinks
void handleBlinkSignals() {
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) { //a neighbor!
      if (processSignal(f, getLastValueReceivedOnFace(f))) {
        break;
      }
    }
  }
}

// Process the communicated signals
bool processSignal(byte faceId, byte valueReceived) {
  // Check if the neighbor is communicating
  if (extractSignalStateFromSignal(valueReceived) != COMMUNICATE) {
    return false;
  }
  // Analyse the game state
  byte state = extractGameStateFromSignal(valueReceived);
  if (state == SETUP) { 
    resetMe();
  }
  else if (state == READY) { 
    playerCount++;
    // Change the role, if required
    if (blinkRole == UNDEFINED) {
      if (extractIsPlayerFromSignal(valueReceived)) { // neighbor is player >> assign role 'weapon'
        playerId = playerCount;
        blinkRole = WEAPON;
        faceIdToPlayer = faceId;
      }
      else if (playerCount == 2) { // assign role 'referee' after the second player is ready
        blinkRole = REFEREE;
        waitTimer.set(10);
      }
    }
  }
  else if (state == PREPARE) { 
    fightTimer.set(PREPARE_DURATION + FIGHT_DURATION);
  }
  else if (state == ATTACK) { 
    if (attackerId > 1) { // Attacker id has not been set
      attackerId = extractAttackerIdFromSignal(valueReceived);
      if (blinkRole == WEAPON) {
        attackTimer.set(ATTACK_DURATION);
        if (attackerId != playerId) { // Not the attacker
          if (!fightTimer.isExpired()) { // The player is still blocking
            result = LOSER;
          }
          else {
            result = WINNER;
          }
        }
      }
    }
  }
  else if (state < GAMEOVER) {
    attackerId = extractAttackerIdFromSignal(valueReceived);
  }
  // Change game state and tell other blinks
  changeGameState(state);
}

// This blink acts according to the assigned role
void actAsRole() {
  // Actions a referee has to do
  if (blinkRole == REFEREE) {
    if (gameState == READY) { 
      if (waitTimer.isExpired()) {
        changeGameState(PREPARE);
        prepareTimer.set(PREPARE_DURATION);
        fightTimer.set(PREPARE_DURATION + FIGHT_DURATION);
      }
    }
    else if (gameState == PREPARE) { 
      if (prepareTimer.isExpired()) {
        changeGameState(FIGHT);
      }
    }
    else if (gameState == FIGHT) { 
      if (fightTimer.isExpired()) {
//        changeGameState(GAMEOVER);
      }
    }
  }
  else if (blinkRole == WEAPON) {
    if (result != GAMEOVER) {
      if (attackTimer.isExpired()) {
        changeGameState(result);
        result = GAMEOVER;
      }
    }
  }
}

// Refresh the colors of all faces of this blink
void refreshFaces() {
debugShowBits(8);
  if (signalState == WAIT) {
    if (blinkRole == UNDEFINED) {
      setColor(C_SETUP);
    }
    else if (blinkRole == PLAYER) {
      if (playerId == 1) {
        setPlayerColor(C_PLAYER1);
      }
      else {
        setPlayerColor(C_PLAYER2);
      }
    }
    else if (blinkRole == WEAPON) {
      if (playerId == 1) {
        setWeaponColor(C_WEAPON1);
      }
      else {
        setWeaponColor(C_WEAPON2);
      }
    }
    else if (blinkRole == REFEREE) {
      setRefereeColor(C_REFEREE);
    }
  }
}

// Set the color if the role is Player
void setPlayerColor(Color color) {
  if (gameState == WINNER) { 
    if (attackerId == playerId) { // The attacker won
      setColor(color);
    }
    else {
      setColor(RED);
    }
  }
  else if (gameState == LOSER) { 
    if (attackerId == playerId) { // The attacker lost
      setColor(RED);
    }
    else {
      setColor(color);
    }
  }
  else {
    setColor(color);
  }
}

// Set the color if the role is Weapon
void setWeaponColor(Color color) {
//debugShowBits(16);
  bool isBlocking = true;
  int remaining = FIGHT_DURATION;
  if (gameState == FIGHT) { 
    remaining = fightTimer.getRemaining();
  }
  if (gameState == ATTACK) { 
    if (attackerId == playerId) { // The weapon owner attacks 
      isBlocking = false;
      remaining = attackTimer.getRemaining();
    }
    else {
      remaining = fightTimer.getRemaining();
    }
  }
  else if (gameState > ATTACK) { 
    remaining = 0;
  }
  byte faceId = faceIdToPlayer + 6;
  if (isBlocking) {
    if (remaining > 3000) {
      setColor(color);
    }
    else if (remaining > 2000) {
      setColorOnFace(color, (faceId % 6));
      setColorOnFace(color, ((faceId - 1) % 6));
      setColorOnFace(color, ((faceId + 1) % 6));
      setColorOnFace(color, ((faceId - 2) % 6));
      setColorOnFace(color, ((faceId + 2) % 6));
      setColorOnFace(OFF, ((faceId + 3) % 6));
    }
    else if (remaining > 1000) {
      setColorOnFace(color, (faceId % 6));
      setColorOnFace(color, ((faceId - 1) % 6));
      setColorOnFace(color, ((faceId + 1) % 6));
      setColorOnFace(OFF, ((faceId - 2) % 6));
      setColorOnFace(OFF, ((faceId + 2) % 6));
      setColorOnFace(OFF, ((faceId + 3) % 6));
    }
    else if (remaining > 0) {
      setColorOnFace(color, (faceId % 6));
      setColorOnFace(OFF, ((faceId - 1) % 6));
      setColorOnFace(OFF, ((faceId + 1) % 6));
      setColorOnFace(OFF, ((faceId - 2) % 6));
      setColorOnFace(OFF, ((faceId + 2) % 6));
      setColorOnFace(OFF, ((faceId + 3) % 6));
    }
    else {
      setColor(OFF);
debugShowBits(gameState);
    }
  }
  else {
    if (remaining > 300) {
      setColorOnFace(color, (faceId % 6));
      setColorOnFace(OFF, ((faceId - 1) % 6));
      setColorOnFace(OFF, ((faceId + 1) % 6));
      setColorOnFace(OFF, ((faceId - 2) % 6));
      setColorOnFace(OFF, ((faceId + 2) % 6));
      setColorOnFace(OFF, ((faceId + 3) % 6));
    }
    else if (remaining > 200) {
      setColorOnFace(OFF, (faceId % 6));
      setColorOnFace(color, ((faceId - 1) % 6));
      setColorOnFace(color, ((faceId + 1) % 6));
      setColorOnFace(OFF, ((faceId - 2) % 6));
      setColorOnFace(OFF, ((faceId + 2) % 6));
      setColorOnFace(OFF, ((faceId + 3) % 6));
    }
    else if (remaining > 100) {
      setColorOnFace(OFF, (faceId % 6));
      setColorOnFace(OFF, ((faceId - 1) % 6));
      setColorOnFace(OFF, ((faceId + 1) % 6));
      setColorOnFace(color, ((faceId - 2) % 6));
      setColorOnFace(color, ((faceId + 2) % 6));
      setColorOnFace(OFF, ((faceId + 3) % 6));
    }
    else if (remaining > 0) {
      setColorOnFace(OFF, (faceId % 6));
      setColorOnFace(OFF, ((faceId - 1) % 6));
      setColorOnFace(OFF, ((faceId + 1) % 6));
      setColorOnFace(OFF, ((faceId - 2) % 6));
      setColorOnFace(OFF, ((faceId + 2) % 6));
      setColorOnFace(color, ((faceId + 3) % 6));
    }
    else {
      setColor(OFF);
debugShowBits(gameState);
    }
  }
}

// Set the color if the role is Referee
void setRefereeColor(Color color) {
debugShowBits(16);
  if (gameState < PREPARE) { 
    setColor(color);
  }
  else if (gameState == PREPARE) { 
    int remaining = prepareTimer.getRemaining();
    int pulseProgress = remaining % 1000;
    byte pulseMapped = map(pulseProgress, 0, 1000, 0, 255);
    byte dimness = sin8_C(pulseMapped);
    Color pulseColor = dim(color, dimness);
    if (remaining > 2000) {
      setColor(pulseColor);
    }
    else if (remaining > 1000) {
      setColorOnFace(OFF, 0);
      setColorOnFace(OFF, 3);
      setColorOnFace(pulseColor, 1);
      setColorOnFace(pulseColor, 4);
      setColorOnFace(pulseColor, 2);
      setColorOnFace(pulseColor, 5);
    }
    else if (remaining > 0) {
      setColorOnFace(pulseColor, 0);
      setColorOnFace(pulseColor, 3);
      setColorOnFace(OFF, 1);
      setColorOnFace(OFF, 4);
      setColorOnFace(OFF, 2);
      setColorOnFace(OFF, 5);
    }
    else {
      setColor(OFF);
    }
  }
  else if (gameState < GAMEOVER) { 
    setColor(OFF);
  }
  else if (gameState == GAMEOVER) { 
    setColor(RED);
  }
}

// Change the game state and communicate it to the other blinks
void changeGameState(byte state) {
  gameState = state;
  signalState = COMMUNICATE;  
}

// All blinks are constantly sending signals >> Using bitwise operations to pack the information into 6 bits
void sendSignal() {
  byte lastByte = 1;//(byte)isPlayer;
  if (gameState < PREPARE) { //SETUP + READY
    bool isPlayer = (blinkRole == PLAYER);
    lastByte = (byte)isPlayer;
  }
  else if (gameState >= ATTACK) { 
    lastByte = (attackerId - 1); // Player 1 is 0, Player 2 is 1
  }
  setValueSentOnAllFaces((signalState << 4) + (gameState << 1) + lastByte);  
}

// Bitwise operations to extract the information from the signal, see sendSignal() above
byte extractSignalStateFromSignal(byte data) {
  return ((data >> 4) & 3);
}
byte extractGameStateFromSignal(byte data) {
  return ((data >> 1) & 7);
}
bool extractIsPlayerFromSignal(byte data) {
  return (bool)(data & 1);
}
byte extractAttackerIdFromSignal(byte data) {
  return (data & 1) + 1; // Player 1 is 0, Player 2 is 1
}

// Checks if none of the neighbors has the given signal state
bool neighborsDontHaveSignalState(byte state) {
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) { //a neighbor!
      if (extractSignalStateFromSignal(getLastValueReceivedOnFace(f)) == state) {
        return false;
      }
    }
  }
  return true;
}

// (Re)set this blink to its initial state
void resetMe() {
    blinkRole = UNDEFINED; 
    result = GAMEOVER;
    playerId = 0;
    faceIdToPlayer = 6;
    playerCount = 0;
    attackerId = 2;
    waitTimer.set(0);
    prepareTimer.set(0);
    fightTimer.set(0);
    attackTimer.set(0);
}

// -------- C O D E   F O R   D E B U G G I N G -------------------------------------------------------------------------------------------------------------------------------------------------------

// The GREEN color is used to show debugging information >> The 6 faces are used as bits to return a number between 0 and 63
void debugShowBits(byte flags) {
  if ((debugState & BITS) != BITS) {
    return;
  }
  setColor(OFF);
  FOREACH_FACE(f) {
    if ((flags >> f) & 1) {
      setColorOnFace(GREEN, f);  
    }
  }
}

// The RED color is used to show the signal state and/or the game state >> This function should be used at the end of the loop to overwrite the existing colors
void debugShowInfo() {
  if ((debugState & SIGNALSTATE) == SIGNALSTATE) {
    setColorOnFace(RED, signalState);  
  }
  if ((debugState & GAMESTATE) == GAMESTATE) {
    setColorOnFace(RED, gameState);  
  }
}
