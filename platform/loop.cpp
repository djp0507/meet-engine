/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */

//#undef __STRICT_ANSI__
#define __STDINT_LIMITS
//#define __STDC_LIMIT_MACROS


#define LOG_TAG "Loop"

#include <stdint.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include "autolock.h"
#include "utils.h"
#include "log.h"
#include "loop.h"

#ifdef OS_ANDROID
#include <jni.h>
extern JavaVM* gs_jvm;
#endif

Loop::Loop()
{
    mRunning = false;
    mNextEventID = 1;
    pthread_mutex_init(&mLock, NULL);
    pthread_cond_init(&mQueueNotEmptyCondition, NULL);
    pthread_cond_init(&mQueueHeadChangedCondition, NULL);
}

Loop::~Loop()
{
    stop();
    
    pthread_mutex_destroy(&mLock);
    pthread_cond_destroy(&mQueueNotEmptyCondition);
    pthread_cond_destroy(&mQueueHeadChangedCondition);
    LOGD("Loop destructor");
}

void Loop::start()
{
    if (mRunning)
        return;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&mThread, &attr, ThreadWrapper, this);

    pthread_attr_destroy(&attr);

    mRunning = true;
}

void Loop::stop()
{
    if (!mRunning)
        return;

    mRunning = false;
    pthread_cond_signal(&mQueueHeadChangedCondition);
    
    void *dummy;
    pthread_join(mThread, &dummy);

    mQueue.Clear();
}

Loop::event_id Loop::postEvent(Event* event)
{
    // Reserve an earlier timeslot an INT64_MIN to be able to post
    // the StopEvent to the absolute head of the queue.
    return postTimedEvent(event, INT64_MIN + 1);
}

Loop::event_id Loop::postEventToBack(Event* event)
{
    return postTimedEvent(event, INT64_MAX);
}

Loop::event_id Loop::postEventWithDelay(Event* event, int64_t delayMs)
{
    return postTimedEvent(event, getNowUs() + delayMs*1000ll);
}

Loop::event_id Loop::postTimedEvent(Event* event, int64_t realtimeUs)
{
    AutoLock autoLock(&mLock);
    
    event->setEventID(mNextEventID++);
    int32_t index = 0;
    while(index<mQueue.GetLength())
    {
        QueueItem* item = (QueueItem*)mQueue[index];
        if(realtimeUs >= item->realtimeUs)
        {
            index++;
        }
        else
        {
            break;
        }
    }
    QueueItem* item = new QueueItem();
    item->event = event;
    item->realtimeUs = realtimeUs;
    if(index == 0)
    {
        pthread_cond_signal(&mQueueHeadChangedCondition);
    }
    
    mQueue.Insert(index, item);
    LOGD("Insert event:%d", event->mEventID);

    if(mQueue.GetLength() == 1)
    {
        pthread_cond_signal(&mQueueNotEmptyCondition);
    }
    
    return event->eventID();
    
    /*
    list<QueueItem>::iterator it = mQueue.begin();
    while (it != mQueue.end() && realtime_us >= (*it).realtime_us) {
        ++it;
    }

    QueueItem item;
    item.event = event;
    item.realtime_us = realtime_us;

    if (it == mQueue.begin()) {
        //mQueueHeadChangedCondition.signal();
        pthread_cond_signal(&mQueueHeadChangedCondition);
    }

    mQueue.insert(it, item);
    */
}

void Loop::cancelEvent(event_id id) {
    if (id == 0) {
        return;
    }

    AutoLock autoLock(&mLock);
    for(int i=0; i<mQueue.GetLength(); i++)
    {
        QueueItem* item = (QueueItem*)mQueue[i];
        if(item->event->eventID() == id)
        {
            mQueue.Remove(i);
            delete item;
            if(i==0)
            {
                pthread_cond_signal(&mQueueHeadChangedCondition);
            }
        }
    }
}

// static
void *Loop::ThreadWrapper(void *me)
{
#ifdef OS_ANDROID
    JNIEnv *env = NULL;
    gs_jvm->AttachCurrentThread(&env, NULL);
    LOGD("getpriority before:%d", getpriority(PRIO_PROCESS, 0));
    LOGD("sched_getscheduler:%d", sched_getscheduler(0));

    int videoThreadPriority = -6;
    if(setpriority(PRIO_PROCESS, 0, videoThreadPriority) != 0)
    {
        LOGE("set video thread priority failed");
    }
    LOGD("getpriority after:%d", getpriority(PRIO_PROCESS, 0));
#endif
    
    static_cast<Loop *>(me)->threadEntry();
    
#ifdef OS_ANDROID
    gs_jvm->DetachCurrentThread();
#endif
    LOGD("exit thread");
    return NULL;
}

void Loop::threadEntry() {
    //prctl(PR_SET_NAME, (unsigned long)"ffplayer Loop", 0, 0, 0);

    while(mRunning)
    {
        int64_t nowUs = 0;
        Event* event = NULL;

        if(mQueue.IsEmpty())
        {
            //mQueueNotEmptyCondition.wait(mLock);
            //pthread_cond_wait(&mQueueNotEmptyCondition, &mLock);
    	    usleep(1000*10); //sleep 10ms
    	    continue;
        }
        
        {
            AutoLock autoLock(&mLock);
            //to get one event for executing.
            event_id eventID = 0;
            while(mRunning)
            {
                if (mQueue.IsEmpty())
                {
                    // The only event in the queue could have been cancelled
                    // while we were waiting for its scheduled time.
                    break;
                }

                //list<QueueItem>::iterator it = mQueue.begin();
                QueueItem* it = (QueueItem*)mQueue[0];
                eventID = (*it).event->eventID();

                nowUs = getNowUs();
                int64_t whenUs = (*it).realtimeUs;
                //LOGD("nowUs:%lld", nowUs);
                //LOGD("whenUs:%lld", whenUs);

                int64_t delayUs = 0;
                if (whenUs < 0 || whenUs == INT64_MAX)
                {
                    delayUs = 0;
                }
                else
                {
                    delayUs = whenUs - nowUs;
                }

                if (delayUs <= 0)
                {
                    break;
                }
                
                static int64_t kMaxTimeoutUs = 10000000ll;// 10 secs
                bool timeoutCapped = false;
                if (delayUs > kMaxTimeoutUs)
                {
                    // We'll never block for more than 10 secs, instead
                    // we will split up the full timeout into chunks of
                    // 10 secs at a time. This will also avoid overflow
                    // when converting from us to ns.
                    delayUs = kMaxTimeoutUs;
                    timeoutCapped = true;
                }
                
                struct timespec ts;
                ts.tv_sec = delayUs/1000000ll;
                ts.tv_nsec = (delayUs%1000000ll)*1000ll;
                //LOGD("delayUs:%lld", delayUs);
                int32_t err = pthread_cond_timedwait_relative_np(&mQueueHeadChangedCondition, &mLock, &ts);
                //LOGD("err:%d", err);

                if (!timeoutCapped && err == ETIMEDOUT)
                {
                    // We finally hit the time this event is supposed to trigger.
                    nowUs = getNowUs();
                    break;
                }
            }

            if(!mRunning) return;

            // The event w/ this id may have been cancelled while we're
            // waiting for its trigger-time, in that case
            // removeEventFromQueue_l will return NULL.
            // Otherwise, the QueueItem will be removed
            // from the queue and the referenced event returned.
            event = removeEventFromQueue_l(eventID);
        }


        if (event != NULL)
        {
            // Fire event with the lock NOT held.
            event->fire(this, nowUs/1000ll);
        }
    }
}

Loop::Event* Loop::removeEventFromQueue_l(        event_id id)
{
    for(int i=0; i<mQueue.GetLength(); i++)
    {
        QueueItem* item = (QueueItem*)mQueue[i];
        if(item ->event->eventID() == id)
        {
            mQueue.Remove(i);
            Event* event = item->event;
            delete item;
            
            event->setEventID(0);
            return event;
        }
    }
    return NULL;
    
    /*
    for (list<QueueItem>::iterator it = mQueue.begin();
         it != mQueue.end(); ++it) {
		    //LOGE("Finding event ID");
        if ((*it).event->eventID() == id) {
            sp<Event> event = (*it).event;
            event->setEventID(0);

            mQueue.erase(it);
		    //LOGE("Got event ID");
            return event;
        }
    }
    */
}

