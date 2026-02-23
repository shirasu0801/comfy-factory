#include "game.h"
#include <ctime>

const std::vector<std::string> Game::BASES = {"vanilla", "chocolate", "strawberry"};
const std::vector<std::string> Game::CREAMS = {"whipped", "chocolate", "strawberry"};
const std::vector<std::string> Game::TOPPINGS = {"cherry", "cookie", "nuts"};
const std::vector<std::string> Game::DECORATIONS = {"sprinkles", "chocolate_chips", "star"};
const std::vector<std::string> Game::SAUCES = {"caramel", "chocolate", "strawberry"};

Game::Game() : rng_(static_cast<unsigned>(std::time(nullptr))) {
    newGame();
}

void Game::newGame() {
    phase_ = Phase::PLAYING;
    orderNumber_ = 1;
    score_ = 0;
    mistakes_ = 0;
    currentStep_ = 0;
    current_.clear();
    generateOrder();
}

std::string Game::pickRandom(const std::vector<std::string>& items) {
    std::uniform_int_distribution<int> dist(0, static_cast<int>(items.size()) - 1);
    return items[dist(rng_)];
}

void Game::generateOrder() {
    order_.clear();
    stepCategories_.clear();
    current_.clear();
    currentStep_ = 0;

    stepCategories_ = {"base", "cream", "topping"};
    order_["base"] = pickRandom(BASES);
    order_["cream"] = pickRandom(CREAMS);
    order_["topping"] = pickRandom(TOPPINGS);

    if (orderNumber_ == MAX_ORDERS) {
        stepCategories_.push_back("decoration");
        stepCategories_.push_back("sauce");
        order_["decoration"] = pickRandom(DECORATIONS);
        order_["sauce"] = pickRandom(SAUCES);
    }
}

bool Game::selectIngredient(const std::string& category, const std::string& value) {
    if (phase_ != Phase::PLAYING) return false;
    if (currentStep_ >= static_cast<int>(stepCategories_.size())) return false;
    if (category != stepCategories_[currentStep_]) return false;

    current_[category] = value;
    currentStep_++;
    return true;
}

bool Game::submit() {
    if (phase_ != Phase::PLAYING) return false;
    if (currentStep_ < static_cast<int>(stepCategories_.size())) return false;

    bool correct = true;
    for (const auto& pair : order_) {
        auto it = current_.find(pair.first);
        if (it == current_.end() || it->second != pair.second) {
            correct = false;
            break;
        }
    }

    if (correct) {
        score_++;
        if (orderNumber_ >= MAX_ORDERS) {
            phase_ = Phase::GAME_CLEAR;
        } else {
            orderNumber_++;
            generateOrder();
        }
    } else {
        mistakes_++;
        if (mistakes_ >= MAX_MISTAKES) {
            phase_ = Phase::GAME_OVER;
        } else {
            currentStep_ = 0;
            current_.clear();
        }
    }

    return correct;
}

void Game::undo() {
    if (phase_ != Phase::PLAYING) return;
    if (currentStep_ <= 0) return;

    currentStep_--;
    std::string cat = stepCategories_[currentStep_];
    current_.erase(cat);
}

std::string Game::toJson() const {
    std::string json = "{";

    const char* phaseStr = "playing";
    if (phase_ == Phase::GAME_OVER) phaseStr = "gameover";
    else if (phase_ == Phase::GAME_CLEAR) phaseStr = "gameclear";
    json += "\"phase\":\""; json += phaseStr; json += "\",";

    json += "\"orderNumber\":" + std::to_string(orderNumber_) + ",";
    json += "\"score\":" + std::to_string(score_) + ",";
    json += "\"mistakes\":" + std::to_string(mistakes_) + ",";
    json += "\"maxMistakes\":" + std::to_string(MAX_MISTAKES) + ",";
    json += "\"maxOrders\":" + std::to_string(MAX_ORDERS) + ",";
    json += "\"currentStep\":" + std::to_string(currentStep_) + ",";
    json += "\"totalSteps\":" + std::to_string(stepCategories_.size()) + ",";

    json += "\"stepCategories\":[";
    for (size_t i = 0; i < stepCategories_.size(); i++) {
        if (i > 0) json += ",";
        json += "\"" + stepCategories_[i] + "\"";
    }
    json += "],";

    json += "\"order\":{";
    {
        bool first = true;
        for (const auto& pair : order_) {
            if (!first) json += ",";
            json += "\"" + pair.first + "\":\"" + pair.second + "\"";
            first = false;
        }
    }
    json += "},";

    json += "\"current\":{";
    {
        bool first = true;
        for (const auto& pair : current_) {
            if (!first) json += ",";
            json += "\"" + pair.first + "\":\"" + pair.second + "\"";
            first = false;
        }
    }
    json += "}";

    json += "}";
    return json;
}
