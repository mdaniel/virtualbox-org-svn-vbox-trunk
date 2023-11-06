/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


/*
  A stack-based lock object that makes using PRLock a bit more
  convenient. It acquires the monitor when constructed, and releases
  it when it goes out of scope.

  For example,

    class Foo {
    private:
        PRLock* mLock;

    public:
        Foo(void) {
            mLock = PR_NewLock();
        }

        ~Foo(void) {
            PR_DestroyLock(mLock);
        }

        void ThreadSafeMethod(void) {
            // we're don't hold the lock yet...

            nsAutoLock lock(mLock);
            // ...but now we do.

            // we even can do wacky stuff like return from arbitrary places w/o
            // worrying about forgetting to release the lock
            if (some_weird_condition)
                return;

            // otherwise do some other stuff
        }

        void ThreadSafeBlockScope(void) {
            // we're not in the lock here...

            {
                nsAutoLock lock(mLock);
                // but we are now, at least until the block scope closes
            }

            // ...now we're not in the lock anymore
        }
    };

    A similar stack-based locking object is available for PRMonitor.  The 
    major difference is that the PRMonitor must be created and destroyed 
    via the static methods on nsAutoMonitor.

    For example:
    Foo::Foo() {
      mMon =  nsAutoMonitor::NewMonitor("FooMonitor");
    }
    nsresult Foo::MyMethod(...) {
       nsAutoMonitor mon(mMon);
       ...
       // go ahead and do deeply nested returns...
                    return NS_ERROR_FAILURE;
       ...
       // or call Wait or Notify...
       mon.Wait();
       ...
       // cleanup is automatic
    }
 */

#ifndef nsAutoLock_h__
#define nsAutoLock_h__

#include "nscore.h"
#include "prlock.h"
#include "prlog.h"

/**
 * nsAutoLockBase
 * This is the base class for the stack-based locking objects.
 * Clients of derived classes need not play with this superclass.
 **/
class NS_COM nsAutoLockBase {
protected:
    nsAutoLockBase() {}
    enum nsAutoLockType {eAutoLock, eAutoMonitor, eAutoCMonitor};

#ifdef DEBUG
    nsAutoLockBase(void* addr, nsAutoLockType type);
    ~nsAutoLockBase();

    void            Show();
    void            Hide();

    void*           mAddr;
    nsAutoLockBase* mDown;
    nsAutoLockType  mType;
#else
    nsAutoLockBase(void* addr, nsAutoLockType type) {}
    ~nsAutoLockBase() {}

    void            Show() {}
    void            Hide() {}
#endif
};

/** 
 * nsAutoLock
 * Stack-based locking object for PRLock.
 **/
class NS_COM nsAutoLock : public nsAutoLockBase {
private:
    PRLock* mLock;
    PRBool mLocked;

    // Not meant to be implemented. This makes it a compiler error to
    // construct or assign an nsAutoLock object incorrectly.
    nsAutoLock(void);
    nsAutoLock(const nsAutoLock& /*aLock*/);
    nsAutoLock& operator =(const nsAutoLock& /*aLock*/);

    // Not meant to be implemented. This makes it a compiler error to
    // attempt to create an nsAutoLock object on the heap.
    static void* operator new(size_t /*size*/) CPP_THROW_NEW;
    static void operator delete(void* /*memory*/);

public:
    /**
     * Constructor
     * The constructor aquires the given lock.  The destructor
     * releases the lock.
     * 
     * @param aLock A valid PRLock* returned from the NSPR's 
     * PR_NewLock() function.
     **/
    nsAutoLock(PRLock* aLock)
        : nsAutoLockBase(aLock, eAutoLock),
          mLock(aLock),
          mLocked(PR_TRUE) {
        PR_ASSERT(mLock);

        // This will assert deep in the bowels of NSPR if you attempt
        // to re-enter the lock.
        PR_Lock(mLock);
    }
    
    ~nsAutoLock(void) {
        if (mLocked)
            PR_Unlock(mLock);
    }

    /** 
     * lock
     * Client may call this to reaquire the given lock. Take special
     * note that attempting to aquire a locked lock will hang or crash.
     **/  
    void lock() {
        Show();
        PR_ASSERT(!mLocked);
        PR_Lock(mLock);
        mLocked = PR_TRUE;
    }


    /** 
     * unlock
     * Client may call this to release the given lock. Take special
     * note unlocking an unlocked lock has undefined results.
     **/ 
     void unlock() {
        PR_ASSERT(mLocked);
        PR_Unlock(mLock);
        mLocked = PR_FALSE;
        Hide();
    }
};

#include "prmon.h"
#include "nsError.h"
#include "nsDebug.h"

class NS_COM nsAutoMonitor : public nsAutoLockBase {
public:

    /**
     * NewMonitor
     * Allocates a new PRMonitor for use with nsAutoMonitor.
     * @param name A (unique /be?) name which can reference this monitor
     * @returns nsnull if failure
     *          A valid PRMonitor* is successful while must be destroyed
     *          by nsAutoMonitor::DestroyMonitor()
     **/
    static PRMonitor* NewMonitor(const char* name);
    static void       DestroyMonitor(PRMonitor* mon);

    
    /**
     * Constructor
     * The constructor locks the given monitor.  During destruction
     * the monitor will be unlocked.
     * 
     * @param mon A valid PRMonitor* returned from 
     *        nsAutoMonitor::NewMonitor().
     **/
    nsAutoMonitor(PRMonitor* mon)
        : nsAutoLockBase((void*)mon, eAutoMonitor),
          mMonitor(mon), mLockCount(0)
    {
        NS_ASSERTION(mMonitor, "null monitor");
        if (mMonitor) {
            PR_EnterMonitor(mMonitor);
            mLockCount = 1;
        }
    }

    ~nsAutoMonitor() {
        NS_ASSERTION(mMonitor, "null monitor");
        if (mMonitor && mLockCount) {
#ifdef DEBUG
            PRStatus status = 
#endif
            PR_ExitMonitor(mMonitor);
            NS_ASSERTION(status == PR_SUCCESS, "PR_ExitMonitor failed");
        }
    }

    /** 
     * Enter
     * Client may call this to reenter the given monitor.
     * @see prmon.h 
     **/  
    void Enter();

    /** 
     * Exit
     * Client may call this to exit the given monitor.
     * @see prmon.h 
     **/      
    void Exit();

    /** 
     * Wait
     * @see prmon.h 
     **/      
    nsresult Wait(PRIntervalTime interval = PR_INTERVAL_NO_TIMEOUT) {
        return PR_Wait(mMonitor, interval) == PR_SUCCESS
            ? NS_OK : NS_ERROR_FAILURE;
    }

    /** 
     * Notify
     * @see prmon.h 
     **/      
    nsresult Notify() {
        return PR_Notify(mMonitor) == PR_SUCCESS
            ? NS_OK : NS_ERROR_FAILURE;
    }

    /** 
     * NotifyAll
     * @see prmon.h 
     **/      
    nsresult NotifyAll() {
        return PR_NotifyAll(mMonitor) == PR_SUCCESS
            ? NS_OK : NS_ERROR_FAILURE;
    }

private:
    PRMonitor*  mMonitor;
    PRInt32     mLockCount;

    // Not meant to be implemented. This makes it a compiler error to
    // construct or assign an nsAutoLock object incorrectly.
    nsAutoMonitor(void);
    nsAutoMonitor(const nsAutoMonitor& /*aMon*/);
    nsAutoMonitor& operator =(const nsAutoMonitor& /*aMon*/);

    // Not meant to be implemented. This makes it a compiler error to
    // attempt to create an nsAutoLock object on the heap.
    static void* operator new(size_t /*size*/) CPP_THROW_NEW;
    static void operator delete(void* /*memory*/);
};

#endif // nsAutoLock_h__

