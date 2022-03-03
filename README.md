# COWStrings 🐄

 A C++ Container Type Library

This is a simple C++ string library utilizing short-string-optimization (SSO),
copy-on-write (COW) and allocation-free construction from string literals. These
three features help to reduce allocations during string construction, copying and
modification. The string objects store and operate on UTF-8 encoded text and
allow for querying on a code point basis. The internal buffer containing the
text data is terminated with a C-style null byte for compatibility with functions
like ```printf```.

```C++
// No allocation: Constructed from literal (char array)
String s1= "Hello world";

// No allocation: Construction of short string (<32 bytes)
const char* ptr= "Hello solarsystem";
String s2= ptr;

// Allocation: Construction of long string
String s3= "Hello Milkyway, Andromeda, Sagittarius, Canis Major, Triangulum and Reticulum";

// No allocation: Construction of string sharing the buffer with s3
String s4= s3;
```

## Small String Optimization (SSO) 🎈
If a string is constructed with a length in bytes short enough, that it fits in
the string object itself, no buffer is allocated. Instead the inner structure of
the string object is interpreted as an array of bytes. The last byte contains a
a two bit flag indicating if it is in this so called "short" mode, "literal" mode
referencing a string constant or "dynamic" mode using an allocated buffer on the
heap. In short mode the last byte additionally stores the number of bytes free.
The flag uses 0 to indicate "short" mode, which leaves the whole last byte with
value 0 when the "short" mode string is completely filled, allowing it to be
also used as the null-termination byte, maximizing efficiency. (I did not come
up with this idea, but I could not find the source where I read it.)

## Copy On Write (COW) 🖨
To reduce allocations and data copying, strings in "dynamic" mode share their
buffers as much as possible. The backing buffers are single-threaded reference
counted, which allows them to be owned and safely referenced by multiple strings
at the same time. When a "dynamic" mode string is copied, the buffer is referenced
and its reference counter is incremented. If a reference is released the counter
is decremented and the buffer gets freed when the counter reaches zero. If the
string references a buffer used by other strings it is in "shared-dynamic" or just
"shared" mode. When a string is first created and needs to store more than can fit
into "short" mode, it allocates a buffer and marks itself as being in "owned-dynamic"
mode. On copying it and the copied string both transition to "shared" mode. If a
write operation happens to one of the two strings it needs to ensure that the
operation won't reflect in the other string, thus forcing it to now allocate its
own buffer and copying the data before executing the write operation. Both are now
again in "owned" mode. If the same happened with three strings in "shared" mode,
the untouched two would be unaffected and remain in "shared" mode.

This feature doesn't do any dynamic deduplication like a "fly-string". Creating
two strings in "dynamic" mode with the same contents will not trigger "shared" mode.
"Shared" mode is only a result of copy-construction and assignment.

Keep in mind, that the reference counting described above doesn't use any atomic
instructions or synchronization. If a string in "shared" mode is therefore used
by multiple threads it is not sufficient to guard it with a mutex or equivalent.
Always make sure the string is in "owned" mode by calling ```reserve()``` before
handing it over to another thread to prevent nasty side-effects.

The decision against atomic reference counting is based on
[this article](https://snf.github.io/2019/02/13/shared-ptr-optimization/) describing
how it is sometimes automatically disabled in ```std::shared_ptr<T>```.

## Allocation free string literals 📃
Creating a string from a string constant is detected using some template magic,
to prevent the array of chars from being decayed into a ```const char*``` pointer
by using an array-reference. This prevents an allocation and buffer length
calculation. But it also assumes that a provided buffer has an indefinite life
span. Handing it a non-static stack local array would result in a dangling
pointer inside the string. Like "shared" mode strings, "literal" mode strings
transition to being "owned" on any kind of write operation.

```C++
String s1= "Hello world!";    // Ok

static char staticBuffer[200]= "Hello solarsystem!";
String s2= staticBuffer;      // Also ok

char stackLocalBuffer[200]= "Hello universe!";
String s3= stackLocalBuffer;  // Bad!
```

The buffers need to be large enough, that the string won't be generated in "short"
mode.

## Lazy length calculation 💤
As the string text data is UTF-8 encoded, the number of stored bytes in the buffer
might not correlate to the number of code points represented by them. Therefore
the length needs to be calculated with an O(n) (linear) time operation. This is
as much avoided as possible and will only be done on demand when the length is
actually queried. Moreover known length values are as much reused and propagated
during copying and appending.

## Quirks ⚡
The distinguishing features of the library come with a few quirks to keep in mind:
* UTF-8 Encoding: As the encoding of the text data is in UTF-8, any indexing
  of the n-th code point is an O(n) (linear) operation. Furthermore setting a
  codepoint might trigger a reallocation even if the string is already in "owned"
  mode if the byte lengths of the old and new code points differ.

* Literal Strings: Construction from any array (not pointer) assumes that its
  lifespan is infinite (or at least as long as the strings lifespan). Construction
  from a stack local array will create a stale pointer. This feature is supposed
  to be used with string literals.

* Copy on write: As the backing buffer might be shared, any writes to the string
  need to be tracked to trigger a transition into "owned" mode. Therefore no mutable
  references into the buffer are handed out. Instead ```CharRef``` objects are used
  that will do a code point lookup by index on every write.
  Non-mutable references might go stale after a reallocation due to an append operation
  for example.

* The reference counting used in "dynamic" mode is limited to single threaded
  use. Therefore either a simultaneously accessed string is only being read or
  it is guaranteed to be in "owned" mode, for example by calling ```reserve()```.
  The same goes for strings copied, moved or handed over to other threads.


## License
MIT-License, see the ```LICENSE``` file for more details
