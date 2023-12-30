/*
*
* Copyright 2013 Telefonica Investigacion y Desarrollo, S.A.U
*
* This file is part of Orion Context Broker.
*
* Orion Context Broker is free software: you can redistribute it and/or
* modify it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* Orion Context Broker is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
* General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with Orion Context Broker. If not, see http://www.gnu.org/licenses/.
*
* For those usages not covered by this license please contact with
* iot_support at tid dot es
*
* Author: Ken Zangelin
*/
#include <stdio.h>
#include <sys/types.h>

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "parseArgs/parseArgs.h"
#include "parseArgs/paBuiltin.h"        // paLsHost, paLsPort
#include "parseArgs/paIsSet.h"

#include "logMsg/logMsg.h"

#undef _i
#include "common/globals.h"
#include "common/sem.h"
#include "mongoBackend/MongoGlobal.h"
#include "ngsiNotify/Notifier.h"
#include "alarmMgr/alarmMgr.h"
#include "logSummary/logSummary.h"

#include "orionld/common/orionldState.h"        // orionldState
#include "orionld/common/orionldTenantInit.h"   // orionldTenantInit
#include "orionld/common/tenantList.h"          // tenant0

#include "unittests/unittest.h"



/* ****************************************************************************
*
* USING
*/
using ::testing::_;



/* ****************************************************************************
*
* global variables
*/
bool            harakiri              = true;
int             logFd                 = -1;
int             fwdPort               = -1;
int             subCacheInterval      = 10;
unsigned int    cprForwardLimit       = 1000;
bool            noCache               = false;
bool            insecureNotif         = false;
bool            ngsiv1Autocast        = false;
char            fwdHost[64];
char            notificationMode[64];
bool            simulatedNotification;
int             lsPeriod             = 0;
bool            disableCusNotif      = false;

char            dbHost[64];
char            rplSet[64];
char            dbName[64];
char            user[64];
char            pwd[64];
int64_t         dbTimeout;
int             dbPoolSize;
int             writeConcern;
char            gtest_filter[1024];
char            gtest_output[1024];
int             contextDownloadAttempts = 5;
int             contextDownloadTimeout  = 10000;
bool            troe                    = false;
bool            multitenancy            = false;
bool            lmtmp                   = false;
char            troeHost[256];
unsigned short  troePort;
char            troeUser[256];
char            troePwd[256];
bool            distributed             = true;
bool            idIndex                 = false;
bool            noNotifyFalseUpdate     = false;
char            dbUser[64]              = { 0 };
char            dbPwd[64]               = { 0 };
bool            experimental            = false;
bool            mongocOnly              = false;
bool            debugCurl               = false;
uint32_t        cSubCounters            = 0;
char            localIpAndPort[135];
char            brokerId[136];
unsigned long long  inReqPayloadMaxSize;
unsigned long long  outReqMsgMaxSize;
bool                triggerOperation = false;



/* ****************************************************************************
*
* parse arguments
*/
PaArgument paArgs[] =
{
  { "-dbhost",         dbHost,        "DB_HOST",        PaString, PaOpt, (int64_t) "localhost",  PaNL, PaNL,  "" },
  { "-rplSet",         rplSet,        "RPL_SET",        PaString, PaOpt, (int64_t) "",           PaNL, PaNL,  "" },
  { "-dbuser",         user,          "DB_USER",        PaString, PaOpt, (int64_t) "",           PaNL, PaNL,  "" },
  { "-dbpwd",          pwd,           "DB_PASSWORD",    PaString, PaOpt, (int64_t) "",           PaNL, PaNL,  "" },
  { "-db",             dbName,        "DB",             PaString, PaOpt, (int64_t) "orion",      PaNL, PaNL,  "" },
  { "-dbTimeout",      &dbTimeout,    "DB_TIMEOUT",     PaInt64,  PaOpt, 10000,                  PaNL, PaNL,  "" },
  { "-dbPoolSize",     &dbPoolSize,   "DB_POOL_SIZE",   PaInt,    PaOpt, 10,                     1,    10000, "" },
  { "-writeConcern",   &writeConcern, "WRITE_CONCERN",  PaInt,    PaOpt, 1,                      0,    1,     "" },
  { "--gtest_filter=", gtest_filter,  "",               PaString, PaOpt, (int64_t) "",           PaNL, PaNL,  "" },
  { "--gtest_output=", gtest_output,  "",               PaString, PaOpt, (int64_t) "",           PaNL, PaNL,  "" },

  PA_END_OF_ARGS
};


/* ****************************************************************************
*
* exitFunction - 
*/
void exitFunction(int code, const std::string& reason)
{
  LM_E(("Orion library asks to exit %d: '%s', but no exit is allowed inside unit tests", code, reason.c_str()));
}


const char* orionUnitTestVersion = "0.0.1-unittest";



/* ****************************************************************************
*
* main - 
*/
int main(int argC, char** argV)
{
  paConfig("usage and exit on any warning", (void*) true);
  paConfig("log to screen",                 (void*) "only errors");
  paConfig("log file line format",          (void*) "TYPE:DATE:EXEC-AUX/FILE[LINE](p.PID)(t.TID) FUNC: TEXT");
  paConfig("screen line format",            (void*) "TYPE@TIME  EXEC: TEXT");
  paConfig("log to file",                   (void*) true);
  paConfig("default value", "-logDir",      (void*) "/tmp");
  paConfig("man author",                    "Fermín Galán and Ken Zangelin");

  paParse(paArgs, argC, (char**) argV, 1, false);

  LM_M(("Init tests"));
  orionldTenantInit();
  orionInit(exitFunction, orionUnitTestVersion, SemReadWriteOp, false, false, false, false, false);
  // Note that multitenancy and mutex time stats are disabled for unit test mongo init
  mongoInit(dbHost, rplSet, dbName, user, pwd, false, dbTimeout, writeConcern, dbPoolSize, false);
  alarmMgr.init(false);
  logSummaryInit(&lsPeriod);
  setupDatabase();
  orionldStateInit(NULL);
  orionldState.tenantP = &tenant0;

  // To not disturb NGSIv2 unit tests ...
  orionldState.uriParams.offset = 0;
  orionldState.uriParams.limit = 0;

  LM_M(("Run all tests"));
  ::testing::InitGoogleMock(&argC, argV);
  return RUN_ALL_TESTS();
}
