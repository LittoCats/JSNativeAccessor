console.log 'JSNativeAccessor ... ...'

# console.log new JSNativeAccessor.Void(123).pointer.value

# JSNativeAccessor.typedef {
#   width: JSNativeAccessor.Float
#   height: JSNativeAccessor.Float

{FFI, STDC, Buffer, typedef} = NativeAccessor

# isalnum = do ({isalnum} = STDC.type, cb = ->)->
#   isalnum.call = new FFI.Signature FFI.BuildIn.Int, FFI.BuildIn.Int
#   return (num)->
#     return false unless typeof num is 'number'
#     rv = new FFI.BuildIn.Int 0
#     num = new FFI.BuildIn.Int num
#     isalnum.call rv, num , cb
#     return rv.getValue() isnt 0

# console.log "#{num} is #{if isalnum num then '' else 'not'} a Number" for num in [0...128]

# count = 100000
# console.log 'start ...'
# start = new Date().getTime()

# # while count-- then new Date
# "#{num} is #{if isalnum num then '' else 'not'} a Number" for num in [0...128]

# end = new Date().getTime()

# console.log "total time: #{end - start}"


sig = new FFI.Signature FFI.BuildIn.Void

Callback = new sig

cb = new Callback ->console.log 'JavaScript Function for C Method from JavaScript'

Echo = new FFI.BuildIn.Pointer echo
# Echo.setValue echo

Echo.call = new FFI.Signature FFI.BuildIn.Void, Callback

Echo.call new FFI.BuildIn.Void, cb, ->
  console.log 'call c function async'

console.log "long long #{FFI.BuildIn.LongLong.__size__}"
console.log "long long #{FFI.BuildIn.Double.__size__}"

# console.log JSON.stringify ("#{key}": (key for key of val) for key, val of STDC), null, 2

print = do ({printf} = STDC.io, {Signature, BuildIn} = FFI)->
  printf.call = new Signature BuildIn.Void, BuildIn.Pointer
  rv = new BuildIn.Void
  async = (args ..., cb)->
    content = (arg.toString() for arg in args).join(',') + '\n'
    av = new BuildIn.Pointer new Buffer(content)
    printf.call rv, av, cb
  print = (args ...)->async.apply @, (args.push(undefined); args)
  print.async = async
  print


print.async 'log message from c printf as async call', ->
  console.log "print done."
print 'log message from c printf'

buffer = new Buffer 'EFAC', 'hex'
console.log buffer.length

current_second = new Date().getTime()

count = 1000000
{Pointer, Void, Int, UInt8} = FFI.BuildIn

int = new UInt8(100)
console.log int.getValue()
int.setValue 255
console.log int.getValue()

while count--
  # new UInt8 count
  # buffer = new Buffer
  # pointer = new Pointer buffer
  # pointer.__buffer__ = buffer
  new Pointer new Buffer

print "#{new Date().getTime() - current_second}"