#include "api.h"
#include "utils.h"
#include <iostream>
#include <queue>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>
#include <random>

using namespace cycles;

class BotClient {
  Connection connection;
  std::string name;
  GameState state;
  Player my_player;
  std::mt19937 rng;

  const std::vector<sf::Vector2i> directionVectors = {
      {0, -1},  
      {1, 0},   
      {0, 1},   
      {-1, 0}   
  };

  bool is_valid_move(Direction direction) {
    auto new_pos = my_player.position + getDirectionVector(direction);
    if (!state.isInsideGrid(new_pos)) {
      return false;
    }
    if (state.getGridCell(new_pos) != 0) {
      return false;
    }
    return true;
  }

  // Flood fill algorithm to calculate the area accessible from a position
  int flood_fill(const sf::Vector2i& position, std::vector<std::vector<bool>>& visited) {
    int area = 0;
    std::queue<sf::Vector2i> q;
    q.push(position);
    visited[position.x][position.y] = true;

    while (!q.empty()) {
      sf::Vector2i current = q.front();
      q.pop();
      area++;

      for (const auto& dir : directionVectors) {
        sf::Vector2i newPos = current + dir;
        if (state.isInsideGrid(newPos) &&
            state.getGridCell(newPos) == 0 &&
            !visited[newPos.x][newPos.y]) {
          visited[newPos.x][newPos.y] = true;
          q.push(newPos);
        }
      }
    }

    return area;
  }

  //evaluate a move by calculating accessible area and potential risks
  int evaluate_move(Direction direction) {
    // If the move is not valid, return a negative score
    if (!is_valid_move(direction)) {
      return -1;
    }

    //we get the new position after making the move
    sf::Vector2i new_pos = my_player.position + getDirectionVector(direction);

    //Code to initialize visited grid for flood fill
    std::vector<std::vector<bool>> visited(state.gridWidth, std::vector<bool>(state.gridHeight, false));
    for (int x = 0; x < state.gridWidth; ++x) {
      for (int y = 0; y < state.gridHeight; ++y) {
        if (state.getGridCell({x, y}) != 0) {
          visited[x][y] = true;  //we mark occupied cells as visited
        }
      }
    }

    //Calculate accessible area from the new position
    int area = flood_fill(new_pos, visited);

    //to assess proximity to walls or other players
    int risk = 0;
    for (const auto& dir : directionVectors) {
      sf::Vector2i adjacentPos = new_pos + dir;
      if (!state.isInsideGrid(adjacentPos) || state.getGridCell(adjacentPos) != 0) {
        risk++;  //to increase risk if adjacent to wall or player trail
      }
    }

    //To calculate a score based on area and risk
    int score = area - (risk * 10);  //to penalize moves with higher risk

    return score;
  }

  //we decide the best move based on evaluating all possible directions
  Direction decide_move() {
    int max_score = std::numeric_limits<int>::min();
    Direction best_direction = Direction::north;

    //Iterating over all possible directions
    for (int i = 0; i < 4; ++i) {
      Direction direction = getDirectionFromValue(i);
      int score = evaluate_move(direction);

      spdlog::debug("{}: Direction {} has score {}", name, getDirectionValue(direction), score);

      // Update best direction if a better score is found
      if (score > max_score) {
        max_score = score;
        best_direction = direction;
      }
    }

    spdlog::debug("{}: Chose direction {} with max score {}", name, getDirectionValue(best_direction), max_score);
    return best_direction;
  }

  //we receive the latest game state from the server
  void receive_game_state() {
    state = connection.receiveGameState();
    for (const auto& player : state.players) {
      if (player.name == name) {
        my_player = player;
        break;
      }
    }
  }

  //to seend the decided move to the server
  void send_move() {
    spdlog::debug("{}: Sending move", name);
    auto move = decide_move();
    connection.sendMove(move);
  }

public:
  BotClient(const std::string& botName) : name(botName) {
    std::random_device rd;
    rng.seed(rd());
    connection.connect(name);
    if (!connection.isActive()) {
      spdlog::critical("{}: Connection failed", name);
      exit(1);
    }
  }

  //The main loop that we use to run the bot
  void run() {
    while (connection.isActive()) {
      receive_game_state();
      send_move();
    }
  }
};

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <bot_name>" << std::endl;
    return 1;
  }
#if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_TRACE
  spdlog::set_level(spdlog::level::debug);
#endif
  std::string botName = argv[1];
  BotClient bot(botName);
  bot.run();
  return 0;
}
