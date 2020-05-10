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

Latest test program:

```
fn main()
{
	bool t = true;
	bool f = false;

	if t || t do print(1);
	else do      print(100);
	
	if t || f do print(2);
	else do      print(200);
	
	if f || t do print(3);
	else do      print(300);
	
	if f || f do print(400);
	else do      print(4);
	
	
	
	
	if t && t do print(5);
	else do      print(500);
	
	if t && f do print(600);
	else do      print(6);
	
	if f && t do print(700);
	else do      print(7);
	
	if f && f do print(800);
	else do      print(8);
	
	int a = 7;
	int b = 6;
	b *= a;
	print(b);
}

/*
Output:
1
2
3
4
5
6
7
8
42
*/
```
