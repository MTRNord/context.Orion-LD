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
#include <bson/bson.h>                                           // bson_t, ...
#include <mongoc/mongoc.h>                                       // MongoDB C Client Driver

extern "C"
{
#include "kjson/KjNode.h"                                        // KjNode
}

#include "logMsg/logMsg.h"                                       // LM_*

#include "orionld/common/orionldState.h"                         // orionldState
#include "orionld/mongoc/mongocConnectionGet.h"                  // mongocConnectionGet
#include "orionld/mongoc/mongocKjTreeToBson.h"                   // mongocKjTreeToBson
#include "orionld/mongoc/mongocEntitiesUpsert.h"                 // Own interface



// -----------------------------------------------------------------------------
//
// mongocEntitiesUpsert -
//
bool mongocEntitiesUpsert(KjNode* entitiesArrayP)
{
  mongocConnectionGet();  // mongocConnectionGet(MONGO_ENTITIES) - do the mongoc_client_get_collection also

  if (orionldState.mongoc.entitiesP == NULL)
    orionldState.mongoc.entitiesP = mongoc_client_get_collection(orionldState.mongoc.client, orionldState.tenantP->mongoDbName, "entities");

  mongoc_bulk_operation_t* bulkP;

  bulkP = mongoc_collection_create_bulk_operation_with_opts(orionldState.mongoc.entitiesP, NULL);

  for (KjNode* entityP = entitiesArrayP->value.firstChildP; entityP != NULL; entityP = entityP->next)
  {
    bson_t doc;

    bson_init(&doc);
    mongocKjTreeToBson(entityP, &doc);  // The entity needs to be DB-Prepared !
    mongoc_bulk_operation_insert(bulkP, &doc);
    bson_destroy(&doc);
  }

  bson_error_t error;
  bson_t       reply;
  bool r = mongoc_bulk_operation_execute(bulkP, &reply, &error);

  if (r == false)
  {
    char* errorString = bson_as_canonical_extended_json(&reply, NULL);
    LM_E(("mongoc_bulk_operation_execute: %s", errorString));
    bson_free(errorString);
  }

  bson_destroy(&reply);
  mongoc_bulk_operation_destroy(bulkP);

  return r;
}
