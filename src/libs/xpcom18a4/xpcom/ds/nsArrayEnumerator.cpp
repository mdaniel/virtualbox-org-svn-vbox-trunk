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
 * The Original Code is XPCOM Array implementation.
 *
 * The Initial Developer of the Original Code
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
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

#include "nsArrayEnumerator.h"

NS_IMPL_ISUPPORTS1(nsSimpleArrayEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
nsSimpleArrayEnumerator::HasMoreElements(PRBool* aResult)
{
    NS_PRECONDITION(aResult != 0, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (!mValueArray) {
        *aResult = PR_FALSE;
        return NS_OK;
    }

    PRUint32 cnt;
    nsresult rv = mValueArray->GetLength(&cnt);
    if (NS_FAILED(rv)) return rv;
    *aResult = (mIndex < cnt);
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleArrayEnumerator::GetNext(nsISupports** aResult)
{
    NS_PRECONDITION(aResult != 0, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (!mValueArray) {
        *aResult = nsnull;
        return NS_OK;
    }

    PRUint32 cnt;
    nsresult rv = mValueArray->GetLength(&cnt);
    if (NS_FAILED(rv)) return rv;
    if (mIndex >= cnt)
        return NS_ERROR_UNEXPECTED;

    return mValueArray->QueryElementAt(mIndex++, NS_GET_IID(nsISupports), (void**)aResult);
}

extern NS_COM nsresult
NS_NewArrayEnumerator(nsISimpleEnumerator* *result,
                      nsIArray* array)
{
    nsSimpleArrayEnumerator* enumer = new nsSimpleArrayEnumerator(array);
    if (enumer == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*result = enumer);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

// enumerator implementation for nsCOMArray
// creates a snapshot of the array in question
// you MUST use NS_NewArrayEnumerator to create this, so that
// allocation is done correctly
class nsCOMArrayEnumerator : public nsISimpleEnumerator
{
public:
    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator interface
    NS_DECL_NSISIMPLEENUMERATOR

    // nsSimpleArrayEnumerator methods
    nsCOMArrayEnumerator() : mIndex(0), mArraySize(0) {
    }

    // specialized operator to make sure we make room for mValues
    void* operator new (size_t size, const nsCOMArray_base& aArray) CPP_THROW_NEW;
    void operator delete(void* ptr) {
        ::operator delete(ptr);
    }

private:
    ~nsCOMArrayEnumerator(void);

protected:
    PRUint32 mIndex;            // current position
    PRUint32 mArraySize;        // size of the array
    
    // this is actually bigger
    nsISupports* mValueArray[1];
};

NS_IMPL_ISUPPORTS1(nsCOMArrayEnumerator, nsISimpleEnumerator)

nsCOMArrayEnumerator::~nsCOMArrayEnumerator()
{
    // only release the entries that we haven't visited yet
    for (; mIndex < mArraySize; ++mIndex) {
        NS_IF_RELEASE(mValueArray[mIndex]);
    }
}

NS_IMETHODIMP
nsCOMArrayEnumerator::HasMoreElements(PRBool* aResult)
{
    NS_PRECONDITION(aResult != 0, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    *aResult = (mIndex < mArraySize);
    return NS_OK;
}

NS_IMETHODIMP
nsCOMArrayEnumerator::GetNext(nsISupports** aResult)
{
    NS_PRECONDITION(aResult != 0, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (mIndex >= mArraySize)
        return NS_ERROR_UNEXPECTED;

    // pass the ownership of the reference to the caller. Since
    // we AddRef'ed during creation of |this|, there is no need
    // to AddRef here
    *aResult = mValueArray[mIndex++];

    // this really isn't necessary. just pretend this happens, since
    // we'll never visit this value again!
    // mValueArray[(mIndex-1)] = nsnull;
    
    return NS_OK;
}

void*
nsCOMArrayEnumerator::operator new (size_t size, const nsCOMArray_base& aArray)
    CPP_THROW_NEW
{
    // create enough space such that mValueArray points to a large
    // enough value. Note that the initial value of size gives us
    // space for mValueArray[0], so we must subtract
    size += (aArray.Count() - 1) * sizeof(aArray[0]);

    // do the actual allocation
    nsCOMArrayEnumerator * result =
        NS_STATIC_CAST(nsCOMArrayEnumerator*, ::operator new(size));

    // now need to copy over the values, and addref each one
    // now this might seem like alot of work, but we're actually just
    // doing all our AddRef's ahead of time since GetNext() doesn't
    // need to AddRef() on the way out
    PRUint32 i;
    PRUint32 max = result->mArraySize = aArray.Count();
    for (i = 0; i<max; i++) {
        result->mValueArray[i] = aArray[i];
        NS_IF_ADDREF(result->mValueArray[i]);
    }

    return result;
}

extern NS_COM nsresult
NS_NewArrayEnumerator(nsISimpleEnumerator* *aResult,
                      const nsCOMArray_base& aArray)
{
    nsCOMArrayEnumerator *enumerator = new (aArray) nsCOMArrayEnumerator();
    if (!enumerator) return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = enumerator);
    return NS_OK;
}
