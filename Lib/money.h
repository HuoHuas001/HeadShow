#pragma once
#include <string>
typedef long long money_t;
typedef std::string xuid_t;

enum LLMoneyEvent
{
    Set, Add, Reduce, Trans
};
typedef bool (*LLMoneyCallback)(LLMoneyEvent type, xuid_t from, xuid_t to, money_t value);


class EconomySystem
{
public:
    static bool init();
    static money_t getMoney(xuid_t player);
};