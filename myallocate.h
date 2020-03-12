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
	void construct(T* p, const T&val)//�������
	{
		new (p)T(val);
	}
	void destory(T* p)//��������
	{
		p->~T();
	}
	T* allocate(size_t _n)      //�ڴ���亯��
	{
		_n = _n * sizeof(T);
		void* ret;
		if (_n > _MAX_BYTES)//����128�ͽ���һ������
		{
			ret = malloc(_n);
			return (T*)ret;
		}
		std::lock_guard<std::mutex> guard(mtx);//�̰߳�ȫ����
		int num = _S_freelist_index(_n);
		Obj* volatile* __my_free_list = _S_free_list + num;//�ҵ���Ӧ�ڴ��С������
		Obj* result = (*__my_free_list);
		if (result != nullptr)
		{
			*__my_free_list = result->_M_free_list_link;//ͷɾ��ȡ��һ���ڴ�
			ret = result;
			return (T*)ret;
		}
		else
		{
			ret = _S_refill(_S_round_up(_n));//������û���ڴ��
			return (T*)ret;
		}
	}
	void deallocate(void* _p, size_t _n)   //�ڴ���պ���
	{
		_n = _n * sizeof(T);
		if (_n > _MAX_BYTES) delete(_p);//����128����һ������
		Obj* volatile*__my_free_list = _S_free_list + _S_freelist_index(_n);
		std::lock_guard<std::mutex> guard(mtx);
		((Obj*)_p)->_M_free_list_link = *__my_free_list;//ͷ�巨����p
		*__my_free_list = (Obj*)_p;
	}
	T* reallocate(void* __p, size_t __old_sz, size_t __new_sz)//���ݺ���
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
		union Obj* _M_free_list_link;    //���������൱��next��     
		char _M_client_data[1];
	};
	 static Obj* volatile _S_free_list[_NFREELISTS];
	 static std::mutex mtx;
	 char* _S_chunk_alloc(size_t __size, size_t& objs) //������ڴ�
	 {
		 size_t total_bytes = __size * objs;//Ĭ�ϴ�С
		 size_t left_bytes = _S_end_free - _S_start_free;//���ÿռ��С
		 char* result;
		 if (left_bytes > total_bytes)
		 {
			 result = _S_start_free;
			 _S_start_free = _S_start_free + total_bytes;
			 return result;
		 }
		 else if (left_bytes > __size)//���ÿռ䲻��Ĭ�ϵĴ�С
		 {
			 objs = left_bytes / __size;
			 result = _S_start_free;
			 _S_start_free = _S_start_free + objs * __size;
			 return result;
		 }
		 else//���ÿռ䲻��Ĭ�ϵ�һ���С
		 {
			 if (left_bytes > 0)//�ѱ��ÿռ����
			 {
				 Obj*volatile* __my_free_list = _S_free_list + _S_freelist_index(left_bytes);
				 ((Obj*)_S_start_free)->_M_free_list_link = *__my_free_list;
				 *__my_free_list = (Obj*)_S_start_free;
			 }
			 size_t bytes_to_get = 2 * total_bytes + _S_round_up(_S_heap_size >> 4);
			 _S_start_free = (char*)malloc(bytes_to_get);//�����ÿռ�����ռ�
			 if (_S_start_free == nullptr)//malloc�����ռ�,ȥ�����������ҳ�һ���ڴ浱�ڴ��
			 {
				 for (int i = __size + _ALIGN; i < _MAX_BYTES; i = i + _ALIGN)
				 {
					 Obj*volatile* __my_free_list = _S_free_list + _S_freelist_index(i);
					 Obj* tmp = *__my_free_list;
					 if (tmp != nullptr)    //����������һ�鵱���ÿռ�
					 {
						 *__my_free_list = tmp->_M_free_list_link;
						 _S_start_free = (char*)tmp;;
						 _S_end_free = _S_start_free + i;
						 return _S_chunk_alloc(__size, objs);//��һ�εݹ�ȥ���ÿռ�Ҫ�ڴ�
					 }
				 }
				 _S_end_free = nullptr;
				 _S_start_free = (char*)malloc(bytes_to_get);//������û�п飬����һ���ռ�������ȥ����ռ䣨��Ԥ�����úõĻص������������ͷŲ���Ҫ���ڴ棩
			 }
			 _S_heap_size += bytes_to_get;
			 _S_end_free = _S_start_free + bytes_to_get;
			 return _S_chunk_alloc(__size, objs);//���뵽�ռ��ˣ��ٵݹ�һ���ҵ����ʿռ䷵�ظ�_S_refillȥ�ָ�ռ䡣
		 }
	 }
	 void*_S_refill(size_t _n)   //�����ڴ�ָ��С���Ѵ��ָ����ͬ��С���ڴ棬���������������ҵ�������
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
		 result = chunk;                                      //��һ�鵱ʹ�ÿ�
		 *__my_free_list = (Obj*)(chunk + _n);
		 next_objs = *__my_free_list;
		 for (int i = 1;; i++)
		 {
			 cur_objs = next_objs;
			 next_objs = (Obj*)((char*)next_objs + _n);
			 if (nobjs - 1 == 1)
			 {
				 cur_objs->_M_free_list_link = nullptr;//���һ��ĺ��Ϊ��
				 break;
			 }
			 else
			 {
				 cur_objs->_M_free_list_link = next_objs;
			 }
		 }
		 return result;
	 }
	 size_t _S_round_up(size_t __bytes)    //��_bytes������8�ı�����
	 {
		 return (((__bytes)+(size_t)_ALIGN - 1) & ~((size_t)_ALIGN - 1));
	 }
	 size_t _S_freelist_index(size_t __bytes)  //����_byte����Ӧ�������±꣬�����ҳ���Ӧ�������±�
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

