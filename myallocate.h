#include<iostream>
#include <mutex>
using namespace std;
#define _ALIGN 8
#define _MAX_BYTES 128
#define _NFREELISTS 16

using size_t = unsigned int;

template<typename T>
class MyAllocate
{
public:	
	void construct(T* p, const T&val)//构造对象
	{
		new (p)T(val);
	}
	void destory(T* p)//析构对象
	{
		p->~T();
	}
	T* allocate(size_t _n)      //内存分配函数
	{
		_n = _n * sizeof(T);
		void* ret;
		if (_n > _MAX_BYTES)//大于128就交给一级管理
		{
			ret = malloc(_n);
			return (T*)ret;
		}
		std::lock_guard<std::mutex> guard(mtx);//线程安全考虑
		int num = _S_freelist_index(_n);
		Obj* volatile* __my_free_list = _S_free_list + num;//找到对应内存大小的链表
		Obj* result = (*__my_free_list);
		if (result != nullptr)
		{
			*__my_free_list = result->_M_free_list_link;//头删法取出一块内存
			ret = result;
			return (T*)ret;
		}
		else
		{
			ret = _S_refill(_S_round_up(_n));//若链表没有内存块
			return (T*)ret;
		}
	}
	void deallocate(void* _p, size_t _n)   //内存回收函数
	{
		_n = _n * sizeof(T);
		if (_n > _MAX_BYTES) delete(_p);//大于128交给一级管理
		Obj* volatile*__my_free_list = _S_free_list + _S_freelist_index(_n);
		std::lock_guard<std::mutex> guard(mtx);
		((Obj*)_p)->_M_free_list_link = *__my_free_list;//头插法回收p
		*__my_free_list = (Obj*)_p;
	}
	T* reallocate(void* __p, size_t __old_sz, size_t __new_sz)//扩容函数
	{
		if (__old_sz > (size_t)_MAX_BYTES && __new_sz > (size_t)_MAX_BYTES) { return(realloc(__p, __new_sz)); }
		if (_S_round_up(__old_sz) == _S_round_up(__new_sz)) return(__p);
		char* result = allocate(__new_sz);
		size_t __copy_sz = __new_sz > __old_sz ? __old_sz : __new_sz;
		memcpy(result, __p, __copy_sz);
		deallocate(__p, __old_sz);
		return(result);
	}
private:
	static char* _S_start_free;
	static char* _S_end_free;
	static size_t _S_heap_size;
	union Obj
	{
		union Obj* _M_free_list_link;    //空闲链表，相当于next域     
		char _M_client_data[1];
	};
	 static Obj* volatile _S_free_list[_NFREELISTS];
	 static std::mutex mtx;
	 char* _S_chunk_alloc(size_t __size, size_t& objs) //申请块内存
	 {
		 size_t total_bytes = __size * objs;//默认大小
		 size_t left_bytes = _S_end_free - _S_start_free;//备用空间大小
		 char* result;
		 if (left_bytes > total_bytes)
		 {
			 result = _S_start_free;
			 _S_start_free = _S_start_free + total_bytes;
			 return result;
		 }
		 else if (left_bytes > __size)//备用空间不够默认的大小
		 {
			 objs = left_bytes / __size;
			 result = _S_start_free;
			 _S_start_free = _S_start_free + objs * __size;
			 return result;
		 }
		 else//备用空间不足默认的一块大小
		 {
			 if (left_bytes > 0)//把备用空间回收
			 {
				 Obj*volatile* __my_free_list = _S_free_list + _S_freelist_index(left_bytes);
				 ((Obj*)_S_start_free)->_M_free_list_link = *__my_free_list;
				 *__my_free_list = (Obj*)_S_start_free;
			 }
			 size_t bytes_to_get = 2 * total_bytes + _S_round_up(_S_heap_size >> 4);
			 _S_start_free = (char*)malloc(bytes_to_get);//给备用空间申请空间
			 if (_S_start_free == nullptr)//malloc不到空间,去后面链表里找出一块内存当内存池
			 {
				 for (int i = __size + _ALIGN; i < _MAX_BYTES; i = i + _ALIGN)
				 {
					 Obj*volatile* __my_free_list = _S_free_list + _S_freelist_index(i);
					 Obj* tmp = *__my_free_list;
					 if (tmp != nullptr)    //向该链表借用一块当备用空间
					 {
						 *__my_free_list = tmp->_M_free_list_link;
						 _S_start_free = (char*)tmp;;
						 _S_end_free = _S_start_free + i;
						 return _S_chunk_alloc(__size, objs);//在一次递归去向备用空间要内存
					 }
				 }
				 _S_end_free = nullptr;
				 _S_start_free = (char*)malloc(bytes_to_get);//若链表没有块，交给一级空间配置器去申请空间（有预先设置好的回调函数帮我们释放不必要的内存）
			 }
			 _S_heap_size += bytes_to_get;
			 _S_end_free = _S_start_free + bytes_to_get;
			 return _S_chunk_alloc(__size, objs);//申请到空间了，再递归一次找到合适空间返回给_S_refill去分割空间。
		 }
	 }
	 void*_S_refill(size_t _n)   //给块内存分割大小，把大块分割成相同的小块内存，把链表链接起来挂到数组下
	 {
		 size_t nobjs = 2;
		 char* chunk = _S_chunk_alloc(_n, nobjs);
		 char* result;
		 if (nobjs == 1)
		 {
			 return chunk;
		 }
		 Obj* cur_objs, *next_objs;
		 Obj*volatile* __my_free_list = _S_free_list + _S_freelist_index(_n);
		 result = chunk;                                      //第一块当使用块
		 *__my_free_list = (Obj*)(chunk + _n);
		 next_objs = *__my_free_list;
		 for (int i = 1;; i++)
		 {
			 cur_objs = next_objs;
			 next_objs = (Obj*)((char*)next_objs + _n);
			 if (nobjs - 1 == 1)
			 {
				 cur_objs->_M_free_list_link = nullptr;//最后一块的后继为空
				 break;
			 }
			 else
			 {
				 cur_objs->_M_free_list_link = next_objs;
			 }
		 }
		 return result;
	 }
	 size_t _S_round_up(size_t __bytes)    //把_bytes调整到8的倍数上
	 {
		 return (((__bytes)+(size_t)_ALIGN - 1) & ~((size_t)_ALIGN - 1));
	 }
	 size_t _S_freelist_index(size_t __bytes)  //返回_byte所对应的数组下标，用来找出对应块所在下标
	 {
		 return (((__bytes)+(size_t)_ALIGN - 1) / (size_t)_ALIGN - 1);
	 }
};
template<typename T>
char* MyAllocate<T>::_S_start_free = nullptr;
template<typename T>
char* MyAllocate<T>::_S_end_free = nullptr;
template<typename T>
size_t MyAllocate<T>::_S_heap_size = 0;
template<typename T>
typename MyAllocate<T>::Obj* volatile MyAllocate<T>::_S_free_list[_NFREELISTS] =
{ nullptr,nullptr,nullptr ,nullptr ,nullptr ,nullptr ,nullptr ,nullptr ,nullptr ,nullptr ,nullptr ,nullptr ,nullptr ,nullptr ,nullptr ,nullptr  };
template <typename T>
std::mutex MyAllocate<T>::mtx;

