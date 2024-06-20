#ifndef _COROUTINE_H_
#define _COROUTINE_H_

#include <iostream>     // std::cout, std::cerr
#include <memory>       // std::shared_ptr
#include <atomic>       // std::atomic
#include <functional>   // std::function
#include <cassert>      // assert
#include <ucontext.h>   // ucontext_t, getcontext, setcontext, swapcontext, makecontext

// 公有继承 -> 继承自这个类的对象可以安全地生成一个指向自身的std::shared_ptr 
// 通过shared_from_this()方法获取调用对象的this指针的shared_ptr
class Fiber : public std::enable_shared_from_this<Fiber>
{
public:
	typedef std::shared_ptr<Fiber> ptr;

	// 协程共三种状态
	// READY <-> RUNNING -> TERM
	enum State
	{
		READY, // 刚创建或者yield之后的状态
		RUNNING, // resume之后的状态
		TERM //协程的回调函数执行完之后的状态
	};

private:
	//  默认构造函数只⽤于创建线程的第⼀个协程，也就是线程主函数对应的协程，
	//  这个协程只能由GetThis()⽅法调⽤，所以定义成私有⽅法
	Fiber();

public:
	// 构造函数，⽤于创建子协程
		// std::function<void()> -> 一个函数对象，接受0个参数且没有返回值
		// 可以存储任何符合这种签名的可调用对象，如普通函数、lambda 表达式、函数对象和成员函数
	// cb -> 协程的入口函数
	// stacksize -> 协程栈空间大小
	// scheduler -> 是否参与调度器调度
	Fiber(std::function<void()> cb, size_t stacksize = 0, bool run_in_scheduler = true);

	~Fiber() {};

	// 重置协程状态和入口函数，重用栈空间，不重新创建栈
	void reset(std::function<void()> cb);

	// 将当前协程切换到执行状态，和正在运行的协程进行交换
	void resume();

	// 将当前协程让出执行权，与上次yield退到后台的协程进行交换
	void yield();

	// 获取协程ID
	uint64_t getId() const {return m_id;}

	// 获取协程状态
	State getState() const {return m_state;}

public:
	// 类中的static成员函数
	// 它们可以在不创建类对象的情况下调用，并且不能访问类的非静态成员变量

	// 设置当前正在运行的协程，即设置t_fiber的值
	static void SetThis(Fiber *f);

	// 返回当前线程正在执行的协程
	// 如果还未创建协程则创建第一个协程作为主协程，负责协程调度
	// 其他协程结束时都要且回到主协程，由主协程选择新的协程进行resume
	// 应该首先执行该方法初始化主函数协程
	static std::shared_ptr<Fiber> GetThis();

	// 获取总协程数
	static uint64_t TotalFibers();

	// 协程入口函数
	static void MainFunc();

	// 获取当前协程id
	static uint64_t GetFiberId();

private:
	// “非静态数据成员初始化” -> 在类中初始化

	// 协程id
	uint64_t m_id = 0;
	// 协程栈大小
	// sylar采取的是独⽴栈的形式，每个协程都有⾃⼰固定⼤⼩的栈空间
	uint32_t m_stacksize = 0;
	// 协程状态
	State m_state = READY;
	// 协程上下文 -> 需要c++的一个额外标准库的支持
	ucontext_t m_ctx;
	// 协程栈地址
	void* m_stack = nullptr;
	// 协程入口函数
	// 数据类型为函数对象
	std::function<void()> m_cb;
	// 本协程是否参与调度器调度
	bool m_runInScheduler;
};

#endif