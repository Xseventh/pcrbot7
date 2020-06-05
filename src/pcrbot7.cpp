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

#include "filter.hpp"
#include "bots/pcr7_bot.hpp"

using namespace cq;
using namespace std;
using Message = cq::message::Message;
using MessageSegment = cq::message::MessageSegment;


using cq::utils::s2ws;
using cq::utils::ws2s;
using cq::utils::ansi;

#define GM_ID 3024447565

set<int64_t> GROUP_GM_ID{GM_ID}; // 这个是私聊用的，只识别GM账号的私聊
set<int64_t> ENABLED_GROUPS = {730713030, 863984185, 930947146, 346573548}; // 支持的群号
PcrTeamWarBotFactory gPcrTeamWarBotFactory;
vector<Filter> gGroupFilters; // qq群消息经过这个filter后分发到对应函数

CQ_INIT {
    on_enable([] {
#pragma region Group_Function_Register
        gGroupFilters.emplace_back("\\s*开启bot\\s*",
                                   [](const FilterInput &input, const smatch &result) -> optional<string> {
                                       auto &bot = gPcrTeamWarBotFactory.getInstance(input);
                                       if ((input.mUserRole == GroupRole::MEMBER
                                               && GROUP_GM_ID.count(*input.mUserId) == 0) || bot.isEnable())
                                           return nullopt;
                                       return bot.botEnable();
                                   });
        gGroupFilters.emplace_back("\\s*关闭bot\\s*",
                                   [](const FilterInput &input, const smatch &result) -> optional<string> {
                                       auto &bot = gPcrTeamWarBotFactory.getInstance(input);
                                       if ((input.mUserRole == GroupRole::MEMBER
                                               && GROUP_GM_ID.count(*input.mUserId) == 0) || !bot.isEnable())
                                           return nullopt;
                                       return bot.botDisable();
                                   });
        gGroupFilters.emplace_back("\\s*((boss)?录入(\\s+[[:alnum:]]{1,9})+)\\s*",
                                   [](const FilterInput &input, const smatch &result) -> optional<string> {
                                       auto &bot = gPcrTeamWarBotFactory.getInstance(input);
                                       if (!bot.isEnable()) return nullopt;
                                       auto &msg = result[1];
                                       if ((input.mUserRole == GroupRole::MEMBER
                                               && GROUP_GM_ID.count(*input.mUserId) == 0))
                                           return nullopt;
                                       return bot.initBossHp(msg);
                                   });
        gGroupFilters.emplace_back("\\s*(boss)?查看\\s*",
                                   [](const FilterInput &input, const smatch &result) -> optional<string> {
                                       auto &bot = gPcrTeamWarBotFactory.getInstance(input);
                                       if (!bot.isEnable()) return nullopt;
                                       return bot.showNowBoss();
                                   });
        gGroupFilters.emplace_back("\\s*((boss)?修正(\\s+[[:alnum:]]{1,9}){3,3})\\s*",
                                   [](const FilterInput &input, const smatch &result) -> optional<string> {
                                       auto &bot = gPcrTeamWarBotFactory.getInstance(input);
                                       if (!bot.isEnable()) return nullopt;
                                       auto &msg = result[1];
                                       if ((input.mUserRole == GroupRole::MEMBER
                                               && GROUP_GM_ID.count(*input.mUserId) == 0))
                                           return nullopt;
                                       return bot.correctedBoss(msg);
                                   });
        gGroupFilters.emplace_back("\\s*申请出刀\\s*",
                                   [](const FilterInput &input, const smatch &result) -> optional<string> {
                                       auto &bot = gPcrTeamWarBotFactory.getInstance(input);
                                       if (!bot.isEnable()) return nullopt;
                                       return bot.applyAttackBoss(*input.mUserId, *input.mUserName);
                                   });
        gGroupFilters.emplace_back("\\s*(完成\\s+(击杀|[[:alnum:]]{1,9}))\\s*",
                                   [](const FilterInput &input, const smatch &result) -> optional<string> {
                                       auto &bot = gPcrTeamWarBotFactory.getInstance(input);
                                       if (!bot.isEnable()) return nullopt;
                                       auto &msg = result[1];
                                       return bot.completeAttackBoss(msg, *input.mUserId, *input.mUserName);
                                   });
        gGroupFilters.emplace_back("\\s*撤回出刀\\s*",
                                   [](const FilterInput &input, const smatch &result) -> optional<string> {
                                       auto &bot = gPcrTeamWarBotFactory.getInstance(input);
                                       if (!bot.isEnable()) return nullopt;
                                       return bot.recallAttack(*input.mUserId, *input.mUserName);
                                   });
        gGroupFilters.emplace_back("\\s*伤害查看\\s*",
                                   [](const FilterInput &input, const smatch &result) -> optional<string> {
                                       auto &bot = gPcrTeamWarBotFactory.getInstance(input);
                                       if (!bot.isEnable()) return nullopt;
                                       return bot.showAllDamage();
                                   });
        gGroupFilters.emplace_back("\\s*大合刀模式开启\\s*",
                                   [](const FilterInput &input, const smatch &result) -> optional<string> {
                                       auto &bot = gPcrTeamWarBotFactory.getInstance(input);
                                       if ((input.mUserRole == GroupRole::MEMBER
                                               && GROUP_GM_ID.count(*input.mUserId) == 0) || bot.isEnableSuperAttack())
                                           return nullopt;
                                       return bot.botEnableSuperAttack();
                                   });
        gGroupFilters.emplace_back("\\s*大合刀模式关闭\\s*",
                                   [](const FilterInput &input, const smatch &result) -> optional<string> {
                                       auto &bot = gPcrTeamWarBotFactory.getInstance(input);
                                       if ((input.mUserRole == GroupRole::MEMBER
                                               && GROUP_GM_ID.count(*input.mUserId) == 0) || !bot.isEnableSuperAttack())
                                           return nullopt;
                                       return bot.botDisableSuperAttack();
                                   });
        gGroupFilters.emplace_back("\\s*指令详情\\s*",
                                   [](const FilterInput &input, const smatch &result) -> optional<string> {
                                       return string()
                                               + "可用指令列表如下：\n"
                                               + "开启bot(需管理员权限)\n"
                                               + "关闭bot(需管理员权限)\n"
                                               + "boss录入(需管理员权限)\n"
                                               + "修正(需管理员权限)\n"
                                               + "大合刀模式开启(需管理员权限)\n"
                                               + "大合刀模式关闭(需管理员权限)\n"
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
            if (event.user_id == 000000)
                return;
            //未实现
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
                    input.mGroupMember = get_group_member_list(event.group_id); // 获取群成员列表
                    for (auto &mem : input.mGroupMember) {
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

            //这里遍历一下所有的function
            optional<string> msg;
            for (const auto &filter : gGroupFilters) {
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

