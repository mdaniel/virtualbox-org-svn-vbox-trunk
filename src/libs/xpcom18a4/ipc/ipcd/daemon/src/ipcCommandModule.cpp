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
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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
#define LOG_GROUP LOG_GROUP_IPC
#include <stdlib.h>
#include <string.h>
#include "ipcCommandModule.h"
#include "ipcClient.h"
#include "ipcMessage.h"
#include "ipcMessageUtils.h"
#include "ipcd.h"
#include "ipcm.h"

#include <VBox/log.h>

struct ipcCommandModule
{
    typedef void (* MsgHandler)(ipcClient *, const ipcMessage *);

    //
    // helpers
    //

    static char **
    BuildStringArray(const ipcStringNode *nodes)
    {
        size_t count = 0;

        const ipcStringNode *node;

        for (node = nodes; node; node = node->mNext)
            count++;

        char **strs = (char **) malloc((count + 1) * sizeof(char *));
        if (!strs)
            return NULL;

        count = 0;
        for (node = nodes; node; node = node->mNext, ++count)
            strs[count] = (char *) node->Value();
        strs[count] = 0;

        return strs;
    }

    static nsID **
    BuildIDArray(const ipcIDNode *nodes)
    {
        size_t count = 0;

        const ipcIDNode *node;

        for (node = nodes; node; node = node->mNext)
            count++;

        nsID **ids = (nsID **) calloc(count + 1, sizeof(nsID *));
        if (!ids)
            return NULL;

        count = 0;
        for (node = nodes; node; node = node->mNext, ++count)
            ids[count] = (nsID *) &node->Value();

        return ids;
    }

    //
    // message handlers
    //

    static void
    OnPing(ipcClient *client, const ipcMessage *rawMsg)
    {
        Log(("got PING\n"));

        IPC_SendMsg(client, new ipcmMessageResult(IPCM_GetRequestIndex(rawMsg), IPCM_OK));
    }

    static void
    OnClientHello(ipcClient *client, const ipcMessage *rawMsg)
    {
        Log(("got CLIENT_HELLO\n"));

        IPC_SendMsg(client, new ipcmMessageClientID(IPCM_GetRequestIndex(rawMsg), client->ID()));

        //
        // NOTE: it would almost make sense for this notification to live
        // in the transport layer code.  however, clients expect to receive
        // a CLIENT_ID as the first message following a CLIENT_HELLO, so we
        // must not allow modules to see a client until after we have sent
        // the CLIENT_ID message.
        //
        IPC_NotifyClientUp(client);
    }

    static void
    OnClientAddName(ipcClient *client, const ipcMessage *rawMsg)
    {
        Log(("got CLIENT_ADD_NAME\n"));

        PRInt32 status = IPCM_OK;
        PRUint32 requestIndex = IPCM_GetRequestIndex(rawMsg);

        ipcMessageCast<ipcmMessageClientAddName> msg(rawMsg);
        const char *name = msg->Name();
        if (name) {
            ipcClient *result = IPC_GetClientByName(msg->Name());
            if (result) {
                Log(("  client with such name already exists (ID = %d)\n", result->ID()));
                status = IPCM_ERROR_ALREADY_EXISTS;
            }
            else
                client->AddName(name);
        }
        else
            status = IPCM_ERROR_INVALID_ARG;

        IPC_SendMsg(client, new ipcmMessageResult(requestIndex, status));
    }

    static void
    OnClientDelName(ipcClient *client, const ipcMessage *rawMsg)
    {
        Log(("got CLIENT_DEL_NAME\n"));

        PRInt32 status = IPCM_OK;
        PRUint32 requestIndex = IPCM_GetRequestIndex(rawMsg);

        ipcMessageCast<ipcmMessageClientDelName> msg(rawMsg);
        const char *name = msg->Name();
        if (name) {
            if (!client->DelName(name)) {
                Log(("  client doesn't have name '%s'\n", name));
                status = IPCM_ERROR_NO_SUCH_DATA;
            }
        }
        else
            status = IPCM_ERROR_INVALID_ARG;

        IPC_SendMsg(client, new ipcmMessageResult(requestIndex, status));
    }

    static void
    OnClientAddTarget(ipcClient *client, const ipcMessage *rawMsg)
    {
        Log(("got CLIENT_ADD_TARGET\n"));

        PRInt32 status = IPCM_OK;
        PRUint32 requestIndex = IPCM_GetRequestIndex(rawMsg);

        ipcMessageCast<ipcmMessageClientAddTarget> msg(rawMsg);
        if (client->HasTarget(msg->Target())) {
            Log(("  target already defined for client\n"));
            status = IPCM_ERROR_ALREADY_EXISTS;
        }
        else
            client->AddTarget(msg->Target());

        IPC_SendMsg(client, new ipcmMessageResult(requestIndex, status));
    }

    static void
    OnClientDelTarget(ipcClient *client, const ipcMessage *rawMsg)
    {
        Log(("got CLIENT_DEL_TARGET\n"));

        PRInt32 status = IPCM_OK;
        PRUint32 requestIndex = IPCM_GetRequestIndex(rawMsg);

        ipcMessageCast<ipcmMessageClientDelTarget> msg(rawMsg);
        if (!client->DelTarget(msg->Target())) {
            Log(("  client doesn't have the given target\n"));
            status = IPCM_ERROR_NO_SUCH_DATA;
        }

        IPC_SendMsg(client, new ipcmMessageResult(requestIndex, status));
    }

    static void
    OnQueryClientByName(ipcClient *client, const ipcMessage *rawMsg)
    {
        Log(("got QUERY_CLIENT_BY_NAME\n"));

        PRUint32 requestIndex = IPCM_GetRequestIndex(rawMsg);

        ipcMessageCast<ipcmMessageQueryClientByName> msg(rawMsg);

        ipcClient *result = IPC_GetClientByName(msg->Name());
        if (result) {
            Log(("  client exists w/ ID = %u\n", result->ID()));
            IPC_SendMsg(client, new ipcmMessageClientID(requestIndex, result->ID()));
        }
        else {
            Log(("  client does not exist\n"));
            IPC_SendMsg(client, new ipcmMessageResult(requestIndex, IPCM_ERROR_NO_CLIENT));
        }
    }

#if 0
    static void
    OnQueryClientInfo(ipcClient *client, const ipcMessage *rawMsg)
    {
        Log(("got QUERY_CLIENT_INFO\n"));

        ipcMessageCast<ipcmMessageQueryClientInfo> msg(rawMsg);
        ipcClient *result = IPC_GetClientByID(msg->ClientID());
        if (result) {
            char **names = BuildStringArray(result->Names());
            nsID **targets = BuildIDArray(result->Targets());

            IPC_SendMsg(client, new ipcmMessageClientInfo(result->ID(),
                                                          msg->RequestIndex(),
                                                          (const char **) names,
                                                          (const nsID **) targets));

            free(names);
            free(targets);
        }
        else {
            Log(("  client does not exist\n"));
            IPC_SendMsg(client, new ipcmMessageError(IPCM_ERROR_NO_CLIENT, msg->RequestIndex()));
        }
    }
#endif

    static void
    OnForward(ipcClient *client, const ipcMessage *rawMsg)
    {
        Log(("got FORWARD\n"));

        ipcMessageCast<ipcmMessageForward> msg(rawMsg);

        ipcClient *dest = IPC_GetClientByID(msg->ClientID());
        if (!dest) {
            Log(("  destination client not found!\n"));
            IPC_SendMsg(client, new ipcmMessageResult(IPCM_GetRequestIndex(rawMsg), IPCM_ERROR_NO_CLIENT));
            return;
        }
        // inform client that its message will be forwarded
        IPC_SendMsg(client, new ipcmMessageResult(IPCM_GetRequestIndex(rawMsg), IPCM_OK));

        ipcMessage *newMsg = new ipcmMessageForward(IPCM_MSG_PSH_FORWARD,
                                                    client->ID(),
                                                    msg->InnerTarget(),
                                                    msg->InnerData(),
                                                    msg->InnerDataLen());
        IPC_SendMsg(dest, newMsg);
    }
};

void
IPCM_HandleMsg(ipcClient *client, const ipcMessage *rawMsg)
{
    static ipcCommandModule::MsgHandler handlers[] =
    {
        ipcCommandModule::OnPing,
        ipcCommandModule::OnForward,
        ipcCommandModule::OnClientHello,
        ipcCommandModule::OnClientAddName,
        ipcCommandModule::OnClientDelName,
        ipcCommandModule::OnClientAddTarget,
        ipcCommandModule::OnClientDelTarget,
        ipcCommandModule::OnQueryClientByName
    };

    int type = IPCM_GetType(rawMsg);
    Log(("IPCM_HandleMsg [type=%x]\n", type));

    if (!(type & IPCM_MSG_CLASS_REQ)) {
        Log(("not a request -- ignoring message\n"));
        return;
    }

    type &= ~IPCM_MSG_CLASS_REQ;
    type--;
    if (type < 0 || type >= (int) (sizeof(handlers)/sizeof(handlers[0]))) {
        Log(("unknown request -- ignoring message\n"));
        return;
    }

    (handlers[type])(client, rawMsg);
}
