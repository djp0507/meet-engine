/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */
#define LOG_TAG "List"
 
#include "log_android.h"
#include "list.h"

List::List() : m_pHead(NULL), m_pTail(NULL), m_nLength(0)
{
}

List::~List()
{
    Clear();
}

bool List::IsEmpty() const
{
    return (m_nLength == 0);
}

int32_t List::GetLength() const
{
    return m_nLength;
}

bool List::Insert(int32_t nIndex, void* data)
{
    if (nIndex < 0 || nIndex > m_nLength)
    {
        return false;
    }

    Node* pNewNode = new Node(data);
    if(nIndex == m_nLength)
    {
        m_pTail = pNewNode;
    }
    
    if (nIndex == 0)
    {
        pNewNode->m_pNext = m_pHead;
        m_pHead = pNewNode;
    }
    else
    {
        Node* p = m_pHead;
        for (int32_t i = 1; i < nIndex; i++)
        {
            p = p->m_pNext;
        }
        pNewNode->m_pNext = p->m_pNext;
        p->m_pNext = pNewNode;
    }

    m_nLength++;

    return true;
}

void* List::Remove(int32_t nIndex)
{
    if ( !IsValidIndex(nIndex) )
    {
        return NULL;
    }

    Node* p = m_pHead;
    for (int32_t i = 0; i < nIndex-1; i++)
    {
        p = p->m_pNext;
    }

    Node* pTemp = NULL;
    if(nIndex == 0)
    {
        pTemp = m_pHead;
        m_pHead = m_pHead->m_pNext;
        if(pTemp == m_pTail)
        {
            m_pTail = NULL;
        }
    }
    else
    {
        pTemp = p->m_pNext;
        p->m_pNext = pTemp->m_pNext;
        if(pTemp == m_pTail)
        {
            m_pTail = p;
        }
    }
    void* data = pTemp->m_data;
    delete pTemp;
    m_nLength--;
    return data;
}

void List::Append(void* data)
{
    Node* pNewNode = new Node(data);
    if ( IsEmpty() )
    {
        m_pHead = m_pTail = pNewNode;
    }
    else
    {
        m_pTail->m_pNext = pNewNode;
        m_pTail = pNewNode;
    }

    m_nLength++;
}

void* List::GetAt(int32_t nIndex) const
{
    if ( !IsValidIndex(nIndex) )
    {
        return NULL;
    }

    Node* p = m_pHead;
    for (int32_t i = 0; i < nIndex; i++)
    {
        p = p->m_pNext;
    }

    return p->m_data;
}

void* List::operator[](int32_t nIndex) const
{
    return GetAt(nIndex);
}

void List::Clear()
{
    while (m_pHead != NULL)
    {
        Node* pTemp = m_pHead;
        m_pHead = m_pHead->m_pNext;
        delete pTemp;
    }
    m_pHead = m_pTail = NULL;
    m_nLength = 0;
}

bool List::IsValidIndex(int32_t nIndex) const
{
    return (nIndex >= 0 && nIndex < m_nLength);
}