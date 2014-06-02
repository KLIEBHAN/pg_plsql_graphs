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
 * concats two cstrings into  a new cstring and returns it
 */
char* concatStrings2(char* str1, char* str2){
    /* allocates a new cstring with size of the sum of both given cstrings */
    char* value = palloc(sizeof(str1)/sizeof(char)+sizeof(str2)/sizeof(char));
    /* copys the first cstring */
    strcpy (value,str1);
    /* concats the second cstring */
    strcat (value,str2);
    return value;
}

/**
 * concats three cstrings
 */
char* concatStrings3(char* str1, char* str2, char* str3){

    char* value = palloc(    sizeof(str1)/sizeof(char)+
                            sizeof(str2)/sizeof(char)+
                            sizeof(str3)/sizeof(char));
    sprintf(value,"%s%s%s",str1,str2,str3);
    return value;
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

