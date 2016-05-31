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
* `Structure`
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
 
`Numeric` 类型实例有一个特殊的方法 `toNumber()`，通过方法可以获取其内存储的数据。
因此，也可以把 `Numeric` 类型实例传递给 `Numeric` 构造函数。

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

    * `rtype` `BuildIn` or `Struct`
    * `atypes` `Array`
    * `abi` FFI_ABI

```coffee
constructor = (rtype, atypes, abi = FFI.ABI.DEFAULT)->

```

```c++
class Signature {
    ffi_cif* cif;
};
```
