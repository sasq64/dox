# dox

C++ documentation generator

`dox -i <infile> -o <outfile>`
  
Infile is any format, with escape codes to allow insertion of special commands.

`{ any code here }`

### Available functions

`print(text)`

Output text into document at this point

`source(sourceFile)`

Parse the given source file as the current AST

`symbol(name)`

Set the current "symbol", cause an error if symbol does not exist in current AST.

Symbols should be a defintion of a class, function, enum

`class(class)`

Start documenting class. Set class as current symbol

`enddoc()`

`method(name)`

`string doc_comment(symbol)`

Extract the comment preceding symbol as text




`param(name)` or `param(no)`

## Example

{ class('File') }
== File class
{ method('open') ; param('name') }
`open(name)`
Opens a file with the name `name`.

## Short hand

`x{y} => { x('y') ; print('y')`

##

```
### doc{File} class
`method@{open}(param@{name})`
Opens a file with the name param@{0}
```

## Classic generation from comments

```
{
    forMethods(class, function(method) {
      print('## ' + method.printableName)
      text = doc_comment(method)
      print text
    });
 }
 ```

## Generation

We deal with several types of generation; the basic one is generating the target, readable document.

There is also _boot strapping_ generation, where we fill in missing data in the source document.
For instance, we can fill in method documentation following a `doc(class)`

The third is _reverse_ generation, where we can update the source files -- for instance putting documentation
as comments above the proto type
