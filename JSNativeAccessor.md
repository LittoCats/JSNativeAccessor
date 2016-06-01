# JSNativAccessor

`JSNativeAccessor`(以下简称 `JNA`) 是 `JavaScriptCore` 全局对象的扩展属性，用于调用 `C` 接口函数。

`Buffer` 模块为 `C` 内存块抽象，主要是从系统分配一块内存，并提供了一系列操作方法。

* [Buffer]()

## Buffer

##### 构造方法 *Constructor*

###### new Buffer(size)
 
 * size Number#0
 
 Allocates a new buffer of size bytes.  size must be less than 1,073,741,824 bytes (1 GB) on 32 bits architectures or 2,147,483,648 bytes (2 GB) on 64 bits architectures, otherwise a RangeError is thrown.
 
 Unlike ArrayBuffers, the underlying memory for buffers is not initialized. So the contents of a newly created Buffer are unknown and could contain sensitive data. Use buf.fill(0) to initialize a buffer to zeroes.
 
###### new Buffer(array)
 
 * array Array
 
 Allocates a new buffer using an array of octets.
 
###### new Buffer(buffer)
 
 * buffer Buffer
 
 Copies the passed buffer data onto a new Buffer instance.
 
###### new Buffer(str[, encoding])#
 
 * str String - string to encode.
 
 * encoding String - encoding to use, Optional. Currently, only `hex` `base64` `utf8` supported.
 
 Allocates a new buffer containing the given str. encoding defaults to 'utf8'.


##### 实例方法 *Instance Method*

###### buf.compare(otherBuffer)

* otherBuffer Buffer
 
 Returns a number indicating whether this comes before(1) or after(-1) or is the same(0) as the otherBuffer in sort order.

######buf.fill(value[, offset][, end])

* value 
* offset Number, Optional
* end Number, Optional

    Fills the buffer with the specified value. If the offset (defaults to 0) and end (defaults to buffer.length) are not given it will fill the entire buffer.
    
    If value is String, fills the buffer with the first byte in value with utf8 encode.
    
###### buf.write(string[, offset][, length][, encoding])#

* string String - data to be written to buffer

* offset Number, Optional, Default: 0

* length Number, Optional, Default: buffer.length - offset

* encoding String, Optional, Default: 'utf8'. Currently, only `hex` `base64` `utf8` supported.

    Writes string to the buffer at offset using the given encoding. offset defaults to 0, encoding defaults to 'utf8'. length is the number of bytes to write. Returns number of octets written. If buffer did not contain enough space to fit the entire string, it will write a partial amount of the string. length defaults to buffer.length - offset. The method will not write partial characters.
    
    ```coffee
buf = new Buffer 256
len = buf.write '\u00bd + \u00bc = \u00be', 0
console.log "#{len} bytes: #{buf.toString 'utf8', 0, len}"
    ```

###### buf.slice([start[, end]])

* start Number, Optional, Default: 0

* end Number, Optional, Default: buffer.length

    Returns a new buffer which references the same memory as the old, but offset and cropped by the start (defaults to 0) and end (defaults to buffer.length) indexes. Negative indexes start from the end of the buffer.

    **Modifying the new buffer slice will modify memory in the original buffer!**
    
    Example: build a Buffer with the ASCII alphabet, take a slice, then modify one byte from the original Buffer.
    
    ```coffee
buf1 = new Buffer 26
for i in [0...26]
  buf1[i] = i + 97 #97 is ASCII a
buf2 = buf1.slice 0, 3
console.log buf2.toString 'ascii', 0, buf2.length
buf1[0] = 33
console.log buf2.toString 'ascii', 0, buf2.length
# abc
# !bc
    ```
    
###### buf.toString([encoding][, start][, end])

* encoding String, Optional, Default: 'utf8'

* start Number, Optional, Default: 0

* end Number, Optional, Default: buffer.length

    Decodes and returns a string from buffer data encoded using the specified character set encoding. If encoding is undefined or null, then encoding defaults to 'utf8'. The start and end parameters default to 0 and buffer.length when undefined.
    
    ```coffee
buf = new Buffer 26
for i in [0...26]
  buf[i] = i + 97           # 97 is ASCII a
buf.toString 'ascii'        # outputs: abcdefghijklmnopqrstuvwxyz
buf.toString 'ascii',0,5    # outputs: abcde
buf.toString 'utf8',0,5     # outputs: abcde
buf.toString undefined,0,5  # encoding defaults to 'utf8', outputs abcde
    ```

##### 实例属性 *Instance Property*

###### buf.length

* Number

    The size of the buffer in bytes. Note that this is not necessarily the size of the contents. length refers to the amount of memory allocated for the buffer object. It does not change when the contents of the buffer are changed.
    
###### buf.\_\_pointer__

* Buffer
    * pointer.\_\_buffer__ Readonly

    `Buffer` 的 `__pointer__` 属性实际上也是一个 `Buffer` 实例，其内存块存储了指向原 `Buffer` 实例的内存块的指针。\_\_pointer__ 的 \_\_buffer__ 属性是对源 `Buffer` 实例的引用。


## FFI

三个关键类：`BuildIn`  `Struct` `Signature`

两个关键函数：

#### BuildIn

顾名思义，`BuildIn` 即内建类，包含常用的基本数据类型，所有的 `BuildIn` 对象既可当作构造函数，也可以当作普通函数调用，但结果为 `Buffer` 实例，`BuildIn` 只是语法糖而已，因此 `BuildIn`（`Buffer`） 实例无法通过 `instanceof` 操作符进行内类检测，可以通过 `BuildIn.isBuildIn(ins)` 方法进行类型判断。

###### isBuildIn

```coffee
BuildIn.isBuildIn = (ins)-> ins instanceOf Buffer
```

* ins `Object`  待测试对象
* return `Boolean` 

`BuildIn` 有以下几种类型：

* `Void`
* `Pointer`
* `Numeric`
    * `Float`
    * `Double`
    * `LongDouble`
    * `Bool`
    * `UInt8`
    * `Int8`
    * `UInt16`
    * `Int16`
    * `UInt32`
    * `Int32`
    * `UInt64`
    * `Int64`
    

`Numeric` 类型下面还有一些别名，这些别名指向的类型，是 `Numeric` 类型中整型的一种：

* `UChar`
* `Char`
* `UShort`
* `Short`
* `ULong`
* `Long`
* `ULongLong`
* `LongLong`
* `Size`
* `SSize`
 
`BuildIn` 实例具有两个 `Buffer` 实例没有的方法：

```coffee
buildIn.getValue = ->
buildIn.setValue = ->
```

`Numeric` 实例，getValue 方法返回一个 Number 对象，但是由于 `vm` 中的数值只有 double，因此部分类型将会溢出。

`Pointer` 实例的 getValue 方法返回一个字符串 (e.g. 0x0000000100704570，部分平台不显示多余的0： 0x100704570)，该字符串是通过 `sprintf` 打印指针获得，不同平台，长度可能不同。



`Pointer` 类型用于存储一个内存指针，`Pointer` 构造函数的参数必须是 `Buffer` 实例，因为只有 `Buffer` 实例是 Ｃ 类型的数据，才能获取其指针；如果没有传递参数，则存储 `NULL` 指针。
同 `Buffer` 实例的 `__pointer__` 属性一样，`Pointer` 具有 `__buffer__` 属性。


`BuildIn` 类型可以通过 `instanceof` 测试：

```coffee
console.log UInt8 instanceof BuildIn
# true
```

###### Structure

`Structure` 是一个构造函数，用于构造结构体类型。

```coffee
# Structure 原型
constructor = (field, ...)->
```

通常情况下，应使用 `typedef` 方法定义新的结构体类型，而不应该直接使用 `new Structrue(define)` 定义结构体类型。

`typedef` 方法支持类型的嵌套定义，

```coffee
Rect = typedef 
  origin:
    x: Float
    y: Float
  size:
    width: Float
    height: Float
```
而 `new Structure(define)` 不支持。

`Structure` 类型可以通过 `instanceof` 测试：

```coffee
console.log Rect instanceof Structure
# true
```

###### Signature

`Signature` 是原生 `C` 函数的签名，生成以后可重复使用。

* `constructor`

    * `abi` FFI.ABI
    * `rtype` `BuildIn` or `Struct`
    * `atype ...` `BuildIn` or `Struct`
    

```coffee
constructor = (abi = FFI.ABI.DEFAULT, rtype, ...)->

```

`Signature` 构造函数的参数除 `abi` 外，其余参数必须为 `BuildIn` 或 `Struct` 类，`c++` 实现中并没有对类型进行检测，如果参数类型不正确，将产生不可预知的错误，这一点需要格外注意。

`Signature` 实例是一个伪函数对象，因此可以使用 `()` 操作符；因为 `Signature` 
实例只是描述了参数及返回值类型信息，并没有实现具体的功能，因此不能直接调用，必须将其赋值给函数指针对象，作为其方法调用；下例是调用标准Ｃ中 `isalnum` 的过程

**`Signature` 实例同时也是一人伪构造函数**，可以通过 `new` 操作符新建一个函数指针类型 `FunctionPointer`。

`FunctionPointer` 类型同其它 `BuildIn` 类型类似，可以传入一个 `Function` 对象进行实例化，得到一个函数指针（同 `Pointer` 类型实例），该函数指针可以作为参数传入 `C` 方法做为回调函数。

***注意：*** 因为 `C` 指针可能被传递到一些 `static` 变量或其它一些异步使用的场景中，因此编程中需要注意，必须保证传递给 `C` 函数的所有变量在 `C` 函数使用时没有被 `GC` 回收。

```coffee
{isalnum} = STDC.type
isalnum.call = new Signature FFI.BuildIn.Int, FFI.BuildIn.Int

rvalue = new FFI.BuildIn.Int
avalue = new FFI.BuildIn.Int 98
isalnum.call rvalue, avalue

console.log rvalue.getValue()
# 1
```

#### Best Practice

这里提供一个简单的 C 函数的包装方式：

```coffee
isalnum = do ({isalnum} = STDC.type, cb = ->)->
  isalnum.call = new FFI.Signature FFI.BuildIn.Int, FFI.BuildIn.Int
  return (num)->
  return false unless typeof num is 'number'
  rv = new FFI.BuildIn.Int 0
  num = new FFI.BuildIn.Int num
  isalnum.call rv, num , cb
  return rv.getValue() isnt 0

console.log isalnum 98  # true
console.log isalnum 96  # false
```