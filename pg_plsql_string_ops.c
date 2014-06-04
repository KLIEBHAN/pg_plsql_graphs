#include "plpgsql.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "pg_plsql_graphs.h"

/**
 * removes a substring of a cstring
 */
void removeSubstring(char *s,const char *toremove)
{
    s=strstr(s,toremove);
    while( s ){
        memmove(s,s+strlen(toremove),1+strlen(s+strlen(toremove)));
        s=strstr(s,toremove);
    }
}

/**
 * removes a substring of a cstring
 */
char* removeFromString(char* string,char* toRemove){
    /* removes the substring */
    removeSubstring(string,toRemove);
    return string;
}

/**
 * copys a cstring into a new cstring , removes a substring and returns the result
 */
char* removeFromStringN(const char* string,char* toRemove){
    /* copy the given string to a new string */
    char* newString = palloc(sizeof(string));
    strcpy(newString,string);
    /* removes the substring */
    removeSubstring(newString,toRemove);
    return newString;
}

