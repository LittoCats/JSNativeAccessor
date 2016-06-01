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
  Object.defineProperty Structure::, '__init__', value: do -> __init__ = (instance, holder = {})->
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
            set: (holder = 0)->F.__setValue__.call field, if holder instanceof Buffer and holder.getValue then holder.getValue() else holder
          }

        instance[N] = holder[N]

      offset += F.__size__

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

do ({Signature, ABI, BuildIn} = FFI)->
  ABI[V] = N for N, V of ABI
  Signature::toJSON = ->
    ABI: ABI[@ABI]
    RType: @RType or BuildIn.Void
    ATypes: (T for I, T of @ATypes)

  Signature::__callable__ = (rval, argv ...)->
    try
      signature = @__FunctionPointer__.__signature__
      [argv[index].getValue, argv[index].setValue] = [signature.ATypes[index].__getValue__, signature.ATypes[index].__setValue__] for index in [0...argv.length]
      
      (signature.RType or BuildIn.Void).__setValue__.call rval, @__callback__.apply {}, argv if typeof @__callback__ is 'function'
    catch e
      console.log.e e if typeof console.log.e is 'function'

Object.defineProperty JNA, N, value: F for N, F of {Buffer: Buffer, STDC: STDC, FFI: FFI, typedef: typedef}
this.NativeAccessor = JNA