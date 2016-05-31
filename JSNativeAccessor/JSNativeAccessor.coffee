# (JNA, Buffer, STDC, FFI)->

Object.defineProperty JNA, 'Buffer', value: Buffer

# Buffer Extension
Buffer::equals = (other)-> 0 is @compare other
Buffer::toJSON = -> "<Buffer #{@toString('hex', 0 , 16).match(/\S{8}/g).join('')}#{if @length > 16 then ' ...' else ''}>"

console.log FFI.BuildIn.Short.__size__

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
            set: (holder = 0)->
              holder = do holder.getValue if holder instanceof Buffer and holder.getValue
              F.__setValue__.call field, holder
          }

        instance[N] = holder[N]

      offset += F.__size__

  typedef = (define)->
    return define if define instanceof BuildIn
    fields = []
    for N, F of define
      F = typedef F unless F instanceof BuildIn or F instanceof Structure
      fields.push [N, F]

    argv = ("fields[#{index}][1]" for index in [0...fields.length]).join ', '
    structure = eval "new Structure(#{argv})"
    Object.defineProperty structure, '__fields__', value: fields
    structure[field[0]] = field[1] for field in fields
    structure

Cube = typedef x: FFI.BuildIn.Float, y: FFI.BuildIn.Float, z: u: FFI.BuildIn.Bool, l: FFI.BuildIn.Double, b: FFI.BuildIn.Short
console.log (F for F of Cube)
  
cube =  new Cube x: 128,y: 512, z: u: 64, b: 16, l: 7461
# console.log cube.x.getValue()
# console.log cube.z.b.getValue()
cube.z.l = new FFI.BuildIn.Float 1234
console.log cube.z.l.getValue()

# console.log new FFI.BuildIn.Pointer(new Buffer).getValue()