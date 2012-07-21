/*******************************************************************************
 * Copyright (c) 2009, 2012 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *******************************************************************************/

/** 8.3 filesystem */
#define MESSAGE_FILENAME_LENGTH 8    
/** Extension of the filename */
#define MESSAGE_FILENAME_EXTENSION ".msg"

/* prototypes of the functions for the default file system persistence */
int pstopen(void** handle, char* clientID, char* serverURI, void* context); 
int pstclose(void* handle); 
int pstput(void* handle, char* key, int bufcount, char* buffers[], int buflens[]); 
int pstget(void* handle, char* key, char** buffer, int* buflen); 
int pstremove(void* handle, char* key); 
int pstkeys(void* handle, char*** keys, int* nkeys); 
int pstclear(void* handle); 
int pstcontainskey(void* handle, char* key);

int pstmkdir(char *pPathname);

