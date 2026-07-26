// Vector[T] - dynamically resizing array.

// Manual ownership: a Vector[T] owns its backing buffer.
//
// DESTRUCTION IS NEVER CALLED BY HAND. There are exactly three ways a thing is
// destroyed, and __delete() is not one of them -- it is the *implementation*
// the language invokes, the way __add is the implementation of `+`:
//
//   delete p      one heap object
//   delete[] p    a heap array (destroys every element, then frees)
//   scope exit    RAII, for any local
//
// Nothing in this file writes `.__delete()`. An earlier version did, and every
// such call was a double free waiting for its object to go out of scope: the
// explicit call does NOT suppress the scope-exit destructor, so the resource
// was released twice. `delete[]` is what made the last of those calls
// unnecessary -- destroying the elements of a heap buffer was the one job RAII
// could not reach, and it is now the language's job rather than this file's.
//
// Element ownership is driven by ONE predicate, asked twice: does T have
// __delete()? Two operations answer it, and every method that moves a T
// through this buffer routes through one of the two -- there is no third
// way to get a T in or out of self.data:
//
//   store_elem(index, item)   T comes IN.  Owning T -> item.copy(), a real
//                             deep copy. Plain T -> a field-copy.
//   move_out(index)           T goes OUT. Owning T -> the slot is blanked
//                             after, so it stops aliasing what left. Plain
//                             T -> the blank is free, nothing to leak.
//
// Both check the exact same thing __delete() itself checks, so there is
// one predicate for "does T own something" -- not the __delete()-decides-
// destruction / copy()-decides-duplication split that let those two
// disagree, which is how this vector's copy() first shipped with a plain
// field-copy loop and double-freed on Vector[Vector[u8]].
//
// Contract for an owning T: define __delete(), and define copy() to
// deep-copy. store_elem()/move_out() enforce that pairing structurally --
// there is no way for push()/copy()/pop() to see __delete() and silently
// alias anyway.
//
// CALL-SITE CONTRACT: get() and pop() return Option[T]. Match an owning T
// with {.Some = *v} (write-through/pointer bind), never {.Some = v} (copy
// bind) -- match's own copy-bind semantics apply here same as anywhere
// else, and copy-binding an owning T's payload double-frees for the same
// reason a raw field-copy anywhere else in this file does.

pub struct Vector[T] {
    T* data
    u32 capacity
    u32 size
}

pub impl Vector[T] {
    // Default `cap = 4` replaces the old create()-forwards-to-with_capacity(4)
    // duplication -- one real constructor, not two.
    static fn with_capacity(u32 cap = 4) Vector[T] {
        u32 c = cap
        if c == 0 { c = 4 }
        return {
            .data = new[c] T,
            .capacity = c,
            .size = 0
        }
    }

    static fn create() Vector[T] {
        return Vector[T].with_capacity()
    }

    // Per-vector step function baked into IteratorCursor's const-generic type.
    // At runtime this is simply the mangled Vector_step[T] function, while
    // keeping the state transition beside the Vector that owns its layout.
    static fn step(struct{T* data  u32 pos  u32 len}* s) Option[T*] {
        if s.pos >= s.len { return .None }
        T* p = &s.data[s.pos]
        s.pos = s.pos + 1
        return .Some(p)
    }

    // Single source of truth for "how does a T get stored into this
    // buffer": deep-copy if T owns a resource, plain value otherwise.
    // push(), __assign(), and copy() all route through this instead of
    // each carrying their own match T { ... } -- one predicate, checked
    // once, impossible to apply inconsistently across call sites.
    fn store_elem(u32 index, T item) void {
        match T {
            Owning { self.data[index] = item.copy() }
            else   { self.data[index] = item }
        }
    }

    // store_elem's counterpart: takes a T OUT of a slot and blanks the
    // slot behind it. For a plain T the blank is free (nothing to leak),
    // but for an owning T it is load-bearing: without it, the vacated
    // slot keeps a live second alias to whatever the moved-out T owns,
    // and a later destroy_elems() walk (or another move_out over the
    // same slot) double-frees through it. pop() and push()'s grow step
    // both move a T out of one slot and into another -- this is that one
    // operation, written once, instead of twice by hand (found the hard
    // way: push()'s grow loop shipped for a while as a raw field-copy
    // before this existed, and it double-freed under exactly this
    // scenario on Vector[Vector[u8]]).
    fn move_out(u32 index) T {
        T out = self.data[index]
        T blank
        self.data[index] = blank
        return out
    }

    // Replaces this Vector's contents from a fixed-size array literal.
    // Frees the old buffer first.
    fn __assign[u32 N](T[N] arr) void {
        // Release the old buffer with delete[] (which destroys any owning
        // elements) rather than calling self.__delete(): that would run this
        // object's whole destructor mid-assignment, and RAII will run it AGAIN
        // when the owner goes out of scope -- a double free through the one
        // path a destructor exists to prevent.
        if self.data != null {
            delete[] self.data
        }
        self.data = new[N] T
        for u32 i = 0 to N {
            self.store_elem(i, arr[i])
        }
        self.capacity = N
        self.size = N
    }

    // Deep copy: allocates a new buffer and copies every element. Routes
    // through store_elem(), so an owning T (Vector[Vector[u8]] etc.) gets
    // its own buffer per element instead of aliasing the source's.
    fn copy() Vector[T] {
        Vector[T] out = Vector[T].with_capacity(self.size)
        for u32 i = 0 to self.size {
            out.store_elem(i, self.data[i])
        }
        out.size = self.size
        return out
    }

    // Frees the backing buffer. `delete[]` destroys every element first when T
    // has a destructor, so Vector[Vector[u8]] recursively frees each inner
    // Vector's buffer with nothing recursive written here -- no element loop and
    // no `match T` deciding whether one is needed. The count comes from the
    // allocation's own cookie, so it cannot disagree with what was allocated.
    //
    // It destroys all CAPACITY slots, not just the live [0, size) ones. That is
    // safe rather than sloppy: `new` zero-initializes, so an unused slot is an
    // all-zero T, and any __delete() that can run on a fresh T must already
    // null-guard -- this one's own `if self.data != null` is exactly that guard.
    fn __delete() void {
        if self.data != null {
            delete[] self.data
            self.data = null
        }
        self.capacity = 0
        self.size = 0
    }

    // Pushes a new element to the back, doubling capacity if necessary.
    // Growth moves existing elements into the new buffer via move_out, so
    // an owning T's vacated old slot is blanked the same way pop() blanks
    // one -- one "move a T out of a slot" operation, not two hand-rolled
    // copies of it.
    fn push(T item) void {
        if self.size == self.capacity {
            u32 new_capacity = self.capacity * 2
            if new_capacity == 0 { new_capacity = 4 }
            T* new_data = new[new_capacity] T

            for u32 i = 0 to self.size {
                new_data[i] = self.move_out(i)
            }

            // delete[] , not delete: this buffer came from `new[...] T`, and for
            // an owning T that allocation carries an element-count cookie in
            // front of it. The single-object `delete` would hand free() the
            // element pointer instead of the true base and corrupt the heap.
            // Every element was already moved out above, so the destructor pass
            // this runs sees only blanked (zeroed) slots and does nothing.
            if self.data != null { delete[] self.data }
            self.data = new_data
            self.capacity = new_capacity
        }

        self.store_elem(self.size, item)
        self.size = self.size + 1
    }

    // Removes and returns the last element, or .None if empty. Routes
    // through move_out (see its comment) so an owning T's vacated slot
    // is blanked, not just aliased.
    fn pop() Option[T] {
        if self.size == 0 {
            return .None
        }
        self.size = self.size - 1
        return {.Some = self.move_out(self.size)}
    }

    // Retrieves an element by index safely.
    fn get(u32 index) Option[T] {
        if index >= self.size {
            return .None
        }
        return {.Some = self.data[index]}
    }

    // Direct, unchecked access. Returns a pointer into the buffer, so
    // `v[i] = x` also works. See get() for the checked equivalent.
    fn __index(u32 index) T* {
        return &self.data[index]
    }

    // Element-wise equality: same length, and every element equal.
    //
    // The operand is a Vector[T]* and the call site therefore writes `a == &b`.
    // That is not about speed -- a Vector[T] is three fields -- it is about
    // ownership. A by-value operand would be a shallow copy (the language's one
    // copy rule: `b = a` is always a memcpy, never a hook), so the parameter
    // would alias this vector's buffer while claiming to be its own value. For a
    // type that owns a heap allocation, taking the operand by pointer is the only
    // spelling that does not manufacture a second owner of the same memory.
    //
    // Elements are compared with `*a == *b` rather than `a == b` on values: the
    // deref form works uniformly for primitives, plain structs, AND an element
    // type whose own __eq takes a pointer, which a value comparison cannot reach
    // from generic code (`a == b` on two T values cannot call __eq(T* o)).
    // So this one spelling covers every T without asking what T chose.
    fn __eq(Vector[T] other) bool {
        if self.size != other.size {
            return false
        }
        for u32 i = 0 to self.size {
            T* mine = &self.data[i]
            T* theirs = &other.data[i]
            if !(*mine == *theirs) {
                return false
            }
        }
        return true
    }

    // Element-wise inequality. `!=` dispatches to __neq on its own -- it does
    // NOT fall back to `!(a == b)` -- so a type defining only __eq has no `!=`
    // at all. Written as its own loop rather than delegating to __eq, because
    // re-entering `==` on two Vector[T] VALUES from inside a generic
    // instantiation still hits "aggregate operator not defined reached backend".
    fn __neq(Vector[T] other) bool {
        if self.size != other.size {
            return true
        }
        for u32 i = 0 to self.size {
            T* mine = &self.data[i]
            T* theirs = &other.data[i]
            if !(*mine == *theirs) {
                return true
            }
        }
        return false
    }

    // Sets an element by index safely. Returns true if successful.
    //
    // The displaced element is moved out first. move_out() hands it back as an
    // ordinary local, so scope exit destroys it -- the same RAII that destroys
    // any other value that goes out of scope, rather than this file calling a
    // destructor by name. It also blanks the slot, so the old element is never
    // aliased by both `old` and the buffer at once.
    fn set(u32 index, T item) bool {
        if index >= self.size {
            return false
        }
        T old = self.move_out(index)
        self.store_elem(index, item)
        return true
    }

    // Removes all elements, destroying any that own a resource. The buffer is
    // released and re-taken at the same capacity rather than being kept and
    // hand-cleared: `delete[]` is the one operation that destroys elements, and
    // it necessarily frees the storage they sat in. Nothing here calls a
    // destructor by name -- __delete() is a procedure the language invokes, not
    // an API this file is allowed to reach for.
    fn clear() void {
        u32 cap = self.capacity
        if self.data != null {
            delete[] self.data
        }
        self.data = new[cap] T
        self.capacity = cap
        self.size = 0
    }

    // True when there are no elements.
    fn is_empty() bool {
        return self.size == 0
    }

    // Returns the current number of elements.
    fn len() u32 {
        return self.size
    }

    // Returns the current capacity of the backing buffer.
    fn cap() u32 {
        return self.capacity
    }

    // Iterator support for `for TYPE val in vec { ... }` / `for unpack ...`.
    // Returns a cursor over the CURRENT buffer/size snapshot -- mutating the
    // vector's length (push/pop past the snapshotted len) during iteration
    // is undefined, same caveat as iterator invalidation in any manual-memory
    // language. The cursor's state and step function are both baked into its
    // type, so there is no vector-specific cursor implementation here.
    fn begin() IteratorCursor[T, struct{T* data  u32 pos  u32 len}, Vector[T].step] {
        return {
            .state = {
                .data = self.data,
                .pos = 0,
                .len = self.size
            }
        }
    }
}