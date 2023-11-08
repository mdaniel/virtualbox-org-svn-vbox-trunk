/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is C++ hashtable templates.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsBaseHashtable_h__
#define nsBaseHashtable_h__

#include "nsTHashtable.h"
#include "nsDebug.h"

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/semaphore.h>

template<class KeyClass,class DataType,class UserDataType>
class nsBaseHashtable; // forward declaration

/**
 * the private nsTHashtable::EntryType class used by nsBaseHashtable
 * @see nsTHashtable for the specification of this class
 * @see nsBaseHashtable for template parameters
 */
template<class KeyClass,class DataType>
class nsBaseHashtableET : public KeyClass
{
public:
  DataType mData;
  friend class nsTHashtable< nsBaseHashtableET<KeyClass,DataType> >;

private:
  typedef typename KeyClass::KeyType KeyType;
  typedef typename KeyClass::KeyTypePointer KeyTypePointer;
  
  nsBaseHashtableET(KeyTypePointer aKey);
  nsBaseHashtableET(nsBaseHashtableET<KeyClass,DataType>& toCopy);
  ~nsBaseHashtableET();
};

/**
 * templated hashtable for simple data types
 * This class manages simple data types that do not need construction or
 * destruction. Thread-safety is optional, via a flag in Init()
 *
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param DataType the datatype stored in the hashtable,
 *   for example, PRUint32 or nsCOMPtr.  If UserDataType is not the same,
 *   DataType must implicitly cast to UserDataType
 * @param UserDataType the user sees, for example PRUint32 or nsISupports*
 */
template<class KeyClass,class DataType,class UserDataType>
class nsBaseHashtable :
  protected nsTHashtable< nsBaseHashtableET<KeyClass,DataType> >
{
public:
  typedef typename KeyClass::KeyType KeyType;
  typedef nsBaseHashtableET<KeyClass,DataType> EntryType;

  // default constructor+destructor are fine

  /**
   * Initialize the object.
   * @param initSize the initial number of buckets in the hashtable,
   *        default 16
   * @param threadSafe whether to provide read/write
   * locking on all class methods
   * @return    PR_TRUE if the object was initialized properly.
   */
  PRBool Init(PRUint32 initSize = PL_DHASH_MIN_SIZE)
  { return nsTHashtable<EntryType>::Init(initSize); }

  /**
   * Check whether the table has been initialized.
   * This function is especially useful for static hashtables.
   * @return PR_TRUE if the table has been initialized.
   */
  PRBool IsInitialized() const { return this->mTable.entrySize; }

  /**
   * Return the number of entries in the table.
   * @return    number of entries
   */
  PRUint32 Count() const
  { return nsTHashtable<EntryType>::Count(); }

  /**
   * retrieve the value for a key.
   * @param aKey the key to retreive
   * @param pData data associated with this key will be placed at this
   *   pointer.  If you only need to check if the key exists, pData
   *   may be null.
   * @return PR_TRUE if the key exists. If key does not exist, pData is not
   *   modified.
   */
  PRBool Get(KeyType aKey, UserDataType* pData) const
  {
    EntryType* ent = this->GetEntry(aKey);

    if (!ent)
      return PR_FALSE;

    if (pData)
      *pData = ent->mData;

    return PR_TRUE;
  }

  /**
   * put a new value for the associated key
   * @param aKey the key to put
   * @param aData the new data
   * @return always PR_TRUE, unless memory allocation failed
   */
  PRBool Put(KeyType aKey, UserDataType aData)
  {
    EntryType* ent = this->PutEntry(aKey);

    if (!ent)
      return PR_FALSE;

    ent->mData = aData;

    return PR_TRUE;
  }

  /**
   * remove the data for the associated key
   * @param aKey the key to remove from the hashtable
   */
  void Remove(KeyType aKey) { this->RemoveEntry(aKey); }

  /**
   * function type provided by the application for enumeration.
   * @param aKey the key being enumerated
   * @param aData data being enumerated
   * @parm userArg passed unchanged from Enumerate
   * @return either
   *   @link PLDHashOperator::PL_DHASH_NEXT PL_DHASH_NEXT @endlink or
   *   @link PLDHashOperator::PL_DHASH_STOP PL_DHASH_STOP @endlink
   */
  typedef PLDHashOperator
    (*PR_CALLBACK EnumReadFunction)(KeyType      aKey,
                                    UserDataType aData,
                                    void*        userArg);

  /**
   * enumerate entries in the hashtable, without allowing changes
   * this function read-locks the hashtable, so other threads may read keys
   * at the same time in multi-thread environments.
   * @param enumFunc enumeration callback
   * @param userArg passed unchanged to the EnumReadFunction
   */
  PRUint32 EnumerateRead(EnumReadFunction enumFunc, void* userArg) const
  {
    NS_ASSERTION(this->mTable.entrySize,
                 "nsBaseHashtable was not initialized properly.");

    s_EnumReadArgs enumData = { enumFunc, userArg };
    return PL_DHashTableEnumerate(NS_CONST_CAST(PLDHashTable*, &this->mTable),
                                  s_EnumReadStub,
                                  &enumData);
  }

  /**
   * function type provided by the application for enumeration.
   * @param aKey the key being enumerated
   * @param aData Reference to data being enumerated, may be altered. e.g. for
   *        nsInterfaceHashtable this is an nsCOMPtr reference...
   * @parm userArg passed unchanged from Enumerate
   * @return bitflag combination of
   *   @link PLDHashOperator::PL_DHASH_REMOVE @endlink,
   *   @link PLDHashOperator::PL_DHASH_NEXT PL_DHASH_NEXT @endlink, or
   *   @link PLDHashOperator::PL_DHASH_STOP PL_DHASH_STOP @endlink
   */
  typedef PLDHashOperator
    (*PR_CALLBACK EnumFunction)(KeyType       aKey,
                                DataType&     aData,
                                void*         userArg);

  /**
   * enumerate entries in the hashtable, allowing changes. This
   * functions write-locks the hashtable.
   * @param enumFunc enumeration callback
   * @param userArg passed unchanged to the EnumFunction
   */
  PRUint32 Enumerate(EnumFunction enumFunc, void* userArg)
  {
    NS_ASSERTION(this->mTable.entrySize,
                 "nsBaseHashtable was not initialized properly.");

    s_EnumArgs enumData = { enumFunc, userArg };
    return PL_DHashTableEnumerate(&this->mTable,
                                  s_EnumStub,
                                  &enumData);
  }

  /**
   * reset the hashtable, removing all entries
   */
  void Clear() { nsTHashtable<EntryType>::Clear(); }

protected:
  /**
   * used internally during EnumerateRead.  Allocated on the stack.
   * @param func the enumerator passed to EnumerateRead
   * @param userArg the userArg passed to EnumerateRead
   */
  struct s_EnumReadArgs
  {
    EnumReadFunction func;
    void* userArg;
  };

  static PLDHashOperator s_EnumReadStub(PLDHashTable    *table,
                                        PLDHashEntryHdr *hdr,
                                        PRUint32         number,
                                        void            *arg);

  struct s_EnumArgs
  {
    EnumFunction func;
    void* userArg;
  };

  static PLDHashOperator s_EnumStub(PLDHashTable      *table,
                                    PLDHashEntryHdr   *hdr,
                                    PRUint32           number,
                                    void              *arg);
};

/**
 * This class is a thread-safe version of nsBaseHashtable.
 */
template<class KeyClass,class DataType,class UserDataType>
class nsBaseHashtableMT :
  protected nsBaseHashtable<KeyClass,DataType,UserDataType>
{
public:
  typedef typename
    nsBaseHashtable<KeyClass,DataType,UserDataType>::EntryType EntryType;
  typedef typename
    nsBaseHashtable<KeyClass,DataType,UserDataType>::KeyType KeyType;
  typedef typename
    nsBaseHashtable<KeyClass,DataType,UserDataType>::EnumFunction EnumFunction;
  typedef typename
    nsBaseHashtable<KeyClass,DataType,UserDataType>::EnumReadFunction EnumReadFunction;

  nsBaseHashtableMT() : mLock(nsnull) { }
  ~nsBaseHashtableMT();

  PRBool Init(PRUint32 initSize = PL_DHASH_MIN_SIZE);
  PRBool IsInitialized() const { return (PRBool) mLock; }
  PRUint32 Count() const;
  PRBool Get(KeyType aKey, UserDataType* pData) const;
  PRBool Put(KeyType aKey, UserDataType aData);
  void Remove(KeyType aKey);

  PRUint32 EnumerateRead(EnumReadFunction enumFunc, void* userArg) const;
  PRUint32 Enumerate(EnumFunction enumFunc, void* userArg);
  void Clear();

protected:
  RTSEMFASTMUTEX mLock;
};
  

//
// nsBaseHashtableET definitions
//

template<class KeyClass,class DataType>
nsBaseHashtableET<KeyClass,DataType>::nsBaseHashtableET(KeyTypePointer aKey) :
  KeyClass(aKey)
{ }

template<class KeyClass,class DataType>
nsBaseHashtableET<KeyClass,DataType>::nsBaseHashtableET
  (nsBaseHashtableET<KeyClass,DataType>& toCopy) :
  KeyClass(toCopy),
  mData(toCopy.mData)
{ }

template<class KeyClass,class DataType>
nsBaseHashtableET<KeyClass,DataType>::~nsBaseHashtableET()
{ }


//
// nsBaseHashtable definitions
//

template<class KeyClass,class DataType,class UserDataType>
PLDHashOperator
nsBaseHashtable<KeyClass,DataType,UserDataType>::s_EnumReadStub
  (PLDHashTable *table, PLDHashEntryHdr *hdr, PRUint32 number, void* arg)
{
  EntryType* ent = NS_STATIC_CAST(EntryType*, hdr);
  s_EnumReadArgs* eargs = (s_EnumReadArgs*) arg;

  PLDHashOperator res = (eargs->func)(ent->GetKey(), ent->mData, eargs->userArg);

  NS_ASSERTION( !(res & PL_DHASH_REMOVE ),
                "PL_DHASH_REMOVE return during const enumeration; ignoring.");

  if (res & PL_DHASH_STOP)
    return PL_DHASH_STOP;

  return PL_DHASH_NEXT;
}

template<class KeyClass,class DataType,class UserDataType>
PLDHashOperator
nsBaseHashtable<KeyClass,DataType,UserDataType>::s_EnumStub
  (PLDHashTable *table, PLDHashEntryHdr *hdr, PRUint32 number, void* arg)
{
  EntryType* ent = NS_STATIC_CAST(EntryType*, hdr);
  s_EnumArgs* eargs = (s_EnumArgs*) arg;

  return (eargs->func)(ent->GetKey(), ent->mData, eargs->userArg);
}


//
// nsBaseHashtableMT  definitions
//

template<class KeyClass,class DataType,class UserDataType>
nsBaseHashtableMT<KeyClass,DataType,UserDataType>::~nsBaseHashtableMT()
{
  if (this->mLock != NIL_RTSEMFASTMUTEX)
  {
    RTSemFastMutexDestroy(this->mLock);
    this->mLock = NIL_RTSEMFASTMUTEX;
  }
}

template<class KeyClass,class DataType,class UserDataType>
PRBool
nsBaseHashtableMT<KeyClass,DataType,UserDataType>::Init(PRUint32 initSize)
{
  if (!nsTHashtable<EntryType>::IsInitialized() && !nsTHashtable<EntryType>::Init(initSize))
    return PR_FALSE;

  this->mLock = NIL_RTSEMFASTMUTEX;
  int vrc = RTSemFastMutexCreate(&this->mLock);
  NS_WARN_IF_FALSE(RT_SUCCESS(vrc), "Error creating lock during nsBaseHashtableL::Init()");

  return (this->mLock != NIL_RTSEMFASTMUTEX);
}

template<class KeyClass,class DataType,class UserDataType>
PRUint32
nsBaseHashtableMT<KeyClass,DataType,UserDataType>::Count() const
{
  RTSemFastMutexRequest(this->mLock);
  PRUint32 count = nsTHashtable<EntryType>::Count();
  RTSemFastMutexRelease(this->mLock);

  return count;
}

template<class KeyClass,class DataType,class UserDataType>
PRBool
nsBaseHashtableMT<KeyClass,DataType,UserDataType>::Get(KeyType       aKey,
                                                           UserDataType* pData) const
{
  RTSemFastMutexRequest(this->mLock);
  PRBool res =
    nsBaseHashtable<KeyClass,DataType,UserDataType>::Get(aKey, pData);
  RTSemFastMutexRelease(this->mLock);

  return res;
}

template<class KeyClass,class DataType,class UserDataType>
PRBool
nsBaseHashtableMT<KeyClass,DataType,UserDataType>::Put(KeyType      aKey,
                                                           UserDataType aData)
{
  RTSemFastMutexRequest(this->mLock);
  PRBool res =
    nsBaseHashtable<KeyClass,DataType,UserDataType>::Put(aKey, aData);
  RTSemFastMutexRelease(this->mLock);

  return res;
}

template<class KeyClass,class DataType,class UserDataType>
void
nsBaseHashtableMT<KeyClass,DataType,UserDataType>::Remove(KeyType aKey)
{
  RTSemFastMutexRequest(this->mLock);
  nsBaseHashtable<KeyClass,DataType,UserDataType>::Remove(aKey);
  RTSemFastMutexRelease(this->mLock);
}

template<class KeyClass,class DataType,class UserDataType>
PRUint32
nsBaseHashtableMT<KeyClass,DataType,UserDataType>::EnumerateRead
  (EnumReadFunction fEnumCall, void* userArg) const
{
  RTSemFastMutexRequest(this->mLock);
  PRUint32 count =
    nsBaseHashtable<KeyClass,DataType,UserDataType>::EnumerateRead(fEnumCall, userArg);
  RTSemFastMutexRelease(this->mLock);

  return count;
}

template<class KeyClass,class DataType,class UserDataType>
PRUint32
nsBaseHashtableMT<KeyClass,DataType,UserDataType>::Enumerate
  (EnumFunction fEnumCall, void* userArg)
{
  RTSemFastMutexRequest(this->mLock);
  PRUint32 count =
    nsBaseHashtable<KeyClass,DataType,UserDataType>::Enumerate(fEnumCall, userArg);
  RTSemFastMutexRelease(this->mLock);

  return count;
}

template<class KeyClass,class DataType,class UserDataType>
void
nsBaseHashtableMT<KeyClass,DataType,UserDataType>::Clear()
{
  RTSemFastMutexRequest(this->mLock);
  nsBaseHashtable<KeyClass,DataType,UserDataType>::Clear();
  RTSemFastMutexRelease(this->mLock);
}

#endif // nsBaseHashtable_h__
