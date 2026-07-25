// Vector[T] - dynamically resizing array.

// Manual ownership: a Vector[T] owns its backing buffer. __delete() frees
// it, called automatically by RAII on scope exit or explicitly via
// `delete v` on a heap-allocated one.
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
            impl { fn __delete() } { self.data[index] = item.copy() }
            else                   { self.data[index] = item }
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
        self.__delete()
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

    // Destroys every live element in [0, size) if T owns a resource
    // (structural: any T with __delete() participates, no trait/
    // registration needed). Shared by __delete() and clear() so the
    // destroy-loop exists in exactly one place instead of two copies
    // that must be kept in sync by hand.
    fn destroy_elems() void {
        match T {
            impl { fn __delete() } {
                for u32 i = 0 to self.size {
                    self.data[i].__delete()
                }
            }
            else { }
        }
    }

    // Frees the backing buffer, first destroying every element (see
    // destroy_elems), so Vector[Vector[u8]] recursively frees each inner
    // Vector's buffer too.
    fn __delete() void {
        if self.data != null {
            self.destroy_elems()
            delete self.data
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

            if self.data != null { delete self.data }
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

    // Sets an element by index safely. Returns true if successful.
    fn set(u32 index, T item) bool {
        if index >= self.size {
            return false
        }
        self.data[index] = item
        return true
    }

    // Removes all elements without freeing the backing buffer. Still runs
    // element destructors first, via the same destroy_elems() __delete()
    // uses -- one destroy-loop, not two copies that could drift apart.
    fn clear() void {
        self.destroy_elems()
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