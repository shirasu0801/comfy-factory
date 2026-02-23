#pragma once
#include <string>
#include <vector>
#include <map>
#include <random>

enum class Phase { PLAYING, GAME_OVER, GAME_CLEAR };

class Game {
public:
    Game();
    void newGame();
    bool selectIngredient(const std::string& category, const std::string& value);
    bool submit();
    void undo();
    std::string toJson() const;

private:
    void generateOrder();
    std::string pickRandom(const std::vector<std::string>& items);

    Phase phase_;
    int orderNumber_;
    int score_;
    int mistakes_;
    int currentStep_;

    std::vector<std::string> stepCategories_;
    std::map<std::string, std::string> order_;
    std::map<std::string, std::string> current_;

    std::mt19937 rng_;

    static const int MAX_ORDERS = 3;
    static const int MAX_MISTAKES = 5;

    static const std::vector<std::string> BASES;
    static const std::vector<std::string> CREAMS;
    static const std::vector<std::string> TOPPINGS;
    static const std::vector<std::string> DECORATIONS;
    static const std::vector<std::string> SAUCES;
};
