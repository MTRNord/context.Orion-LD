/*
*
* Copyright 2022 FIWARE Foundation e.V.
*
* This file is part of Orion-LD Context Broker.
*
* Orion-LD Context Broker is free software: you can redistribute it and/or
* modify it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* Orion-LD Context Broker is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
* General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with Orion-LD Context Broker. If not, see http://www.gnu.org/licenses/.
*
* For those usages not covered by this license please contact with
* orionld at fiware dot org
*
* Author: Ken Zangelin
*/
#include <mongoc/mongoc.h>                                       // MongoDB C Client Driver

extern "C"
{
#include "kjson/KjNode.h"                                        // KjNode
}

#include "logMsg/logMsg.h"                                       // LM_*

#include "orionld/common/orionldState.h"                         // orionldState
#include "orionld/mongoc/mongocConnectionGet.h"                  // mongocConnectionGet
#include "orionld/mongoc/mongocKjTreeFromBson.h"                 // mongocKjTreeFromBson
#include "orionld/mongoc/mongocWriteLog.h"                       // MONGOC_WLOG - FIXME: change name to mongocLog.h
#include "orionld/mongoc/mongocSubscriptionLookup.h"             // Own interface



// -----------------------------------------------------------------------------
//
// mongocSubscriptionLookup -
//
KjNode* mongocSubscriptionLookup(const char* subscriptionId)
{
  bson_t            mongoFilter;
  const bson_t*     mongoDocP;
  mongoc_cursor_t*  mongoCursorP;
  bson_error_t      mongoError;
  char*             title;
  char*             details;
  KjNode*           subscriptionNodeP = NULL;

  //
  // Create the filter for the query
  //
  bson_init(&mongoFilter);
  bson_append_utf8(&mongoFilter, "_id", 3, subscriptionId, -1);

  mongocConnectionGet();

  if (orionldState.mongoc.subscriptionsP == NULL)
    orionldState.mongoc.subscriptionsP = mongoc_client_get_collection(orionldState.mongoc.client, orionldState.tenantP->mongoDbName, "csubs");

  //
  // Run the query
  //
  // semTake(&mongoSubscriptionsSem);
  MONGOC_RLOG("Lookup Subscription", orionldState.tenantP->mongoDbName, "subscriptions", &mongoFilter, NULL, LmtMongoc);
  if ((mongoCursorP = mongoc_collection_find_with_opts(orionldState.mongoc.subscriptionsP, &mongoFilter, NULL, NULL)) == NULL)
  {
    LM_E(("Internal Error (mongoc_collection_find_with_opts ERROR)"));
    return NULL;
  }

  while (mongoc_cursor_next(mongoCursorP, &mongoDocP))
  {
    subscriptionNodeP = mongocKjTreeFromBson(mongoDocP, &title, &details);
    break;  // Just using the first one - should be no more than one!
  }

  if (mongoc_cursor_error(mongoCursorP, &mongoError))
  {
    LM_E(("Internal Error (DB Error '%s')", mongoError.message));
    return NULL;
  }

  mongoc_cursor_destroy(mongoCursorP);
  // semGive(&mongoSubscriptionsSem);
  bson_destroy(&mongoFilter);

  return subscriptionNodeP;
}
