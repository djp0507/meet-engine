/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */

#ifndef FF_LIST_H_
#define FF_LIST_H_
#include <stdint.h>

class Node
{
friend class List;
private:
	Node() : m_data(NULL), m_pNext(NULL) {}
	Node(void* data) : m_data(data), m_pNext(NULL) {}
	~Node() {}

private:
	void* m_data;
	Node* m_pNext;
};

class List
{
public:
	List();
	~List();
	bool IsEmpty() const;
	int32_t GetLength() const;
	bool Insert(int32_t nIndex, void* data);
	void* Remove(int32_t nIndex);
	void Append(void* data);
	void* GetAt(int32_t nIndex) const;
	void* operator[](int32_t nIndex) const;
	void Clear();

private:
	bool IsValidIndex(int32_t nIndex) const;

	Node* m_pHead;
	Node* m_pTail;
	uint32_t m_nLength;
};


#endif

