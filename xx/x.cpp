#include <stdio.h>
#include <cstdint>
#include <map>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <iostream>
#include <cassert>
#include <unistd.h>
#include <chrono>

using namespace std;
using namespace std::chrono;

//UID
//TypeId


#define	MAX_PAGE_NUM 20
#define	MAX_ITEM_IN_PAGE 50



#define	ITEM_UNREAD  0			/*  */
#define	ITEM_READ    1			/*  */
#define	ITEM_DEL     2			/*  */

int testTime =1000000;
int ThSize = 1;


enum {
ITEMTYPE_1,
ITEMTYPE_2,
ITEMTYPE_3,
};

#define	USER_REPORT 0
#define	AL_REPORT 1	


//CacheLRU 收缩阀值
//int init_partion_size= 100000; 

int init_partion_size= 100000;
size_t partion_limit= 100000;


std::atomic_int g_gen_rid;
std::atomic_int g_gen_id;//uid, aid

#define	gen_rid()  (++g_gen_rid)
#define	gen_uid()  (++g_gen_id)
#define	gen_aid()  (++g_gen_id)

inline uint64_t get_id() {
    return rand() % (partion_limit * 5);
}

int get_type() {
    if (rand() % 10 < 8)
        return USER_REPORT;

    return AL_REPORT;
}

struct Item {
    int64_t rid;
    uint8_t rtype;
    uint8_t ntype;
    uint8_t state;
};

struct ItemExt {
    Item item;
    uint64_t seq;
    int pos;
};

struct ItemPage {
    Item items[MAX_ITEM_IN_PAGE];
    uint64_t seq=0;
    uint32_t size=0;

};

struct Partion {
    uint64_t uid;
    int64_t seq;
    int size;
    int32_t head;
    int32_t tail;

    Partion *perv = nullptr;
    Partion *next = nullptr;

    Item itemqueu[MAX_PAGE_NUM * MAX_ITEM_IN_PAGE];
    bool pageFlag[MAX_PAGE_NUM]{};


    void fullItem() {
        size=0;

        int rate = rand() % 10;

        int pageNum;
        if (rate < 3) {
            pageNum = 2;
        } else {
            pageNum = MAX_PAGE_NUM;
        }

        int index=0;
        for (int p=0; p<pageNum; ++p) {
            for (int i=0; i<MAX_ITEM_IN_PAGE; ++i) {
                Item item;
                item.rid = gen_rid();
                item.state = ITEM_UNREAD;
                item.rtype = ITEMTYPE_1;
                item.ntype = 0;

                itemqueu[index++] = item;
                size=index;
            }
        }
        
        //生成20page
    }


};


struct CacheMgr {
    Partion *_head[2]{};
    Partion *_tail[2]{};
    map<uint64_t, Partion*> _cache[2];


    //两次锁，对RLU操作，对key,单个partion
    mutex _keylock;
    mutex _RLUlock[2];

    bool isExist(int32_t type, uint64_t id) 
    {
        std::lock_guard<std::mutex> __(_RLUlock[type]);

        return _cache[type].count(id) != 0;
    }


    
    size_t _partionCount{};
    int _outCut{};
    int _insertnewErr{};
    int _getItemCnt{};
    int _addItemCnt{};

    void showInfo() {
        printf("Cache user_size=%lu, al_size=%lu\n" 
               "回收数量=%d, insetNewErr=%d\n"
               "_getItemCnt=%d, addItems=%d\n" ,
               _cache[USER_REPORT].size(), _cache[AL_REPORT].size(),
               _outCut, _insertnewErr, _getItemCnt, _addItemCnt);
    }

    bool _LRUInsert(Partion *p, int type) {
        auto it = _cache[type].find(p->uid);
        if (it != _cache[type].end()) {
            ++_insertnewErr;
            return false; //已经存在
        }

        _cache[type].insert({p->uid, p});
        if (_head[type] == nullptr) {
            assert(_tail[type] == nullptr);
            _head[type] = _tail[type] = p;
            p->next = p->perv = nullptr;
        } else {
            assert(_tail[type] != nullptr);
            _head[type]->perv = p;
            p->next = _head[type];
            _head[type]=p;
            p->perv = nullptr;
        }
        return true;
    }


    void addItems(uint64_t uid, int type, ItemExt input[]) {
        std::lock_guard<std::mutex> _(_keylock);
        std::lock_guard<std::mutex> __(_RLUlock[type]);

        auto it = _cache[type].find(uid);
        Partion *p;
        if (it == _cache[type].end()) {
            p = _LRUout(type);

            assert(p != nullptr);
            _LRUInsert(p, type);
        } else 
            p = it->second;

        for (int i = 0; i < MAX_PAGE_NUM*MAX_ITEM_IN_PAGE; ++i) {
            p->itemqueu[i] = input[i].item;
        }

        ++_addItemCnt;
    }
    
    void getItems(uint64_t uid, int type, ItemExt input[]) {
        std::lock_guard<std::mutex> _(_keylock);
        std::lock_guard<std::mutex> __(_RLUlock[type]);
        ++_getItemCnt;

        auto it = _cache[type].find(uid);
        if (it == _cache[type].end())
            return;

        Partion *p = it->second;

        for (int i = 0; i < MAX_PAGE_NUM * MAX_ITEM_IN_PAGE; ++i) {
            input[i].item = p->itemqueu[i];
        }

        //update LRU move to first
        // leve list
        if (_head[type] == p)
            return;

        p->perv->next = p->next;

        if (_tail[type] == p) {
            _tail[type] = p->perv;
            assert(p->next == nullptr);
        } else {
            p->next->perv = p->perv;
        }

        //insert list
        _head[type]->perv = p;
        p->next = _head[type];
        p->perv = nullptr;
        _head[type] = p;


    }


    Partion* _LRUout(int type) {
        Partion *p = _tail[type];
        if (p == nullptr)
            return nullptr;

        _cache[type].erase(p->uid);

        assert(p->next==nullptr);
        if (_head[type] == p) {
            _head[type] = _tail[type]= nullptr;
            return p;
        }

        _tail[type] = p->perv;
        p->perv->next = nullptr;
        p->perv = nullptr;

        return p;
    }

    bool insertCache(Partion *p, int type) {
        assert(p != nullptr);

        std::lock_guard<std::mutex> _(_keylock);
        std::lock_guard<std::mutex> __(_RLUlock[type]);


        //check is out
        if (_partionCount > partion_limit) {
            Partion *out = _LRUout(type);
            delete(out);
            ++_outCut;
            --_partionCount;
        } 

        return _LRUInsert(p, type);
    }

    //int removeCache(Partion *p) {

    //}



};


CacheMgr * g_cache;

//数据预热
void initCache() {
    g_cache = new CacheMgr();
    int type;
    for (int i=0; i<100000; ++i) {
        Partion * p = new Partion();
        p->uid = gen_uid();
        p->fullItem();
        if (rand() % 10 <= 1)
            type = AL_REPORT;
        else
            type = USER_REPORT;

        g_cache->insertCache(p, type);
    }
    printf("预加载完成 数量为=%d\n", init_partion_size);
}

void worker() {
    //构造数据
    
    ItemExt input[MAX_PAGE_NUM * MAX_ITEM_IN_PAGE];

    auto p1 = std::chrono::system_clock::now();
    for (int i=0; i<testTime/ThSize; ++i) {
        uint64_t uid = get_id();
        int type = get_type();
        if (g_cache->isExist(type, uid)) {
            g_cache->getItems(uid, type, input);
        } else {
            g_cache->addItems(uid, type, input);
        }
    }

    auto p2 = std::chrono::system_clock::now();
    duration<double> time_span = duration_cast<duration<double>>(p2 - p1);
    std::cout << "thread" << std::this_thread::get_id() << "over " 
        << time_span.count() << " sec" << std::endl;

    //printf("input[1]=%d\n", input[3].pos);

}

//模拟 
// 1. 内存加载 50WItem,
// 2. 查询多线程，每个线程都有目标, 也可争夺另一个线程的任务
//   查询中90% 设置为user, 10%设置为al
//   read操作占70%, put 占30%
//   * 测LRU统计淘汰, 
//   * Item内写满回写
//   
// 3. 查看处理速度, 
// 4. 一不同线程模型测试
// * 陷入内核速度
// * 加锁解锁速度性能基准

//NOTE
//画出两种不同的线程模型，以及数据差异的原因

//水平扩展， 一致性哈希，虚拟节点
//一个在线玩家 patition 大小为16070 ~= 16k
// 加载
//内存消耗
// 1W ~= 160M
// 10W ~= 1.6G
// 100W ~= 16G
// 
//
// ~= 1k * 
// 
/*
//由于uid的锁太大，导致多核多线程下同时几乎只有一个线程能够拿到所，其他线程都在
//　挂起状态，而且全局内存锁的使用，内存屏障会导致cpu,无法使用,而且多核之间
// 会产生大量cacheMiss, 导致cpu性能无法高效工作
 
同样的100W插入，　
一个线程 1.44sec O0 O2 0.6esc
预加载完成 数量为=100000
thread140165650024192over 1.44327 sec
Cache user_size=79896, al_size=20104
回收数量=0, insetNewErr=0
_getItemCnt=34100, addItems=215900

real    0m2.925s
user    0m2.568s
sys     0m0.352s

2线程并行
O0 9sec, O2 3.5sec
预加载完成 数量为=100000
thread139627121067776over 8.87627 sec
thread139627129460480over 9.05083 sec
task all time9.05091 sec
Cache user_size=79896, al_size=20104
回收数量=0, insetNewErr=0
_getItemCnt=135982, addItems=864018

real    0m10.544s
user    0m10.188s
sys     0m2.516s

task all time3.5842 sec
Cache user_size=79896, al_size=20104
回收数量=0, insetNewErr=0
_getItemCnt=135984, addItems=864016

real    0m4.595s
user    0m4.608s
sys     0m1.644s


4线程
O0
预加载完成 数量为=100000
^Bkthread140280783554304over 9.21519 sec
thread140280758376192over 9.35373 sec
thread140280766768896over 9.40811 sec
thread140280775161600over 9.45845 sec
task all time9.4605 sec
Cache user_size=79896, al_size=20104
回收数量=0, insetNewErr=0
_getItemCnt=135984, addItems=864016

real    0m10.950s
user    0m10.664s
sys     0m2.584s

O2
thread140365678819072over 3.62396 sec
thread140365687211776over 3.76896 sec
thread140365695604480over 3.83638 sec
thread140365703997184over 3.91361 sec
task all time3.91422 sec
Cache user_size=79896, al_size=20104
回收数量=0, insetNewErr=0
_getItemCnt=135983, addItems=864017

real    0m4.949s
user    0m4.924s
sys     0m1.928s

8线程并行
100W 操作共消耗O0 10sec  O2 3.5esc
预加载完成 数量为=100000
thread140279081830144over 9.154 sec
thread140279056652032over 9.25427 sec
thread140279048259328over 9.3131 sec
thread140279031473920over 9.33132 sec
thread140279073437440over 9.35944 sec
thread140279039866624over 9.48927 sec
thread140279090222848over 9.54631 sec
thread140279065044736over 9.57392 sec
task all time9.57433 sec
Cache user_size=79896, al_size=20104
回收数量=0, insetNewErr=0
_getItemCnt=135983, addItems=864017

real    0m11.131s
user    0m11.060s
sys     0m2.516s


O2
thread139984228198144over 3.62536 sec
thread139984236590848over 3.73356 sec
thread139984244983552over 3.75196 sec
thread139984253376256over 3.88145 sec
task all time3.88153 sec
Cache user_size=79896, al_size=20104
回收数量=0, insetNewErr=0
_getItemCnt=135983, addItems=864017

real    0m4.895s
user    0m4.896s
sys     0m1.860s

*/

int main() {
    //init
    //数据预热
    printf("partion:%lu\n", sizeof(Partion));
    initCache();


    auto t1 = std::thread(worker);
    //auto t2 = std::thread(worker);
    //auto t3 = std::thread(worker);
    //auto t4 = std::thread(worker);
    //auto t5 = std::thread(worker);
    //auto t6 = std::thread(worker);
    //auto t7 = std::thread(worker);
    //auto t8 = std::thread(worker);

    auto p1 = std::chrono::system_clock::now();

    t1.join();
    //t2.join();
    //t3.join();
    //t4.join();
    //t5.join();
    //t6.join();
    //t7.join();
    //t8.join();

    auto p2 = std::chrono::system_clock::now();
    duration<double> time_span = duration_cast<duration<double>>(p2 - p1);
    std::cout << "task all time" << time_span.count() << " sec" << std::endl;

    g_cache->showInfo();

    // 开启worker线程

    
    //sleep(100);

}


