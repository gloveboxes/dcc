#include <stdio.h>

#define KEY_ESC 27

int main() 
{
    while (1)
    {
        int c = getch();
        printf( "read_key: got %d\n", c);
        if ( 'q' == c )
            break;

        if (c != KEY_ESC)
            continue;
        
        if (!kbhit())
            continue;

        c = getch();
        printf( "read_key: got %d after ESC\n", c);    
        if (c == '[') 
        {
            c = getch();
            printf( "read_key: got %d after ESC [\n", c);        
        }
    }
    return 0;
}