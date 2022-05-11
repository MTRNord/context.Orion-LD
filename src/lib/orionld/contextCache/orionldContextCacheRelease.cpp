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
* Author: Ken Zangelin
*/
#include <unistd.h>                                              // NULL

extern "C"
{
#include "kjson/kjFree.h"                                        // kjFree
}

#include "logMsg/logMsg.h"                                       // LM_*
#include "logMsg/traceLevels.h"                                  // Lmt*

#include "orionld/contextCache/orionldContextCache.h"            // orionldContextCache, orionldContextCacheSlotIx
#include "orionld/contextCache/orionldContextCacheRelease.h"     // Own interface



// -----------------------------------------------------------------------------
//
// orionldContextCacheRelease -
//
void orionldContextCacheRelease(void)
{
  int freed = 0;
  for (int ix = 0; ix < orionldContextCacheSlotIx; ix++)
  {
    if (orionldContextCache[ix] == NULL)
      continue;

    ++freed;
    if (orionldContextCache[ix]->tree != NULL)
    {
      LM_TMP(("VL: FREE(%d, context '%s', tree:%p)", freed, orionldContextCache[ix]->url, orionldContextCache[ix]->tree));
      kjFree(orionldContextCache[ix]->tree);
      orionldContextCache[ix]->tree = NULL;
    }
    else
      LM_TMP(("VL: NFRE(%d, context '%s', tree:NULL)", freed, orionldContextCache[ix]->url));
  }
}
