/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include <iprt/cdefs.h>

#include <stdio.h>
#include <stdlib.h>
#include "nsISupportsArray.h"

// {9e70a320-be02-11d1-8031-006008159b5a}
#define NS_IFOO_IID \
  {0x9e70a320, 0xbe02, 0x11d1,    \
    {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

static const PRBool kExitOnError = PR_TRUE;

class IFoo : public nsISupports {
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFOO_IID)

  NS_IMETHOD_(nsrefcnt) RefCnt() = 0;
  NS_IMETHOD_(PRInt32) ID() = 0;
};

class Foo : public IFoo {
public:

  Foo(PRInt32 aID);

  // nsISupports implementation
  NS_DECL_ISUPPORTS

  // IFoo implementation
  NS_IMETHOD_(nsrefcnt) RefCnt() { return mRefCnt; }
  NS_IMETHOD_(PRInt32) ID() { return mID; }

  static PRInt32 gCount;

  PRInt32 mID;

private:
  ~Foo();
};

PRInt32 Foo::gCount;

Foo::Foo(PRInt32 aID)
{
  mID = aID;
  ++gCount;
  fprintf(stdout, "init: %d (%p), %d total)\n", mID, this, gCount);
}

Foo::~Foo()
{
  --gCount;
  fprintf(stdout, "destruct: %d (%p), %d remain)\n", mID, this, gCount);
}

NS_IMPL_ISUPPORTS1(Foo, IFoo)

static const char* AssertEqual(PRInt32 aValue1, PRInt32 aValue2)
{
  if (aValue1 == aValue2) {
    return "OK";
  }
  if (kExitOnError) {
    exit(1);
  }
  return "ERROR";
}

static void DumpArray(nsISupportsArray* aArray, PRInt32 aExpectedCount, PRInt32 aElementIDs[], PRInt32 aExpectedTotal)
{
  PRUint32 cnt = 0;
  nsresult rv = aArray->Count(&cnt);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed"); RT_NOREF(rv);
  PRInt32 count = cnt;
  PRInt32 index;

  fprintf(stdout, "object count %d = %d %s\n", Foo::gCount, aExpectedTotal, 
          AssertEqual(Foo::gCount, aExpectedTotal));
  fprintf(stdout, "array count %d = %d %s\n", count, aExpectedCount,
          AssertEqual(count, aExpectedCount));
  
  for (index = 0; (index < count) && (index < aExpectedCount); index++) {
    IFoo* foo = (IFoo*)(aArray->ElementAt(index));
    fprintf(stdout, "%2d: %d=%d (%p) c: %d %s\n", 
            index, aElementIDs[index], foo->ID(), foo, foo->RefCnt() - 1,
            AssertEqual(foo->ID(), aElementIDs[index]));
    foo->Release();
  }
}

static void FillArray(nsISupportsArray* aArray, PRInt32 aCount)
{
  PRInt32 index;
  for (index = 0; index < aCount; index++) {
    nsCOMPtr<IFoo> foo = new Foo(index);
    aArray->AppendElement(foo);
  }
}

int main(int argc, char *argv[])
{
  nsISupportsArray* array;
  nsresult  rv;
  
  if (NS_OK == (rv = NS_NewISupportsArray(&array))) {
    FillArray(array, 10);
    fprintf(stdout, "Array created:\n");
    PRInt32   fillResult[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    DumpArray(array, 10, fillResult, 10);

    // test insert
    IFoo* foo = (IFoo*)array->ElementAt(3);
    foo->Release();  // pre-release to fix ref count for dumps
    array->InsertElementAt(foo, 5);
    fprintf(stdout, "insert 3 at 5:\n");
    PRInt32   insertResult[11] = {0, 1, 2, 3, 4, 3, 5, 6, 7, 8, 9};
    DumpArray(array, 11, insertResult, 10);
    fprintf(stdout, "insert 3 at 0:\n");
    array->InsertElementAt(foo, 0);
    PRInt32   insertResult2[12] = {3, 0, 1, 2, 3, 4, 3, 5, 6, 7, 8, 9};
    DumpArray(array, 12, insertResult2, 10);
    fprintf(stdout, "append 3:\n");
    array->AppendElement(foo);
    PRInt32   appendResult[13] = {3, 0, 1, 2, 3, 4, 3, 5, 6, 7, 8, 9, 3};
    DumpArray(array, 13, appendResult, 10);


    // test IndexOf && LastIndexOf
    PRInt32 expectedIndex[5] = {0, 4, 6, 12, -1};
    PRInt32 count = 0;
    PRInt32 index = array->IndexOf(foo);
    fprintf(stdout, "IndexOf(foo): %d=%d %s\n", index, expectedIndex[count], 
            AssertEqual(index, expectedIndex[count]));
    while (-1 != index) {
      count++;
      index = array->IndexOfStartingAt(foo, index + 1);
      if (-1 != index)
        fprintf(stdout, "IndexOf(foo): %d=%d %s\n", index, expectedIndex[count], 
                AssertEqual(index, expectedIndex[count]));
    }
    index = array->LastIndexOf(foo);
    count--;
    fprintf(stdout, "LastIndexOf(foo): %d=%d %s\n", index, expectedIndex[count], 
            AssertEqual(index, expectedIndex[count]));

    // test ReplaceElementAt
    fprintf(stdout, "ReplaceElementAt(8):\n");
    array->ReplaceElementAt(foo, 8);
    PRInt32   replaceResult[13] = {3, 0, 1, 2, 3, 4, 3, 5, 3, 7, 8, 9, 3};
    DumpArray(array, 13, replaceResult, 9);

    // test RemoveElementAt, RemoveElement RemoveLastElement
    fprintf(stdout, "RemoveElementAt(0):\n");
    array->RemoveElementAt(0);
    PRInt32   removeResult[12] = {0, 1, 2, 3, 4, 3, 5, 3, 7, 8, 9, 3};
    DumpArray(array, 12, removeResult, 9);
    fprintf(stdout, "RemoveElementAt(7):\n");
    array->RemoveElementAt(7);
    PRInt32   removeResult2[11] = {0, 1, 2, 3, 4, 3, 5, 7, 8, 9, 3};
    DumpArray(array, 11, removeResult2, 9);
    fprintf(stdout, "RemoveElement(foo):\n");
    array->RemoveElement(foo);
    PRInt32   removeResult3[10] = {0, 1, 2, 4, 3, 5, 7, 8, 9, 3};
    DumpArray(array, 10, removeResult3, 9);
    fprintf(stdout, "RemoveLastElement(foo):\n");
    array->RemoveLastElement(foo);
    PRInt32   removeResult4[9] = {0, 1, 2, 4, 3, 5, 7, 8, 9};
    DumpArray(array, 9, removeResult4, 9);

    // test clear
    fprintf(stdout, "clear array:\n");
    array->Clear();
    DumpArray(array, 0, 0, 0);
    fprintf(stdout, "add 4 new:\n");
    FillArray(array, 4);
    DumpArray(array, 4, fillResult, 4);

    // test compact
    fprintf(stdout, "compact array:\n");
    array->Compact();
    DumpArray(array, 4, fillResult, 4);

    // test delete
    fprintf(stdout, "release array:\n");
    NS_RELEASE(array);
  }
  else {
    fprintf(stdout, "error can't create array: %x\n", rv);
  }

  return 0;
}
