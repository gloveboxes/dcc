program e;

const
   DIGITS = 200;

type
    arrayType = array[ 0..DIGITS ] of byte;

var
    high, n, x : integer;
    a : arrayType;

begin
    high := DIGITS;
    x := 0;

    n := high - 1;
    repeat
        a[ n ] := 1;
        n := n - 1;
    until 1 = n;

    a[ 1 ] := 2;
    a[ 0 ] := 0;

    repeat
        high := high - 1;
        n := high;
        repeat
            a[ n ] := x MOD n;
            x := 10 * a[ n - 1 ] + x DIV n;
            n := n - 1;
        until n = 0;

        Write( x );
    until 9 = high;

    writeln;
    writeln( 'done' );
end.

