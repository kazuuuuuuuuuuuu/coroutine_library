#include "coroutine.h"
#include <iostream>

/*
// 上下⽂结构体定义
// 这个结构体是平台相关的，因为不同平台的寄存器不⼀样
// 下⾯列出的是所有平台都⾄少会包含的4个成员
typedef struct ucontext_t {
 // 当前上下⽂结束后，下⼀个激活的上下⽂对象的指针，只在当前上下⽂是由makecontext创建时有效
 struct ucontext_t *uc_link;
 // 当前上下⽂的信号屏蔽掩码
 sigset_t uc_sigmask;
 // 当前上下⽂使⽤的栈内存空间，只在当前上下⽂是由makecontext创建时有效
 stack_t uc_stack;
 // 平台相关的上下⽂具体内容，包含寄存器的值
 mcontext_t uc_mcontext;
 ...
} ucontext_t;
*/

// 获取当前的上下⽂
int getcontext(ucontext_t *ucp);

// 恢复ucp指向的上下⽂，
// 不会返回，⽽是会跳转到ucp上下⽂对应的函数中执⾏，相当于变相调⽤了函数
int setcontext(const ucontext_t *ucp);

// 修改由getcontext获取到的上下⽂指针ucp，将其与⼀个函数func进⾏绑定，指定func运⾏时的参数，
// 在调⽤makecontext之前，必须⼿动给ucp分配⼀段内存空间，存储在ucp->uc_stack中，作为func函数运⾏时的栈空间
// 同时也可以指定ucp->uc_link，表示函数运⾏结束后恢复uc_link指向的上下⽂，
// 如果不赋值uc_link，那func函数结束时必须调⽤setcontext或swapcontext以重新指定⼀个有效的上下⽂，否则程序就跑⻜了
// makecontext执⾏完后，ucp就与函数func绑定了，调⽤setcontext或swapcontext激活ucp时，func就会被运⾏
void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...);

// 恢复ucp指向的上下⽂，同时将当前的上下⽂存储到oucp中，
// 不返回，⽽是会跳转到ucp上下⽂对应的函数中执⾏，相当于调⽤了函数
// swapcontext是sylar⾮对称协程实现的关键，线程主协程和⼦协程⽤这个接⼝进⾏上下⽂切换
int swapcontext(ucontext_t *oucp, const ucontext_t *ucp);

// 线程局部变量
// 当前线程正在运行的协程
static thread_local Fiber* t_fiber = nullptr;

// 线程局部变量
// 当前线程的主协程
static thread_local std::shared_ptr<Fiber> t_thread_fiber = nullptr;

// 协程数量 
static thread_local std::atomic<int> s_fiber_count(0);

// 协程id
static thread_local std::atomic<int> s_fiber_id(0);


void Fiber::SetThis(Fiber *f)
{
	t_fiber = f;
}

// 返回当前线程正在执⾏的协程
std::shared_ptr<Fiber> Fiber::GetThis()
{
	if(t_fiber)
	{	// 返回this的shared_ptr
		return t_fiber->shared_from_this();
	}

	// 如果当前线程还未创建协程，则创建线程的主协程
	//std::shared_ptr<Fiber> main_fiber = std::make_shared<Fiber>();
	// Fiber()是私有 只能这么写
	std::shared_ptr<Fiber> main_fiber(new Fiber());

	// .get() -> 智能指针对象获取原始指针
	assert(t_fiber == main_fiber.get());
	// 主协程的智能指针赋值
	t_thread_fiber = main_fiber;
	
	return t_fiber->shared_from_this();
}

// 仅由GetThis方法调用，定义为私有方法
// 运行时首先调用GetThis
// 创建线程的主协程 -> 将负责协程调度
Fiber::Fiber()
{
	// 设置当前正在运行的协程，即t_fiber的值
	SetThis(this);
	m_state = RUNNING;

	if(getcontext(&m_ctx))
	{
		std::cerr << "Fiber() failed\n";
		exit(0);
	}

	s_fiber_count ++;
	m_id = s_fiber_id++;

	std::cout << "Fiber(): main id = " << m_id << std::endl;
}

// 创建线程的子协程
Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool run_in_scheduler):
m_id(s_fiber_id++), m_cb(cb)
{
	s_fiber_count ++;

	// 子协程分配运行时栈
	//m_stacksize = stacksize ? stacksize : g_fiber_stack_size->getValue();
	//m_stack = StackAllocator::Alloc(m_stacksize);
	m_stacksize = 128000;
	m_stack = malloc(m_stacksize);



	if(getcontext(&m_ctx))
	{
		std::cerr << "Fiber(std::function<void()> cb, size_t stacksize, bool run_in_scheduler) failed\n";
		exit(0);
	}
	// 设置子协程上下文
	m_ctx.uc_link = nullptr;
	m_ctx.uc_stack.ss_sp = m_stack;
	m_ctx.uc_stack.ss_size = m_stacksize;

	// 将协程上下文和函数入口（方法函数指针 -> 要加&）绑定
	makecontext(&m_ctx, &Fiber::MainFunc, 0);

	std::cout << "Fiber(): id = " << m_id << std::endl;
}



// 在⾮对称协程⾥，执⾏resume时的当前执⾏环境⼀定是位于线程主协程⾥
// 将子协程切换到执行状态，和主协程进行交换
void Fiber::resume()
{
	assert(m_state==READY);
	SetThis(this);
	m_state = RUNNING;

	// 切换
	if(swapcontext(&(t_thread_fiber->m_ctx), &m_ctx))
	{
		std::cerr << "resume() failed\n";
		exit(0);
	}
}

// ⽽执⾏yield时，当前执⾏环境⼀定是位于⼦协程⾥
// 将当前协程让出执行权，切换到主协程
void Fiber::yield()
{
	// 协程运⾏完之后会⾃动yield⼀次，⽤于回到主协程，此时状态已为结束状态
	assert(m_state==RUNNING || m_state==TERM);
	SetThis(t_thread_fiber.get());
	if(m_state!=TERM)
	{
		m_state = READY;
	}

	// 切换
	if(swapcontext(&m_ctx, &(t_thread_fiber->m_ctx)))
	{
		std::cerr << "yield() failed\n";
		exit(0);
	}	
}

// 协程入口函数的封装 -> 对比线程函数的封装
void Fiber::MainFunc()
{
	// 返回当前线程正在执行的协程 -> shared_from_this()使引用计数加1
	std::shared_ptr<Fiber> curr = GetThis();
	assert(curr!=nullptr);

	curr->m_cb(); // 调用真正的入口函数
	
	curr->m_cb = nullptr;
	curr->m_state = TERM;

	auto raw_ptr = curr.get();
	curr.reset(); // 在yield之前 手动关闭共享指针curr
	
	// 运行完毕yield
	raw_ptr->yield(); 
}

// 协程的重置，重置协程就是重复利⽤已结束的协程，复⽤其栈空间
void Fiber::reset(std::function<void()> cb)
{
	assert(m_stack != nullptr);
	// 为简化状态管理，强制只有TERM状态的协程才可以重置
	assert(m_state == TERM);
	m_cb = cb;

	if(getcontext(&m_ctx))
	{
		std::cerr << "reset() failed\n";
		exit(0);
	}
	m_ctx.uc_link = nullptr;
	m_ctx.uc_stack.ss_sp = m_stack;
	m_ctx.uc_stack.ss_size = m_stacksize;

	makecontext(&m_ctx, &Fiber::MainFunc, 0);
	m_state = READY;
}