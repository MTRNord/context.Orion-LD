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
#include <stdlib.h>                                            // free

extern "C"
{
#include "kjson/kjFree.h"                                      // kjFree
}

#include "logMsg/logMsg.h"                                     // LM_*

#include "orionld/regCache/RegCache.h"                         // RegCache
#include "orionld/regCache/regCacheItemRegexRelease.h"         // regCacheItemRegexRelease
#include "orionld/regCache/regCacheRelease.h"                  // Own interface



// -----------------------------------------------------------------------------
//
// regCacheRelease -
//
void regCacheRelease(RegCache* regCacheP)
{
  RegCacheItem* rciP = regCacheP->regList;
  RegCacheItem* next;

  while (rciP != NULL)
  {
    next = rciP->next;

    if (rciP->regId != NULL)
      free(rciP->regId);

    kjFree(rciP->regTree);
    if (rciP->idPatternRegexList != NULL)
      regCacheItemRegexRelease(rciP);

    if (rciP->ipAndPort != NULL)
      free(rciP->ipAndPort);

    if ((rciP->hostAlias != NULL) && (rciP->hostAlias != rciP->ipAndPort))
      free(rciP->hostAlias);

    free(rciP);

    rciP = next;
  }

  free(regCacheP);
}
