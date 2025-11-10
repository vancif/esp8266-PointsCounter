template<typename T> 
const T& getArrayElement(const T* array, int pos, int length) {
  return array[pos % length];
}

void resetPlayerPoints(uint8_t startingPoints) {
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    playerPoints[i][0] = startingPoints;
    for (uint8_t j = 1; j < MAX_PLAYERS; j++) {
      playerPoints[i][j] = 0;
    }
  }
}