//@ expect stdout
//@ | 15
//@ | 5
//@ | 50
//@ | 2
//@ | 10
extern fn printf(u8* fmt, ...) i32;
enum Op { i32 Add  i32 Sub  i32 Mul  i32 Div  None }

fn apply(Op o, i32 x) i32 {
    match o {
        .Add{n} { return x + n }
        .Sub{n} { return x - n }
        .Mul{n} { return x * n }
        .Div{n} { return x / n }
        .None { return x }
    }
}

fn main() i32 {
    printf("%d\n", apply(.Add{5}, 10))
    printf("%d\n", apply(.Sub{5}, 10))
    printf("%d\n", apply(.Mul{5}, 10))
    printf("%d\n", apply(.Div{5}, 10))
    printf("%d\n", apply(.None, 10))
    return 0
}
