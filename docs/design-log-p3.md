# Project 3 Design Log

**Author:** Logan Maher 

**P2 code used:** Instructor reference (`-DUSE_P2_REFERENCE=ON`) was used.

## 1. Hash table

`ToolRegistry` uses a hand-written hash table with separate chaining. The bucket array is a `std::vector` of bucket vectors. Each entry stores the tool's name and a `std::unique_ptr<Tool>`. Chaining is straightforward for this project because tools are never removed: collisions can be handled by scanning one bucket, and resizing does not require special deleted-slot markers. The hash function is 64-bit FNV-1a. It processes every byte in order, so different characters and different character order affect the result. A name's bucket index is computed separately using the hash modulo the current bucket count.

The table starts with 16 buckets and doubles when an insertion would raise the load factor above 0.75. The load factor is the number of registered tools divided by the number of buckets. Checking the threshold before insertion keeps the table comfortably below the required maximum of 1.0. Rehashing allocates the larger bucket array and moves each entry into its new bucket based on the new bucket count. `add()` is amortized O(1): most insertions examine only a short chain, and although a resize is O(n), doubling means that resize cost is spread across many later insertions.

The required 10,000-tool test produced these measurements on the test environment:

| Measurement | Result |
|---|---:|
| Registered tools | 10,000 |
| Bucket count | 16,384 |
| Load factor | 0.610352 |
| Longest FNV-1a chain | 6 |
| Longest chain using `name[0] + name.size()` | 9,000 |

The weak alternative ignores almost all of each name. For names such as `tool_0` through `tool_9999`, it places thousands of names into only a few buckets. The comparison demonstrates why a hash should consider every character and preserve order. The FNV-1a result also meets the required maximum chain length of 64.

## 2. Pointer stability

The registry stores each tool in a separately allocated object owned by `std::unique_ptr`. During rehashing, the entries and their smart pointers move between bucket vectors, but the actual `Tool` objects do not move in memory. Therefore, a `Tool*` returned by `find()` remains valid for the registry's lifetime, including after later insertions cause one or more resizes. The registry owns the objects and destroys each one when its entry is destroyed.

## 3. Calculator trace

For `2 * (3 + 4) - 5`, the evaluator keeps a values stack and an operators stack. It pushes `2` to values, then `*` to operators, then `(`. It pushes `3`, then `+`, then `4`. When `)` arrives, it applies `+` to `3` and `4`, pushing `7`, and removes the matching `(`. The stacks now contain values `[2, 7]` and operators `[*]`. On `-`, multiplication has higher precedence, so it applies `2 * 7` and pushes `14`, then pushes `-`. It pushes `5`; at the end, it applies `14 - 5`. The final value is `9`.

## 4. Errors as values

A rejected path or division by zero is an expected input-level failure, not necessarily a programming bug. Returning `false` and filling an error string lets the caller turn the failure into a tool result that the model can read and respond to. Throwing for ordinary user-input errors would make normal control flow harder to handle. Exceptions are more appropriate for exceptional implementation failures or violated programming assumptions; the dispatcher also provides a defensive catch boundary for tools.

## 5. What I would design differently

I would separate the registry's hash-table mechanics from the tool ownership details more explicitly if the project grew. For this assignment, a bucket vector and a small entry type keep the code easy to inspect. In a larger system, I would also add timing benchmarks alongside the chain-length measurements, because chain length is useful evidence about collisions but does not measure allocation and string-comparison costs directly.
