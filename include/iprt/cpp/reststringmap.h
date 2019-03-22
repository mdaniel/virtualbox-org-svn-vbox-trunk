/** @file
 * IPRT - C++ Representational State Transfer (REST) String Map Template.
 */

/*
 * Copyright (C) 2008-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___iprt_cpp_reststringmap_h
#define ___iprt_cpp_reststringmap_h

#include <iprt/list.h>
#include <iprt/string.h>
#include <iprt/cpp/restbase.h>


/** @defgroup grp_rt_cpp_reststingmap   C++ Representational State Transfer (REST) String Map Template
 * @ingroup grp_rt_cpp
 * @{
 */

/**
 * Abstract base class for the RTCRestStringMap template.
 */
class RT_DECL_CLASS RTCRestStringMapBase : public RTCRestObjectBase
{
public:
    /** Default destructor. */
    RTCRestStringMapBase();
    /** Copy constructor. */
    RTCRestStringMapBase(RTCRestStringMapBase const &a_rThat);
    /** Destructor. */
    virtual ~RTCRestStringMapBase();
    /** Copy assignment operator. */
    RTCRestStringMapBase &operator=(RTCRestStringMapBase const &a_rThat);

    /* Overridden methods: */
    virtual int resetToDefault() RT_OVERRIDE;
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE;
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    // later?
    //virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = kCollectionFormat_Unspecified) const RT_OVERRIDE;
    //virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
    //                       uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE;
    virtual kTypeClass typeClass(void) const RT_OVERRIDE;
    virtual const char *typeName(void) const RT_OVERRIDE;

    /**
     * Clear the content of the map.
     */
    void clear();

    /**
     * Gets the number of entries in the map.
     */
    size_t size() const;

    /**
     * Checks if the map contains the given key.
     * @returns true if key found, false if not.
     * @param   a_pszKey   The key to check fo.
     */
    bool constainsKey(const char *a_pszKey) const;

    /**
     * Checks if the map contains the given key.
     * @returns true if key found, false if not.
     * @param   a_rStrKey   The key to check fo.
     */
    bool constainsKey(RTCString const &a_rStrKey) const;

    /**
     * Remove any key-value pair with the given key.
     * @returns true if anything was removed, false if not found.
     * @param   a_pszKey    The key to remove.
     */
    bool remove(const char *a_pszKey);

    /**
     * Remove any key-value pair with the given key.
     * @returns true if anything was removed, false if not found.
     * @param   a_rStrKey   The key to remove.
     */
    bool remove(RTCString const &a_rStrKey);

    /**
     * Creates a new value and inserts it under the given key, returning the new value.
     *
     * @returns VINF_SUCCESS or VWRN_ALREADY_EXISTS on success.
     *          VERR_ALREADY_EXISTS, VERR_NO_MEMORY or VERR_NO_STR_MEMORY on failure.
     * @param   a_ppValue   Where to return the pointer to the value.
     * @param   a_pszKey    The key to put it under.
     * @param   a_cchKey    The length of the key.  Default is the entire string.
     * @param   a_fReplace  Whether to replace or fail on key collision.
     */
    int putNewValue(RTCRestObjectBase **a_ppValue, const char *a_pszKey, size_t a_cchKey = RTSTR_MAX, bool a_fReplace = false);

    /**
     * Creates a new value and inserts it under the given key, returning the new value.
     *
     * @returns VINF_SUCCESS or VWRN_ALREADY_EXISTS on success.
     *          VERR_ALREADY_EXISTS, VERR_NO_MEMORY or VERR_NO_STR_MEMORY on failure.
     * @param   a_ppValue   Where to return the pointer to the value.
     * @param   a_rStrKey   The key to put it under.
     * @param   a_fReplace  Whether to replace or fail on key collision.
     */
    int putNewValue(RTCRestObjectBase **a_ppValue, RTCString const &a_rStrKey, bool a_fReplace = false);

protected:
    /** Map entry. */
    typedef struct MapEntry
    {
        /** String space core. */
        RTSTRSPACECORE      Core;
        /** List node for enumeration. */
        RTLISTNODE          ListEntry;
        /** The key.
         * @remarks Core.pszString points to the value of this object.  So, consider it const. */
        RTCString           strKey;
        /** The value. */
        RTCRestObjectBase  *pValue;
    } MapEntry;
    /** The map tree. */
    RTSTRSPACE          m_Map;
    /** The enumeration list head (MapEntry). */
    RTLISTANCHOR        m_ListHead;
    /** Number of map entries. */
    size_t              m_cEntries;

public:
    /** @name Map Iteration
     * @{  */
    /** Const iterator. */
    class ConstIterator
    {
    private:
        MapEntry            *m_pCur;
        ConstIterator();
    protected:
        ConstIterator(MapEntry *a_pEntry) : m_pCur(a_pEntry) { }
    public:
        ConstIterator(ConstIterator const &a_rThat) : m_pCur(a_rThat.m_pCur) { }

        /** Gets the key string. */
        RTCString const         &getKey()   { return m_pCur->strKey; }
        /** Gets poitner to the value object. */
        RTCRestObjectBase const *getValue() { return m_pCur->pValue; }

        /** Advance to the next map entry. */
        ConstIterator &operator++()
        {
            m_pCur = RTListNodeGetNextCpp(&m_pCur->ListEntry, MapEntry, ListEntry);
            return *this;
        }

        /** Advance to the previous map entry. */
        ConstIterator &operator--()
        {
            m_pCur = RTListNodeGetPrevCpp(&m_pCur->ListEntry, MapEntry, ListEntry);
            return *this;
        }

        /** Compare equal. */
        bool operator==(ConstIterator const &a_rThat) { return m_pCur == a_rThat.m_pCur; }
        /** Compare not equal. */
        bool operator!=(ConstIterator const &a_rThat) { return m_pCur != a_rThat.m_pCur; }

        /* Map class must be friend so it can use the MapEntry constructor. */
        friend class RTCRestStringMapBase;
    };

    /** Returns iterator for the first map entry (unless it's empty and it's also the end). */
    ConstIterator begin() const { return ConstIterator(RTListGetFirstCpp(&m_ListHead, MapEntry, ListEntry)); }
    /** Returns iterator for the last map entry (unless it's empty and it's also the end). */
    ConstIterator last() const  { return ConstIterator(RTListGetLastCpp(&m_ListHead, MapEntry, ListEntry)); }
    /** Returns the end iterator.  This does not ever refer to an actual map entry. */
    ConstIterator end() const   { return ConstIterator(RT_FROM_CPP_MEMBER(&m_ListHead, MapEntry, ListEntry)); }
    /** @} */


protected:
    /**
     * Wrapper around the value constructor.
     *
     * @returns Pointer to new value object on success, NULL if out of memory.
     */
    virtual RTCRestObjectBase *createValue(void) = 0;

    /**
     * Wrapper around the value copy constructor.
     *
     * @returns Pointer to copy on success, NULL if out of memory.
     * @param   a_pSrc      The value to copy.
     */
    virtual RTCRestObjectBase *createValueCopy(RTCRestObjectBase const *a_pSrc) = 0;

    /**
     * Worker for the copy constructor and the assignment operator.
     *
     * This will use createEntryCopy to do the copying.
     *
     * @returns VINF_SUCCESS on success, VERR_NO_MEMORY or VERR_NO_STR_MEMORY on failure.
     * @param   a_rThat     The map to copy.  Caller makes 100% sure the it has
     *                      the same type as the destination.
     * @param   a_fThrow    Whether to throw error.
     */
    int copyMapWorker(RTCRestStringMapBase const &a_rThat, bool a_fThrow);

    /**
     * Worker for performing inserts.
     *
     * @returns VINF_SUCCESS or VWRN_ALREADY_EXISTS on success.
     *          VERR_ALREADY_EXISTS, VERR_NO_MEMORY or VERR_NO_STR_MEMORY on failure.
     * @param   a_pszKey        The key.
     * @param   a_pValue        The value to insert.  Ownership is transferred to the map on success.
     * @param   a_fReplace      Whether to replace existing key-value pair with matching key.
     * @param   a_cchKey        The key length, the whole string by default.
     */
    int putWorker(const char *a_pszKey, RTCRestObjectBase *a_pValue, bool a_fReplace, size_t a_cchKey = RTSTR_MAX);

    /**
     * Worker for performing inserts.
     *
     * @returns VINF_SUCCESS or VWRN_ALREADY_EXISTS on success.
     *          VERR_ALREADY_EXISTS, VERR_NO_MEMORY or VERR_NO_STR_MEMORY on failure.
     * @param   a_pszKey        The key.
     * @param   a_rValue        The value to copy into the map.
     * @param   a_fReplace      Whether to replace existing key-value pair with matching key.
     * @param   a_cchKey        The key length, the whole string by default.
     */
    int putCopyWorker(const char *a_pszKey, RTCRestObjectBase const &a_rValue, bool a_fReplace, size_t a_cchKey = RTSTR_MAX);

    /**
     * Worker for getting the value corresponding to the given key.
     *
     * @returns Pointer to the value object if found, NULL if key not in the map.
     * @param   a_pszKey        The key which value to look up.
     */
    RTCRestObjectBase *getWorker(const char *a_pszKey);

    /**
     * Worker for getting the value corresponding to the given key, const variant.
     *
     * @returns Pointer to the value object if found, NULL if key not in the map.
     * @param   a_pszKey        The key which value to look up.
     */
    RTCRestObjectBase const *getWorker(const char *a_pszKey) const;

private:
    static DECLCALLBACK(int) stringSpaceDestructorCallback(PRTSTRSPACECORE pStr, void *pvUser);
};


/**
 * Limited map class.
 */
template<class ValueType> class RTCRestStringMap : public RTCRestStringMapBase
{
public:
    /** Default constructor, creates emtpy map. */
    RTCRestStringMap()
        : RTCRestStringMapBase()
    {}

    /** Copy constructor. */
    RTCRestStringMap(RTCRestStringMap const &a_rThat)
        : RTCRestStringMapBase()
    {
        copyMapWorker(a_rThat, true /*a_fThrow*/);
    }

    /** Destructor. */
    virtual ~RTCRestStringMap()
    {
       /* nothing to do here. */
    }

    /** Copy assignment operator. */
    RTCRestStringMap &operator=(RTCRestStringMap const &a_rThat)
    {
        copyMapWorker(a_rThat, true /*a_fThrow*/);
        return *this;
    }

    /** Safe copy assignment method. */
    int assignCopy(RTCRestStringMap const &a_rThat)
    {
        return copyMapWorker(a_rThat, false /*a_fThrow*/);
    }

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void)
    {
        return new (std::nothrow) RTCRestStringMap<ValueType>();
    }

    /** Factory method for values. */
    static DECLCALLBACK(RTCRestObjectBase *) createValueInstance(void)
    {
        return new (std::nothrow) ValueType();
    }

    /**
     * Inserts the given object into the map.
     *
     * @returns VINF_SUCCESS or VWRN_ALREADY_EXISTS on success.
     *          VERR_ALREADY_EXISTS, VERR_NO_MEMORY or VERR_NO_STR_MEMORY on failure.
     * @param   a_pszKey        The key.
     * @param   a_pValue        The value to insert.  Ownership is transferred to the map on success.
     * @param   a_fReplace      Whether to replace existing key-value pair with matching key.
     */
    int put(const char *a_pszKey, ValueType *a_pValue, bool a_fReplace = false)
    {
        return putWorker(a_pszKey, a_pValue, a_fReplace);
    }

    /**
     * Inserts the given object into the map.
     *
     * @returns VINF_SUCCESS or VWRN_ALREADY_EXISTS on success.
     *          VERR_ALREADY_EXISTS, VERR_NO_MEMORY or VERR_NO_STR_MEMORY on failure.
     * @param   a_rStrKey       The key.
     * @param   a_pValue        The value to insert.  Ownership is transferred to the map on success.
     * @param   a_fReplace      Whether to replace existing key-value pair with matching key.
     */
    int put(RTCString const &a_rStrKey, ValueType *a_pValue, bool a_fReplace = false)
    {
        return putWorker(a_rStrKey.c_str(), a_pValue, a_fReplace, a_rStrKey.length());
    }

    /**
     * Inserts a copy of the given object into the map.
     *
     * @returns VINF_SUCCESS or VWRN_ALREADY_EXISTS on success.
     *          VERR_ALREADY_EXISTS, VERR_NO_MEMORY or VERR_NO_STR_MEMORY on failure.
     * @param   a_pszKey        The key.
     * @param   a_rValue        The value to insert a copy of.
     * @param   a_fReplace      Whether to replace existing key-value pair with matching key.
     */
    int putCopy(const char *a_pszKey, const ValueType &a_rValue, bool a_fReplace = false)
    {
        return putCopyWorker(a_pszKey, a_rValue, a_fReplace);
    }

    /**
     * Inserts a copy of the given object into the map.
     *
     * @returns VINF_SUCCESS or VWRN_ALREADY_EXISTS on success.
     *          VERR_ALREADY_EXISTS, VERR_NO_MEMORY or VERR_NO_STR_MEMORY on failure.
     * @param   a_rStrKey       The key.
     * @param   a_rValue        The value to insert a copy of.
     * @param   a_fReplace      Whether to replace existing key-value pair with matching key.
     */
    int putCopy(RTCString const &a_rStrKey, const ValueType &a_rValue, bool a_fReplace = false)
    {
        return putCopyWorker(a_rStrKey.c_str(), a_rValue, a_fReplace, a_rStrKey.length());
    }

    /**
     * Gets the value corresponding to the given key.
     *
     * @returns Pointer to the value object if found, NULL if key not in the map.
     * @param   a_pszKey        The key which value to look up.
     */
    ValueType *get(const char *a_pszKey)
    {
        return (ValueType *)getWorker(a_pszKey);
    }

    /**
     * Gets the value corresponding to the given key.
     *
     * @returns Pointer to the value object if found, NULL if key not in the map.
     * @param   a_rStrKey       The key which value to look up.
     */
    ValueType *get(RTCString const &a_rStrKey)
    {
        return (ValueType *)getWorker(a_rStrKey.c_str());
    }

    /**
     * Gets the const value corresponding to the given key.
     *
     * @returns Pointer to the value object if found, NULL if key not in the map.
     * @param   a_pszKey        The key which value to look up.
     */
    ValueType const *get(const char *a_pszKey) const
    {
        return (ValueType const *)getWorker(a_pszKey);
    }

    /**
     * Gets the const value corresponding to the given key.
     *
     * @returns Pointer to the value object if found, NULL if key not in the map.
     * @param   a_rStrKey       The key which value to look up.
     */
    ValueType const *get(RTCString const &a_rStrKey) const
    {
        return (ValueType const *)getWorker(a_rStrKey.c_str());
    }

    /** @todo enumerator*/

protected:
    virtual RTCRestObjectBase *createValue(void) RT_OVERRIDE
    {
        return new (std::nothrow) ValueType();
    }

    virtual RTCRestObjectBase *createValueCopy(RTCRestObjectBase const *a_pSrc) RT_OVERRIDE
    {
        ValueType *pCopy = new (std::nothrow) ValueType();
        if (pCopy)
        {
            int rc = pCopy->assignCopy(*(ValueType const *)a_pSrc);
            if (RT_SUCCESS(rc))
                return pCopy;
            delete pCopy;
        }
        return NULL;
    }
};


/** @} */

#endif

