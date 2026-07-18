//@ expect val 42
// 3 levels deep, mixing type params and value params at every level:
//   Grid[T, u32 N]   ->  Row[T, u32 N]  ->  Cell[T]
// with a generic enum wrapping the innermost level.
enum Slot[T] { T Filled  None }
struct Cell[T] { Slot[T] s }
struct Row[T, u32 N] { Cell[T][N] cells }
struct Grid[T, u32 N] { Row[T, N][N] rows }

fn main() i32 {
    Grid[i32, 2] g
    g.rows[0].cells[0].s = .Filled(10)
    g.rows[0].cells[1].s = .Filled(20)
    g.rows[1].cells[0].s = .Filled(5)
    g.rows[1].cells[1].s = .None

    i32 sum = 0
    u32 r = 0
    while r < 2 {
        u32 c = 0
        while c < 2 {
            match g.rows[r].cells[c].s {
                .Filled(v) { sum = sum + v }
                .None {}
            }
            c = c + 1
        }
        r = r + 1
    }
    return sum + 7
}
