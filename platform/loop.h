/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */


#ifndef FF_LOOP_H_

#define FF_LOOP_H_

#include <pthread.h>
#include "list.h"

class Loop
{
public:
    typedef int32_t event_id;

    class Event
	{
	public:
        Event()  : mEventID(0) {}
        virtual ~Event() {}
        event_id eventID(){ return mEventID;}
    protected:
        virtual void fire(Loop *queue, int64_t nowMs) = 0;
    private:
        friend class Loop;
        event_id mEventID;
        void setEventID(event_id id){ mEventID = id;}
        Event(const Event &);
        Event &operator=(const Event &);
    };

    Loop();
    ~Loop();

    void start();
    void stop();
	bool isRunning(){return mRunning;}

    // Posts an event to the front of the queue (after all events that
    // have previously been posted to the front but before timed events).
    event_id postEvent(Event* event);

    event_id postEventToBack(Event* event);

    // It is an error to post an event with a negative delay.
    event_id postEventWithDelay(Event* event, int64_t delayMs);

    // Returns true iff event is currently in the queue and has been
    // successfully cancelled. In this case the event will have been
    // removed from the queue and won't fire.
    void cancelEvent(event_id id);

private:
    struct QueueItem {
        Event* event;
        int64_t realtimeUs;
    };

    pthread_t mThread;
    pthread_mutex_t mLock;
    pthread_cond_t mQueueNotEmptyCondition;
    pthread_cond_t mQueueHeadChangedCondition;
	
    List mQueue;
    event_id mNextEventID;
    bool mRunning;
	
    // If the event is to be posted at a time that has already passed,
    // it will fire as soon as possible.
    event_id postTimedEvent(Event* event, int64_t realtimeUs);
    static void *ThreadWrapper(void *me);
    void threadEntry();
    Event* removeEventFromQueue_l(event_id id);

    Loop(const Loop &);
    Loop &operator=(const Loop &);
};

#endif 
