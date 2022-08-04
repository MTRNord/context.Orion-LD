/*
*
* Copyright 2019 FIWARE Foundation e.V.
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
* Author: Gabriel Quaresma and Ken Zangelin
*/
#include <string>                                              // std::string
#include <vector>                                              // std::vector

extern "C"
{
#include "kbase/kMacros.h"                                     // K_FT
#include "kjson/KjNode.h"                                      // KjNode
#include "kjson/kjBuilder.h"                                   // kjString, kjObject, ...
#include "kjson/kjLookup.h"                                    // kjLookup
#include "kjson/kjClone.h"                                     // kjClone
#include "kjson/kjRender.h"                                    // kjFastRender    - DEBUG
}

#include "logMsg/logMsg.h"                                     // LM_*
#include "logMsg/traceLevels.h"                                // Lmt*

#include "common/globals.h"                                    // parse8601Time
#include "orionTypes/OrionValueType.h"                         // orion::ValueType
#include "orionTypes/UpdateActionType.h"                       // ActionType
#include "parse/CompoundValueNode.h"                           // CompoundValueNode
#include "ngsi/ContextAttribute.h"                             // ContextAttribute
#include "ngsi10/UpdateContextRequest.h"                       // UpdateContextRequest
#include "ngsi10/UpdateContextResponse.h"                      // UpdateContextResponse
#include "mongoBackend/mongoUpdateContext.h"                   // mongoUpdateContext
#include "rest/uriParamNames.h"                                // URI_PARAM_PAGINATION_OFFSET, URI_PARAM_PAGINATION_LIMIT
#include "mongoBackend/MongoGlobal.h"                          // getMongoConnection()

#include "orionld/common/orionldState.h"                       // orionldState
#include "orionld/common/orionldError.h"                       // orionldError
#include "orionld/common/SCOMPARE.h"                           // SCOMPAREx
#include "orionld/common/CHECK.h"                              // CHECK
#include "orionld/common/entitySuccessPush.h"                  // entitySuccessPush
#include "orionld/common/entityErrorPush.h"                    // entityErrorPush
#include "orionld/common/entityIdCheck.h"                      // entityIdCheck
#include "orionld/common/entityTypeCheck.h"                    // entityTypeCheck
#include "orionld/common/entityIdAndTypeGet.h"                 // entityIdAndTypeGet
#include "orionld/common/entityLookupById.h"                   // entityLookupById
#include "orionld/common/typeCheckForNonExistingEntities.h"    // typeCheckForNonExistingEntities
#include "orionld/common/duplicatedInstances.h"                // duplicatedInstances
#include "orionld/common/tenantList.h"                         // tenant0
#include "orionld/types/OrionldProblemDetails.h"               // OrionldProblemDetails
#include "orionld/rest/orionldServiceInit.h"                   // orionldHostName, orionldHostNameLen
#include "orionld/context/orionldCoreContext.h"                // orionldDefaultUrl, orionldCoreContext
#include "orionld/context/orionldContextPresent.h"             // orionldContextPresent
#include "orionld/context/orionldContextItemAliasLookup.h"     // orionldContextItemAliasLookup
#include "orionld/context/orionldContextFromTree.h"            // orionldContextFromTree
#include "orionld/kjTree/kjStringValueLookupInArray.h"         // kjStringValueLookupInArray
#include "orionld/kjTree/kjEntityIdLookupInEntityArray.h"      // kjEntityIdLookupInEntityArray
#include "orionld/kjTree/kjTreeToUpdateContextRequest.h"       // kjTreeToUpdateContextRequest
#include "orionld/kjTree/kjEntityIdArrayExtract.h"             // kjEntityIdArrayExtract
#include "orionld/kjTree/kjEntityArrayErrorPurge.h"            // kjEntityArrayErrorPurge
#include "orionld/payloadCheck/pCheckEntity.h"                 // pCheckEntity
#include "orionld/legacyDriver/legacyPostBatchUpsert.h"        // Own Interface



// -----------------------------------------------------------------------------
//
// entityTypeGet - lookup 'type' in a KjTree
//
static char* entityTypeGet(KjNode* entityNodeP, KjNode** contextNodePP)
{
  char* type = NULL;

  for (KjNode* itemP = entityNodeP->value.firstChildP; itemP != NULL; itemP = itemP->next)
  {
    if (SCOMPARE5(itemP->name, 't', 'y', 'p', 'e', 0) || SCOMPARE6(itemP->name, '@', 't', 'y', 'p', 'e', 0))
      type = itemP->value.s;
    if (SCOMPARE9(itemP->name, '@', 'c', 'o', 'n', 't', 'e', 'x', 't', 0))
      *contextNodePP = itemP;
  }

  return type;
}



// -----------------------------------------------------------------------------
//
// entityTypeAndCreDateGet -
//
static void entityTypeAndCreDateGet(KjNode* dbEntityP, char** idP, char** typeP, double* creDateP)
{
  for (KjNode* nodeP = dbEntityP->value.firstChildP; nodeP != NULL; nodeP = nodeP->next)
  {
    if (SCOMPARE3(nodeP->name, 'i', 'd', 0) || SCOMPARE4(nodeP->name, '@', 'i', 'd', 0))
      *idP = nodeP->value.s;
    else if (SCOMPARE5(nodeP->name, 't', 'y', 'p', 'e', 0) || SCOMPARE6(nodeP->name, '@', 't', 'y', 'p', 'e', 0))
      *typeP = nodeP->value.s;
    else if (SCOMPARE8(nodeP->name, 'c', 'r', 'e', 'D', 'a', 't', 'e', 0))
    {
      if (nodeP->type == KjFloat)
        *creDateP = nodeP->value.f;
      else if (nodeP->type == KjInt)
        *creDateP = (double) nodeP->value.i;
    }
  }
}


// ----------------------------------------------------------------------------
//
// entityLookupInDb - lookup an entioty id in an array of entities
//
// idTypeAndCreDateFromDb looks like this:
// [
//   {
//     "id":"urn:ngsi-ld:entity:E1",
//     "type":"https://uri.etsi.org/ngsi-ld/default-context/Vehicle",
//     "creDate":1619077285.144012
//   },
//   {
//     "id":"urn:ngsi-ld:entity:E2",
//     "type":"https://uri.etsi.org/ngsi-ld/default-context/Vehicle",
//     "creDate":1619077285.144012
//   }
// ]
static KjNode* entityLookupInDb(KjNode* idTypeAndCreDateFromDb, const char* entityId)
{
  if (idTypeAndCreDateFromDb == NULL)
    return NULL;

  for (KjNode* entityP = idTypeAndCreDateFromDb->value.firstChildP; entityP != NULL; entityP = entityP->next)
  {
    KjNode* idNodeP = kjLookup(entityP, "id");

    if ((idNodeP != NULL) && (idNodeP->type == KjString))
    {
      if (strcmp(idNodeP->value.s, entityId) == 0)
        return entityP;
    }
  }

  return NULL;
}



// ----------------------------------------------------------------------------
//
// legacyPostBatchUpsert -
//
// POST /ngsi-ld/v1/entityOperations/upsert
//
// From the spec:
//   This operation allows creating a batch of NGSI-LD Entities, updating each of them if they already exist.
//
//   An optional flag indicating the update mode (only applies in case the Entity already exists):
//     - ?options=replace  (default)
//     - ?options=update
//
//   Replace:  All the existing Entity content shall be replaced  - like PUT
//   Update:   Existing Entity content shall be updated           - like PATCH
//
bool legacyPostBatchUpsert(void)
{
  KjNode* incomingTree   = orionldState.requestTree;
  KjNode* createdArrayP  = kjArray(orionldState.kjsonP, "created");
  KjNode* updatedArrayP  = kjArray(orionldState.kjsonP, "updated");
  KjNode* errorsArrayP   = kjArray(orionldState.kjsonP, "errors");

  // Error or not, the Link header should never be present in the reponse
  orionldState.noLinkHeader = true;

  // The response is never JSON-LD
  orionldState.out.contentType = JSON;

  //
  // Prerequisites for URI params:
  // * both 'update' and 'replace' cannot be set in options (replace is default)
  //
  if ((orionldState.uriParamOptions.update == true) && (orionldState.uriParamOptions.replace == true))
  {
    orionldError(OrionldBadRequestData, "URI Param Error", "options: both /update/ and /replace/ present", 400);
    return false;
  }

  //
  // Prerequisites for the payload in orionldState.requestTree:
  // * must be an array with objects
  // * cannot be empty
  // * all entities must contain an entity::id (one level down)
  // * no entity can contain an entity::type (one level down)
  //
  ARRAY_CHECK(orionldState.requestTree, "toplevel");
  EMPTY_ARRAY_CHECK(orionldState.requestTree, "toplevel");


  //
  // 00. Checking and expanding items in the incoming payload
  //
  // FIXME: pCheckEntity/pCheckAttribute use orionldState.contextP as context
  //        Because of this, I need to save it, modify it before each call to pCheckEntity
  //        and then out it back after the loop
  //        Would be better to have the context as a parameter to pCheckEntity/pCheckAttribute
  //
  OrionldContext*  savedContextP = orionldState.contextP;
  KjNode*          entityP       = orionldState.requestTree->value.firstChildP;
  KjNode*          next;

  while (entityP != NULL)
  {
    next = entityP->next;

    KjNode*                idNodeP      = kjLookup(entityP, "id");        // To populate removeArray
    KjNode*                atidNodeP    = kjLookup(entityP, "@id");       // To populate removeArray
    KjNode*                contextNodeP = kjLookup(entityP, "@context");
    OrionldContext*        contextP     = NULL;

    // Error if both - taken care of by pCheckEntity ... I hope!
    if (idNodeP == NULL)
      idNodeP = atidNodeP;

    if (contextNodeP != NULL)
      contextP = orionldContextFromTree(NULL, OrionldContextFromInline, NULL, contextNodeP);

    if (contextP != NULL)
      orionldState.contextP = contextP;

    // No Entity from DB needed as all attributes are always overwritten
    if (pCheckEntity(entityP, true, NULL) == false)
    {
      const char* entityId = (idNodeP == NULL)? "No entity::id" : (idNodeP->type == KjString)? idNodeP->value.s : "Invalid entity::id";

      entityErrorPush(errorsArrayP, entityId, OrionldBadRequestData, orionldState.pd.title, orionldState.pd.detail, 400, false);
      kjChildRemove(orionldState.requestTree, entityP);
    }

    entityP = next;
  }
  orionldState.contextP = savedContextP;


  //
  // 01. Entities that already exist in the DB cannot have a type != type-in-db
  //     To assure this, we need to extract all existing entities from the database
  //     Also, those entities that do not exist MUST have an entity type present.
  //
  //     Create idArray as an array of entity IDs, extracted from orionldState.requestTree
  //
  KjNode* idArray = kjEntityIdArrayExtract(orionldState.requestTree, errorsArrayP);

  //
  // 02. Query database extracting three fields: { id, type and creDate } for each of the entities
  //     whose Entity::Id is part of the array "idArray".
  //     The result is "idTypeAndCredateFromDb" - an array of "tiny" entities with { id, type and creDate }
  //
  KjNode* idTypeAndCreDateFromDb = dbEntityListLookupWithIdTypeCreDate(idArray, false);

  orionldState.batchEntities = idTypeAndCreDateFromDb;  // So that TRoE knows what entities existed prior to the upsert call


  //
  // 03. Creation Date from DB entities, and type-check
  //
  // LOOP OVER idTypeAndCreDateFromDb.
  // Add all the entities to "removeArray", unless an error occurs (with non-matching types for example)
  //
  KjNode* removeArray       = NULL;  // This array contains the Entity::Id of all entities to be removed from DB
  int     entitiesToRemove  = 0;

  if (idTypeAndCreDateFromDb != NULL)
  {
    //
    // This loop loops over the results from the DB query (over the entity ids from the incoming payload)
    // As what already exists will be overwritten, we might as well REMOVE those entities before we push to DB
    //
    for (KjNode* dbEntityP = idTypeAndCreDateFromDb->value.firstChildP; dbEntityP != NULL; dbEntityP = dbEntityP->next)
    {
      char*                  idInDb        = NULL;
      char*                  typeInDb      = NULL;
      double                 creDateInDb   = 0;
      char*                  typeInPayload = NULL;
      KjNode*                contextNodeP  = NULL;
      KjNode*                entityP;

      // Get entity id, type and creDate from the DB
      entityTypeAndCreDateGet(dbEntityP, &idInDb, &typeInDb, &creDateInDb);

      //
      // For the entity in question - get id and type from the incoming payload
      // First look up the entity with ID 'idInDb' in the incoming payload
      //
      entityP = entityLookupById(incomingTree, idInDb);
      if (entityP == NULL)
      {
        // DB entity not part of incoming payload ... is that not a bit weird ... ?
        continue;
      }

      typeInPayload = entityTypeGet(entityP, &contextNodeP);  // Already expanded (by pCheckEntity)


      //
      // If type exists in the incoming payload, it must be equal to the type in the DB
      // If not, it's an error, so:
      //   - add entityId to errorsArrayP
      //   - add entityId to removeArray
      //   - remove from incomingTree
      //
      // Remember, the type in DB is expanded. We must expand the 'type' in the incoming payload as well, before we compare
      //
      if (typeInPayload != NULL)
      {
        if (strcmp(typeInPayload, typeInDb) != 0)
        {
          //
          // As the entity type differed, this entity will not be updated in DB, nor will it be removed:
          // - removed from incomingTree
          // - not added to "removeArray"
          //
          LM_W(("Bad Input (orig entity type: '%s'. New entity type: '%s'", typeInDb, typeInPayload));
          entityErrorPush(errorsArrayP, idInDb, OrionldBadRequestData, "non-matching entity type", typeInPayload, 400, false);
          kjChildRemove(incomingTree, entityP);
          continue;
        }
      }
      else
      {
        // Add 'type' to entity in incoming tree, if necessary
        KjNode* typeNodeP = kjString(orionldState.kjsonP, "type", typeInDb);
        kjChildAdd(entityP, typeNodeP);
      }

      //
      // Add creDate from DB to the entity of the incoming tree
      //
      KjNode* creDateNodeP = kjFloat(orionldState.kjsonP, idInDb, creDateInDb);
      if (orionldState.creDatesP == NULL)
        orionldState.creDatesP = kjObject(orionldState.kjsonP, NULL);
      kjChildAdd(orionldState.creDatesP, creDateNodeP);

      //
      // Add the Entity-ID to "removeArray" for later removal, before re-creation
      //
      if (removeArray == NULL)
        removeArray = kjArray(orionldState.kjsonP, NULL);

      KjNode* idNodeP = kjString(orionldState.kjsonP, NULL, idInDb);
      kjChildAdd(removeArray, idNodeP);
      ++entitiesToRemove;
    }
  }


  //
  // 04. Entity::type is MANDATORY for entities that did not already exist
  //     Erroneous entities must be:
  //     - reported via entityErrorPush()
  //     - removed from "removeArray"
  //     - removed from "incomingTree"
  //
  // So, before calling 'typeCheckForNonExistingEntities' we must make sure the removeArray exists
  //
  if (removeArray == NULL)
    removeArray = kjArray(orionldState.kjsonP, NULL);

  typeCheckForNonExistingEntities(incomingTree, idTypeAndCreDateFromDb, errorsArrayP, removeArray);


  //
  // 05. Remove the entities in "removeArray" from DB
  //
  if (orionldState.uriParamOptions.update == false)
  {
    if ((removeArray != NULL) && (removeArray->value.firstChildP != NULL))
      dbEntitiesDelete(removeArray);
  }


  //
  // 06. Fill in UpdateContextRequest from "incomingTree"
  //


  //
  // Before the tree is destroyed by kjTreeToUpdateContextRequest(),
  // remove+merge any duplicated entities from the array and save the array for TRoE.
  //
  // If more than ONE instance of an entity:
  //   - For REPLACE - remove all entity instances but the last
  //   - For UPDATE  - remove all instances and add a new one - merged from all of them
  //
  if (orionldState.uriParamOptions.update == true)
    duplicatedInstances(incomingTree, NULL, false, true, errorsArrayP);  // Existing entities are MERGED, existing attributes are replaced
  else
    duplicatedInstances(incomingTree, NULL, true, true, errorsArrayP);   // Existing entities are REPLACED

  KjNode*               treeP    = (troe == true)? kjClone(orionldState.kjsonP, incomingTree) : incomingTree;
  UpdateContextRequest  mongoRequest;

  mongoRequest.updateActionType = ActionTypeAppend;

  kjTreeToUpdateContextRequest(&mongoRequest, treeP, errorsArrayP, idTypeAndCreDateFromDb);

  //
  // 07. Set 'modDate' to "RIGHT NOW"
  //
  for (unsigned int ix = 0; ix < mongoRequest.contextElementVector.size(); ++ix)
  {
    mongoRequest.contextElementVector[ix]->entityId.modDate = orionldState.timestamp.tv_sec;
  }


  //
  // 08. Call mongoBackend - to create/modify the entities
  //     In case of REPLACE, all entities have been removed from the DB prior to this call, so, they will all be created.
  //
  UpdateContextResponse    mongoResponse;
  std::vector<std::string> servicePathV;
  servicePathV.push_back("/");

  orionldState.httpStatusCode = mongoUpdateContext(&mongoRequest,
                                                   &mongoResponse,
                                                   orionldState.tenantP,
                                                   servicePathV,
                                                   orionldState.in.xAuthToken,
                                                   orionldState.correlator,
                                                   orionldState.attrsFormat,
                                                   orionldState.apiVersion,
                                                   NGSIV2_NO_FLAVOUR);

  //
  // Now check orionldState.errorAttributeArray to see whether any attribute failed to be updated
  //
  // bool partialUpdate = (orionldState.errorAttributeArrayP[0] == 0)? false : true;
  // bool retValue      = true;
  //
  if (orionldState.httpStatusCode == SccOk)
  {
    for (unsigned int ix = 0; ix < mongoResponse.contextElementResponseVector.vec.size(); ix++)
    {
      const char* entityId = mongoResponse.contextElementResponseVector.vec[ix]->contextElement.entityId.id.c_str();

      if (mongoResponse.contextElementResponseVector.vec[ix]->statusCode.code == SccOk)
      {
        // Creation or Update?
        KjNode* dbEntityP = entityLookupInDb(idTypeAndCreDateFromDb, entityId);

        if (dbEntityP == NULL)
          entitySuccessPush(createdArrayP, entityId);
        else
          entitySuccessPush(updatedArrayP, entityId);
      }
      else
        entityErrorPush(errorsArrayP,
                        entityId,
                        OrionldBadRequestData,
                        "",
                        mongoResponse.contextElementResponseVector.vec[ix]->statusCode.reasonPhrase.c_str(),
                        400,
                        false);
    }

    for (unsigned int ix = 0; ix < mongoRequest.contextElementVector.vec.size(); ix++)
    {
      const char* entityId = mongoRequest.contextElementVector.vec[ix]->entityId.id.c_str();

      // Creation or Update?
      KjNode* dbEntityP = entityLookupInDb(idTypeAndCreDateFromDb, entityId);

      if (dbEntityP == NULL)
      {
        if (kjStringValueLookupInArray(createdArrayP, entityId) == NULL)
          entitySuccessPush(createdArrayP, entityId);
      }
      else
      {
        if (kjStringValueLookupInArray(updatedArrayP, entityId) == NULL)
          entitySuccessPush(updatedArrayP, entityId);
      }
    }
  }

  //
  // We have entity ids in three arrays:
  //   errorsArrayP
  //   updatedArrayP
  //   createdArrayP
  //
  // 1. The broker returns 201 if all entities have been created:
  //    - errorsArrayP  EMPTY
  //    - updatedArrayP EMPTY
  //    - createdArrayP NOT EMPTY
  //
  // 2. The broker returns 204 if all entities have been updated:
  //    - errorsArrayP  EMPTY
  //    - createdArrayP EMPTY
  //    - updatedArrayP NOT EMPTY
  //
  // 3. Else, 207 is returned
  //
  KjNode* successArrayP  = NULL;

  if ((errorsArrayP->value.firstChildP == NULL) && (updatedArrayP->value.firstChildP == NULL))
  {
    orionldState.httpStatusCode = 201;
    orionldState.responseTree   = createdArrayP;
    successArrayP               = createdArrayP;  // for kjEntityArrayErrorPurge
    successArrayP->name         = (char*) "success";
  }
  else if ((errorsArrayP->value.firstChildP == NULL) && (createdArrayP->value.firstChildP == NULL))
  {
    orionldState.httpStatusCode = 204;
    orionldState.responseTree   = NULL;
    successArrayP               = updatedArrayP;  // for kjEntityArrayErrorPurge
    successArrayP->name         = (char*) "success";
  }
  else
  {
    orionldState.httpStatusCode = 207;

    //
    // For v1.3.1, merge updatedArrayP and createdArrayP into successArrayP
    // FIXME:  This will be amended in the spec and this code will need to change
    //
    if (updatedArrayP->value.firstChildP != NULL)
    {
      successArrayP = updatedArrayP;

      if (createdArrayP->value.firstChildP != NULL)
      {
        // Concatenate arrays
        successArrayP->lastChild->next = createdArrayP->value.firstChildP;
        successArrayP->lastChild       = createdArrayP->lastChild;
      }
    }
    else if (createdArrayP->value.firstChildP != NULL)
    {
      successArrayP = createdArrayP;

      if (updatedArrayP->value.firstChildP != NULL)
      {
        // Concatenate arrays
        successArrayP->lastChild->next = updatedArrayP->value.firstChildP;
        successArrayP->lastChild       = updatedArrayP->lastChild;
      }
    }
    else
      successArrayP = updatedArrayP;  // Empty array

    successArrayP->name = (char*) "success";

    //
    // Add the success/error arrays to the response-tree
    //
    orionldState.responseTree = kjObject(orionldState.kjsonP, NULL);

    kjChildAdd(orionldState.responseTree, successArrayP);
    kjChildAdd(orionldState.responseTree, errorsArrayP);
  }


  if ((createdArrayP->value.firstChildP != NULL) && (orionldState.tenantP != &tenant0))
    orionldHeaderAdd(&orionldState.out.headers, HttpTenant, orionldState.tenantP->tenant, 0);

  mongoRequest.release();
  mongoResponse.release();

  if (troe == true)
    kjEntityArrayErrorPurge(orionldState.requestTree, errorsArrayP, successArrayP);

  return true;
}
