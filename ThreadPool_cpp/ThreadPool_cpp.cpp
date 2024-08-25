// threadpool.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "threadpool.h"
#include "MyLog.h"

using namespace myLog;
int sum(int a, int b)
{
    std::this_thread::sleep_for(std::chrono::seconds(3));
    return a + b;
}

int main()

{
#if 1
    EnableFile();

    {
        ThreadPool pool;
        pool.setPoolMode(POOLMODE::CACHED);
        pool.startPool(2);
        std::future<int> res1 = pool.submitTask(sum, 1, 20);
        std::future<int> res2 = pool.submitTask(sum, 1, 20);
        std::future<int> res3 = pool.submitTask(sum, 1, 20);
        std::future<int> res4 = pool.submitTask(sum, 1, 20);
        std::future<int> res5 = pool.submitTask(sum, 1, 20);
        std::future<int> res6 = pool.submitTask(sum, 1, 50);


        std::cout << "-----" << res1.get() << "-----" << std::endl;
        std::cout << "-----" << res2.get() << "-----" << std::endl;
        std::cout << "-----" << res3.get() << "-----" << std::endl;
        std::cout << "-----" << res4.get() << "-----" << std::endl;
        std::cout << "-----" << res5.get() << "-----" << std::endl;
        std::cout << "-----" << res6.get() << "-----" << std::endl;
    }
    int a;
    std::cin >> a;
#endif

#if 0
    //fixed模式线程池测试
    {
        ThreadPool pool;
        pool.startPool();
        std::future<int> res1 = pool.submitTask(sum, 1, 20);
        std::future<int> res2 = pool.submitTask(sum, 1, 20);
        std::future<int> res3 = pool.submitTask(sum, 1, 20);
        std::future<int> res4 = pool.submitTask(sum, 1, 20);
        std::future<int> res5 = pool.submitTask(sum, 1, 20);
        std::future<int> res6 = pool.submitTask(sum, 1, 50);


        std::cout << "-----" << res1.get() << "-----" << std::endl;
        std::cout << "-----" << res2.get() << "-----" << std::endl;
        std::cout << "-----" << res3.get() << "-----" << std::endl;
        std::cout << "-----" << res4.get() << "-----" << std::endl;
        std::cout << "-----" << res5.get() << "-----" << std::endl;
        std::cout << "-----" << res6.get() << "-----" << std::endl;
    }
    int a;
    std::cin >> a;
#endif // 0

    return 0;
}
