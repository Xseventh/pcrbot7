//
// Created by seventh on 2020/6/5.
//

#ifndef PCRBOT_SRC_FILTER_HPP_
#define PCRBOT_SRC_FILTER_HPP_
#include <cqcppsdk/cqcppsdk.h>

#include <optional>
#include <string>
using namespace cq;
using namespace std;

struct FilterInput {
    optional<int64_t> mGroupId;
    optional<int64_t> mUserId;
    optional<string> mUserName;
    optional<string> mMessage;
    std::vector<GroupMember> mGroupMember;
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
#endif //PCRBOT_SRC_FILTER_HPP_
