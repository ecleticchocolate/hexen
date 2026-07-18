//@ expect val 30
struct BinOp { Expr* l  Expr* r }
enum Expr {
    i32 Num
    Expr* Neg
    BinOp Add
    BinOp Mul
}
fn eval(Expr* e) i32 {
    match *e {
        .Num(n) { return n }
        .Neg(inner) { return -eval(inner) }
        .Add(pair) { return eval(pair.l) + eval(pair.r) }
        .Mul(pair) { return eval(pair.l) * eval(pair.r) }
    }
    return 0
}
fn build() i32 {
    Expr* two = new Expr
    *two = .Num(2)
    Expr* three = new Expr
    *three = .Num(3)
    Expr* ten = new Expr
    *ten = .Num(10)
    Expr* four = new Expr
    *four = .Num(4)
    Expr* neg_four = new Expr
    *neg_four = .Neg(four)
    Expr* sum = new Expr
    *sum = .Add({.l = two, .r = three})
    Expr* diff = new Expr
    *diff = .Add({.l = ten, .r = neg_four})
    Expr* product = new Expr
    *product = .Mul({.l = sum, .r = diff})
    return eval(product)
}
const i32 RESULT = build()
fn main() i32 { return RESULT }
