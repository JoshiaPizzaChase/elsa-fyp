# Style guide
In the future, these will be enforced in a .clang-tidy file.

## Defining functions
Prefer to use the trailing return syntax introduced in C++11. It provides consistent function formatting and less cluttering as return types get longer and longer.
```
auto foo(int i) -> int;
auto bar(double y) -> double;
```

## Pointers
Always use standard library smart pointers, such as `std::unique_ptr` and `std::shared_ptr`. They enforce RAII and move semantics. Avoid naked `new` and `delete`, instead prefer `std::make_unique` and `std::make_shared` to replace `new`.

## For-loops
Avoid explicit loops, prefer range-based for loops.

## Containers
Avoid node containers in release builds. Node containers are slow as they are dispersed throughout memory. For example, `std::list` is a node container implemented via a doubly linked list.

## Linting and formatting
`.clang-format` and `.clang-tidy` have been defined to provide format rules and linting rules respectively. Feel free to add/remove opinionated rules.
