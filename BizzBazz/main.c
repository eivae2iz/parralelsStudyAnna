#include "bizzbazz.h"
#include <stdio.h>


int main()
{
    char a;
    struct bb_info* bb_stream =  bb_create( " ;\n.,", NULL);
    while( a = getchar()  )
    {
        if (bb_give_byte( a, bb_stream ))
            write(2, "Some Errors!\n", strlen( "Some Errors!\n")  );
        if ( a == EOF)
            break;
    }
}

