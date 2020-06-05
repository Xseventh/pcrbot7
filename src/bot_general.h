//
// Created by seventh on 2020/6/5.
//

#ifndef PCRBOT_SRC_BOT_GENERAL_H_
#define PCRBOT_SRC_BOT_GENERAL_H_
#include <map>
#include "filter.hpp"
using ::std::map;
class IBot {
  public:
    IBot(const FilterInput &) {}
  private:
};
template<typename Bot>
class BotFactory {
  public:
    Bot &getInstance(const FilterInput &input) {
        auto Iter = mBotObjects.find(*input.mGroupId);
        if (Iter == mBotObjects.end()) {
            Iter = mBotObjects.insert(decltype(mBotObjects)::value_type(*input.mGroupId, new Bot(input))).first;
        }
        return *Iter->second;
    }
  private:
    map<int64_t, Bot *> mBotObjects;
};
#endif //PCRBOT_SRC_BOT_GENERAL_H_
