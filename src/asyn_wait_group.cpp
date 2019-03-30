#include "asyn_wait_group.h"
#include "asyn_panic.h"

using namespace asyn;

wait_group::wait_group() : _count(0) {
}

void wait_group::done() {
    _count.fetch_sub(1, std::memory_order_release);
}

void wait_group::wait() {
    auto cur_worker = worker::current();
    if (!cur_worker) {
        panic("!cur_worker");
    }

    while (true) {
        int count = _count.load(std::memory_order_consume);
        if (count == 0) {
            return;
        }

        printf("wg.count=%d\n", count);
        cur_worker->pause();
    }
}