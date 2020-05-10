# Meek

Meek is a statically typed, imperative, procedural programming language.

Meek is currently under development.

It looks a lot like C right now, but I have plenty of ideas to try out once development is further along.

The initial implementation compiles to bytecode which gets interpreted by a stack-based VM. This lets me rapidly prototype with a single back-end. Ultimately, I would like to integrate LLVM and output a compiled executable. The VM target will still be used to support compile-time code execution.

Currently working on...

- ~Scanner~
  - 100% complete
- ~Parser~
  - 100% complete
- Analysis (type checking, name resolution, etc.)
  - 80% complete
- Bytecode emitter + interpreter
  - 30% complete
- LLVM output + integration
  - 0% complete
