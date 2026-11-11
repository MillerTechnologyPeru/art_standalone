/*
 * libprocessgroup for Mead: Android's cgroup policies for its processes and
 * threads (foreground, background, top app, cpusets, per-app memory
 * groups), which don't exist off Android. Policies set are recorded, so
 * that they read back as set, and not applied; process groups are just
 * processes.
 */

#include <processgroup/processgroup.h>
#include <processgroup/sched_policy.h>

#include <signal.h>
#include <sys/types.h>

#include <map>
#include <mutex>

namespace {

std::mutex gLock;
std::map<int, SchedPolicy> gPolicies;

} // namespace

bool cpusets_enabled() {
    return false;
}

bool schedboost_enabled() {
    return false;
}

int set_cpuset_policy(int tid, SchedPolicy policy) {
    return set_sched_policy(tid, policy);
}

int set_sched_policy(int tid, SchedPolicy policy) {
    std::lock_guard<std::mutex> lock(gLock);
    gPolicies[tid == 0 ? gettid() : tid] = policy;
    return 0;
}

int get_sched_policy(int tid, SchedPolicy* policy) {
    std::lock_guard<std::mutex> lock(gLock);
    auto found = gPolicies.find(tid == 0 ? gettid() : tid);
    *policy = found != gPolicies.end() ? found->second : SP_FOREGROUND;
    return 0;
}

const char* get_sched_policy_name(SchedPolicy policy) {
    static const char* const names[] = {"bg", "fg", "  ", "aa", "as", "ta", "rt", "rs"};
    return policy >= 0 && policy < SP_CNT ? names[policy] : "error";
}

bool CgroupGetControllerPath(const std::string&, std::string*) {
    return false;
}

bool CgroupGetAttributePath(const std::string&, std::string*) {
    return false;
}

bool CgroupGetAttributePathForTask(const std::string&, int, std::string*) {
    return false;
}

bool SetTaskProfiles(int, const std::vector<std::string>&, bool) {
    return true;
}

bool SetProcessProfiles(uid_t, pid_t, const std::vector<std::string>&, bool) {
    return true;
}

bool UsePerAppMemcg() {
    return false;
}

int killProcessGroup(uid_t, int initialPid, int signal) {
    return kill(initialPid, signal);
}

int killProcessGroupOnce(uid_t uid, int initialPid, int signal) {
    return killProcessGroup(uid, initialPid, signal);
}

int createProcessGroup(uid_t, int, bool) {
    return 0;
}

bool setProcessGroupSwappiness(uid_t, int, int) {
    return false;
}

bool setProcessGroupSoftLimit(uid_t, int, int64_t) {
    return false;
}

bool setProcessGroupLimit(uid_t, int, int64_t) {
    return false;
}

void removeAllProcessGroups() {}
