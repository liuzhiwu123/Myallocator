#include"myallocate.h"

template<typename T, typename Alloc = MyAllocate<T>>
class vector
{
public:
	vector(int size = 5)
	{
		cout << "vector(int size = 5)" << endl;
		first = alloc.allocate(size);
		end = first + size;
		last = first;
	}
	~vector()
	{
		T* ptr = first;
		while (ptr != last)
		{
			alloc.destory(ptr);
			ptr++;
		}
		int size = last - first;
		alloc.deallocate(first,size);
	}

	vector(const vector<T>& rhs)
	{
		int size = rhs.last - rhs.first;
		int len = rhs.end - rhs.first;
		first = alloc.allocate(len);
		for (int i = 0; i < size; i++)
		{
			alloc.construct(first + i, *(rhs.first + i));
		}
		last = rhs.last;
		end = rhs.end;
	}
	vector<T>& operator=(const vector<T> &rhs)
	{
		if (this == &rhs)
			return *this;
		T* ptr = first;
		while (ptr != last)
		{
			alloc.destory(ptr);
			ptr++;
		}
		int mysize = end - first;
		alloc.deallocate(first, mysize);
		int size = rhs.last - rhs.first;
		int len = rhs.end - rhs.first;
		first = alloc.allocate(len);
		for (int i = 0; i < size; i++)
		{
			alloc.construct(first + i, *(rhs.first + i));
		}
		last =first+size;
		end = first+len;
	}
	void push_back(const T &val)//ÏòÄ©Î²Ìí¼ÓÔªËØ
	{
		if (full())
			expand();
		alloc.construct(last, val);
		last++;
	}
	void pop_back()//´ÓÄ©Î²É¾³ýÔªËØ
	{
		if (empty())
			return;
		alloc.destory(--last);
	}
	T back()const//·µ»ØÈÝÆ÷Ä©Î²µÄÔªËØÖµ
	{
		return*(last - 1);
	}
	bool empty()const
	{
		return first == last;
	}
	bool full()const
	{
		return last == end;
	}
private:
	T* first;
	T* last;
	T* end;
	Alloc alloc;
	bool expand()
	{
		int size = last - first;
		T* newdata = alloc.allocate(size * 2);
		for (int i = 0; i < size; i++)
		{
			alloc.construct(newdata + i, *(first + i));
			alloc.destory(first + i);
		}
		alloc.deallocate(first,size);
		first = newdata;
		last = first + size;
		end = first + size * 2;
		cout << "expand" << endl;
		return true;
	}
};
