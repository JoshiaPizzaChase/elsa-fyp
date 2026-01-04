# Style guide
In the future, these will be enforced in a .clang-tidy file.

## Defining functions
Prefer to use the trailing return syntax introduced in C++11. It provides consistent function formatting and less cluttering as return types get longer and longer.
```
auto foo(int i) -> int;
auto bar(double y) -> double;
```

## Pointers
Always use standard library smart pointers, such as `std::unique_etr` and `std::shared_ptr`. They enforce RAII and move semantics. Avoid naked `new` and `delete`, instead prefer `std::make_unique` and `std::make_shared` to replace `new`.

## For-loops
Avoid explicit loops, prefer range-based for loops.
