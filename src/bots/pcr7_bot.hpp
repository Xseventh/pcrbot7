//
// Created by seventh on 2020/6/5.
//

#ifndef PCRBOT_SRC_BOTS_PRC7_BOT_HPP_
#define PCRBOT_SRC_BOTS_PRC7_BOT_HPP_
#include <cqcppsdk/cqcppsdk.h>

#include <optional>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include <fstream>

#include "bot_general.h"

using namespace cq;
using namespace std;

using cq::utils::s2ws;
using cq::utils::ws2s;
using cq::utils::ansi;

namespace std::chrono {
using days = chrono::duration<int, ratio<86400>>;
}

class PcrTeamWarBot : IBot {
  public:
    PcrTeamWarBot(const FilterInput &input)
            : IBot(input),
              mDamageTable(*input.mGroupId),
              mEnable(false),
              mNGTime(1),
              mBossId(1),
              mBossHp(6000000),
              mBossMaxHp({6000000, 8000000, 10000000, 12000000, 20000000}) {
        mDamageTable.loadFromFile();
    }
    bool isEnable() const {
        return mEnable;
    }
    string botEnable() {
        mEnable = true;
        return "pcrbot开启！";
    }
    string botDisable() {
        mEnable = false;
        return "pcrbot关闭！";
    }
    bool isEnableSuperAttack() const {
        return mSuperAttack;
    }
    string botEnableSuperAttack() {
        mSuperAttack = true;
        return "大合刀模式启动！";
    }
    string botDisableSuperAttack() {
        mSuperAttack = false;
        return "大合刀模式关闭！";
    }
    string initBossHp(string msg) {//boss录入
        regex splitRegex("\\s+");
        mBossMaxHp.clear();
        string message = "Boss血量录入成功\n";
        for (auto result = ++sregex_token_iterator(msg.begin(), msg.end(), splitRegex, -1);
             result != sregex_token_iterator();
             result++) {
            message += *result;
            message += " ";
            mBossMaxHp.emplace_back(stoi(*result) * 10000);
        }
        mNGTime = 1;
        mBossId = 1;
        mBossHp = mBossMaxHp[0];
        return message;
    }
    string showNowBoss() const { //查看
        return "当前" + to_string(mNGTime) + "周目，" + to_string(mBossId) + "号boss生命值" + to_string(mBossHp);
    }
    string correctedBoss(string msg) { // 修正
        regex splitRegex("\\s+");
        string message = "Boss修正成功\n";
        auto result = ++sregex_token_iterator(msg.begin(), msg.end(), splitRegex, -1);
        mNGTime = stoi(*result++);
        mBossId = stoi(*result++);
        mBossHp = stoi(*result++);
        message += showNowBoss();
        return message;
    }
    string applyAttackBoss(int64_t user_id, string user_name) { //申请出刀
        string attackerMessage = "正在挑战Boss的人员名单：\n";
        for (auto &attacker : mAttacker) {
            attackerMessage += attacker.second + "\n";
        }
        if (mSuperAttack) {
            if (mAttacker.size() >= 30) {
                return attackerMessage + "正在挑战Boss人数已达到上限，请稍等！";
            }
        } else {
            if (mAttacker.size() >= 3) {
                return attackerMessage + "正在挑战Boss人数已达到上限，请稍等！";
            }
        }

        mAttacker.insert(make_pair(user_id, user_name));
        attackerMessage += user_name + "\n";
        attackerMessage += "当前出刀人数：" + to_string(mAttacker.size()) + "\n";
        return user_name + "已开始挑战Boss\n" + attackerMessage + showNowBoss();
    }
    optional<string> completeAttackBoss(string msg, int64_t user_id, const string &user_name) { // 完成击杀
        if (mAttacker.count(user_id) == 0) return nullopt;
        regex splitRegex("\\s+");
        auto result = ++sregex_token_iterator(msg.begin(), msg.end(), splitRegex, -1);
        int damage;
        if (*result == "击杀") {
            damage = mBossHp;
        } else {
            damage = stoi(*result);
        }
        if (!attack(damage)) {
            return "请正确输入伤害，如果已击杀请输入“完成 击杀”";
        }
        mDamageTable.setName(user_id, user_name);
        mDamageTable.insertDamage(user_id, damage);
        string message = user_name + "已完成挑战Boss\n";
        mAttacker.erase(user_id);
        mDamageTable.saveToFile();
        return message + showNowBoss();
    }
    optional<string> recallAttack(int64_t user_id, string user_name) {
        if(mAttacker.count(user_id)) return "撤回失败，请先完成当前的出刀再进行撤回操作";
        mDamageTable.setName(user_id, user_name);
        optional<int> damage = mDamageTable.popBackDamage(user_id);
        if (damage == nullopt) return nullopt;
        attack(-*damage);
        mDamageTable.saveToFile();
        return "撤回最后一次出刀成功\n" + showNowBoss();
    }
    string showAllDamage() {
        string message = "成员出刀伤害表\n";
        for (auto &i : mDamageTable.getTable()) {
            auto &user = i.second;
            message += user.first + " 出刀数： " + to_string(user.second.size()) + "伤害： ";
            int sumDamage = 0;
            for (auto damage : user.second) {
                message += to_string(damage) + "\t";
                sumDamage += damage;
            }
            message += "总伤害： " + to_string(sumDamage) + "\n";
        }
        return message;
    }

  private:
    bool attack(int damage) {
        if (damage > mBossHp) return false;
        mBossHp -= damage;
        if (mBossHp == 0) {
            mBossId++;
            if (mBossId > mBossMaxHp.size()) {
                mNGTime++;
                mBossId = 1;
            }
            mBossHp = mBossMaxHp[mBossId - 1];
        }
        if (mBossHp > mBossMaxHp[mBossId - 1]) {
            mBossHp = mBossHp - mBossMaxHp[mBossId - 1];
            mBossId--;
            if (mBossId == 0) {
                mNGTime--;
                mBossId = 5;
            }
        }
        return true;
    }

    bool mEnable;
    bool mSuperAttack;
    map<int64_t, string> mAttacker{};
    int mNGTime;//周目数
    int mBossId;
    int mBossHp;
    vector<int> mBossMaxHp;
    class DamageTable {
      public:
        DamageTable(int64_t groupId) {
            mGroupId = groupId;
        }
        const map<int64_t, pair<string, vector<int>>> &getTable() const {
            return mTable;
        }
        optional<string> getName(int64_t userId) const {
            if (mTable.count(userId) == 0)
                return nullopt;
            return mTable.at(userId).first;
        }
        void setName(int64_t userId, string userName) {
            mTable[userId].first = userName;
        }
        void insertDamage(int64_t userId, int damage) {
            mTable[userId].second.emplace_back(damage);
        }
        optional<int> popBackDamage(int64_t userId) {
            if (mTable[userId].second.size() == 0)return nullopt;
            int damage = mTable[userId].second.back();
            mTable[userId].second.pop_back();
            return damage;
        }
        void saveToFile() {
            auto now = chrono::system_clock::now();
            auto hours = (chrono::duration_cast<chrono::hours>(now.time_since_epoch()).count() + 8) % 24;
            if (hours < 5) {
                now -= chrono::days(1);
            }
            time_t now_time_t = chrono::system_clock::to_time_t(now);
            std::tm buf;
            localtime_s(&buf, &now_time_t);
            string date;
            {
                stringstream timestream;
                timestream << put_time(&buf, "%Y-%m-%d");
                timestream >> date;
            }
            _mkdir(("./" + to_string(mGroupId)).c_str());
            ofstream outFile(s2ws("./" + to_string(mGroupId) + "/" + date + "出刀结果.csv"));
            for (auto &row : mTable) {
                outFile << row.first << "," << row.second.first;
                auto &damages = row.second.second;
                for (auto &damage:damages) {
                    outFile << " ," << damage;
                }
                outFile << "\n";
            }
        }
        void loadFromFile() {
            auto now = chrono::system_clock::now();
            auto hours = (chrono::duration_cast<chrono::hours>(now.time_since_epoch()).count() + 8) % 24;
            if (hours < 5) {
                now -= chrono::days(1);
            }
            time_t now_time_t = chrono::system_clock::to_time_t(now);
            std::tm buf;
            localtime_s(&buf, &now_time_t);
            string date;
            {
                stringstream timestream;
                timestream << put_time(&buf, "%Y-%m-%d");
                timestream >> date;
            }
            ifstream inFile(s2ws("./" + to_string(mGroupId) + "/" + date + "出刀结果.csv"));
            if (inFile.is_open()) {
                if (!mTable.empty())return;
                mTable.clear();
                for (std::string line; getline(inFile, line);) {
                    stringstream lineStream(line);
                    string sUserId;
                    getline(lineStream, sUserId, ',');
                    int64_t userId = stoll(sUserId);
                    getline(lineStream, mTable[userId].first, ',');
                    for (string damage; getline(lineStream, damage, ',');) {
                        mTable[userId].second.emplace_back(stoi(damage));
                    }
                }
            }
        }
      private:
        int64_t mGroupId;
        map<int64_t, pair<string, vector<int>>> mTable;//QQ号->群名片->伤害
    } mDamageTable;
};

using PcrTeamWarBotFactory = BotFactory<PcrTeamWarBot>;
#endif //PCRBOT_SRC_BOTS_PRC7_BOT_HPP_
