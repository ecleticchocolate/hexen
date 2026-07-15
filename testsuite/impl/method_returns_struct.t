//@ expect val 33
struct Point { i32 x  i32 y }
impl Point {
    fn translate(i32 dx, i32 dy) Point {
        Point r = {.x = self.x + dx, .y = self.y + dy}
        return r
    }
    fn sum() i32 { return self.x + self.y }
}
fn main() i32 {
    Point p = {.x = 1, .y = 2}
    Point q = p.translate(10, 20)
    return q.sum()
}
