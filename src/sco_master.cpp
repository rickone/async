#include "sco_master.h"
#include <vector>
#include <unistd.h>
#include <signal.h>
#include "sco_env.h"

using namespace sco;

static void on_quit(int sig) {
    master::inst()->quit(1);
}

master::master() : _master_co(std::bind(&master::main, this)) {
}

master* master::inst() {
    static master s_inst;
    return &s_inst;
}

void master::enter() {
    auto co = start_routine(nullptr);

    _master_co.init();
    co->swap(&_master_co);
}

void master::quit(int code) {
    _code = code;
    _startup = false;
}

void master::main() {
    signal(SIGINT, on_quit); // ctrl + c
    signal(SIGTERM, on_quit); // kill
    signal(SIGQUIT, on_quit); // ctrl + '\'
    signal(SIGCHLD, SIG_IGN);

    auto env = env::inst();
    env->init();

    int worker_num = env->get_env_int("SCO_WORKER_NUM");
    int cpu_num = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (worker_num <= 0) {
        worker_num = cpu_num;
    }

    std::vector<std::shared_ptr<worker>> workers;
    for (int i = 0; i < worker_num; i++) {
        workers.emplace_back(new worker());
    }

    _startup = true;
    for (int i = 1; i < worker_num; i++) {
        workers[i]->run_in_thread();
    }

    bool bind_cpu_core = env->get_env_bool("SCO_BIND_CPU_CORE");
    if (bind_cpu_core) {
        for (int i = 0; i < worker_num && i < cpu_num; i++) {
            workers[i]->bind_cpu_core(i);
        }
    }

    workers[0]->run(&_master_co);

    for (int i = 1; i < worker_num; i++) {
        workers[i]->join();
    }
    _exit(_code);
}

std::shared_ptr<routine> master::start_routine(const routine::func_t& func) {
    auto co = std::make_shared<routine>(func);
    _routines.push(co);
    return co;
}

std::shared_ptr<routine> master::pop_routine() {
    std::shared_ptr<routine> co;
    _routines.pop(co);
    return co;
}