//@ expect err cannot use a bare type as an operand of an operator
fn f(i32 dummy) bool {
    return dummy == i32
}
fn main() i32 { return 0 }
