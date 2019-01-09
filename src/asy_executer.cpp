#include "asy_executer.h"
#include "asy_scheduler.h"
#include "asy_coroutine.h"
#include "asy_timer.h"
#include "asy_poller.h"
#include "asy_context.h"
#include "asy_override.h"

ASY_ORIGIN_DEF(pthread_create);
ASY_ORIGIN_DEF(pthread_join);

using namespace asy;
using namespace std::chrono_literals;

static void* start_routine(void* arg) {
    auto inst = (executer*)arg;
    inst->on_exec();
    return nullptr;
}

void executer::run(scheduler* sche) {
    _sche = sche;
    ASY_ORIGIN(pthread_create)(&_thread, nullptr, start_routine, this);
}

void executer::join() {
    ASY_ORIGIN(pthread_join)(_thread, nullptr);
}

void executer::on_exec() {
    coroutine self;
    self.init();

    timer timer;

    poller poller;
    poller.init();

    auto ctx = init_context();
    ctx->self = &self;
    ctx->timer = &timer;
    ctx->poller = &poller;

    std::list<std::shared_ptr<coroutine>> coroutine_list;

    while (_sche->is_running()) {
        //auto tp_begin = std::chrono::steady_clock::now();
        
        auto it = coroutine_list.begin();
        auto it_end = coroutine_list.end();
        while (it != it_end) {
            auto& co = *it;
            if (co->status() == COROUTINE_DEAD) {
                coroutine_list.erase(it++);
            } else {
                ++it;
            }
        }

        timer.tick();

        auto co = _sche->pop_coroutine();
        if (co) {
            co->init();
            co->resume();
            coroutine_list.push_back(co);
        }

        poller.wait(10'000'000);
    }
}