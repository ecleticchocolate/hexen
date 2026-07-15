//@ expect err union 'Bad' contains itself by value
union Bad { i32 x  Bad self }
fn main() i32 {
    return (i32) sizeof(Bad)
}
