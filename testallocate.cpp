#include"vectoralocate.h"
#if 0
size_t eight(size_t __bytes)
{
	return (((__bytes)+(size_t)_ALIGN - 1) & ~((size_t)_ALIGN - 1));
}
size_t aindex(size_t __bytes)
{
	return (((__bytes)+(size_t)_ALIGN - 1) / (size_t)_ALIGN - 1);
}
#endif
int main()
{
	vector<int> vec;
	for (int i = 0; i < 5; ++i)
	{
		vec.push_back(i);
	}
	vector<int> vec1;
	vec1 = vec;
	while (!vec1.empty())
	{
		int val = vec1.back();
		cout << val << " ";
		vec1.pop_back();
	}

	cout << endl;
	while (!vec.empty())
	{
		int val = vec.back();
		cout << val << " ";
		vec.pop_back();
	}
	return 0;
}