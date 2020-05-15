#include <iostream>
#include <set>
#include <sstream>
#include <regex>
#include <cstdio>
#include <vector>

#include <cqcppsdk/cqcppsdk.h>
#include <Windows.h>

using namespace cq;
using namespace std;
using Message = cq::message::Message;
using MessageSegment = cq::message::MessageSegment;

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
    bool isEnable() {
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
    string showNowBoss() { //查看
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
    optional<string> completeAttackBoss(string msg, int64_t user_id, string user_name) { // 完成击杀
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
        mDamageTable[user_id].first = user_name;
        mDamageTable[user_id].second.emplace_back(damage);
        string message = user_name + "已完成挑战Boss\n";
        mAttackerId.reset();
        mAttackerName.reset();
        return message + showNowBoss();
    }
    optional<string> recallAttack(int64_t user_id, string user_name) {
        mDamageTable[user_id].first = user_name;
        if (mDamageTable[user_id].second.size() == 0) return nullopt;
        int damage = mDamageTable[user_id].second.back();
        mDamageTable[user_id].second.pop_back();
        attack(-damage);
        return "撤回最后一次出刀成功\n" + showNowBoss();
    }
    string showAllDamage() {
        string message = "成员出刀伤害表如下：\n";
        for (auto &i : mDamageTable) {
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
    map<int64_t, pair<string, vector<int>>> mDamageTable; //QQ号->群名片->伤害
};
set<int64_t> GROUP_GM_ID{GM_ID}; // 这个是私聊用的，只识别GM账号的私聊
map<int64_t, PcrTeamWar> TEAM_WAR_BOT; //每个群号映射一个会战bot类
set<int64_t> ENABLED_GROUPS = {GROUP_ID}; // 暂时没用

struct FilterInput {
    optional<int64_t> mGroupId;
    optional<int64_t> mUserId;
    optional<string> mUserName;
    optional<string> mMessage;
    optional<GroupRole> mUserRole;
};
using FilterFunction = function<optional<string>(FilterInput, smatch)>;
struct Filter {
    Filter(string _regex_string, FilterFunction _func)
            : regex_string(_regex_string), func(_func) {
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
        GroupFilters.emplace_back("\\s*开启bot\\s*",
                                  [](const FilterInput &input, const smatch &result) -> optional<string> {
                                      auto &bot = TEAM_WAR_BOT[*input.mGroupId];
                                      if (input.mUserRole == GroupRole::MEMBER || bot.isEnable()) return nullopt;
                                      return bot.botEnable();
                                  });
        GroupFilters.emplace_back("\\s*关闭bot\\s*",
                                  [](const FilterInput &input, const smatch &result) -> optional<string> {
                                      auto &bot = TEAM_WAR_BOT[*input.mGroupId];
                                      if (input.mUserRole == GroupRole::MEMBER || !bot.isEnable()) return nullopt;
                                      return bot.botDisable();
                                  });
        GroupFilters.emplace_back("\\s*((boss)?录入(\\s+[[:alnum:]]{1,9})+)\\s*",
                                  [](const FilterInput &input, const smatch &result) -> optional<string> {
                                      auto &bot = TEAM_WAR_BOT[*input.mGroupId];
                                      auto &msg = result[1];
                                      if (input.mUserRole == GroupRole::MEMBER) return nullopt;
                                      return bot.initBossHp(msg);
                                  });
        GroupFilters.emplace_back("\\s*(boss)?查看\\s*",
                                  [](const FilterInput &input, const smatch &result) -> optional<string> {
                                      auto &bot = TEAM_WAR_BOT[*input.mGroupId];
                                      return bot.showNowBoss();
                                  });
        GroupFilters.emplace_back("\\s*((boss)?修正(\\s+[[:alnum:]]{1,9}){3,3})\\s*",
                                  [](const FilterInput &input, const smatch &result) -> optional<string> {
                                      auto &bot = TEAM_WAR_BOT[*input.mGroupId];
                                      auto &msg = result[1];
                                      if (input.mUserRole == GroupRole::MEMBER) return nullopt;
                                      return bot.correctedBoss(msg);
                                  });
        GroupFilters.emplace_back("\\s*申请出刀\\s*",
                                  [](const FilterInput &input, const smatch &result) -> optional<string> {
                                      auto &bot = TEAM_WAR_BOT[*input.mGroupId];
                                      return bot.applyAttackBoss(*input.mUserId, *input.mUserName);
                                  });
        GroupFilters.emplace_back("\\s*(完成\\s+(击杀|[[:alnum:]]{1,9}))\\s*",
                                  [](const FilterInput &input, const smatch &result) -> optional<string> {
                                      auto &bot = TEAM_WAR_BOT[*input.mGroupId];
                                      auto &msg = result[1];
                                      return bot.completeAttackBoss(msg, *input.mUserId, *input.mUserName);
                                  });
        GroupFilters.emplace_back("\\s*撤回出刀\\s*",
                                  [](const FilterInput &input, const smatch &result) -> optional<string> {
                                      auto &bot = TEAM_WAR_BOT[*input.mGroupId];
                                      return bot.recallAttack(*input.mUserId, *input.mUserName);
                                  });
        GroupFilters.emplace_back("\\s*伤害查看\\s*",
                                  [](const FilterInput &input, const smatch &result) -> optional<string> {
                                      auto &bot = TEAM_WAR_BOT[*input.mGroupId];
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
                    for (auto i = 0; i < static_cast<int>(mem_list.size()); i++) {
                        if (mem_list[i].user_id == *input.mUserId) {
                            input.mUserName = mem_list[i].card == "" ? mem_list[i].nickname : mem_list[i].card;
                            input.mUserRole = mem_list[i].role;
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
            optional<string> msg;
            for (const auto &filter : GroupFilters) {
                smatch result;
                if (regex_match(event.message, result, regex(filter.regex_string, regex_constants::icase))) {
                    msg = filter.func(input, result);
                    break;
                }
            }
            // filter后调用
            if (msg != nullopt)
                send_group_message(event.group_id, *msg); // 发送群消息
        } catch (ApiError &) { // 忽略发送失败
        }
        if (event.is_anonymous()) {
            logging::info("群聊", "消息是匿名消息, 匿名昵称: " + event.anonymous.name);
        }
        event.block(); // 阻止当前事件传递到下一个插件
    });
}

