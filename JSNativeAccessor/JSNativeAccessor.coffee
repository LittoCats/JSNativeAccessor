# (JNA, Buffer, STDC, FFI)->

Object.defineProperty JNA, 'Buffer', value: Buffer

# Buffer Extension
Buffer::equals = (other)-> 0 is @compare other
Buffer::toJSON = -> "<Buffer #{@toString('hex', 0 , 16).match(/\S{8}/g).join('')}#{if @length > 16 then ' ...' else ''}>"

# 重写 BuildIn 类型的 toJSON 方法
do ({BuildIn} = FFI)->
  for N, T of BuildIn
    do (N, T)->T.toJSON or T.toJSON = ->N

typedef = do ({BuildIn, Structure} = FFI)->
  Object.defineProperty Structure::, '__init__', value: do -> __init__ = (instance, holder, noHolder = false)->
    offset = 0
    for [N, F] in @__fields__
      do (N, F, field = instance.slice offset, offset + F.__size__)->
        if F instanceof Structure
          __init__.call F, field, holder[N]
          Object.defineProperty instance, N, {
            get: -> field
            set: (holder = {})->field[N] = holder[N] for [N] in F.__fields__
          }
        else if F instanceof BuildIn
          Object.defineProperty field, 'getValue', value: F.__getValue__
          Object.defineProperty field, 'setValue', value: F.__setValue__
          Object.defineProperty instance, N, {
            get: -> field
            set: (holder = 0)->
              holder = do holder.getValue if holder instanceof Buffer and holder.getValue
              F.__setValue__.call field, holder if not noHolder
          }

        instance[N] = holder[N] if not noHolder

      offset += F.__size__

    # 设置 setValue 方法
    if @ instanceof Structure then instance.setValue = do (fields = @__fields__)->(holder)-> instance[N] = holder[N] for [N] in fields

  toJSON = ->
    (json or json = {})[N] = T for [N, T] in @__fields__
    json

  typedef = (define)->
    return define if define instanceof BuildIn
    fields = ([N, if F instanceof BuildIn then F else typedef F] for N, F of define)

    argv = ("fields[#{index}][1]" for index in [0...fields.length]).join ', '
    structure = eval "new Structure(#{argv})"
    Object.defineProperty structure, '__fields__', value: fields
    structure[N] = F for [N, F] in fields
    Object.defineProperty structure, 'toJSON', value: toJSON

    structure

do ({Signature, ABI, BuildIn, Structure} = FFI)->
  ABI[V] = N for N, V of ABI
  Signature::toJSON = ->
    ABI: ABI[@ABI]
    RType: @RType or BuildIn.Void
    ATypes: (T for I, T of @ATypes)

  Signature::__callable__ = (rval, argv ...)->
    try
      signature = @__FunctionPointer__.__signature__
      for index in [0...argv.length]
        if signature.ATypes[index] instanceof Structure
          # 初始化 自定义 Struct 的实例，但忽略 holder，因为参数是传过来的
          Structure::__init__.call signature.ATypes[index], argv[index], undefined, true
        else
          [argv[index].getValue, argv[index].setValue] = [signature.ATypes[index].__getValue__, signature.ATypes[index].__setValue__] 

      (signature.RType or BuildIn.Void).__setValue__.call rval, @__callback__.apply {}, argv if typeof @__callback__ is 'function'
    catch e
      console.log.e e if typeof console.log.e is 'function'

Object.defineProperty JNA, N, value: F for N, F of {Buffer: Buffer, STDC: STDC, FFI: FFI, typedef: typedef}
this.NativeAccessor = this.JSNativeAccessor = this.JNA = JNA