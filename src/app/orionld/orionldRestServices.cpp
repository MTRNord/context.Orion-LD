/*
*
* Copyright 2018 FIWARE Foundation e.V.
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
#include <unistd.h>                                             // NULL

#include "orionld/serviceRoutines/orionldPostEntities.h"
#include "orionld/serviceRoutines/orionldPostNotify.h"
#include "orionld/serviceRoutines/orionldPostEntity.h"
#include "orionld/serviceRoutines/orionldPostSubscriptions.h"
#include "orionld/serviceRoutines/orionldPostRegistrations.h"
#include "orionld/serviceRoutines/orionldGetEntity.h"
#include "orionld/serviceRoutines/orionldGetEntities.h"
#include "orionld/serviceRoutines/orionldGetEntityTypes.h"
#include "orionld/serviceRoutines/orionldPostBatchDelete.h"
#include "orionld/serviceRoutines/orionldGetSubscriptions.h"
#include "orionld/serviceRoutines/orionldGetSubscription.h"
#include "orionld/serviceRoutines/orionldGetRegistrations.h"
#include "orionld/serviceRoutines/orionldGetRegistration.h"
#include "orionld/serviceRoutines/orionldPatchEntity.h"
#include "orionld/serviceRoutines/orionldPatchEntity2.h"
#include "orionld/serviceRoutines/orionldPatchAttribute.h"
#include "orionld/serviceRoutines/orionldPatchSubscription.h"
#include "orionld/serviceRoutines/orionldPatchRegistration.h"
#include "orionld/serviceRoutines/orionldDeleteEntity.h"
#include "orionld/serviceRoutines/orionldDeleteAttribute.h"
#include "orionld/serviceRoutines/orionldDeleteSubscription.h"
#include "orionld/serviceRoutines/orionldDeleteRegistration.h"
#include "orionld/serviceRoutines/orionldGetContext.h"
#include "orionld/serviceRoutines/orionldGetContexts.h"
#include "orionld/serviceRoutines/orionldGetVersion.h"
#include "orionld/serviceRoutines/orionldGetPing.h"
#include "orionld/serviceRoutines/orionldPostBatchUpsert.h"
#include "orionld/serviceRoutines/orionldPostBatchCreate.h"
#include "orionld/serviceRoutines/orionldPostBatchUpdate.h"
#include "orionld/serviceRoutines/orionldGetEntityTypes.h"
#include "orionld/serviceRoutines/orionldGetEntityType.h"
#include "orionld/serviceRoutines/orionldGetEntityAttributes.h"
#include "orionld/serviceRoutines/orionldGetEntityAttribute.h"
#include "orionld/serviceRoutines/orionldGetTenants.h"
#include "orionld/serviceRoutines/orionldGetDbIndexes.h"
#include "orionld/serviceRoutines/orionldPostQuery.h"
#include "orionld/serviceRoutines/orionldPostContexts.h"
#include "orionld/serviceRoutines/orionldDeleteContext.h"
#include "orionld/serviceRoutines/orionldOptions.h"
#include "orionld/serviceRoutines/orionldPutEntity.h"
#include "orionld/serviceRoutines/orionldPutAttribute.h"
#include "orionld/serviceRoutines/orionldGetEntityMap.h"
#include "orionld/serviceRoutines/orionldDeleteEntityMap.h"

#include "orionld/serviceRoutines/orionldGetTemporalEntities.h"
#include "orionld/serviceRoutines/orionldGetTemporalEntity.h"
#include "orionld/serviceRoutines/orionldPostTemporalQuery.h"
#include "orionld/serviceRoutines/orionldPostTemporalEntities.h"
#include "orionld/serviceRoutines/orionldDeleteTemporalAttribute.h"          // orionldDeleteTemporalAttribute
#include "orionld/serviceRoutines/orionldDeleteTemporalAttributeInstance.h"  // orionldDeleteTemporalAttributeInstance
#include "orionld/serviceRoutines/orionldDeleteTemporalEntity.h"             // orionldDeleteTemporalEntity
#include "orionld/serviceRoutines/orionldPatchTemporalAttributeInstance.h"   // orionldPatchTemporalAttributeInstance
#include "orionld/serviceRoutines/orionldPostTemporalAttributes.h"           // orionldPostTemporalAttributes

#include "orionld/rest/OrionLdRestService.h"       // OrionLdRestServiceSimplified
#include "orionld/orionldRestServices.h"           // Own Interface



// -----------------------------------------------------------------------------
//
// getServiceV -
//
static OrionLdRestServiceSimplified getServiceV[] =
{
  { "/ngsi-ld/ex/v1/ping",                 orionldGetPing             },
  { "/ngsi-ld/v1/entities/*",              orionldGetEntity           },
  { "/ngsi-ld/v1/entities",                orionldGetEntities         },
  { "/ngsi-ld/v1/entityMaps/*",            orionldGetEntityMap        },
  { "/ngsi-ld/v1/types/*",                 orionldGetEntityType       },
  { "/ngsi-ld/v1/types",                   orionldGetEntityTypes      },
  { "/ngsi-ld/v1/attributes/*",            orionldGetEntityAttribute  },
  { "/ngsi-ld/v1/attributes",              orionldGetEntityAttributes },
  { "/ngsi-ld/v1/subscriptions/*",         orionldGetSubscription     },
  { "/ngsi-ld/v1/subscriptions",           orionldGetSubscriptions    },
  { "/ngsi-ld/v1/csourceRegistrations/*",  orionldGetRegistration     },
  { "/ngsi-ld/v1/csourceRegistrations",    orionldGetRegistrations    },
  { "/ngsi-ld/v1/jsonldContexts/*",        orionldGetContext          },
  { "/ngsi-ld/v1/jsonldContexts",          orionldGetContexts         },
  { "/ngsi-ld/v1/temporal/entities/*",     orionldGetTemporalEntity   },
  { "/ngsi-ld/v1/temporal/entities",       orionldGetTemporalEntities },
  { "/ngsi-ld/ex/v1/version",              orionldGetVersion          },
  { "/ngsi-ld/ex/v1/tenants",              orionldGetTenants          },
  { "/ngsi-ld/ex/v1/dbIndexes",            orionldGetDbIndexes        },
};
static const int getServices = (sizeof(getServiceV) / sizeof(getServiceV[0]));



// ----------------------------------------------------------------------------
//
// postServiceV -
//
static OrionLdRestServiceSimplified postServiceV[] =
{
  { "/ngsi-ld/v1/entities/*/attrs",                orionldPostEntity             },
  { "/ngsi-ld/v1/entities",                        orionldPostEntities           },
  { "/ngsi-ld/ex/v1/notify",                       orionldPostNotify             },
  { "/ngsi-ld/v1/entityOperations/create",         orionldPostBatchCreate        },
  { "/ngsi-ld/v1/entityOperations/upsert",         orionldPostBatchUpsert        },
  { "/ngsi-ld/v1/entityOperations/update",         orionldPostBatchUpdate        },
  { "/ngsi-ld/v1/entityOperations/delete",         orionldPostBatchDelete        },
  { "/ngsi-ld/v1/entityOperations/query",          orionldPostQuery              },
  { "/ngsi-ld/v1/subscriptions",                   orionldPostSubscriptions      },
  { "/ngsi-ld/v1/csourceRegistrations",            orionldPostRegistrations      },
  { "/ngsi-ld/v1/temporal/entities/*/attrs",       orionldPostTemporalAttributes },
  { "/ngsi-ld/v1/temporal/entities",               orionldPostTemporalEntities   },
  { "/ngsi-ld/v1/temporal/entityOperations/query", orionldPostTemporalQuery      },
  { "/ngsi-ld/v1/jsonldContexts",                  orionldPostContexts           }
};
static const int postServices = (sizeof(postServiceV) / sizeof(postServiceV[0]));



// ----------------------------------------------------------------------------
//
// patchServiceV -
//
static OrionLdRestServiceSimplified patchServiceV[] =
{
  { "/ngsi-ld/v1/entities/*/attrs/*",            orionldPatchAttribute                  },
  { "/ngsi-ld/v1/entities/*/attrs",              orionldPatchEntity                     },
  { "/ngsi-ld/v1/entities/*",                    orionldPatchEntity2                    },
  { "/ngsi-ld/v1/subscriptions/*",               orionldPatchSubscription               },
  { "/ngsi-ld/v1/csourceRegistrations/*",        orionldPatchRegistration               },
  { "/ngsi-ld/v1/temporal/entities/*/attrs/*/*", orionldPatchTemporalAttributeInstance  }
};
static const int patchServices = (sizeof(patchServiceV) / sizeof(patchServiceV[0]));



// ----------------------------------------------------------------------------
//
// putServiceV -
//
static OrionLdRestServiceSimplified putServiceV[] =
{
  { "/ngsi-ld/v1/entities/*/attrs/*",            orionldPutAttribute     },
  { "/ngsi-ld/v1/entities/*",                    orionldPutEntity        }
};
static const int putServices = (sizeof(putServiceV) / sizeof(putServiceV[0]));



// ----------------------------------------------------------------------------
//
// deleteServiceV -
//
static OrionLdRestServiceSimplified deleteServiceV[] =
{
  { "/ngsi-ld/v1/entities/*/attrs/*",             orionldDeleteAttribute                 },
  { "/ngsi-ld/v1/entities/*",                     orionldDeleteEntity                    },
  { "/ngsi-ld/v1/entityMaps/*",                   orionldDeleteEntityMap                 },
  { "/ngsi-ld/v1/subscriptions/*",                orionldDeleteSubscription              },
  { "/ngsi-ld/v1/csourceRegistrations/*",         orionldDeleteRegistration              },
  { "/ngsi-ld/v1/jsonldContexts/*",               orionldDeleteContext                   },
  { "/ngsi-ld/v1/temporal/entities/*/attrs/*",    orionldDeleteTemporalAttribute         },
  { "/ngsi-ld/v1/temporal/entities/*",            orionldDeleteTemporalEntity            },
};
static const int deleteServices = (sizeof(deleteServiceV) / sizeof(deleteServiceV[0]));



// -----------------------------------------------------------------------------
//
// optionsServiceV -
//
static OrionLdRestServiceSimplified optionsServiceV[] =
{
  { "/ngsi-ld/v1/entities/*/attrs/*",              orionldOptions },
  { "/ngsi-ld/v1/entities/*/attrs",                orionldOptions },
  { "/ngsi-ld/v1/entities/*",                      orionldOptions },
  { "/ngsi-ld/v1/entities",                        orionldOptions },
  { "/ngsi-ld/v1/entityOperations/create",         orionldOptions },
  { "/ngsi-ld/v1/entityOperations/upsert",         orionldOptions },
  { "/ngsi-ld/v1/entityOperations/update",         orionldOptions },
  { "/ngsi-ld/v1/entityOperations/delete",         orionldOptions },
  { "/ngsi-ld/v1/entityOperations/query",          orionldOptions },
  { "/ngsi-ld/v1/types/*",                         orionldOptions },
  { "/ngsi-ld/v1/types",                           orionldOptions },
  { "/ngsi-ld/v1/attributes/*",                    orionldOptions },
  { "/ngsi-ld/v1/attributes",                      orionldOptions },
  { "/ngsi-ld/v1/subscriptions/*",                 orionldOptions },
  { "/ngsi-ld/v1/subscriptions",                   orionldOptions },
  { "/ngsi-ld/v1/csourceRegistrations/*",          orionldOptions },
  { "/ngsi-ld/v1/csourceRegistrations",            orionldOptions },
  { "/ngsi-ld/v1/jsonldContexts/*",                orionldOptions },
  { "/ngsi-ld/v1/jsonldContexts",                  orionldOptions },
  { "/ngsi-ld/v1/temporal/entities/*/attrs/*",     orionldOptions },
  { "/ngsi-ld/v1/temporal/entities/*/attrs",       orionldOptions },
  { "/ngsi-ld/v1/temporal/entities/*",             orionldOptions },
  { "/ngsi-ld/v1/temporal/entities",               orionldOptions },
  { "/ngsi-ld/v1/temporal/entityOperations/query", orionldOptions }
};
static const int optionsServices = (sizeof(optionsServiceV) / sizeof(optionsServiceV[0]));



// -----------------------------------------------------------------------------
//
// restServiceVV -
//
OrionLdRestServiceSimplifiedVector restServiceVV[] =
{
  { getServiceV,      getServices      },
  { putServiceV,      putServices      },
  { postServiceV,     postServices     },
  { deleteServiceV,   deleteServices   },
  { patchServiceV,    patchServices    },
  { NULL,             0                },
  { optionsServiceV,  optionsServices  },
  { NULL,             0                },
  { NULL,             0                }
};
