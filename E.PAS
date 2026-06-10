program e;

const
   DIGITS = 200;

type
    arrayType = array[ 0..DIGITS ] of byte;

var
    high, n, x, d, val : integer;
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
            d := x DIV n;
            a[ n ] := x - ( d * n );
            n := n - 1;
            x := a[ n ] shl 1;
            x := x + ( x shl 2 ) + d;
        until n = 0;

        Write( x );
    until 9 = high;

    writeln;
    writeln( 'done' );
end.

