/*
*
* Copyright 2023 FIWARE Foundation e.V.
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
#include <string.h>                                                     // strcmp

#include "orionld/common/orionldState.h"                                // entityMaps
#include "orionld/types/EntityMap.h"                                    // EntityMap
#include "orionld/entityMaps/entityMapLookup.h"                         // Own interface



// -----------------------------------------------------------------------------
//
// entityMapLookup
//
EntityMap* entityMapLookup(const char* mapId)
{
  EntityMap* emP = entityMaps;

  while (emP != NULL)
  {
    if (strcmp(emP->id, mapId) == 0)
      return emP;

    emP = emP->next;
  }

  return NULL;
}
