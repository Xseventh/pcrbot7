#include <iostream>
#include <set>
#include <sstream>
#include <regex>
#include <cstdio>
#include <utility>
#include <vector>
#include <string>

#include <cqcppsdk/cqcppsdk.h>
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <direct.h>

using namespace cq;
using namespace std;
using Message = cq::message::Message;
using MessageSegment = cq::message::MessageSegment;

namespace std::chrono {
using days = chrono::duration<int, ratio<86400>>;
}

using cq::utils::s2ws;
using cq::utils::ws2s;
using cq::utils::ansi;

#define GM_ID 3024447565
#define GROUP_ID 730713030
class PcrTeamWar {
  public:
    PcrTeamWar()
            : mEnable(false),
              mAttackerId(nullopt),
              mAttackerName(nullopt),
              mNGTime(1),
              mBossId(1),
              mBossHp(6000000),
              mBossMaxHp({6000000, 8000000, 10000000, 12000000, 20000000}) {
    }
    void setGroupId(int64_t groupId) {
        mDamageTable.setGroupId(groupId);
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
        if (mAttackerId != nullopt) return *mAttackerName + "正在挑战Boss，请稍等！";
        mAttackerId = user_id;
        mAttackerName = user_name;
        return *mAttackerName + "已开始挑战Boss\n" + showNowBoss();
    }
    optional<string> completeAttackBoss(string msg, int64_t user_id, const string &user_name) { // 完成击杀
        if (user_id != *mAttackerId) return nullopt;
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
        mAttackerId.reset();
        mAttackerName.reset();
        mDamageTable.saveToFile();
        return message + showNowBoss();
    }
    optional<string> recallAttack(int64_t user_id, string user_name) {
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
    optional<int64_t> mAttackerId;
    optional<string> mAttackerName;
    int mNGTime;//周目数
    int mBossId;
    int mBossHp;
    vector<int> mBossMaxHp;
    class DamageTable {
      public:
        void setGroupId(int64_t groupId) {
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
        int mGroupId;
        map<int64_t, pair<string, vector<int>>> mTable;//QQ号->群名片->伤害
    } mDamageTable;
};
set<int64_t> GROUP_GM_ID{GM_ID}; // 这个是私聊用的，只识别GM账号的私聊
map<int64_t, PcrTeamWar> TEAM_WAR_BOT; //每个群号映射一个会战bot类
set<int64_t> ENABLED_GROUPS = {GROUP_ID, 863984185}; // 暂时没用

struct FilterInput {
    optional<int64_t> mGroupId;
    optional<int64_t> mUserId;
    optional<string> mUserName;
    optional<string> mMessage;
    optional<GroupRole> mUserRole;
};
struct Filter {
    using FilterFunction = function<optional<string>(FilterInput, smatch)>;
    Filter(string _regex_string, FilterFunction _func)
            : regex_string(std::move(_regex_string)), func(std::move(_func)) {
    }
    string regex_string;
    FilterFunction func;
};
vector<Filter> GroupFilters; // qq群消息经过这个filter后分发到对应函数

/*
  每个群一个独立的bot
  bot会分出管理员操作和成员操作


*/

CQ_INIT {
    on_enable([] {
#pragma region Group_Function_Register
        GroupFilters.emplace_back("\\s*开启bot\\s*",
                                  [](const FilterInput &input, const smatch &result) -> optional<string> {
                                      auto &bot = TEAM_WAR_BOT[*input.mGroupId];
                                      if ((input.mUserRole == GroupRole::MEMBER
                                              && GROUP_GM_ID.count(*input.mUserId) == 0) || bot.isEnable())
                                          return nullopt;
                                      return bot.botEnable();
                                  });
        GroupFilters.emplace_back("\\s*关闭bot\\s*",
                                  [](const FilterInput &input, const smatch &result) -> optional<string> {
                                      auto &bot = TEAM_WAR_BOT[*input.mGroupId];
                                      if ((input.mUserRole == GroupRole::MEMBER
                                              && GROUP_GM_ID.count(*input.mUserId) == 0) || !bot.isEnable())
                                          return nullopt;
                                      return bot.botDisable();
                                  });
        GroupFilters.emplace_back("\\s*((boss)?录入(\\s+[[:alnum:]]{1,9})+)\\s*",
                                  [](const FilterInput &input, const smatch &result) -> optional<string> {
                                      auto &bot = TEAM_WAR_BOT[*input.mGroupId];
                                      if (!bot.isEnable()) return nullopt;
                                      auto &msg = result[1];
                                      if (input.mUserRole == GroupRole::MEMBER) return nullopt;
                                      return bot.initBossHp(msg);
                                  });
        GroupFilters.emplace_back("\\s*(boss)?查看\\s*",
                                  [](const FilterInput &input, const smatch &result) -> optional<string> {
                                      auto &bot = TEAM_WAR_BOT[*input.mGroupId];
                                      if (!bot.isEnable()) return nullopt;
                                      return bot.showNowBoss();
                                  });
        GroupFilters.emplace_back("\\s*((boss)?修正(\\s+[[:alnum:]]{1,9}){3,3})\\s*",
                                  [](const FilterInput &input, const smatch &result) -> optional<string> {
                                      auto &bot = TEAM_WAR_BOT[*input.mGroupId];
                                      if (!bot.isEnable()) return nullopt;
                                      auto &msg = result[1];
                                      if (input.mUserRole == GroupRole::MEMBER) return nullopt;
                                      return bot.correctedBoss(msg);
                                  });
        GroupFilters.emplace_back("\\s*申请出刀\\s*",
                                  [](const FilterInput &input, const smatch &result) -> optional<string> {
                                      auto &bot = TEAM_WAR_BOT[*input.mGroupId];
                                      if (!bot.isEnable()) return nullopt;
                                      return bot.applyAttackBoss(*input.mUserId, *input.mUserName);
                                  });
        GroupFilters.emplace_back("\\s*(完成\\s+(击杀|[[:alnum:]]{1,9}))\\s*",
                                  [](const FilterInput &input, const smatch &result) -> optional<string> {
                                      auto &bot = TEAM_WAR_BOT[*input.mGroupId];
                                      if (!bot.isEnable()) return nullopt;
                                      auto &msg = result[1];
                                      return bot.completeAttackBoss(msg, *input.mUserId, *input.mUserName);
                                  });
        GroupFilters.emplace_back("\\s*撤回出刀\\s*",
                                  [](const FilterInput &input, const smatch &result) -> optional<string> {
                                      auto &bot = TEAM_WAR_BOT[*input.mGroupId];
                                      if (!bot.isEnable()) return nullopt;
                                      return bot.recallAttack(*input.mUserId, *input.mUserName);
                                  });
        GroupFilters.emplace_back("\\s*伤害查看\\s*",
                                  [](const FilterInput &input, const smatch &result) -> optional<string> {
                                      auto &bot = TEAM_WAR_BOT[*input.mGroupId];
                                      if (!bot.isEnable()) return nullopt;
                                      return bot.showAllDamage();
                                  });
        GroupFilters.emplace_back("\\s*指令详情\\s*",
                                  [](const FilterInput &input, const smatch &result) -> optional<string> {
                                      return string()
                                              + "可用指令列表如下：\n"
                                              + "开启bot(需管理员权限)\n"
                                              + "关闭bot(需管理员权限)\n"
                                              + "boss录入(需管理员权限)\n"
                                              + "修正(需管理员权限)\n"
                                              + "查看\n"
                                              + "申请出刀\n"
                                              + "完成\n"
                                              + "撤回出刀\n"
                                              + "伤害查看\n";
                                  });
#pragma endregion Group_Function_Register
        logging::info("初始化", "插件初始化完成");
        logging::info("启用", "插件已启用");
    });

    on_private_message([](const PrivateMessageEvent &event) {
        try {
            if (event.user_id == GM_ID) {
                const string &message = event.message;
                if (regex_match(message, regex("添加管理\\s*[[:alnum:]]{5,12}\\s*"))) {
                    int64_t id;
                    sscanf(message.c_str(), "添加管理%lld", &id);
                    if (GROUP_GM_ID.find(id) != GROUP_GM_ID.end()) {
                        send_message(event.target, "添加管理" + to_string(id) + "失败(该账号已经是管理)");
                    } else {
                        GROUP_GM_ID.insert(id);
                        send_message(event.target, "添加管理" + to_string(id) + "成功");
                    }
                } else if (regex_match(message, regex("查看管理"))) {
                    string response = "管理员列表如下\n";
                    for (auto id : GROUP_GM_ID)
                        response += to_string(id) + "\n";
                    send_message(event.target, response);
                } else if (regex_match(message, regex("删除管理\\s*[[:alnum:]]{5,12}\\s*"))) {
                    int64_t id;
                    sscanf(message.c_str(), "删除管理%lld", &id);
                    if (GROUP_GM_ID.find(id) != GROUP_GM_ID.end()) {
                        GROUP_GM_ID.erase(id);
                        send_message(event.target, "删除管理" + to_string(id) + "成功");
                    } else {
                        send_message(event.target, "删除管理" + to_string(id) + "失败(该账号不是管理)");
                    }
                } else if (regex_match(message, regex("添加群\\s*[[:alnum:]]{5,12}\\s*"))) {
                    int64_t id;
                    sscanf(message.c_str(), "添加群%lld", &id);
                    if (ENABLED_GROUPS.find(id) != ENABLED_GROUPS.end()) {
                        send_message(event.target, "添加群" + to_string(id) + "失败(该群已添加)");
                    } else {
                        ENABLED_GROUPS.insert(id);
                        send_message(event.target, "添加群" + to_string(id) + "成功");
                    }
                } else if (regex_match(message, regex("查看群"))) {
                    string response = "支持的群列表如下\n";
                    for (auto id : ENABLED_GROUPS) response += to_string(id) + "\n";
                    send_message(event.target, response);
                } else if (regex_match(message, regex("删除群\\s*[[:alnum:]]{5,12}\\s*"))) {
                    int64_t id;
                    sscanf(message.c_str(), "删除群%lld", &id);
                    if (ENABLED_GROUPS.find(id) != ENABLED_GROUPS.end()) {
                        ENABLED_GROUPS.erase(id);
                        send_message(event.target, "删除群" + to_string(id) + "成功");
                    } else {
                        send_message(event.target, "删除群" + to_string(id) + "失败(该群未添加)");
                    }
                }
            }
        } catch (ApiError &err) {
            logging::warning("私聊", "私聊消息复读失败, 错误码: " + to_string(err.code));
        }
    });

    on_group_message([](const GroupMessageEvent &event) {
        if (ENABLED_GROUPS.count(event.group_id) == 0) return; // 不在启用的群中, 忽略
        try {
            FilterInput input;
            {
                if (event.target.user_id == nullopt) {
                    logging::warning("群聊", "消息不存在用户Id, 消息内容: " + event.message);
                    return;
                }
                input.mMessage = event.message;
                input.mUserId = event.target.user_id;
                input.mGroupId = event.group_id;
                {
                    auto mem_list = get_group_member_list(event.group_id); // 获取群成员列表
                    for (auto &mem : mem_list) {
                        if (mem.user_id == *input.mUserId) {
                            input.mUserName = mem.card.empty() ? mem.nickname : mem.card;
                            input.mUserRole = mem.role;
                            break;
                        }
                    }
                }
                if (input.mUserName == nullopt) {
                    logging::warning("群聊", "消息不存在用户名, 消息内容: " + event.message);
                    return;
                }
            }
            // 以上为input初始化

            // 这里苟一下把groupid传进去
            TEAM_WAR_BOT[*input.mGroupId].setGroupId(*input.mGroupId);
            //这里遍历一下所有的function
            optional<string> msg;
            for (const auto &filter : GroupFilters) {
                smatch result;
                if (regex_match(event.message, result, regex(filter.regex_string, regex_constants::icase))) {
                    // filter后调用
                    msg = filter.func(input, result);
                    break;
                }
            }
            if (msg != nullopt)
                send_group_message(event.group_id, *msg); // 发送群消息
        } catch (ApiError &e) {
            logging::error("群聊", e.what());
        }
        if (event.is_anonymous()) {
            logging::info("群聊", "消息是匿名消息, 匿名昵称: " + event.anonymous.name);
        }
        event.block(); // 阻止当前事件传递到下一个插件
    });
}

