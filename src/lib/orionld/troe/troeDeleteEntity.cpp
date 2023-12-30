/*
*
* Copyright 2020 FIWARE Foundation e.V.
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
#include "logMsg/logMsg.h"                                     // LM_*
#include "logMsg/traceLevels.h"                                // Lmt*

#include "orionld/common/orionldState.h"                       // orionldState
#include "orionld/common/uuidGenerate.h"                       // uuidGenerate
#include "orionld/troe/PgTableDefinitions.h"                   // PG_ENTITY_INSERT_START
#include "orionld/troe/PgAppendBuffer.h"                       // PgAppendBuffer
#include "orionld/troe/pgAppendInit.h"                         // pgAppendInit
#include "orionld/troe/pgAppend.h"                             // pgAppend
#include "orionld/troe/pgEntityAppend.h"                       // pgEntityAppend
#include "orionld/troe/pgCommands.h"                           // pgCommands
#include "orionld/troe/troeDeleteEntity.h"                     // Own interface



// ----------------------------------------------------------------------------
//
// troeDeleteEntity -
//
bool troeDeleteEntity(void)
{
  PgAppendBuffer  entitiesBuffer;
  char*           entityId = orionldState.wildcard[0];
  char            instanceId[80];

  uuidGenerate(instanceId, sizeof(instanceId), "urn:ngsi-ld:attribute:instance:");

  pgAppendInit(&entitiesBuffer, 512);       // Enough for deletion of a single entity
  pgAppend(&entitiesBuffer, PG_ENTITY_INSERT_START, 0);

  pgEntityAppend(&entitiesBuffer, "Delete", entityId, "NULL", instanceId);

  if (entitiesBuffer.values > 0)
  {
    char* sqlV[1]  = { entitiesBuffer.buf };

    pgCommands(sqlV, 1);
  }

  return true;
}
