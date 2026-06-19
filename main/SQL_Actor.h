/*
 * SQL_Actor.h
 *
 *  Created on: 31-Oct-2023
 *      Author: acer
 */

#ifndef MAIN_SQL_ACTOR_H_
#define MAIN_SQL_ACTOR_H_

#include "sqlite3.h"

#define SQL_FILENAME_LEN  		100  			// 14 byte BLE Device name + 1 byte null character
#define SQL_QUERY_LEN  		2048	//200
void SQL_ConsoleWriteToActor_xface(void *msg);

#endif /* MAIN_SQL_ACTOR_H_ */
